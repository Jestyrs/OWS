namespace OWSShared.Messages
{
    public class TransferPlayerStateResponse {
        public bool Success { get; set; }
        public string ErrorMessage { get; set; }
        public string HandoffSessionToken { get; set; } // For the client to use with the target server
    }
}
