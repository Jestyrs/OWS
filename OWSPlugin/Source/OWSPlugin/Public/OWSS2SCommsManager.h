// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h" // Added for GameInstanceSubsystem base
#include "HttpServerModule.h" // For IHttpRouter
#include "Interfaces/IHttpRequest.h" // For IHttpRequest
#include "OWSPlayerStateData.h" // For FPlayerStateDataForHandoff
#include "OWSS2SCommsManager.generated.h"

class UOWSPlayerReconstructionManager; // Forward declaration

DECLARE_LOG_CATEGORY_EXTERN(LogOWSS2SComms, Log, All);

// Delegate for when acknowledgment is received on S_Src
// Parameters: Status (e.g. "ready", "error"), Message from S_Tgt, HandoffToken from S_Tgt
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnS2SAcknowledgmentReceived, const FString&, Status, const FString&, Message, const FString&, HandoffToken);
// Delegate for when player state is received on S_Tgt
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnS2SPlayerStateReceived, const FPlayerStateDataForHandoff&, PlayerState);


UCLASS(Config=Game, Blueprintable, BlueprintType) // Added Config=Game
class OWSPLUGIN_API UOWSS2SCommsManager : public UGameInstanceSubsystem // Changed base class
{
    GENERATED_BODY()

public:
    // UOWSS2SCommsManager(); // Constructor removed for Subsystem

    // Begin USubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // End USubsystem

    // --- Configuration ---
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "OWS S2S Comms | Configuration") // Added Config
    FString ListenAddress = TEXT("0.0.0.0");

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "OWS S2S Comms | Configuration") // Added Config
    int32 ListenPort = 7778; // Default S2S port, should be configurable per instance

    // --- S_Tgt: Target Server Methods ---

    /**
     * Starts the HTTP listener on S_Tgt to receive player state from S_Src.
     * Binds to /handoff/receive_state
     */
    UFUNCTION(BlueprintCallable, Category = "OWS S2S Comms | Target Server")
    bool StartReceiveStateListener();

    /**
     * Stops the HTTP listener on S_Tgt.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS S2S Comms | Target Server")
    void StopReceiveStateListener();

    /**
     * Delegate broadcast when player state is successfully received and parsed on S_Tgt.
     */
    UPROPERTY(BlueprintAssignable, Category = "OWS S2S Comms | Target Server")
    FOnS2SPlayerStateReceived OnPlayerStateReceived;

    // --- S_Src: Source Server Methods ---

    /**
     * Sends the player state data to the target server's S2S endpoint.
     * @param TargetS2SEndpoint The full URL of the target server (e.g., "http://ip:port").
     * @param PlayerStateData The player state data to send.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS S2S Comms | Source Server")
    void SendPlayerState(const FString& TargetS2SEndpoint, const FPlayerStateDataForHandoff& PlayerStateData);

    /**
     * Starts the HTTP listener on S_Src to receive acknowledgment from S_Tgt.
     * Binds to /handoff/acknowledge_preparation
     */
    UFUNCTION(BlueprintCallable, Category = "OWS S2S Comms | Source Server")
    bool StartAcknowledgePreparationListener();

    /**
     * Stops the HTTP listener on S_Src.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS S2S Comms | Source Server")
    void StopAcknowledgePreparationListener();

    /**
     * Delegate broadcast when an acknowledgment is received from S_Tgt.
     */
    UPROPERTY(BlueprintAssignable, Category = "OWS S2S Comms | Source Server")
    FOnS2SAcknowledgmentReceived OnAcknowledgmentReceived;


private:
    // --- HTTP Listener Management (Common for S_Src and S_Tgt roles) ---
    TSharedPtr<IHttpRouter> HttpRouter;
    FHttpRouteHandle ReceiveStateRouteHandle;
    FHttpRouteHandle AcknowledgePreparationRouteHandle;

    UPROPERTY() // To keep it alive and allow access
    UOWSPlayerReconstructionManager* PlayerReconstructionManager;

    // --- S_Tgt: Listener Callbacks & Senders ---
    bool HandleReceiveStateRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // Added CharacterNameToAck to fetch the token, changed bSuccess to bCanProceed
    void SendAcknowledgePreparation(const FString& SourceServerS2SEndpoint, const FString& CharacterNameToAck, bool bCanProceed, const FString& Message);
    void OnAcknowledgePreparationResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    // --- S_Src: Listener Callbacks & Senders ---
    bool HandleAcknowledgePreparationRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    void OnPlayerStateResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    // Helper to get base URL for this server
    FString GetFullListenAddress() const;
};
