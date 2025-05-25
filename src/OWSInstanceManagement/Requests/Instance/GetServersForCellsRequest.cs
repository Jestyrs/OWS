using System.Collections.Generic;

namespace OWSInstanceManagement.Requests.Instance
{
    public class GetServersForCellsRequest {
        public IEnumerable<string> CellIDs { get; set; }
    }
}
