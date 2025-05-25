// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OWSPlayerStateData.h" // For FPlayerStateDataForHandoff
#include "OWSPlayerReconstructionManager.generated.h"

class UOWSS2SCommsManager;
class ACharacter; // Forward declaration

DECLARE_LOG_CATEGORY_EXTERN(LogOWSPlayerReconstruction, Log, All);

UCLASS()
class OWSPLUGIN_API UOWSPlayerReconstructionManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Begin USubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // End USubsystem

    /**
     * Retrieves a pending handoff token for a given character name.
     * This is intended to be called by the GameMode during player login/handoff.
     * Returns an empty string if no token is found.
     * Consumes the token after retrieval.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS Handoff")
    FString GetPendingHandoffTokenForCharacterAck(const FString& CharacterName) const;

    /**
     * Validates the handoff token for a character and consumes the token along with the pawn.
     * Returns a TTuple containing the UserSessionGUID (from PlayerData originally) and the pre-spawned Pawn if the token is valid and matches.
     * Otherwise, returns an empty UserSessionGUID and null Pawn.
     */
    UFUNCTION(BlueprintCallable, Category = "OWS Handoff")
    TTuple<FString, TWeakObjectPtr<ACharacter>> ConsumeHandoffDataForCharacter(const FString& CharacterName, const FString& ExpectedToken);


private:
    void HandlePlayerStateReceived(const FPlayerStateDataForHandoff& PlayerData);

    // Pointer to the S2S Communications Manager
    UPROPERTY()
    UOWSS2SCommsManager* S2SCommsManager;

    // Stores generated handoff tokens and associated pre-spawned pawns.
    // Key: CharacterName, Value: <HandoffSessionToken, PawnPtr, UserSessionGUID>
    UPROPERTY()
    TMap<FString, TTuple<FString, TWeakObjectPtr<ACharacter>, FString>> PendingHandoffData;

    // Helper to get a default pawn class.
    // In a real game, this would be more sophisticated, possibly based on PlayerData.ClassName
    TSubclassOf<ACharacter> GetDefaultCharacterClass() const;
};
