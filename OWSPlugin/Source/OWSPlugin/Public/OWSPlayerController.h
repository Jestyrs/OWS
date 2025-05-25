// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OWSPlayerController.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOWSPlayerController, Log, All);

UCLASS()
class OWSPLUGIN_API AOWSPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AOWSPlayerController();

    /**
     * Client RPC to instruct the client to perform a seamless handoff to a new server.
     * @param TargetServerIP The IP address of the target server.
     * @param TargetServerPort The port of the target server.
     * @param HandoffToken The token provided by S_Tgt, which S_Src passes to client, and client passes to S_Tgt.
     * @param CharacterNameToHandoff The name of the character being handed off.
     */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category = "OWS Handoff")
    void ClientRPC_ExecuteSeamlessHandoff(const FString& TargetServerIP, int32 TargetServerPort, const FString& HandoffToken, const FString& CharacterNameToHandoff);
};
