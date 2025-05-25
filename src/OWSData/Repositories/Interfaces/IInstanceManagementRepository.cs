using OWSData.Models.Composites;
using OWSData.Models.StoredProcs;
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;

namespace OWSData.Repositories.Interfaces
{
    public interface IInstanceManagementRepository
    {
        Task<GetServerInstanceFromPort> GetZoneInstance(Guid customerGUID, int zoneInstanceId);
        Task<GetServerInstanceFromPort> GetServerInstanceFromPort(Guid customerGUID, string serverIP, int port);
        Task<IEnumerable<GetZoneInstancesForWorldServer>> GetZoneInstancesForWorldServer(Guid customerGUID, int worldServerID);
        Task<SuccessAndErrorMessage> SetZoneInstanceStatus(Guid customerGUID, int zoneInstanceID, int instanceStatus);
        Task<SuccessAndErrorMessage> ShutDownWorldServer(Guid customerGUID, int worldServerID);
        Task<int> StartWorldServer(Guid customerGUID, string launcherGuid);
        Task<SuccessAndErrorMessage> UpdateNumberOfPlayers(Guid customerGUID, int zoneInstanceId, int numberOfPlayers);
        Task<IEnumerable<GetZoneInstancesForZone>> GetZoneInstancesOfZone(Guid customerGUID, string ZoneName);
        Task<GetCurrentWorldTime> GetCurrentWorldTime(Guid customerGUID);
        Task<SuccessAndErrorMessage> RegisterLauncher(Guid customerGUID, string launcherGuid, string serverIp, int maxNumberOfInstances, string internalServerIp, int startingInstancePort);
        Task<SuccessAndErrorMessage> AddZone(Guid customerGUID, string mapName,	string zoneName, string worldCompContainsFilter, string worldCompListFilter, int softPlayerCap, int hardPlayerCap, int mapMode);
        Task<SuccessAndErrorMessage> UpdateZone(Guid customerGUID, int mapId, string mapName, string zoneName, string worldCompContainsFilter, string worldCompListFilter, int softPlayerCap, int hardPlayerCap, int mapMode);

        // Methods for Grid Cell Management (Server Meshing)
        Task<SuccessAndErrorMessage> AssignGridCellToWorldServer(Guid customerGUID, string cellID, int worldServerID, int mapInstanceID);
        Task<SuccessAndErrorMessage> ClearGridCellAssignment(Guid customerGUID, string cellID);
        Task<IEnumerable<string>> GetAssignedCellsForWorldServer(Guid customerGUID, int worldServerID); // Returns list of CellIDs
        Task<WorldServerInfoWithEndpoint> GetWorldServerManagingCell(Guid customerGUID, string cellID);
        Task<IEnumerable<WorldServerInfoWithEndpoint>> GetWorldServersForCells(Guid customerGUID, IEnumerable<string> cellIDs);

        // Method to update/register S2S endpoint for a WorldServer (Server Meshing)
        Task<SuccessAndErrorMessage> UpdateWorldServerS2SEndpoint(Guid customerGUID, int worldServerID, string s2sEndpoint);
    }
}
