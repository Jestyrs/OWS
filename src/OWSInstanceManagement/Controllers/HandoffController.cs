using Microsoft.AspNetCore.Mvc;
using OWSInstanceManagement.Requests.Instance; // Or .Requests.Handoff
using OWSData.Repositories.Interfaces;
using OWSShared.Interfaces;
using OWSShared.Messages; // For MQPrepareToReceivePlayerMessage
using RabbitMQ.Client; // Required for publishing
using Microsoft.Extensions.Options;
using OWSShared.Options; // For RabbitMQOptions
using System;
using System.Text; // For Encoding
using System.Text.Json; // For JsonSerializer
using System.Threading.Tasks;
using OWSData.Models.Composites; // For WorldServerInfoWithEndpoint
using Serilog;

namespace OWSInstanceManagement.Controllers
{
    [Route("api/[controller]")]
    [ApiController]
    public class HandoffController : ControllerBase 
    {
        private readonly IInstanceManagementRepository _instanceManagementRepository;
        private readonly IHeaderCustomerGUID _customerGuid;
        private readonly IConnection _rabbitConnection; 
        private readonly RabbitMQOptions _rabbitMQOptions;
        private readonly ILogger _logger;

        public HandoffController(
            IInstanceManagementRepository instanceManagementRepository,
            IHeaderCustomerGUID customerGuid,
            IOptions<RabbitMQOptions> rabbitMQOptionsAccessor,
            ILogger logger) 
        {
            _instanceManagementRepository = instanceManagementRepository;
            _customerGuid = customerGuid;
            _rabbitMQOptions = rabbitMQOptionsAccessor.Value; 
            _logger = logger;

            var factory = new ConnectionFactory() { 
                HostName = _rabbitMQOptions.RabbitMQHostName, 
                Port = _rabbitMQOptions.RabbitMQPort, 
                UserName = _rabbitMQOptions.RabbitMQUserName, 
                Password = _rabbitMQOptions.RabbitMQPassword 
            };
            try 
            {
                _rabbitConnection = factory.CreateConnection();
                _logger.Information("Successfully connected to RabbitMQ for HandoffController.");
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "HandoffController could not connect to RabbitMQ during initialization.");
                _rabbitConnection = null; 
            }
        }

