// Copyright Open World Server Org. All Rights Reserved.

#include "OWSHandoffReceiverComponent.h"
#include "OWSCharacter.h"
#include "OWSPlayerState.h"
#include "OWSInventoryManagerComponent.h" // Example for component deserialization
#include "OWSAbilitySystemComponent.h"   // Example for component deserialization
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Guid.h" // For FGuid for token generation
#include "Serialization/FMemoryReader.h" // For FMemoryReader for deserialization
// Potentially include OWSGameInstanceSubsystem if it holds helper functions or shared state for handoff

// Define a log category for this component
DEFINE_LOG_CATEGORY_STATIC(LogOWSHandoffReceiver, Log, All);

UOWSHandoffReceiverComponent::UOWSHandoffReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Not ticking by default
    StaleTokenClearInterval = 60.0f; // Default: clear stale tokens every 60 seconds
    HandoffTokenLifetime = 120.0f;   // Default: tokens are valid for 120 seconds
}

void UOWSHandoffReceiverComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner() && GetOwner()->HasAuthority()) // Ensure owner exists and is server
    {
        GetWorld()->GetTimerManager().SetTimer(ClearStaleTokensTimerHandle, this, &UOWSHandoffReceiverComponent::ClearStaleHandoffTokens, StaleTokenClearInterval, true);
        UE_LOG(LogOWSHandoffReceiver, Log, TEXT("ClearStaleHandoffTokens timer started. Interval: %f seconds"), StaleTokenClearInterval);
    }
}

void UOWSHandoffReceiverComponent::HandleIncomingPlayerStateTransfer(const FPlayerStateDataForHandoff_UE& IncomingPlayerState, FString& OutHandoffSessionToken, FString& OutErrorMessage)
{
    UE_LOG(LogOWSHandoffReceiver, Log, TEXT("HandleIncomingPlayerStateTransfer received for CharacterName: %s, UserSessionGUID: %s"), 
        *IncomingPlayerState.CharacterName, *IncomingPlayerState.UserSessionGUID.ToString());

    ACharacter* ReconstructedCharacter = ReconstructPlayerCharacter(IncomingPlayerState);

    if (ReconstructedCharacter)
    {
        FString NewToken = GenerateHandoffToken(IncomingPlayerState.CharacterName);
        StoreHandoffToken(NewToken, ReconstructedCharacter);

        OutHandoffSessionToken = NewToken;
        OutErrorMessage = TEXT("");

        UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Player %s reconstructed successfully. Handoff Token: %s generated."), *IncomingPlayerState.CharacterName, *NewToken);
    }
    else
    {
        OutHandoffSessionToken = TEXT("");
        OutErrorMessage = TEXT("Failed to reconstruct player on target server.");
        UE_LOG(LogOWSHandoffReceiver, Error, TEXT("Failed to reconstruct player %s."), *IncomingPlayerState.CharacterName);
    }
}

ACharacter* UOWSHandoffReceiverComponent::ReconstructPlayerCharacter(const FPlayerStateDataForHandoff_UE& PlayerState)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogOWSHandoffReceiver, Error, TEXT("ReconstructPlayerCharacter: World is null."));
        return nullptr;
    }

    // 1. Determine Character Class
    UClass* CharacterClass = AOWSCharacter::StaticClass(); // Default to AOWSCharacter
    // TODO: Add logic to select specific character class based on PlayerState.ClassName or similar
    // Example: if (PlayerState.ClassName == "MyWarriorClass") CharacterClass = LoadClass<AOWSCharacter>(nullptr, TEXT("/Game/Blueprints/Characters/BP_MyWarriorClass.BP_MyWarriorClass_C"));
    
    if (!CharacterClass)
    {
        UE_LOG(LogOWSHandoffReceiver, Error, TEXT("ReconstructPlayerCharacter: Could not determine CharacterClass for ClassName: %s"), *PlayerState.ClassName);
        return nullptr;
    }

    // 2. Spawn Character
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    FVector SpawnLocation(PlayerState.LivePosition.X, PlayerState.LivePosition.Y, PlayerState.LivePosition.Z);
    FRotator SpawnRotation(PlayerState.LiveRotation.Pitch, PlayerState.LiveRotation.Yaw, PlayerState.LiveRotation.Roll);

    AOWSCharacter* NewCharacter = World->SpawnActor<AOWSCharacter>(CharacterClass, SpawnLocation, SpawnRotation, SpawnParams);
    if (!NewCharacter)
    {
        UE_LOG(LogOWSHandoffReceiver, Error, TEXT("ReconstructPlayerCharacter: Failed to spawn character of class %s for %s."), *CharacterClass->GetName(), *PlayerState.CharacterName);
        return nullptr;
    }
    UE_LOG(LogOWSHandoffReceiver, Log, TEXT("ReconstructPlayerCharacter: Spawned %s for %s."), *NewCharacter->GetName(), *PlayerState.CharacterName);

    // 3. Reconstruct PlayerState
    AOWSPlayerState* NewPlayerState = NewCharacter->GetPlayerState<AOWSPlayerState>();
    if (!NewPlayerState)
    {
        UE_LOG(LogOWSHandoffReceiver, Error, TEXT("ReconstructPlayerCharacter: NewCharacter %s has no AOWSPlayerState after spawn!"), *NewCharacter->GetName());
        NewCharacter->Destroy(); 
        return nullptr;
    }

    if (PlayerState.PlayerStateSnapshot.Num() > 0)
    {
        FMemoryReader PlayerStateAr(PlayerState.PlayerStateSnapshot, true);
        NewPlayerState->Serialize(PlayerStateAr); 
        UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Applied PlayerStateSnapshot to %s. Size: %d bytes"), *NewPlayerState->GetName(), PlayerState.PlayerStateSnapshot.Num());
    }
    NewPlayerState->SetPlayerName(PlayerState.CharacterName); 
    NewPlayerState->UserSessionGUID = PlayerState.UserSessionGUID; 
    NewPlayerState->SetOWSCharacterName(FText::FromString(PlayerState.CharacterName)); 
    NewPlayerState->CharacterLevel = PlayerState.CharacterLevel; 
    // TODO: Apply other specific non-UPROPERTY fields from PlayerState data

    // 4. Apply CharacterSnapshot
    if (PlayerState.CharacterSnapshot.Num() > 0)
    {
        FMemoryReader CharacterAr(PlayerState.CharacterSnapshot, true);
        NewCharacter->Serialize(CharacterAr); 
        UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Applied CharacterSnapshot to %s. Size: %d bytes"), *NewCharacter->GetName(), PlayerState.CharacterSnapshot.Num());
    }
    NewCharacter->SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (NewCharacter->GetCharacterMovement())
    {
        NewCharacter->GetCharacterMovement()->Velocity = FVector(PlayerState.LiveVelocity.X, PlayerState.LiveVelocity.Y, PlayerState.LiveVelocity.Z);
    }

    // 5. Apply Component Snapshots & Dynamic Data
    for (const auto& Pair : PlayerState.ActorComponentSnapshots)
    {
        UActorComponent* Component = NewCharacter->FindComponentByName(FName(*Pair.Key));
        if (Component)
        {
            FMemoryReader CompAr(Pair.Value, true);
            Component->Serialize(CompAr); // Assumes component supports Serialize
            UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Applied %s snapshot. Size: %d bytes"), *Pair.Key, Pair.Value.Num());
        }
        else
        {
            UE_LOG(LogOWSHandoffReceiver, Warning, TEXT("Could not find component %s on reconstructed character %s to apply snapshot."), *Pair.Key, *PlayerState.CharacterName);
        }
    }
    
    // TODO: Apply ActiveAbilityCooldowns, ActiveGameplayEffects, LiveCustomCharacterData
    // This requires specific integration with how these systems store and re-apply their state.
    UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Conceptual: Apply AbilitySystem, Quests, CustomData for %s"), *PlayerState.CharacterName);

    // Apply other dynamic stats not covered by snapshots (if any)
    // Example: NewCharacter->Health = PlayerState.CurrentHealth; // If Health is a direct property

    UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Player Character %s fully reconstructed."), *PlayerState.CharacterName);
    return NewCharacter;
}

