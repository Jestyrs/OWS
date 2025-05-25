// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OWSHandoffStructs.h" // For FRequestHandoffPreparation, FHandoffPreparationResponse
#include "OWSPlayerStateData.h" // For FPlayerStateDataForHandoff
#include "Interfaces/IHttpRequest.h" // For FHttpRequestPtr, FHttpResponsePtr
#include "UHandoffComponent.generated.h"

class UOWSAPISubsystem;
class UOWSS2SCommsManager;

DECLARE_LOG_CATEGORY_EXTERN(LogOWSHandoffComponent, Log, All);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OWSPLUGIN_API UHandoffComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHandoffComponent();

    // --- Configuration Properties ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS Handoff | Configuration")
    FString CurrentGridCellID; // Should be updated by game logic

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS Handoff | Configuration")
    int32 SourceWorldServerID; // Should be set on server startup

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS Handoff | Configuration")
    FString MockTargetGridCellID = TEXT("MockTargetCell_002"); // For testing boundary transition

    // Interval for checking boundary transition (in seconds)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS Handoff | Configuration")
    float BoundaryCheckInterval = 5.0f; 

    // --- Public Methods ---
    UFUNCTION(BlueprintCallable, Category = "OWS Handoff")
    void InitiateHandoffPreparation(const FString& TargetGridCellID);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // --- Boundary Detection ---
    void CheckForBoundaryTransition(); // Periodically called

private:
    // --- Internal Handoff Logic ---
    void OnHandoffPreparationResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    UFUNCTION() // Must be UFUNCTION to bind to delegate
    void HandleS2SAcknowledgment(const FString& Message);
    void TriggerPlayerStateSend();
    void FinalizeHandoffOnSource();

    // --- Helper Methods & Properties ---
    APlayerController* GetPlayerController() const;
    ACharacter* GetOwningCharacter() const;
    FString GetPlayerUserSessionGUID() const; // Placeholder
    FString GetCharacterName() const; // Placeholder

    // --- Subsystem Pointers ---
    UPROPERTY()
    UOWSAPISubsystem* OWSAPISubsystem;
    UPROPERTY()
    UOWSS2SCommsManager* OWSS2SCommsManager;

    // --- State Variables ---
    FTimerHandle TimerHandle_CheckBoundaryTransition;
    FString StoredTargetWorldServerS2SEndpoint;
    bool bIsHandoffInProgress;
    FString HandoffTargetGridCellID; // Store the target cell ID for the current handoff process
};
