namespace OWSData.Models.Composites
{
    public class WorldServerInfoWithEndpoint
    {
        public int WorldServerID { get; set; }
        public string ServerIP { get; set; } // Public IP of the WorldServer/InstanceLauncher machine
        public string InternalServerIP { get; set; }
        public string S2SEndpoint { get; set; } // e.g., "grpc://<internal_ip>:<s2s_port>"
        public int CurrentLoad { get; set; } // e.g., number of active cells or players, can be 0 initially
    }
}
