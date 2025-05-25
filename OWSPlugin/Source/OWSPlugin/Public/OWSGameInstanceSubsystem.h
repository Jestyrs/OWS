// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OWSGameInstanceSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOWSGameInstanceSubsystem, Log, All);

UCLASS()
class OWSPLUGIN_API UOWSGameInstanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Connects the client to a target meshed server, typically for handoff.
     * @param TargetServerIP The IP address of the target server.
     * @param TargetServerPort The port of the target server.
     * @param HandoffToken The token to be passed in the URL for handoff authentication.
     * @param CharacterNameToHandoff The name of the character being handed off.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS Handoff")
    void ConnectToTargetMeshedServer(const FString& TargetServerIP, int32 TargetServerPort, const FString& HandoffToken, const FString& CharacterNameToHandoff);

    // Configurable map name for handoff travel
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OWS Handoff | Configuration")
    FString HandoffDefaultMapName = TEXT("/Game/Maps/DefaultHandoffMap"); // Example, make sure this map exists or change it
};
