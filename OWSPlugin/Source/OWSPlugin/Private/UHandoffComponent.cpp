// Copyright Open World Server, PLLC.

#include "UHandoffComponent.h"
#include "OWSAPISubsystem.h"
#include "OWSS2SCommsManager.h"
#include "OWSPlayerController.h" // Added for ClientRPC call
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HttpModule.h" // For FHttpModule
#include "Interfaces/IHttpResponse.h" // For FHttpResponsePtr
// Removed: #include "OWSGameInstance.h" - This file does not exist in the provided context

DEFINE_LOG_CATEGORY(LogOWSHandoffComponent);

UHandoffComponent::UHandoffComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // No tick needed for now
    bIsHandoffInProgress = false;

    // Default values, should be overridden by game specifics or configuration
    CurrentGridCellID = TEXT("MockInitialCell_001");
    SourceWorldServerID = 1; // Placeholder, should be set based on actual server ID
}

void UHandoffComponent::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("BeginPlay: World is null."));
        return;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("BeginPlay: GameInstance is null."));
        return;
    }

    OWSAPISubsystem = GameInstance->GetSubsystem<UOWSAPISubsystem>();
    if (!OWSAPISubsystem)
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("BeginPlay: OWSAPISubsystem is not available."));
    }

    // Attempt to get UOWSS2SCommsManager from GameInstance as a subsystem
    OWSS2SCommsManager = GameInstance->GetSubsystem<UOWSS2SCommsManager>();
    
    if (!OWSS2SCommsManager)
    {
        // If not found as a subsystem, create a new instance. 
        // This is not ideal for a manager that should ideally be a singleton,
        // but ensures the component can function for now.
        // A better approach would be to ensure UOWSS2SCommsManager is registered as a GameInstanceSubsystem.
        UE_LOG(LogOWSHandoffComponent, Warning, TEXT("BeginPlay: OWSS2SCommsManager not found as a GameInstance subsystem. Creating a new instance for this component. This may lead to issues if multiple HandoffComponents exist. Consider registering UOWSS2SCommsManager as a UGameInstanceSubsystem."));
        OWSS2SCommsManager = NewObject<UOWSS2SCommsManager>(GameInstance, TEXT("HandoffCommsManager_Instance"));
        // It's crucial that if created this way, its lifecycle is managed, potentially by the GameInstance or this component needs to ensure it's cleaned up.
        // However, UObjects created with a UGameInstance outer are typically managed by the GameInstance.
    }

    if (OWSS2SCommsManager)
    {
        // Bind to the S2S acknowledgment delegate
        OWSS2SCommsManager->OnAcknowledgmentReceived.AddUniqueDynamic(this, &UHandoffComponent::HandleS2SAcknowledgment);
        UE_LOG(LogOWSHandoffComponent, Log, TEXT("Successfully bound to OnAcknowledgmentReceived."));

        // Start S2S Listeners required for this component's role (primarily S_Src listening for Acks)
        // S_Tgt listener (ReceiveState) would be started by game logic on S_Tgt when it expects handoffs.
        // S_Src listener (AcknowledgePreparation) is crucial for the handoff flow.
        if (!OWSS2SCommsManager->StartAcknowledgePreparationListener())
        {
            UE_LOG(LogOWSHandoffComponent, Error, TEXT("Failed to start S2S Acknowledge Preparation Listener. Handoffs may not complete."));
        }
        else
        {
            UE_LOG(LogOWSHandoffComponent, Log, TEXT("S2S Acknowledge Preparation Listener started successfully."));
        }
    }
    else
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("OWSS2SCommsManager is NULL. Cannot bind to OnAcknowledgmentReceived or start listeners."));
    }

    // Start the timer for checking boundary transitions
    World->GetTimerManager().SetTimer(TimerHandle_CheckBoundaryTransition, this, &UHandoffComponent::CheckForBoundaryTransition, BoundaryCheckInterval, true);
    UE_LOG(LogOWSHandoffComponent, Log, TEXT("Boundary check timer started with interval: %f seconds."), BoundaryCheckInterval);
}

void UHandoffComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(TimerHandle_CheckBoundaryTransition);
    }

    if (OWSS2SCommsManager)
    {
        OWSS2SCommsManager->OnAcknowledgmentReceived.RemoveDynamic(this, &UHandoffComponent::HandleS2SAcknowledgment);
        // Optionally stop listeners if this component was the sole user, but generally listeners are game instance level
    }
}

APlayerController* UHandoffComponent::GetPlayerController() const
{
    ACharacter* OwningChar = Cast<ACharacter>(GetOwner());
    if (OwningChar)
    {
        return Cast<APlayerController>(OwningChar->GetController());
    }
    return nullptr;
}

ACharacter* UHandoffComponent::GetOwningCharacter() const
{
    return Cast<ACharacter>(GetOwner());
}

FString UHandoffComponent::GetPlayerUserSessionGUID() const
{
    // Placeholder: In a real scenario, this would come from the player's session state,
    // often managed by a system like OWSUserSession.
    APlayerController* PC = GetPlayerController();
    if (PC && PC->PlayerState && !PC->PlayerState->GetUniqueId().ToString().IsEmpty()) // Using UniqueId as a proxy
    {
        // This is likely NOT the UserSessionGUID you need. This is the UE net unique ID.
        // You'll need to integrate with your actual UserSessionGUID source.
        // For now, returning a mock one if the unique ID is too simple.
        FString NetId = PC->PlayerState->GetUniqueId().ToString();
        if (NetId.Len() < 10) return TEXT("MockUserSessionGUID_Player_") + NetId; // Make it more GUID like for testing
        return NetId; 
    }
    return TEXT("MockUserSessionGUID_UnknownPlayer_ABC123"); // Fallback mock
}

FString UHandoffComponent::GetCharacterName() const
{
    // Placeholder: Get from Character's state or PlayerState
    ACharacter* Char = GetOwningCharacter();
    if (Char)
    {
        // APlayerState* PS = Char->GetPlayerState(); if using PlayerState for name
        // return PS ? PS->GetPlayerName() : TEXT("MockCharacter");
        return Char->GetName(); // Returns the Actor's FName, might be like "MyCharacter_C_0"
    }
    return TEXT("MockCharacter_Unknown"); // Fallback mock
}

void UHandoffComponent::CheckForBoundaryTransition()
{
    if (bIsHandoffInProgress)
    {
        // UE_LOG(LogOWSHandoffComponent, Log, TEXT("CheckForBoundaryTransition: Handoff already in progress. Skipping."));
        return;
    }

    // Placeholder: Simulate boundary crossing.
    // In a real game, this would check player position against grid cell boundaries.
    // For now, let's trigger it manually or after a few checks.
    static int CheckCount = 0;
    CheckCount++;

    // Example: Trigger after 3 checks (15 seconds if interval is 5s)
    if (CheckCount > 3) 
    {
        UE_LOG(LogOWSHandoffComponent, Log, TEXT("CheckForBoundaryTransition: Simulated boundary crossed. Initiating handoff preparation to MockTargetCellID: %s"), *MockTargetGridCellID);
        InitiateHandoffPreparation(MockTargetGridCellID);
        CheckCount = 0; // Reset for next simulated transition
    }
    else
    {
        // UE_LOG(LogOWSHandoffComponent, Log, TEXT("CheckForBoundaryTransition: No boundary crossed yet. (Check %d)"), CheckCount);
    }
}

