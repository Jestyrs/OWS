using System;
using System.Collections.Generic;

// Define helper structs/classes for S2S communication.
// These are simplified C# representations. UE side would use FVector, FRotator etc.
// Placing them outside the namespace to be globally accessible within this context,
// or they could be in a dedicated OWSShared.DTOs.S2S namespace if preferred.
public struct S2SVector 
{ 
    public float X { get; set; } 
    public float Y { get; set; } 
    public float Z { get; set; } 
}

public struct S2SRotator 
{ 
    public float Pitch { get; set; } 
    public float Yaw { get; set; } 
    public float Roll { get; set; } 
}

[Serializable] // Ensure helper classes are also serializable if they are part of the main DTO
public class S2SActiveGameplayEffectInfo 
{ 
    public string EffectName { get; set; }
    public float RemainingDuration { get; set; }
    public int StackCount { get; set; }
    // Add other relevant data, e.g., Level, SourceActorID etc.
}

[Serializable]
public class CustomCharacterDataPair
{
    public string CustomFieldName { get; set; }
    public string FieldValue { get; set; }
}

namespace OWSShared.Messages // Or a new sub-namespace like OWSShared.Messages.S2S
{
    [Serializable] // Good practice for DTOs that might be serialized in various ways
    public class PlayerStateDataForHandoff
    {
        // Identifying Info
        public Guid UserSessionGUID { get; set; } 
        public string CharacterName { get; set; }

        // Persisted State Snapshot (Mimicking some fields from GetCharByCharName for baseline)
        public string ClassName { get; set; }
        public short CharacterLevel { get; set; } // OWSData.Models.Tables.Characters uses short for Level
        public double X_DB { get; set; } // Persisted Location from Characters table
        public double Y_DB { get; set; }
        public double Z_DB { get; set; }
        public double RX_DB { get; set; } // Persisted Rotation from Characters table
        public double RY_DB { get; set; }
        public double RZ_DB { get; set; }
        public double MaxHealth_DB { get; set; } // From Characters table (or computed)
        public double MaxMana_DB { get; set; }   // From Characters table (or computed)
        public double MaxEnergy_DB { get; set; } // From Characters table (or computed)
        public double MaxStamina_DB { get; set; }// From Characters table (or computed)
        // Consider adding other important stats that define the character's build fundamentally from DB

        // Dynamic Runtime State (from live UE server)
        public double CurrentHealth { get; set; }
        public double CurrentMana { get; set; }
        public double CurrentEnergy { get; set; }
        public double CurrentStamina { get; set; }
        
        public S2SVector LivePosition { get; set; }
        public S2SRotator LiveRotation { get; set; }
        public S2SVector LiveVelocity { get; set; }
        
        // Serialized UProperties of key actors (byte arrays or JSON strings)
        // For gRPC, byte[] is more natural (bytes type in proto).
        // For JSON over HTTP/RabbitMQ, base64 encoded string of bytes, or structured JSON.
        public byte[] PlayerStateSnapshot { get; set; }   // For APlayerState
        public byte[] CharacterSnapshot { get; set; }     // For ACharacter & UCharacterMovementComponent
        public Dictionary<string, byte[]> ActorComponentSnapshots { get; set; } // Key: ComponentName or Path

        // Non-UPROPERTY dynamic state
        public Dictionary<string, float> ActiveAbilityCooldowns { get; set; } // Key: AbilityName, Value: RemainingCooldownSeconds
        public List<S2SActiveGameplayEffectInfo> ActiveGameplayEffects { get; set; } // For buffs/debuffs

        // List of custom character data fields (live values from server)
        // OWSData.Models.Tables.CustomCharacterData uses string for FieldValue, so this aligns.
        public List<CustomCharacterDataPair> LiveCustomCharacterData { get; set; }

        // Add other game-specific state as needed (e.g., current target, interaction state)
    }
}
