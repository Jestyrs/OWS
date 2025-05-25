namespace OWSShared.Messages
{
    public class NotifyReadyToReceiveRequest {
        public string CharacterName { get; set; }
        public string SourceServerS2SEndpoint { get; set; } // The S2S endpoint of the server that will send the state (S_Src)
        public string TargetGridCellID { get; set; } // The cell S_Tgt is preparing for
    }
}
