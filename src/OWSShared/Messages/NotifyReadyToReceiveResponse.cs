namespace OWSShared.Messages
{
    public class NotifyReadyToReceiveResponse {
        public bool Acknowledged { get; set; }
        // Optionally, add an error message if Acknowledged is false
        public string ErrorMessage { get; set; }
    }
}
