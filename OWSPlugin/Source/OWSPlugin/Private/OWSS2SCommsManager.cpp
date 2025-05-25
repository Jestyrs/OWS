// Copyright Open World Server, PLLC.

#include "OWSS2SCommsManager.h"
#include "HttpModule.h"
#include "HttpServerModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h" // For FJsonObjectConverter
#include "Runtime/Online/HTTP/Public/HttpManager.h" // Required for FHttpManager
#include "Engine/Engine.h" // For GEngine
#include "OWSPlayerReconstructionManager.h" // Added to get token
#include "Engine/GameInstance.h" // For GetGameInstance()

DEFINE_LOG_CATEGORY(LogOWSS2SComms);

// UOWSS2SCommsManager::UOWSS2SCommsManager() {} // Constructor removed for Subsystem

void UOWSS2SCommsManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogOWSS2SComms, Log, TEXT("UOWSS2SCommsManager Initializing."));
    
    // Initialize PlayerReconstructionManager by getting it from the collection (it should also be a GameInstanceSubsystem)
    // This ensures it's created before this subsystem if not already.
    PlayerReconstructionManager = Collection.InitializeDependency<UOWSPlayerReconstructionManager>();
    if (!PlayerReconstructionManager)
    {
        // Fallback: Try to get from GameInstance directly if InitializeDependency didn't work as expected
        // (e.g. if PlayerReconstructionManager was not added to collection explicitly or is not a GISubsystem)
        UGameInstance* GameInstance = GetGameInstance();
        if (GameInstance)
        {
            PlayerReconstructionManager = GameInstance->GetSubsystem<UOWSPlayerReconstructionManager>();
        }
    }

    if (!PlayerReconstructionManager)
    {
        UE_LOG(LogOWSS2SComms, Warning, TEXT("PlayerReconstructionManager not found during Initialize. Token retrieval for acknowledgment might fail. Make sure UOWSPlayerReconstructionManager is registered as a GameInstanceSubsystem."));
    }
    
    LoadConfig(); // Load configuration properties like ListenPort from INI files if UCLASS(Config=Game) is used
    UE_LOG(LogOWSS2SComms, Log, TEXT("Using ListenAddress: %s, ListenPort: %d"), *ListenAddress, ListenPort);
}

void UOWSS2SCommsManager::Deinitialize()
{
    UE_LOG(LogOWSS2SComms, Log, TEXT("UOWSS2SCommsManager Deinitializing."));
    StopReceiveStateListener(); // Ensure listeners are stopped
    StopAcknowledgePreparationListener();

    // Consider stopping the HTTP server if this subsystem was the sole manager of it
    // This is important to free up the port.
    if (HttpRouter.IsValid())
    {
        // This is a bit simplified; proper management might involve checking if other users of the port exist.
        // FHttpServerModule::Get().StopHttpServer(HttpRouter); // This API might not exist or work as expected for shared routers.
        // Typically, routes are unbound, and the server itself is managed globally or per-port by the HttpServerModule.
        // For now, just ensure our routes are unbound.
        HttpRouter.Reset();
    }

    Super::Deinitialize();
}


FString UOWSS2SCommsManager::GetFullListenAddress() const
{
    // If ListenAddress is "0.0.0.0", it means listen on all available network interfaces.
    // For constructing a URL that can be *reached* by an external server,
    // we might need a more specific IP or a configured domain name.
    // However, for binding the listener, "0.0.0.0" is correct.
    // For now, this is mostly for logging and debugging.
    return FString::Printf(TEXT("http://%s:%d"), *ListenAddress, ListenPort);
}

// --- S_Tgt: Target Server Methods ---

