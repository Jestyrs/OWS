// Copyright Open World Server, PLLC.

#include "OWSPlayerReconstructionManager.h"
#include "OWSS2SCommsManager.h" // For UOWSS2SCommsManager and FPlayerStateDataForHandoff
#include "GameFramework/Character.h" // For ACharacter
#include "Engine/World.h" // For GetWorld()
#include "GameFramework/DefaultPawn.h" // Fallback pawn
#include "Kismet/GameplayStatics.h" // For UGameplayStatics
#include "OWSHealthComponent.h" // Assuming a UOWSHealthComponent exists for stats
#include "GameFramework/PlayerState.h" // For APlayerState (conceptual snapshot)

DEFINE_LOG_CATEGORY(LogOWSPlayerReconstruction);

void UOWSPlayerReconstructionManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("UOWSPlayerReconstructionManager Initializing."));

    // Get the S2S Comms Manager (expected to be another GameInstanceSubsystem or manually added to the collection)
    S2SCommsManager = Collection.InitializeDependency<UOWSS2SCommsManager>(); 
    // Or: S2SCommsManager = GetGameInstance()->GetSubsystem<UOWSS2SCommsManager>();

    if (S2SCommsManager)
    {
        UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Found UOWSS2SCommsManager. Subscribing to OnPlayerStateReceived."));
        S2SCommsManager->OnPlayerStateReceived.AddUObject(this, &UOWSPlayerReconstructionManager::HandlePlayerStateReceived);
        
        // S_Tgt needs to start its listener for receiving player state from S_Src
        if(!S2SCommsManager->StartReceiveStateListener())
        {
            UE_LOG(LogOWSPlayerReconstruction, Error, TEXT("Failed to start S2S Receive State Listener on S_Tgt! Player handoff will fail."));
        }
        else
        {
            UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("S2S Receive State Listener started successfully on S_Tgt."));
        }
    }
    else
    {
        UE_LOG(LogOWSPlayerReconstruction, Error, TEXT("UOWSS2SCommsManager not found! Player reconstruction will not function."));
    }
}

void UOWSPlayerReconstructionManager::Deinitialize()
{
    UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("UOWSPlayerReconstructionManager Deinitializing."));
    if (S2SCommsManager)
    {
        S2SCommsManager->OnPlayerStateReceived.RemoveUObject(this, &UOWSPlayerReconstructionManager::HandlePlayerStateReceived);
        UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Unsubscribed from UOWSS2SCommsManager::OnPlayerStateReceived."));
        
        // Stop the listener when the subsystem deinitializes
        S2SCommsManager->StopReceiveStateListener();
        UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("S2S Receive State Listener stopped on S_Tgt."));
    }
    Super::Deinitialize();
}

TSubclassOf<ACharacter> UOWSPlayerReconstructionManager::GetDefaultCharacterClass() const
{
    // Placeholder: Return a default character class.
    // This could be configurable via a Blueprint subclass of this manager,
    // or determined from PlayerData.ClassName.
    // For now, use ACharacter or a known default.
    // If you have a specific base character like AMyBaseCharacter, use that.
    // Using ADefaultPawn if ACharacter isn't appropriate for some reason, but usually it is.
    // Make sure this class is valid and can be spawned.
    // UClass* DefaultCharClass = FindObject<UClass>(ANY_PACKAGE, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    // if(DefaultCharClass) return DefaultCharClass;

    return ACharacter::StaticClass(); 
}


