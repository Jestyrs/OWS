// Copyright Open World Server Org. All Rights Reserved.

#include "OWSHandoffManagerComponent.h"
#include "OWSAPISubsystem.h"
#include "OWSPlayerController.h"
#include "OWSPlayerState.h"
#include "OWSCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Shared/OWSInstanceManagementRequests.h" // For FHandoffPreparationRequest_UE, FHandoffPreparationResponse_UE
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h" // For GetVelocity()
#include "Serialization/JsonSerializer.h" // For FJsonObjectConverter
#include "OWSGameInstanceSubsystem.h" // To potentially get WorldServerID if not passed directly

// Define a log category for this component
DEFINE_LOG_CATEGORY_STATIC(LogOWSHandoffManager, Log, All);

UOWSHandoffManagerComponent::UOWSHandoffManagerComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false; // Using a timer instead of tick for boundary checks
    PrimaryComponentTick.TickInterval = 0.25f; // Only relevant if bCanEverTick is true

    BoundaryCheckInterval = 0.25f; 
    HandoffProximityThreshold = 500.0f; // Example: 5 meters
    WorldServerID = -1; // Initialize with an invalid ID
}

void UOWSHandoffManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner()->HasAuthority()) // Only run on server
    {
        if (BoundaryCheckInterval > 0.f)
        {
            GetWorld()->GetTimerManager().SetTimer(BoundaryCheckTimerHandle, this, &UOWSHandoffManagerComponent::PeriodicBoundaryCheck, BoundaryCheckInterval, true, FMath::FRandRange(0.0f, BoundaryCheckInterval)); // Add initial delay jitter
            UE_LOG(LogOWSHandoffManager, Log, TEXT("PeriodicBoundaryCheck timer started with interval: %f"), BoundaryCheckInterval);
        }
        else
        {
            UE_LOG(LogOWSHandoffManager, Warning, TEXT("BoundaryCheckInterval is 0 or negative, periodic boundary checks disabled."));
        }
    }
}

void UOWSHandoffManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // If PrimaryComponentTick.bCanEverTick were true, logic could go here.
    // For now, using timer in BeginPlay.
}

void UOWSHandoffManagerComponent::InitializeFromServer(const TArray<FString>& InAuthoritativeGridCells, const FString& InCurrentWorldServerS2SEndpoint, int32 InWorldServerID)
{
    AuthoritativeGridCells = InAuthoritativeGridCells;
    CurrentWorldServerS2SEndpoint = InCurrentWorldServerS2SEndpoint;
    WorldServerID = InWorldServerID;

    UE_LOG(LogOWSHandoffManager, Log, TEXT("OWSHandoffManagerComponent Initialized for WorldServerID: %d. Authoritative Cells: %d, S2S Endpoint: %s"),
        WorldServerID, AuthoritativeGridCells.Num(), *CurrentWorldServerS2SEndpoint);
}

void UOWSHandoffManagerComponent::PeriodicBoundaryCheck()
{
    AOWSCharacter* OwningChar = GetOwningOWSCharacter();
    if (OwningChar && OwningChar->HasAuthority()) 
    {
        CheckPlayerBoundary(OwningChar->GetActorLocation(), OwningChar->GetCharacterMovement()->Velocity);
    }
}

AOWSCharacter* UOWSHandoffManagerComponent::GetOwningOWSCharacter() const
{
    return Cast<AOWSCharacter>(GetOwner());
}

AOWSPlayerController* UOWSHandoffManagerComponent::GetOwningOWSPlayerController() const
{
    AOWSCharacter* OwningChar = GetOwningOWSCharacter();
    return OwningChar ? Cast<AOWSPlayerController>(OwningChar->GetController()) : nullptr;
}