bool UOWSS2SCommsManager::StartReceiveStateListener()
{
    if (!FHttpServerModule::Get().IsHttpServerAvailable())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("HTTP Server module is not available. Cannot start S2S listener."));
        return false;
    }

    HttpRouter = FHttpServerModule::Get().GetHttpRouter(ListenPort);
    if (!HttpRouter.IsValid())
    {
        // Try to bind to port if not already bound by GetHttpRouter
        HttpRouter = FHttpServerModule::Get().StartHttpServer(TArray<FHttpPath>(), ListenPort, false, FIPv4Address::Parse(ListenAddress));
    }
    
    if (!HttpRouter.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("Failed to get or start HTTP router on port %d."), ListenPort);
        return false;
    }

    FHttpRouteHandle ExistingHandle = HttpRouter->GetRouteHandle(TEXT("/handoff/receive_state"), TEXT("POST"));
    if(ExistingHandle.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Warning, TEXT("ReceiveStateListener: Route /handoff/receive_state POST was already bound. Unbinding first."));
        HttpRouter->UnbindRoute(ExistingHandle);
    }

    ReceiveStateRouteHandle = HttpRouter->BindRoute(
        FHttpPath(TEXT("/handoff/receive_state")),
        EHttpServerRequestVerbs::VERB_POST,
        FHttpServerRequestCompleted::CreateUObject(this, &UOWSS2SCommsManager::HandleReceiveStateRequest)
    );

    if (ReceiveStateRouteHandle.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: ReceiveStateListener started on %s/handoff/receive_state"), *GetFullListenAddress());
        return true;
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Tgt: Failed to bind /handoff/receive_state"));
        return false;
    }
}

void UOWSS2SCommsManager::StopReceiveStateListener()
{
    if (HttpRouter.IsValid() && ReceiveStateRouteHandle.IsValid())
    {
        HttpRouter->UnbindRoute(ReceiveStateRouteHandle);
        ReceiveStateRouteHandle.Reset(); // Invalidate the handle
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: ReceiveStateListener stopped."));
        // Consider stopping the whole FHttpServerModule listener if this was the only route,
        // but typically other routes might exist or be added.
        // FHttpServerModule::Get().StopHttpServer(HttpRouter);
    }
}

bool UOWSS2SCommsManager::HandleReceiveStateRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: Received request for /handoff/receive_state"));

    FString RequestBody = Request.BodyContext.GetBodyAsString();
    FPlayerStateDataForHandoff ReceivedState;
    bool bDeserializationSuccess = false;
    FString ResponseMessageToSrcPost; // Message for the immediate HTTP response to S_Src's POST
    FString SourceServerS2SEndpointToAck = TEXT("");
    FString CharacterNameToAck = TEXT(""); // Store CharName for ack

    if (ReceivedState.FromJsonString(RequestBody))
    {
        bDeserializationSuccess = true;
        SourceServerS2SEndpointToAck = ReceivedState.SourceServerS2SEndpoint;
        CharacterNameToAck = ReceivedState.CharacterName; // Get CharacterName

        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: Successfully deserialized PlayerStateDataForHandoff for Character: %s, UserSession: %s"), 
            *CharacterNameToAck, *ReceivedState.UserSessionGUID);
        
        if (OnPlayerStateReceived.IsBound())
        {
            // This delegate will trigger UOWSPlayerReconstructionManager::HandlePlayerStateReceived
            // which should generate and store the HandoffToken.
            OnPlayerStateReceived.Broadcast(ReceivedState); 
            ResponseMessageToSrcPost = TEXT("Player state received, processing initiated.");
        }
        else
        {
            UE_LOG(LogOWSS2SComms, Warning, TEXT("S_Tgt: OnPlayerStateReceived delegate is not bound. State data will be logged but not processed further. Handoff token might not be generated."));
            ResponseMessageToSrcPost = TEXT("Player state received, but no processor bound.");
        }
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Tgt: Failed to deserialize PlayerStateDataForHandoff. Payload: %s"), *RequestBody);
        ResponseMessageToSrcPost = TEXT("Error: Could not deserialize player state data.");
    }

    // Respond to the S_Src's initial POST request. This response DOES NOT contain the handoff token.
    TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(ResponseMessageToSrcPost, TEXT("text/plain"));
    HttpResponse->Code = bDeserializationSuccess ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::BadRequest;
    OnComplete(MoveTemp(HttpResponse));

    // After processing and responding to S_Src's POST, S_Tgt sends a *separate* acknowledgment POST back to S_Src.
    // This acknowledgment WILL contain the handoff token.
    if (bDeserializationSuccess && !SourceServerS2SEndpointToAck.IsEmpty() && !CharacterNameToAck.IsEmpty())
    {
        // Pass CharacterNameToAck so SendAcknowledgePreparation can fetch the token
        SendAcknowledgePreparation(SourceServerS2SEndpointToAck, CharacterNameToAck, true, TEXT("Player state processed, reconstruction initiated."));
    }
    else if (!SourceServerS2SEndpointToAck.IsEmpty()) // Deserialization failed, but we might know who to tell (if SourceServerS2SEndpoint was part of a wrapper object not PlayerStateDataForHandoff itself)
    {
        // If ReceivedState.FromJsonString failed, CharacterNameToAck might be empty.
        // If SourceServerS2SEndpointToAck is also empty (e.g. total garbage request), can't send ack.
        SendAcknowledgePreparation(SourceServerS2SEndpointToAck, CharacterNameToAck, false, TEXT("Failed to process player state (deserialization)."));
    }
    else
    {
         UE_LOG(LogOWSS2SComms, Error, TEXT("S_Tgt: Cannot send AcknowledgePreparation because SourceServerS2SEndpoint is unknown or deserialization failed too early."));
    }

    return true; // Request handled
}

