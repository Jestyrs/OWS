namespace OWSInstanceManagement.Requests.Instance
{
    public class UpdateWorldServerS2SEndpointRequest {
        public int WorldServerID { get; set; }
        public string S2SEndpoint { get; set; }
    }
}
