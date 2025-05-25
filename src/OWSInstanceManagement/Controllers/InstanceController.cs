using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.Filters;
using Microsoft.Extensions.Options;
using SimpleInjector;
using OWSData.Models.StoredProcs;
using OWSShared.Interfaces;
using OWSInstanceManagement.Requests.Instance;
using OWSData.Models.Composites;
using OWSData.Repositories.Interfaces;
using OWSShared.Options;
using System.Net.Http;
using Serilog;

namespace OWSInstanceManagement.Controllers
{
    [Route("api/[controller]")]
    [ApiController]
    public class InstanceController : Controller
    {
        private readonly Container _container;
        private readonly IInstanceManagementRepository _instanceManagementRepository;
        private readonly ICharactersRepository _charactersRepository;
        private readonly IOptions<RabbitMQOptions> _rabbitMQOptions;
        private readonly IHeaderCustomerGUID _customerGuid;

        public InstanceController(Container container,
            IInstanceManagementRepository instanceManagementRepository,
            ICharactersRepository charactersRepository,
            IOptions<RabbitMQOptions> rabbitMQOptions,
            IHeaderCustomerGUID customerGuid)
        {
            _container = container;
            _instanceManagementRepository = instanceManagementRepository;
            _charactersRepository = charactersRepository;
            _rabbitMQOptions = rabbitMQOptions;
            _customerGuid = customerGuid;
        }

        public override void OnActionExecuting(ActionExecutingContext context)
        {
            IHeaderCustomerGUID customerGuid = _container.GetInstance<IHeaderCustomerGUID>();

            if (customerGuid.CustomerGUID == Guid.Empty)
            {
                context.Result = new UnauthorizedResult();
            }
        }

        [HttpPost]
        [Route("SetZoneInstanceStatus")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> SetZoneInstanceStatusRequest([FromBody] SetZoneInstanceStatusRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("ShutDownServerInstance")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> ShutDownServerInstance([FromBody] ShutDownServerInstanceRequest request)
        {
            request.SetData(_rabbitMQOptions, _instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("RegisterLauncher")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<SuccessAndErrorMessage> RegisterLauncher([FromBody] RegisterInstanceLauncherRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("SpinUpServerInstance")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> SpinUpServerInstance([FromBody] SpinUpServerInstanceRequest request)
        {
            request.SetData(_rabbitMQOptions, _charactersRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpGet]
        [Route("StartInstanceLauncher")]
        [Produces(typeof(int))]
        public async Task<IActionResult> StartInstanceLauncher()
        {
            StartInstanceLauncherRequest request = new StartInstanceLauncherRequest();
            var launcherGuid = Request.Headers["X-LauncherGUID"].FirstOrDefault();
            if (string.IsNullOrEmpty(launcherGuid))
            {
                Log.Error("Http Header X-LauncherGUID is empty!");
            }
            request.SetData(_instanceManagementRepository, launcherGuid, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("ShutDownInstanceLauncher")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> ShutDownInstanceLauncher([FromBody] ShutDownInstanceLauncherRequest request)
        {
            var launcherGuid = Request.Headers["X-LauncherGUID"].FirstOrDefault();
            if (string.IsNullOrEmpty(launcherGuid))
            {
                Log.Error("Http Header X-LauncherGUID is empty!");
            }
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("GetServerToConnectTo")]
        [Produces(typeof(JoinMapByCharName))]
        public async Task<IActionResult> GetServerToConnectToRequest([FromBody] GetServerToConnectToRequest request)
        {
            request.SetData(_charactersRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("GetZoneInstance")]
        [Produces(typeof(GetServerInstanceFromPort))]
        public async Task<GetServerInstanceFromPort> GetZoneInstance([FromBody] int ZoneInstanceId)
        {
            GetZoneInstanceRequest request = new GetZoneInstanceRequest(ZoneInstanceId, _instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("GetServerInstanceFromPort")]
        [Produces(typeof(GetServerInstanceFromPort))]
        public async Task<GetServerInstanceFromPort> GetServerInstanceFromPort([FromBody] GetServerInstanceFromPortRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid, Request.HttpContext.Connection.RemoteIpAddress.ToString());
            return await request.Handle();
        }

        [HttpPost]
        [Route("GetZoneInstancesForWorldServer")]
        [Produces(typeof(IEnumerable<GetZoneInstancesForWorldServer>))]
        public async Task<IActionResult> GetZoneInstancesForWorldServer([FromBody] GetZoneInstancesForWorldServerRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("UpdateNumberOfPlayers")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> UpdateNumberOfPlayers([FromBody] UpdateNumberOfPlayersRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("GetZoneInstancesForZone")]
        [Produces(typeof(IEnumerable<GetZoneInstancesForZone>))]
        public async Task<IActionResult> GetZoneInstancesForZone([FromBody] GetZoneInstancesForZoneRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        [HttpPost]
        [Route("GetCurrentWorldTime")]
        [Produces(typeof(GetCurrentWorldTime))]
        public async Task<IActionResult> GetCurrentWorldTime([FromBody] GetCurrentWorldTimeRequest request)
        {
            request.SetData(_instanceManagementRepository, _customerGuid);
            return await request.Handle();
        }

        // Grid Management Endpoints
        [HttpPost]
        [Route("Grid/AssignCell")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> AssignGridCell([FromBody] AssignGridCellRequest request)
        {
            var result = await _instanceManagementRepository.AssignGridCellToWorldServer(_customerGuid.CustomerGUID, request.CellID, request.WorldServerID, request.MapInstanceID);
            return Ok(result);
        }

        [HttpPost]
        [Route("Grid/ClearCellAssignment")]
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> ClearCellAssignment([FromBody] ClearCellAssignmentRequest request)
        {
            var result = await _instanceManagementRepository.ClearGridCellAssignment(_customerGuid.CustomerGUID, request.CellID);
            return Ok(result);
        }

        [HttpGet]
        [Route("Grid/MyAssignedCells")]
        [Produces(typeof(IEnumerable<string>))]
        public async Task<IActionResult> GetMyAssignedCells([FromQuery] int worldServerID) 
        {
            var result = await _instanceManagementRepository.GetAssignedCellsForWorldServer(_customerGuid.CustomerGUID, worldServerID);
            return Ok(result);
        }

        [HttpGet]
        [Route("Grid/GetServerForCell")]
        [Produces(typeof(WorldServerInfoWithEndpoint))] 
        public async Task<IActionResult> GetServerForCell([FromQuery] string cellID)
        {
            var result = await _instanceManagementRepository.GetWorldServerManagingCell(_customerGuid.CustomerGUID, cellID);
            if (result == null) return NotFound();
            return Ok(result);
        }

        [HttpPost]
        [Route("Grid/GetServersForCells")]
        [Produces(typeof(IEnumerable<WorldServerInfoWithEndpoint>))] 
        public async Task<IActionResult> GetServersForCells([FromBody] GetServersForCellsRequest request)
        {
            var result = await _instanceManagementRepository.GetWorldServersForCells(_customerGuid.CustomerGUID, request.CellIDs);
            return Ok(result);
        }
            
        [HttpPost]
        [Route("UpdateWorldServerS2SEndpoint")] 
        [Produces(typeof(SuccessAndErrorMessage))]
        public async Task<IActionResult> UpdateWorldServerS2SEndpoint([FromBody] UpdateWorldServerS2SEndpointRequest request)
        {
            var result = await _instanceManagementRepository.UpdateWorldServerS2SEndpoint(_customerGuid.CustomerGUID, request.WorldServerID, request.S2SEndpoint);
            return Ok(result);
        }
    }
}