// SendAcknowledgePreparation now includes CharacterNameToAck to fetch the token
void UOWSS2SCommsManager::SendAcknowledgePreparation(const FString& SourceServerS2SEndpoint, const FString& CharacterNameToAck, bool bCanProceed, const FString& Message)
{
    if (SourceServerS2SEndpoint.IsEmpty())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Tgt: SourceServerS2SEndpoint is empty. Cannot send AcknowledgePreparation."));
        return;
    }

    FString TargetURL = FString::Printf(TEXT("%s/handoff/acknowledge_preparation"), *SourceServerS2SEndpoint);
    UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: Sending AcknowledgePreparation to %s. Success: %s, Message: %s"), *TargetURL, bSuccess ? TEXT("true") : TEXT("false"), *Message);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(TargetURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField(TEXT("status"), bCanProceed ? TEXT("ready") : TEXT("error"));
    JsonObject->SetStringField(TEXT("message"), Message);
    JsonObject->SetStringField(TEXT("characterName"), CharacterNameToAck); // Include CharacterName for S_Src correlation

    FString HandoffToken = TEXT("");
    if (bCanProceed && PlayerReconstructionManager && !CharacterNameToAck.IsEmpty())
    {
        // Attempt to get the token from PlayerReconstructionManager
        // This method GetPendingHandoffTokenForCharacterAck needs to be created in UOWSPlayerReconstructionManager
        HandoffToken = PlayerReconstructionManager->GetPendingHandoffTokenForCharacterAck(CharacterNameToAck); 
    }
    
    if (!HandoffToken.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("handoffToken"), HandoffToken);
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: Including HandoffToken %s in AcknowledgePreparation for Character %s."), *HandoffToken, *CharacterNameToAck);
    }
    else if (bCanProceed)
    {
        UE_LOG(LogOWSS2SComms, Warning, TEXT("S_Tgt: HandoffToken is empty for Character %s even though bCanProceed is true. PlayerReconstructionManager valid: %s. CharacterNameToAck: %s"), 
            *CharacterNameToAck, PlayerReconstructionManager ? TEXT("Yes") : TEXT("No"), *CharacterNameToAck);
        // Still send the ack, but client handoff might fail if token is crucial.
        JsonObject->SetStringField(TEXT("handoffToken"), TEXT("")); // Explicitly send empty if expected
    }


    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOWSS2SCommsManager::OnAcknowledgePreparationResponseReceived);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Tgt: Failed to initiate AcknowledgePreparation request to %s."), *TargetURL);
    }
}

