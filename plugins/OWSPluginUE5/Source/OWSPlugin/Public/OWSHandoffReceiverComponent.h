// Copyright Open World Server Org. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shared/OWSS2SCommsShared.h" // For FPlayerStateDataForHandoff_UE
#include "OWSHandoffReceiverComponent.generated.h"

// Forward declaration
class ACharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OWSPLUGIN_API UOWSHandoffReceiverComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    UOWSHandoffReceiverComponent();

    /**
     * Handles the incoming player state data transferred from another server.
     * This function is intended to be called by the underlying S2S communication system (e.g., gRPC service endpoint).
     * It attempts to reconstruct the player and generates a handoff token for the client.
     *
     * @param IncomingPlayerState The player state data received from the source server.
     * @param OutHandoffSessionToken The generated session token if player reconstruction is successful.
     * @param OutErrorMessage Error message if the process fails.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS|Handoff|S2S Receiver") // UFUNCTION for potential Blueprint exposure if S2S is handled via BP-friendly system
    virtual void HandleIncomingPlayerStateTransfer(const FPlayerStateDataForHandoff_UE& IncomingPlayerState, FString& OutHandoffSessionToken, FString& OutErrorMessage);

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /**
     * Reconstructs the player character on this server based on the provided state data.
     * This involves spawning the character, deserializing component snapshots, and applying dynamic state.
     * @param PlayerState The comprehensive state data for the player.
     * @return The reconstructed ACharacter, or nullptr if reconstruction fails.
     */
    virtual ACharacter* ReconstructPlayerCharacter(const FPlayerStateDataForHandoff_UE& PlayerState);

    /**
     * Generates a unique handoff token for a given character.
     * @param CharacterName The name of the character for whom the token is generated.
     * @return A unique string token.
     */
    virtual FString GenerateHandoffToken(const FString& CharacterName);

    /**
     * Stores the generated handoff token and associates it with the reconstructed character.
     * This allows the GameMode to later find this character when the client connects with the token.
     * @param Token The handoff token.
     * @param AssociatedCharacter The character associated with this token.
     */
    virtual void StoreHandoffToken(const FString& Token, ACharacter* AssociatedCharacter);

    // Map to store handoff tokens and their associated characters (or character data for validation)
    // TWeakObjectPtr is used to avoid preventing garbage collection of the Character if it's destroyed for other reasons.
    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    TMap<FString, TWeakObjectPtr<ACharacter>> HandoffTokenToCharacterMap;

    // Timer handles for clearing out stale tokens
    FTimerHandle ClearStaleTokensTimerHandle;

    UFUNCTION()
    virtual void ClearStaleHandoffTokens();

    UPROPERTY(EditAnywhere, Category = "OWS|Handoff|Config")
    float StaleTokenClearInterval; // How often to check for stale tokens

    UPROPERTY(EditAnywhere, Category = "OWS|Handoff|Config")
    float HandoffTokenLifetime; // How long a token is valid

    // Helper struct to store token creation time for cleanup
    struct FHandoffTokenData
    {
        TWeakObjectPtr<ACharacter> CharacterPtr;
        FDateTime CreationTime;

        FHandoffTokenData(ACharacter* InCharacter = nullptr)
            : CharacterPtr(InCharacter), CreationTime(FDateTime::UtcNow())
        {}
    };

    UPROPERTY(VisibleAnywhere, Category = "OWS|Handoff|State", Transient)
    TMap<FString, FHandoffTokenData> HandoffTokenDataMap;


public:	
    // Called every frame
    // virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
