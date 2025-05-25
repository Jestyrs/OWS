namespace OWSInstanceManagement.Requests.Instance
{
    public class AssignGridCellRequest {
        public string CellID { get; set; }
        public int WorldServerID { get; set; }
        public int MapInstanceID { get; set; }
    }
}