void UOWSHandoffManagerComponent::CheckPlayerBoundary(FVector PlayerLocation, FVector PlayerVelocity)
{
    if (AuthoritativeGridCells.Num() == 0 || WorldServerID <= 0)
    {
        // Not initialized or not authoritative for any cells.
        return;
    }

    // --- Conceptual Geometry Logic ---
    // This is a placeholder for robust geometric boundary checking.
    // A real implementation would need a grid manager subsystem that can:
    // 1. Convert PlayerLocation to its current GridCellID.
    // 2. For that GridCellID, provide its geometric boundaries.
    // 3. Check if PlayerLocation + Velocity * HandoffProximityThreshold is near an *external* boundary.

    bool bIsNearExternalBoundary = false;
    FString CurrentPlayerCellID = TEXT("CONCEPTUAL_Player_Cell_ID"); // Placeholder: Get this from a grid manager
    
    // Example: If player is within HandoffProximityThreshold of any edge of its current cell,
    // and that edge is also an edge of the total authoritative region for this server.
    // For this conceptual step, we'll assume a simple check:
    // if (IsNearAuthoritativeRegionEdge(PlayerLocation, CurrentPlayerCellID)) 
    // {
    //    bIsNearExternalBoundary = true;
    // }
    // To avoid making this too complex with pure conceptual geometry, let's assume
    // for this placeholder that if the player is moving and is valid, we *might* check.
    // A real check would be much more involved.
    
    // Simplified: Assume if player is moving, they *might* be approaching a boundary.
    // In a real scenario, you'd check against actual cell geometry.
    if (PlayerVelocity.SizeSquared() > 1.0f) // Only proceed if player is moving
    {
        // This is where you'd check if PlayerLocation is near an edge of the AuthoritativeGridCells region
        // For now, we'll just try to get a target cell if moving.
        bIsNearExternalBoundary = true; // Placeholder: Assume we are near a boundary if moving.
                                        // A real system needs to check against actual geometry of AuthoritativeGridCells.
    }


    if (bIsNearExternalBoundary)
    {
        FString PredictedTargetCellID = GetTargetGridCell(PlayerLocation, PlayerVelocity);

        if (!PredictedTargetCellID.IsEmpty() && !AuthoritativeGridCells.Contains(PredictedTargetCellID))
        {
            // Prevent spamming requests for the same target cell if one is already in progress
            // This simple check might need to be more robust (e.g., a timestamp or a boolean flag like bHandoffInProgress)
            static FString LastAttemptedTargetCellID; // Basic debounce
            if (LastAttemptedTargetCellID != PredictedTargetCellID)
            {
                UE_LOG(LogOWSHandoffManager, Log, TEXT("Player approaching boundary, CurrentCell: %s, PredictedTargetCell: %s. Requesting handoff."), *CurrentPlayerCellID, *PredictedTargetCellID);
                LastAttemptedTargetCellID = PredictedTargetCellID; // Mark this cell as being attempted
                RequestHandoffToServer(PredictedTargetCellID);
                // Could add a short timer here to clear LastAttemptedTargetCellID to allow retries after a delay
            }
        }
    }
}

FString UOWSHandoffManagerComponent::GetTargetGridCell(FVector PlayerLocation, FVector PlayerVelocity)
{
    // --- Conceptual Prediction Logic ---
    // This is highly conceptual and depends on the grid system's specifics (naming, size, origin).
    // Assume GridCellID format like "MapName_XIndex_YIndex" and a known cell size.
    float CellSize = 10000.0f; // Example: 100m cells in Unreal Units
    float PredictionDistance = HandoffProximityThreshold * 1.5f; // Look slightly beyond threshold

    if (PlayerVelocity.IsNearlyZero())
    {
        return FString(); // Not moving, no target cell
    }

    FVector PredictedLocation = PlayerLocation + PlayerVelocity.GetSafeNormal() * PredictionDistance;

    // Conceptual conversion - this needs to map to your specific world origin and grid layout
    // This is a placeholder and will not work without a proper grid system.
    // int GridX = FMath::FloorToInt(PredictedLocation.X / CellSize);
    // int GridY = FMath::FloorToInt(PredictedLocation.Y / CellSize);
    // FString MapName = UGameplayStatics::GetCurrentLevelName(GetWorld()); // Or a predefined map name for the grid
    // return FString::Printf(TEXT("%s_%d_%d"), *MapName, GridX, GridY);

    // For this conceptual step, return a dummy value if moving.
    // A real implementation needs a UGridManagerSubsystem or similar to convert location to cell ID.
    return TEXT("CONCEPTUAL_Target_Cell_ID"); // Placeholder
}