void UHandoffComponent::InitiateHandoffPreparation(const FString& TargetGridCellID)
{
    if (bIsHandoffInProgress)
    {
        UE_LOG(LogOWSHandoffComponent, Warning, TEXT("InitiateHandoffPreparation: Handoff already in progress. Request for TargetCell %s ignored."), *TargetGridCellID);
        return;
    }

    if (!OWSAPISubsystem)
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("InitiateHandoffPreparation: OWSAPISubsystem is null. Cannot make API call."));
        return;
    }

    bIsHandoffInProgress = true;
    HandoffTargetGridCellID = TargetGridCellID; // Store for later use

    FRequestHandoffPreparation RequestData;
    RequestData.PlayerUserSessionGUID = GetPlayerUserSessionGUID();
    RequestData.CharacterName = GetCharacterName();
    RequestData.SourceWorldServerID = SourceWorldServerID; // From component property
    RequestData.CurrentGridCellID = CurrentGridCellID;   // From component property
    RequestData.TargetGridCellID = TargetGridCellID;

    FString JsonRequestBody;
    if (!RequestData.ToJsonString(JsonRequestBody))
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("InitiateHandoffPreparation: Failed to serialize FRequestHandoffPreparation to JSON."));
        bIsHandoffInProgress = false;
        return;
    }

    UE_LOG(LogOWSHandoffComponent, Log, TEXT("InitiateHandoffPreparation: Sending request to /api/Handoff/RequestHandoffPreparation for Char: %s, TargetCell: %s. Body: %s"), 
        *RequestData.CharacterName, *RequestData.TargetGridCellID, *JsonRequestBody);

    // Define the callback using a lambda
    FProcessSimpleHttpRequestCompleteDelegate RequestCompleteDelegate;
    RequestCompleteDelegate.BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        this->OnHandoffPreparationResponseReceived(Request, Response, bWasSuccessful);
    });

    // Make the API call using OWSAPISubsystem
    OWSAPISubsystem->PostStandard(
        TEXT("/api/Handoff/RequestHandoffPreparation"), 
        JsonRequestBody, 
        RequestCompleteDelegate
    );
}

void UHandoffComponent::OnHandoffPreparationResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("OnHandoffPreparationResponseReceived: Request failed or response invalid. Error: %s"), 
            (Response.IsValid() ? *Response->GetContentAsString() : TEXT("No response")));
        bIsHandoffInProgress = false;
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    UE_LOG(LogOWSHandoffComponent, Log, TEXT("OnHandoffPreparationResponseReceived: Received response. Code: %d, Body: %s"), Response->GetResponseCode(), *ResponseBody);

    FHandoffPreparationResponse HandoffResponse;
    if (!HandoffResponse.FromJsonString(ResponseBody))
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("OnHandoffPreparationResponseReceived: Failed to deserialize FHandoffPreparationResponse from JSON: %s"), *ResponseBody);
        bIsHandoffInProgress = false;
        return;
    }

    if (HandoffResponse.CanProceed && !HandoffResponse.TargetWorldServerS2SEndpoint.IsEmpty())
    {
        StoredTargetWorldServerS2SEndpoint = HandoffResponse.TargetWorldServerS2SEndpoint;
        UE_LOG(LogOWSHandoffComponent, Log, TEXT("OnHandoffPreparationResponseReceived: Handoff can proceed. Target S2S Endpoint: %s. Waiting for S2S acknowledgment from S_Tgt..."), *StoredTargetWorldServerS2SEndpoint);
        // Now we wait for HandleS2SAcknowledgment to be called via the delegate from OWSS2SCommsManager
    }
    else
    {
        UE_LOG(LogOWSHandoffComponent, Warning, TEXT("OnHandoffPreparationResponseReceived: Handoff cannot proceed or Target S2S Endpoint is missing. Reason: %s"), *HandoffResponse.ErrorMessage);
        bIsHandoffInProgress = false;
    }
}

void UHandoffComponent::HandleS2SAcknowledgment(const FString& Message)
{
    if (!bIsHandoffInProgress)
    {
        UE_LOG(LogOWSHandoffComponent, Warning, TEXT("HandleS2SAcknowledgment: Received acknowledgment but handoff is not marked as in progress. Message: %s. Ignoring."), *Message);
        return;
    }

    if (StoredTargetWorldServerS2SEndpoint.IsEmpty())
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("HandleS2SAcknowledgment: Received acknowledgment but StoredTargetWorldServerS2SEndpoint is empty. Cannot proceed. Message: %s"), *Message);
        bIsHandoffInProgress = false; // Reset state
        return;
    }
    
    UE_LOG(LogOWSHandoffComponent, Log, TEXT("HandleS2SAcknowledgment: Received S2S Acknowledgment from S_Tgt: '%s'. Proceeding to send player state."), *Message);
    // Here, you might want to parse Message if it contains structured data (e.g. JSON "status":"ready")
    // For now, any message means S_Tgt is ready.
    
    TriggerPlayerStateSend();
}