void UOWSPlayerReconstructionManager::HandlePlayerStateReceived(const FPlayerStateDataForHandoff& PlayerData)
{
    UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("HandlePlayerStateReceived for Character: %s, UserSessionGUID: %s"), 
        *PlayerData.CharacterName, *PlayerData.UserSessionGUID);

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogOWSPlayerReconstruction, Error, TEXT("World is null. Cannot spawn character."));
        return;
    }

    // --- Spawn Character (Placeholder/Basic) ---
    TSubclassOf<ACharacter> CharacterClassToSpawn = GetDefaultCharacterClass();
    ACharacter* SpawnedCharacter = nullptr;

    if (CharacterClassToSpawn)
    {
        FVector SpawnLocation = PlayerData.PlayerPosition.ToFVector();
        FRotator SpawnRotation = FRotator::MakeFromEuler(PlayerData.PlayerRotation.ToFVector()); // Assuming Euler for now

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = nullptr; // No owner initially
        SpawnParams.Instigator = nullptr;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        
        UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Attempting to spawn character %s of class %s at Location: %s, Rotation: %s"),
            *PlayerData.CharacterName, *CharacterClassToSpawn->GetName(), *SpawnLocation.ToString(), *SpawnRotation.ToString());

        SpawnedCharacter = World->SpawnActor<ACharacter>(CharacterClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);

        if (SpawnedCharacter)
        {
            UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Successfully spawned character: %s"), *SpawnedCharacter->GetName());
            // Set a temporary name or tag to identify this character until proper login/possession
            SpawnedCharacter->Tags.Add(FName(*FString::Printf(TEXT("Handoff_%s"), *PlayerData.CharacterName)));
        }
        else
        {
            UE_LOG(LogOWSPlayerReconstruction, Error, TEXT("Failed to spawn character for %s."), *PlayerData.CharacterName);
            // If spawn fails, we probably can't proceed further with this handoff.
            // S2S Comms Manager will still send an ACK based on successful receipt of data,
            // but the ACK could be enhanced to indicate reconstruction failure.
            return;
        }
    }
    else
    {
        UE_LOG(LogOWSPlayerReconstruction, Error, TEXT("CharacterClassToSpawn is null. Cannot spawn character for %s."), *PlayerData.CharacterName);
        return;
    }

    // --- Apply State (Focus on non-snapshot data first) ---
    if (SpawnedCharacter)
    {
        // Velocity (needs MovementComponent)
        if (UCharacterMovementComponent* MoveComp = SpawnedCharacter->GetCharacterMovement())
        {
            MoveComp->Velocity = PlayerData.PlayerVelocity.ToFVector();
            UE_LOG(LogOWSPlayerReconstruction, Verbose, TEXT("Applied Velocity: %s"), *MoveComp->Velocity.ToString());
        }

        // Basic Stats (conceptual - assuming UOWSHealthComponent exists on the character)
        UOWSHealthComponent* HealthComponent = SpawnedCharacter->FindComponentByClass<UOWSHealthComponent>();
        if (HealthComponent)
        {
            // These would typically be methods like SetCurrentHealth, etc.
            // For now, direct property access if they were public, or log the intent.
            // HealthComponent->CurrentHealth = PlayerData.CurrentHealth; // Example
            // HealthComponent->CurrentMana = PlayerData.CurrentMana;   // Example
            UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Applied Basic Stats: Health: %.2f, Mana: %.2f, Energy: %.2f, Stamina: %.2f (conceptual)"),
                PlayerData.CurrentHealth, PlayerData.CurrentMana, PlayerData.CurrentEnergy, PlayerData.CurrentStamina);
        }
        else
        {
            UE_LOG(LogOWSPlayerReconstruction, Warning, TEXT("UOWSHealthComponent not found on spawned character %s. Cannot apply basic stats."), *SpawnedCharacter->GetName());
        }

        // CustomCharacterData (example logging)
        for (const auto& Elem : PlayerData.CustomCharacterData)
        {
            UE_LOG(LogOWSPlayerReconstruction, Verbose, TEXT("CustomData: Key=%s, Value=%s"), *Elem.Key, *Elem.Value);
            // Here you would apply this data to relevant components or properties
        }

        // Inventory (example logging)
        for (const FInventoryItemData& Item : PlayerData.Inventory)
        {
            UE_LOG(LogOWSPlayerReconstruction, Verbose, TEXT("InventoryItem: Name=%s, Qty=%d, CustomData=%s"), *Item.ItemName, Item.Quantity, *Item.CustomData);
            // Here you would add items to an inventory component
        }
    }

    // --- Snapshot Deserialization (Conceptual/Placeholder) ---
    UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("PlayerStateSnapshot received. Size: %d bytes."), PlayerData.PlayerStateSnapshot.Num());
    UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("PawnStateSnapshot received. Size: %d bytes."), PlayerData.PawnStateSnapshot.Num());
    // TODO: Implement actual deserialization using FMemoryReader if feasible as a stretch goal.
    // Example for PlayerState (very conceptual, real implementation is complex):
    /*
    if (SpawnedCharacter && PlayerData.PlayerStateSnapshot.Num() > 0)
    {
        APlayerState* NewPlayerState = SpawnedCharacter->GetPlayerState(); // May not exist yet or be the right one
        if (!NewPlayerState && World) // If no PlayerState, try to spawn one (uncommon for handoff)
        {
            NewPlayerState = World->SpawnActor<APlayerState>(); // Very basic, likely needs specific class
        }

        if (NewPlayerState)
        {
            FMemoryReader Reader(PlayerData.PlayerStateSnapshot, true);
            // NewPlayerState->Serialize(Reader); // This is NOT how Serialize works for applying state.
            // Actor state replication is complex. This is a placeholder for future deep dive.
            UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Conceptual: Would attempt to apply PlayerStateSnapshot here."));
        }
    }
    */

    // --- Handoff Token Generation & Storage ---
    FString HandoffSessionToken = FGuid::NewGuid().ToString();
    // Store Token -> CharacterName mapping.
    // For client login, we'd need to look up by CharacterName or UserSessionGUID.
    // Let's use CharacterName as the key as per the function ConsumeHandoffTokenForCharacter
    PendingHandoffTokens.Add(PlayerData.CharacterName, HandoffSessionToken);

    UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Generated HandoffSessionToken: %s for Character: %s (UserSessionGUID: %s)"), 
        *HandoffSessionToken, *PlayerData.CharacterName, *PlayerData.UserSessionGUID);

    // The UOWSS2SCommsManager will send the acknowledgment back to S_Src
    // after this delegate (OnPlayerStateReceived) completes.
}

FString UOWSPlayerReconstructionManager::ConsumeHandoffTokenForCharacter(const FString& CharacterName)
{
    if (CharacterName.IsEmpty())
    {
        UE_LOG(LogOWSPlayerReconstruction, Warning, TEXT("ConsumeHandoffTokenForCharacter: CharacterName is empty."));
        return TEXT("");
    }

    FString* FoundToken = PendingHandoffTokens.Find(CharacterName);
    if (FoundToken)
    {
        FString TokenToReturn = *FoundToken;
        PendingHandoffTokens.Remove(CharacterName); // Consume the token
        UE_LOG(LogOWSPlayerReconstruction, Log, TEXT("Consumed HandoffSessionToken: %s for Character: %s"), *TokenToReturn, *CharacterName);
        return TokenToReturn;
    }
    else
    {
        UE_LOG(LogOWSPlayerReconstruction, Warning, TEXT("No HandoffSessionToken found for Character: %s"), *CharacterName);
        return TEXT("");
    }
}