void UOWSHandoffManagerComponent::RequestHandoffToServer(const FString& InTargetGridCellID)
{
    AOWSPlayerController* PC = GetOwningOWSPlayerController();
    if (!PC)
    {
        UE_LOG(LogOWSHandoffManager, Warning, TEXT("RequestHandoffToServer: Owning AOWSPlayerController not found."));
        OnHandoffPreparationErrorDelegate.ExecuteIfBound(TEXT("Player controller not found."));
        return;
    }

    AOWSPlayerState* PS = PC->GetPlayerState<AOWSPlayerState>();
    if (!PS)
    {
        UE_LOG(LogOWSHandoffManager, Warning, TEXT("RequestHandoffToServer: Owning AOWSPlayerState not found."));
        OnHandoffPreparationErrorDelegate.ExecuteIfBound(TEXT("Player state not found."));
        return;
    }
    
    // WorldServerID should have been set during InitializeFromServer
    if (WorldServerID <= 0)
    {
         UE_LOG(LogOWSHandoffManager, Error, TEXT("RequestHandoffToServer: Invalid SourceWorldServerID (%d). Was InitializeFromServer called?"), WorldServerID);
         OnHandoffPreparationErrorDelegate.ExecuteIfBound(TEXT("Source server ID not initialized."));
         return;
    }

    UOWSAPISubsystem* OWSAPISubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UOWSAPISubsystem>();
    if (!OWSAPISubsystem)
    {
        UE_LOG(LogOWSHandoffManager, Error, TEXT("RequestHandoffToServer: UOWSAPISubsystem not found."));
        OnHandoffPreparationErrorDelegate.ExecuteIfBound(TEXT("API Subsystem not available."));
        return;
    }

    FHandoffPreparationRequest_UE RequestData;
    RequestData.PlayerUserSessionGUID = PC->UserSessionGUID;
    RequestData.CharacterName = PS->GetOWSCharacterName().ToString(); // Use existing method from AOWSPlayerState
    RequestData.SourceWorldServerID = this->WorldServerID;
    RequestData.CurrentGridCellID = TEXT("CONCEPTUAL_Player_Cell_ID"); // Placeholder: This needs to be accurately determined
    RequestData.TargetGridCellID = InTargetGridCellID;

    TargetCellForHandoff = InTargetGridCellID; // Store the cell we are attempting to handoff to

    FString RequestJsonString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(RequestData, RequestJsonString, 0, 0))
    {
        UE_LOG(LogOWSHandoffManager, Error, TEXT("RequestHandoffToServer: Failed to serialize FHandoffPreparationRequest_UE to JSON."));
        OnHandoffPreparationErrorDelegate.ExecuteIfBound(TEXT("Failed to serialize handoff request."));
        TargetCellForHandoff.Empty(); // Clear if serialization fails
        return;
    }

    UE_LOG(LogOWSHandoffManager, Log, TEXT("Requesting handoff to OWSInstanceManagement. Payload: %s"), *RequestJsonString);

    // Using the generic POST request method from UOWSAPISubsystem
    // A more specific wrapper could be added to UOWSAPISubsystem like:
    // OWSAPISubsystem->RequestServerHandoffPreparation(RequestData, FOnHandoffPreparationHTTPResponse::CreateUObject(this, &UOWSHandoffManagerComponent::HandleHandoffPreparationResponse));
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = OWSAPISubsystem->CreateOWS2POSTRequest(
        TEXT("/api/Handoff/RequestHandoffPreparation"), // Endpoint on OWSInstanceManagement HandoffController
        RequestJsonString,
        TEXT("RequestHandoffToServer") 
    );

    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOWSHandoffManagerComponent::HandleHandoffPreparationResponse);
    HttpRequest->ProcessRequest();
}

void UOWSHandoffManagerComponent::HandleHandoffPreparationResponse(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
    FString ResponseErrorMessage;

    if (bSucceeded && HttpResponse.IsValid())
    {
        if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
        {
            FHandoffPreparationResponse_UE ResponseData;
            if (FJsonObjectConverter::JsonObjectStringToUStruct(HttpResponse->GetContentAsString(), &ResponseData, 0, 0))
            {
                if (ResponseData.CanProceed && !ResponseData.TargetWorldServerS2SEndpoint.IsEmpty())
                {
                    UE_LOG(LogOWSHandoffManager, Log, TEXT("Handoff preparation successful for cell %s. Target S2S Endpoint: %s"), *TargetCellForHandoff, *ResponseData.TargetWorldServerS2SEndpoint);
                    OnHandoffPreparationSuccessDelegate.ExecuteIfBound(ResponseData.TargetWorldServerS2SEndpoint);
                    // TargetCellForHandoff remains set, indicating an attempt was successful for this cell.
                    // Consider clearing it after a timeout if the next S2S steps don't complete.
                    return; 
                }
                else
                {
                    ResponseErrorMessage = FString::Printf(TEXT("Handoff preparation denied by server for cell %s. Message: %s"), *TargetCellForHandoff, *ResponseData.ErrorMessage);
                }
            }
            else
            {
                ResponseErrorMessage = FString::Printf(TEXT("Failed to deserialize handoff preparation response JSON for cell %s. Response: %s"), *TargetCellForHandoff, *HttpResponse->GetContentAsString());
            }
        }
        else
        {
            ResponseErrorMessage = FString::Printf(TEXT("Handoff preparation request failed with HTTP code: %d for cell %s. Response: %s"), HttpResponse->GetResponseCode(), *TargetCellForHandoff, *HttpResponse->GetContentAsString());
        }
    }
    else
    {
        ResponseErrorMessage = FString::Printf(TEXT("Handoff preparation request failed for cell %s: No response or connection failure."), *TargetCellForHandoff);
    }

    UE_LOG(LogOWSHandoffManager, Warning, TEXT("HandleHandoffPreparationResponse Error: %s"), *ResponseErrorMessage);
    OnHandoffPreparationErrorDelegate.ExecuteIfBound(ResponseErrorMessage);
    TargetCellForHandoff.Empty(); // Clear pending target on error so it can be retried for this cell or another.
}
