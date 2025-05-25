namespace OWSInstanceManagement.Requests.Instance // Or OWSInstanceManagement.Requests.Handoff;
{
    public class HandoffPreparationResponse {
        public bool CanProceed { get; set; }
        public string TargetWorldServerS2SEndpoint { get; set; }
        public string ErrorMessage { get; set; }
    }
}