FString UOWSHandoffReceiverComponent::GenerateHandoffToken(const FString& CharacterName)
{
    FString Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Generated Handoff Token %s for Character %s"), *Token, *CharacterName);
    return Token;
}

void UOWSHandoffReceiverComponent::StoreHandoffToken(const FString& Token, ACharacter* AssociatedCharacter)
{
    if (!AssociatedCharacter) return;
    HandoffTokenDataMap.Add(Token, FHandoffTokenData(AssociatedCharacter));
    UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Stored Handoff Token %s for Character %s. Total tokens: %d"), *Token, *AssociatedCharacter->GetName(), HandoffTokenDataMap.Num());
}

void UOWSHandoffReceiverComponent::ClearStaleHandoffTokens()
{
    FDateTime CurrentTime = FDateTime::UtcNow();
    float MaxLifetimeSeconds = HandoffTokenLifetime;
    TArray<FString> TokensToRemove;

    for (const auto& Pair : HandoffTokenDataMap)
    {
        FTimespan TokenAge = CurrentTime - Pair.Value.CreationTime;
        if (TokenAge.GetTotalSeconds() > MaxLifetimeSeconds || !Pair.Value.CharacterPtr.IsValid())
        {
            TokensToRemove.Add(Pair.Key);
        }
    }

    for (const FString& Token : TokensToRemove)
    {
        HandoffTokenDataMap.Remove(Token);
        UE_LOG(LogOWSHandoffReceiver, Log, TEXT("Removed stale/invalid Handoff Token: %s"), *Token);
    }
    if (TokensToRemove.Num() > 0)
    {
        UE_LOG(LogOWSHandoffReceiver, Log, TEXT("ClearStaleHandoffTokens completed. Removed %d tokens. Total tokens remaining: %d"), TokensToRemove.Num(), HandoffTokenDataMap.Num());
    }
    else
    {
        UE_LOG(LogOWSHandoffReceiver, Verbose, TEXT("ClearStaleHandoffTokens: No stale tokens to remove. Total tokens: %d"), HandoffTokenDataMap.Num());
    }
}

// Example of how GameMode might retrieve the character (not part of this component, but for context)
/* 
ACharacter* UMyGameMode::GetCharacterForHandoff(const FString& Token)
{
    if (HandoffReceiverComponent) // Assuming GameMode has a pointer to this component
    {
        if (UOWSHandoffReceiverComponent::FHandoffTokenData* TokenData = HandoffReceiverComponent->HandoffTokenDataMap.Find(Token))
        {
            if (TokenData->CharacterPtr.IsValid())
            {
                ACharacter* ClaimedChar = TokenData->CharacterPtr.Get();
                HandoffReceiverComponent->HandoffTokenDataMap.Remove(Token); // Single use
                return ClaimedChar;
            }
            else
            {
                HandoffReceiverComponent->HandoffTokenDataMap.Remove(Token); // Stale pointer
            }
        }
    }
    return nullptr;
}
*/

```
