using System;

// Assuming a simple DTO for baseline data for now. This can be expanded.
// Or reference an existing DTO like OWSData.Models.StoredProcs.GetCharByCharName if suitable.
// For simplicity, let's define a minimal one here.
public class CharacterBaselineDataForHandoff {
    public string CharacterName { get; set; }
    public string ClassName {get; set;}
    public short CharacterLevel { get; set; }
    public string MapName { get; set; }
    public double MaxHealth { get; set; }
    public double MaxMana { get; set; }
    public double MaxEnergy { get; set; }
    public double MaxStamina { get; set; }
}

namespace OWSShared.Messages
{
    [Serializable] // Keep consistent with other MQ messages
    public class MQPrepareToReceivePlayerMessage {
        public Guid CustomerGUID { get; set; }
        public Guid PlayerUserSessionGUID { get; set; }
        public string CharacterName { get; set; }
        public int SourceWorldServerID { get; set; }
        public string SourceServerS2SEndpoint { get; set; } // S_Src's S2S endpoint
        public string TargetGridCellID { get; set; }
        public CharacterBaselineDataForHandoff CharacterBaseline { get; set; } // Optional baseline data

        // Add Serialize/Deserialize methods like in other MQ messages if needed,
        // or assume RabbitMQ library handles it if System.Text.Json is configured.
        // For consistency with existing MQSpinUpServerMessage, let's assume manual is not strictly needed here if not asked.
    }
}
