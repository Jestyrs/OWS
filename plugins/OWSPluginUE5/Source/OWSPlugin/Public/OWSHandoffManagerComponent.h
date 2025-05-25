// Copyright Open World Server Org. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/Online/HTTP/Public/Http.h" // Required for FHttpRequestPtr, FHttpResponsePtr
#include "OWSHandoffManagerComponent.generated.h"

// Forward declare UOWSAPISubsystem if needed, or include its header if it's lightweight
// For now, assuming it will be included in the .cpp file.

// Delegate signatures
// Copyright Open World Server Org. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/Online/HTTP/Public/Http.h" // Required for FHttpRequestPtr, FHttpResponsePtr
#include "Shared/OWSS2SCommsShared.h" // For FPlayerStateDataForHandoff_UE
#include "OWSHandoffManagerComponent.generated.h"


// Delegate signatures
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnHandoffPreparationSuccess, const FString&, TargetServerS2SEndpoint);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnHandoffPreparationError, const FString&, ErrorMessage);
// Delegate for when S2S state transfer to target server is complete (or failed) and we have a handoff token (or error)
// Matching the previous definition from Step 5, ensuring consistency
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnS2SStateTransferComplete, const FString&, HandoffSessionToken, const FString&, TargetS2SEndpoint);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OWSPLUGIN_API UOWSHandoffManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    UOWSHandoffManagerComponent();

    UFUNCTION(BlueprintCallable, Category = "OWS|Handoff")
    void InitializeFromServer(const TArray<FString>& InAuthoritativeGridCells, const FString& InCurrentWorldServerS2SEndpoint, int32 InWorldServerID);

    UFUNCTION(BlueprintCallable, Category = "OWS|Handoff")
    void PeriodicBoundaryCheck(); 

    UFUNCTION(BlueprintCallable, Category = "OWS|Handoff")
    FString GetTargetGridCell(FVector PlayerLocation, FVector PlayerVelocity);

    UFUNCTION(BlueprintCallable, Category = "OWS|Handoff")
    void RequestHandoffToServer(const FString& TargetGridCellID);

    UPROPERTY(BlueprintAssignable, Category = "OWS|Handoff Events")
    FOnHandoffPreparationSuccess OnHandoffPreparationSuccessDelegate;

    UPROPERTY(BlueprintAssignable, Category = "OWS|Handoff Events")
    FOnHandoffPreparationError OnHandoffPreparationErrorDelegate;

    UPROPERTY(BlueprintAssignable, Category = "OWS|Handoff Events")
    FOnS2SStateTransferComplete OnS2SStateTransferComplete; // Ensured this is the correct delegate name

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleHandoffPreparationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    TArray<FString> AuthoritativeGridCells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS|Handoff|Config")
    float BoundaryCheckInterval;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS|Handoff|Config")
    float HandoffProximityThreshold;
    
    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    FString CurrentWorldServerS2SEndpoint; 

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    int32 WorldServerID; 

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    FString TargetCellForHandoff; 

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    FString TargetS2SEndpointForCurrentHandoff; // To store the endpoint for S2S transfer

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    FString CharacterNameForHandoff; // Cache for the character being handed off

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    FString CachedTargetClientFacingIP; // Cache for target server client IP

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    int32 CachedTargetClientFacingPort = 0; // Cache for target server client port


    FTimerHandle BoundaryCheckTimerHandle;
    FTimerHandle S2SResponseSimulatorTimerHandle; 

    class AOWSCharacter* GetOwningOWSCharacter() const;
    class AOWSPlayerController* GetOwningOWSPlayerController() const;

    void CheckPlayerBoundary(FVector PlayerLocation, FVector PlayerVelocity);

private:
    // Prepares FPlayerStateDataForHandoff_UE and initiates the S2S transfer (currently simulated)
    void InitiateS2SPlayerStateTransfer(const FString& TargetS2SEndpoint); 
    
    // Handles the (simulated) response from the S2S transfer.
    // The parameters should align with FOnS2SStateTransferComplete if we want to broadcast directly.
    // For simulation, we might simplify, but for real use, ensure parameter consistency or adapt.
    // Let's align it: bool bSuccess, const FString& HandoffSessionToken, const FString& ErrorMessage (if !bSuccess)
    // And we need to pass the original TargetS2SEndpoint for the delegate.
    UFUNCTION() 
    void HandleS2STransferResponse(bool bSuccess, const FString& HandoffTokenOrError, FString OriginalTargetS2SEndpoint); 

public:	
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;		
};
