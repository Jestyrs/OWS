using System;

namespace OWSInstanceManagement.Requests.Instance
{
    public class HandoffPreparationRequest {
        public Guid PlayerUserSessionGUID { get; set; }
        public string CharacterName { get; set; }
        public int SourceWorldServerID { get; set; }
        public string CurrentGridCellID { get; set; }
        public string TargetGridCellID { get; set; }
    }
}