void UHandoffComponent::TriggerPlayerStateSend()
{
    if (StoredTargetWorldServerS2SEndpoint.IsEmpty())
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("TriggerPlayerStateSend: TargetWorldServerS2SEndpoint is empty. Cannot send player state."));
        bIsHandoffInProgress = false; // Reset state
        return;
    }

    if (!OWSS2SCommsManager)
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("TriggerPlayerStateSend: OWSS2SCommsManager is null. Cannot send player state."));
        bIsHandoffInProgress = false; // Reset state
        return;
    }

    FPlayerStateDataForHandoff PlayerStateData;
    PlayerStateData.UserSessionGUID = GetPlayerUserSessionGUID();
    PlayerStateData.CharacterName = GetCharacterName();
    PlayerStateData.SourceServerS2SEndpoint = OWSS2SCommsManager->GetFullListenAddress(); // S_Src's own S2S endpoint for potential future use by S_Tgt
    PlayerStateData.TargetGridCellID = HandoffTargetGridCellID; // The cell on S_Tgt this player is going to

    // Placeholder Player Transform Data
    ACharacter* Char = GetOwningCharacter();
    if (Char)
    {
        PlayerStateData.PlayerPosition = FS2SVector(Char->GetActorLocation());
        PlayerStateData.PlayerRotation = FS2SVector(Char->GetActorRotation().Euler()); // FRotator to Euler for simplicity
        PlayerStateData.PlayerVelocity = FS2SVector(Char->GetVelocity());
    }
    else
    {
        PlayerStateData.PlayerPosition = FS2SVector(0.f, 0.f, 100.f); // Default position
        PlayerStateData.PlayerRotation = FS2SVector(0.f, 0.f, 0.f);
        PlayerStateData.PlayerVelocity = FS2SVector(0.f, 0.f, 0.f);
    }
    
    // Placeholder Snapshots (empty for now)
    // In a real scenario, these would be populated using FMemoryWriter
    // PlayerStateData.PlayerStateSnapshot = TArray<uint8>(); 
    // PlayerStateData.PawnStateSnapshot = TArray<uint8>();

    // Placeholder Dynamic Character Data
    PlayerStateData.CurrentHealth = 100.f;
    PlayerStateData.CurrentMana = 50.f;
    PlayerStateData.CurrentEnergy = 75.f;
    PlayerStateData.CurrentStamina = 80.f;

    // Placeholder Custom Character Data
    PlayerStateData.CustomCharacterData.Add(TEXT("LastKnownZone"), TEXT("StartingArea_North"));
    PlayerStateData.CustomCharacterData.Add(TEXT("FactionRank"), TEXT("Private"));

    // Placeholder Inventory
    FInventoryItemData Sword;
    Sword.ItemName = TEXT("Basic Sword");
    Sword.Quantity = 1;
    Sword.CustomData = TEXT("{\"damage\":10, \"rarity\":\"common\"}");
    PlayerStateData.Inventory.Add(Sword);

    FInventoryItemData Potion;
    Potion.ItemName = TEXT("Health Potion");
    Potion.Quantity = 5;
    PlayerStateData.Inventory.Add(Potion);

    UE_LOG(LogOWSHandoffComponent, Log, TEXT("TriggerPlayerStateSend: Sending player state for Character %s to S2S Endpoint: %s"), *PlayerStateData.CharacterName, *StoredTargetWorldServerS2SEndpoint);
    
    OWSS2SCommsManager->SendPlayerState(StoredTargetWorldServerS2SEndpoint, PlayerStateData);

    // After sending state, proceed to finalize on the source server.
    FinalizeHandoffOnSource();
}