        [HttpPost]
        [Route("RequestHandoffPreparation")]
        [Produces(typeof(HandoffPreparationResponse))]
        public async Task<IActionResult> RequestHandoffPreparation([FromBody] HandoffPreparationRequest request)
        {
            if (_rabbitConnection == null || !_rabbitConnection.IsOpen) {
                _logger.Error("RequestHandoffPreparation: RabbitMQ connection is not open or available.");
                return StatusCode(503, new HandoffPreparationResponse { CanProceed = false, ErrorMessage = "Message queue service unavailable." });
            }

            Guid customerGUID = _customerGuid.CustomerGUID;
            if (customerGUID == Guid.Empty) {
                _logger.Warning("RequestHandoffPreparation: Unauthorized attempt due to empty CustomerGUID.");
                return Unauthorized(); 
            }

            if (request == null || string.IsNullOrEmpty(request.TargetGridCellID) || request.SourceWorldServerID <= 0 || string.IsNullOrEmpty(request.CharacterName) || request.PlayerUserSessionGUID == Guid.Empty) {
                _logger.Warning("RequestHandoffPreparation: Invalid request parameters {@Request}", request);
                return BadRequest(new HandoffPreparationResponse { CanProceed = false, ErrorMessage = "Invalid request parameters." });
            }
            
            _logger.Information("RequestHandoffPreparation called for Character: {CharacterName}, TargetCell: {TargetGridCellID}", request.CharacterName, request.TargetGridCellID);

            WorldServerInfoWithEndpoint targetWorldServerInfo = await _instanceManagementRepository.GetWorldServerManagingCell(customerGUID, request.TargetGridCellID);

            if (targetWorldServerInfo == null || string.IsNullOrEmpty(targetWorldServerInfo.S2SEndpoint)) {
                _logger.Warning("RequestHandoffPreparation: Target cell {TargetGridCellID} not managed or target server S2S endpoint not configured. {@TargetWorldServerInfo}", request.TargetGridCellID, targetWorldServerInfo);
                return NotFound(new HandoffPreparationResponse { CanProceed = false, ErrorMessage = "Target cell is not managed or target server S2S endpoint not configured." });
            }
            
            WorldServerInfoWithEndpoint sourceWorldServerInfo = await _instanceManagementRepository.GetWorldServerManagingCell(customerGUID, request.CurrentGridCellID); // Assuming S_Src manages CurrentGridCellID
             if (sourceWorldServerInfo == null || string.IsNullOrEmpty(sourceWorldServerInfo.S2SEndpoint)) {
                _logger.Error("RequestHandoffPreparation: Source server (managing cell {CurrentGridCellID}) S2S endpoint not configured. {@SourceWorldServerInfo}", request.CurrentGridCellID, sourceWorldServerInfo);
                return StatusCode(500, new HandoffPreparationResponse { CanProceed = false, ErrorMessage = "Source server S2S endpoint not configured." });
            }

            try
            {
                using (var channel = _rabbitConnection.CreateModel())
                {
                    string exchangeName = _rabbitMQOptions.ExchangeName ?? "ows.instance.management"; 
                    string routingKey = $"ows.handoff.prepare.{targetWorldServerInfo.WorldServerID}"; 
                    
                    channel.ExchangeDeclare(exchange: exchangeName, type: ExchangeType.Direct, durable: _rabbitMQOptions.Durable, autoDelete: _rabbitMQOptions.AutoDelete);
                    
                    // It is assumed that the target UE server (or its launcher proxy) will have a queue bound with this routingKey.
                    // For example, QueueName: "ows.handoff.prepare.queue.{WorldServerID}"

                    var message = new MQPrepareToReceivePlayerMessage
                    {
                        CustomerGUID = customerGUID,
                        PlayerUserSessionGUID = request.PlayerUserSessionGUID,
                        CharacterName = request.CharacterName,
                        SourceWorldServerID = request.SourceWorldServerID,
                        SourceServerS2SEndpoint = sourceWorldServerInfo.S2SEndpoint, 
                        TargetGridCellID = request.TargetGridCellID,
                        CharacterBaseline = null // TODO: Populate with actual baseline data from ICharactersRepository if needed.
                                                 // Example: var charData = await _charactersRepository.GetCharByCharName(request.CharacterName, request.PlayerUserSessionGUID);
                                                 // then map relevant fields to CharacterBaselineDataForHandoff.
                    };

                    var body = JsonSerializer.SerializeToUtf8Bytes(message, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
                    var properties = channel.CreateBasicProperties();
                    properties.Persistent = true; // Make message persistent if broker restarts

                    _logger.Information("Publishing MQPrepareToReceivePlayerMessage to Exchange: {Exchange}, RoutingKey: {RoutingKey} for TargetWorldServerID: {TargetWorldServerID}", exchangeName, routingKey, targetWorldServerInfo.WorldServerID);
                    channel.BasicPublish(exchange: exchangeName, routingKey: routingKey, basicProperties: properties, body: body);
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "RequestHandoffPreparation: Error publishing MQPrepareToReceivePlayerMessage to RabbitMQ for Character: {CharacterName}, TargetCell: {TargetGridCellID}", request.CharacterName, request.TargetGridCellID);
                return StatusCode(500, new HandoffPreparationResponse { CanProceed = false, ErrorMessage = "Failed to notify target server due to message queue error." });
            }

            _logger.Information("RequestHandoffPreparation successful for Character: {CharacterName}. Target S2S Endpoint: {TargetS2SEndpoint}", request.CharacterName, targetWorldServerInfo.S2SEndpoint);
            return Ok(new HandoffPreparationResponse { 
                CanProceed = true, 
                TargetWorldServerS2SEndpoint = targetWorldServerInfo.S2SEndpoint 
            });
        }
        
        // Proper disposal of RabbitMQ connection if managed directly by this controller.
        // However, in a typical ASP.NET Core setup, IConnection might be registered as a singleton
        // and managed by the DI container, making direct disposal here unnecessary or even harmful.
        // For this exercise, assuming simplified direct management as per instructions:
        // public void Dispose() { _rabbitConnection?.Close(); _rabbitConnection?.Dispose(); _logger.Information("HandoffController disposed, RabbitMQ connection closed."); }
    }
}