void UOWSS2SCommsManager::OnAcknowledgePreparationResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Tgt: AcknowledgePreparation sent successfully to %s. Response Code: %d, Body: %s"), 
            *Request->GetURL(), Response->GetResponseCode(), *Response->GetContentAsString());
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Tgt: Failed to send AcknowledgePreparation to %s. Error: %s"), 
            *Request->GetURL(), (Response.IsValid() ? *Response->GetContentAsString() : TEXT("No response/timeout")));
    }
}


// --- S_Src: Source Server Methods ---

void UOWSS2SCommsManager::SendPlayerState(const FString& TargetS2SEndpoint, const FPlayerStateDataForHandoff& PlayerStateData)
{
    if (TargetS2SEndpoint.IsEmpty())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Src: TargetS2SEndpoint is empty. Cannot send PlayerState."));
        return;
    }

    FString JsonPayload;
    if (!PlayerStateData.ToJsonString(JsonPayload))
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Src: Failed to serialize PlayerStateDataForHandoff for Character: %s"), *PlayerStateData.CharacterName);
        return;
    }

    FString TargetURL = FString::Printf(TEXT("%s/handoff/receive_state"), *TargetS2SEndpoint);
    UE_LOG(LogOWSS2SComms, Log, TEXT("S_Src: Sending PlayerState for Character %s to %s"), *PlayerStateData.CharacterName, *TargetURL);
    // UE_LOG(LogOWSS2SComms, Verbose, TEXT("S_Src: Payload: %s"), *JsonPayload); // Can be very verbose

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(TargetURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(JsonPayload);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOWSS2SCommsManager::OnPlayerStateResponseReceived);
    
    if (!HttpRequest->ProcessRequest())
    {
         UE_LOG(LogOWSS2SComms, Error, TEXT("S_Src: Failed to initiate SendPlayerState request to %s."), *TargetURL);
    }
}

void UOWSS2SCommsManager::OnPlayerStateResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Src: PlayerState sent to %s. Response Code: %d, Body: %s"), 
            *Request->GetURL(), Response->GetResponseCode(), *Response->GetContentAsString());
        // If S_Tgt's /handoff/receive_state returns a specific body on success, parse it here.
        // Currently, it just returns a plain text message.
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Src: Failed to send PlayerState to %s. Error: %s"), 
            *Request->GetURL(), (Response.IsValid() ? *Response->GetContentAsString() : TEXT("No response/timeout")));
    }
    // Note: The actual "acknowledgment" that S_Tgt is ready comes via a *separate* request 
    // from S_Tgt to S_Src's /handoff/acknowledge_preparation endpoint.
}

