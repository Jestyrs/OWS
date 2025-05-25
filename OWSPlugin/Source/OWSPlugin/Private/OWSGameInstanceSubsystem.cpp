// Copyright Open World Server, PLLC.

#include "OWSGameInstanceSubsystem.h"
#include "Engine/World.h" // For GetWorld()
#include "Kismet/GameplayStatics.h" // For GetPlayerController
#include "Engine/NetDriver.h" // For UNetDriver

DEFINE_LOG_CATEGORY(LogOWSGameInstanceSubsystem);

void UOWSGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogOWSGameInstanceSubsystem, Log, TEXT("OWSGameInstanceSubsystem Initialized"));
}

void UOWSGameInstanceSubsystem::Deinitialize()
{
    Super::Deinitialize();
    UE_LOG(LogOWSGameInstanceSubsystem, Log, TEXT("OWSGameInstanceSubsystem Deinitialized"));
}

void UOWSGameInstanceSubsystem::ConnectToTargetMeshedServer(const FString& TargetServerIP, int32 TargetServerPort, const FString& HandoffToken, const FString& CharacterNameToHandoff)
{
    UE_LOG(LogOWSGameInstanceSubsystem, Log, TEXT("ConnectToTargetMeshedServer: Attempting to connect Character %s to %s:%d with Token %s"), 
        *CharacterNameToHandoff, *TargetServerIP, TargetServerPort, *HandoffToken);

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogOWSGameInstanceSubsystem, Error, TEXT("ConnectToTargetMeshedServer: GetWorld() returned null. Cannot travel."));
        return;
    }

    APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
    if (!PC)
    {
        UE_LOG(LogOWSGameInstanceSubsystem, Error, TEXT("ConnectToTargetMeshedServer: GetFirstLocalPlayerController() returned null. Cannot travel."));
        return;
    }

    // Optional: Gracefully close the current connection to S_Src
    // This might not always be necessary or desired, as ClientTravel should handle it.
    // However, explicitly shutting down the net driver can sometimes be cleaner.
    /*
    UNetDriver* NetDriver = World->GetNetDriver();
    if (NetDriver)
    {
        UE_LOG(LogOWSGameInstanceSubsystem, Log, TEXT("ConnectToTargetMeshedServer: Shutting down existing NetDriver."));
        NetDriver->Shutdown();
    }
    */

    // Construct the travel URL
    // The map name here is crucial. It should be a map that exists in the target server's build.
    // Using HandoffDefaultMapName which is configurable.
    FString URL = FString::Printf(TEXT("%s:%d%s?CharacterName=%s?HandoffToken=%s?bIsHandoff=true"), 
        *TargetServerIP, 
        TargetServerPort, 
        *HandoffDefaultMapName, // Configurable map name
        *CharacterNameToHandoff, 
        *HandoffToken);

    UE_LOG(LogOWSGameInstanceSubsystem, Log, TEXT("ConnectToTargetMeshedServer: Traveling to URL: %s"), *URL);

    // Use ClientTravel to connect to the new server
    PC->ClientTravel(URL, ETravelType::TRAVEL_Absolute, false /*bSeamless*/); 
    // Note: True seamless travel (bSeamless=true) has specific requirements for map setup and actor persistence
    // which are likely beyond the scope of this initial handoff implementation.
    // Using TRAVEL_Absolute ensures a full reconnect.
}
