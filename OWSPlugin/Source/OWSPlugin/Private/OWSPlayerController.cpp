// Copyright Open World Server, PLLC.

#include "OWSPlayerController.h"
#include "OWSGameInstanceSubsystem.h" // Will create this next
#include "Engine/GameInstance.h"
#include "Engine/World.h" // Required for GetWorld()

DEFINE_LOG_CATEGORY(LogOWSPlayerController);

AOWSPlayerController::AOWSPlayerController()
{
    // Constructor
}

void AOWSPlayerController::ClientRPC_ExecuteSeamlessHandoff_Implementation(const FString& TargetServerIP, int32 TargetServerPort, const FString& HandoffToken, const FString& CharacterNameToHandoff)
{
    UE_LOG(LogOWSPlayerController, Log, TEXT("ClientRPC_ExecuteSeamlessHandoff: Received request to handoff Character %s to %s:%d with Token %s"), 
        *CharacterNameToHandoff, *TargetServerIP, TargetServerPort, *HandoffToken);

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        UOWSGameInstanceSubsystem* OWSGameInstanceSubsystem = GameInstance->GetSubsystem<UOWSGameInstanceSubsystem>();
        if (OWSGameInstanceSubsystem)
        {
            // Pass the HandoffToken along with other parameters
            OWSGameInstanceSubsystem->ConnectToTargetMeshedServer(TargetServerIP, TargetServerPort, HandoffToken, CharacterNameToHandoff);
        }
        else
        {
            UE_LOG(LogOWSPlayerController, Error, TEXT("ClientRPC_ExecuteSeamlessHandoff: UOWSGameInstanceSubsystem not found!"));
        }
    }
    else
    {
        UE_LOG(LogOWSPlayerController, Error, TEXT("ClientRPC_ExecuteSeamlessHandoff: GameInstance is null!"));
    }
}