void UHandoffComponent::FinalizeHandoffOnSource()
{
    // Placeholder: In a real scenario, this would involve removing the player's actor from the current server,
    // after S_Tgt confirms it has successfully received and processed the player state.
    // For now, we just log. The actual removal might happen after another S2S message from S_Tgt.
    FString CharName = GetCharacterName();
    UE_LOG(LogOWSHandoffComponent, Log, TEXT("FinalizeHandoffOnSource: Player %s handoff process initiated. State sent to S_Tgt. Pending S_Tgt confirmation and player removal from S_Src."), *CharName);
    
    // Reset handoff state
    bIsHandoffInProgress = false;
    StoredTargetWorldServerS2SEndpoint.Empty();
    HandoffTargetGridCellID.Empty();

    // Potentially, kick the player or put them in a "limbo" state
    // APlayerController* PC = GetPlayerController();
    // if (PC)
    // {
    //    PC->ClientTravel(TEXT("?/Game/Maps/TransitionMap"), ETravelType::TRAVEL_Absolute); // Example of moving player
    //    // Or just destroy the pawn: GetOwningCharacter()->Destroy();
    // }

    // Call ClientRPC to instruct client to travel
    AOWSPlayerController* OWSPlayerController = Cast<AOWSPlayerController>(GetPlayerController());
    if (OWSPlayerController && !StoredTargetWorldServerS2SEndpoint.IsEmpty())
    {
        // Placeholder: Parse IP and Port from StoredTargetWorldServerS2SEndpoint
        // Example: "http://127.0.0.1:7778" -> IP: "127.0.0.1", Port: 7778
        // This parsing logic needs to be robust.
        FString TargetIP = TEXT("");
        int32 TargetPort = 0;
        FString Scheme;
        FString AddressAndPort;

        if (StoredTargetWorldServerS2SEndpoint.Split(TEXT("://"), &Scheme, &AddressAndPort, ESearchCase::IgnoreCase))
        {
            FString PortString;
            if (AddressAndPort.Split(TEXT(":"), &TargetIP, &PortString, ESearchCase::IgnoreCase))
            {
                TargetPort = FCString::Atoi(*PortString);
            }
            else
            {
                // No port specified, assume default HTTP/HTTPS or game port if known
                TargetIP = AddressAndPort;
                UE_LOG(LogOWSHandoffComponent, Warning, TEXT("FinalizeHandoffOnSource: Port not found in S2S Endpoint '%s'. Using default 0."), *StoredTargetWorldServerS2SEndpoint);
                // TargetPort = SomeDefaultGamePort; // Or handle error
            }
        }
        else
        {
            UE_LOG(LogOWSHandoffComponent, Error, TEXT("FinalizeHandoffOnSource: Could not parse S2S Endpoint: %s"), *StoredTargetWorldServerS2SEndpoint);
            // Handle error - cannot proceed with ClientRPC if IP/Port are unknown
            bIsHandoffInProgress = false; // Reset state as we can't proceed
            return;
        }

        if (TargetIP.IsEmpty() || TargetPort == 0)
        {
            UE_LOG(LogOWSHandoffComponent, Error, TEXT("FinalizeHandoffOnSource: Failed to parse Target IP/Port from S2S Endpoint: %s. IP: %s, Port: %d"), 
                *StoredTargetWorldServerS2SEndpoint, *TargetIP, TargetPort);
            bIsHandoffInProgress = false; // Reset state
            return;
        }

        // CRITICAL: The HandoffToken should be received from S_Tgt's acknowledgment.
        // This part of the S2S communication (S_Tgt sending token to S_Src) is not yet detailed.
        // For now, using a placeholder. This needs to be correctly implemented.
        // The token should be part of the acknowledgment message S_Tgt sends to S_Src's /handoff/acknowledge_preparation
        // And then UHandoffComponent::HandleS2SAcknowledgment needs to parse it and store it.
        FString ActualHandoffTokenFromSTgt = TEXT("PlaceholderToken_GetFromServerAck"); // Placeholder!

        UE_LOG(LogOWSHandoffComponent, Log, TEXT("FinalizeHandoffOnSource: Calling ClientRPC_ExecuteSeamlessHandoff for Character %s to IP %s, Port %d, with Token %s"), 
            *CharName, *TargetIP, TargetPort, *ActualHandoffTokenFromSTgt);
        
        OWSPlayerController->ClientRPC_ExecuteSeamlessHandoff(TargetIP, TargetPort, ActualHandoffTokenFromSTgt, CharName);
    }
    else if (!OWSPlayerController)
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("FinalizeHandoffOnSource: PlayerController is not AOWSPlayerController or is null. Cannot call ClientRPC."));
        bIsHandoffInProgress = false; // Reset state
    }
    else
    {
        UE_LOG(LogOWSHandoffComponent, Error, TEXT("FinalizeHandoffOnSource: StoredTargetWorldServerS2SEndpoint is empty. Cannot determine target for ClientRPC."));
        bIsHandoffInProgress = false; // Reset state
    }
}