bool UOWSS2SCommsManager::StartAcknowledgePreparationListener()
{
    if (!FHttpServerModule::Get().IsHttpServerAvailable())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("HTTP Server module is not available. Cannot start S2S Acknowledge listener."));
        return false;
    }

    // Assume HttpRouter is already created by StartReceiveStateListener or needs to be created if this server is only an S_Src for a particular handoff.
    // If this manager can be *only* an S_Src and not an S_Tgt, it would need its own HttpRouter initialization.
    // For simplicity, we assume one UOWSS2SCommsManager per server instance, potentially handling both roles (e.g. server A sends to B, then B sends to A).
    if (!HttpRouter.IsValid())
    {
        HttpRouter = FHttpServerModule::Get().GetHttpRouter(ListenPort);
        if (!HttpRouter.IsValid())
        {
            HttpRouter = FHttpServerModule::Get().StartHttpServer(TArray<FHttpPath>(), ListenPort, false, FIPv4Address::Parse(ListenAddress));
        }
    }

    if (!HttpRouter.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("Failed to get or start HTTP router on port %d for Acknowledge listener."), ListenPort);
        return false;
    }

    FHttpRouteHandle ExistingHandle = HttpRouter->GetRouteHandle(TEXT("/handoff/acknowledge_preparation"), TEXT("POST"));
    if(ExistingHandle.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Warning, TEXT("AcknowledgePreparationListener: Route /handoff/acknowledge_preparation POST was already bound. Unbinding first."));
        HttpRouter->UnbindRoute(ExistingHandle);
    }

    AcknowledgePreparationRouteHandle = HttpRouter->BindRoute(
        FHttpPath(TEXT("/handoff/acknowledge_preparation")),
        EHttpServerRequestVerbs::VERB_POST,
        FHttpServerRequestCompleted::CreateUObject(this, &UOWSS2SCommsManager::HandleAcknowledgePreparationRequest)
    );

    if (AcknowledgePreparationRouteHandle.IsValid())
    {
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Src: AcknowledgePreparationListener started on %s/handoff/acknowledge_preparation"), *GetFullListenAddress());
        return true;
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Error, TEXT("S_Src: Failed to bind /handoff/acknowledge_preparation"));
        return false;
    }
}

void UOWSS2SCommsManager::StopAcknowledgePreparationListener()
{
    if (HttpRouter.IsValid() && AcknowledgePreparationRouteHandle.IsValid())
    {
        HttpRouter->UnbindRoute(AcknowledgePreparationRouteHandle);
        AcknowledgePreparationRouteHandle.Reset();
        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Src: AcknowledgePreparationListener stopped."));
    }
}

bool UOWSS2SCommsManager::HandleAcknowledgePreparationRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString RequestBody = Request.BodyContext.GetBodyAsString();
    UE_LOG(LogOWSS2SComms, Log, TEXT("S_Src: Received /handoff/acknowledge_preparation. Payload: %s"), *RequestBody);

    FString HandoffTokenReceived = TEXT("");
    FString ReceivedCharacterName = TEXT(""); // For S_Src to correlate if needed

    // Attempt to parse the JSON to make it more structured
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);
    FString StatusStr = TEXT("unknown");
    FString DetailMsg = TEXT("");

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        JsonObject->TryGetStringField(TEXT("status"), StatusStr);
        JsonObject->TryGetStringField(TEXT("message"), DetailMsg);
        JsonObject->TryGetStringField(TEXT("handoffToken"), HandoffTokenReceived); 
        JsonObject->TryGetStringField(TEXT("characterName"), ReceivedCharacterName); // Parse CharacterName

        UE_LOG(LogOWSS2SComms, Log, TEXT("S_Src: Parsed Acknowledgment: Status: %s, Message: %s, Token: %s, CharacterName: %s"), 
            *StatusStr, *DetailMsg, *HandoffTokenReceived, *ReceivedCharacterName);
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Warning, TEXT("S_Src: Could not parse JSON from acknowledge_preparation. Raw: %s"), *RequestBody);
    }
    
    if (OnAcknowledgmentReceived.IsBound())
    {
        // Broadcast with the new signature
        OnAcknowledgmentReceived.Broadcast(StatusStr, DetailMsg, HandoffTokenReceived);
    }
    else
    {
        UE_LOG(LogOWSS2SComms, Warning, TEXT("S_Src: OnAcknowledgmentReceived delegate is not bound. Acknowledgment will be logged but not processed further by game logic."));
    }

    // Respond to S_Tgt's POST request
    TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(TEXT("Acknowledgment processed by S_Src."), TEXT("text/plain"));
    HttpResponse->Code = EHttpServerResponseCodes::Ok;
    OnComplete(MoveTemp(HttpResponse));

    return true; // Request handled
}
