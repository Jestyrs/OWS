# Server Meshing Handoff Mechanism

This document outlines the design and current implementation status of the server meshing handoff mechanism for Open World Server (OWS). This feature allows players to move between different Unreal Engine server instances (zones or grid cells) managing different parts of the game world seamlessly, without a traditional loading screen.

## 1. Handoff Token Flow Design

The core of the seamless handoff relies on a `HandoffSessionToken` to securely transition a player's session from a source server (S_Src) to a target server (S_Tgt).

1.  **Initiation (S_Src):**
    *   The `UHandoffComponent` attached to a player character on S_Src detects that the player needs to be handed off to an adjacent server instance (managing a new grid cell/zone). (Current boundary detection in C++ is placeholder).
    *   `UHandoffComponent` calls the `/api/Handoff/RequestHandoffPreparation` endpoint on the `OWSInstanceManagement` service. This request includes identifiers for the player, character, source server, current grid cell, and target grid cell.

2.  **Preparation & Notification (OWSInstanceManagement & S_Tgt):**
    *   `OWSInstanceManagement` validates the request and identifies S_Tgt responsible for the target grid cell.
    *   `OWSInstanceManagement` sends an `MQPrepareToReceivePlayerMessage` via RabbitMQ to S_Tgt. This message includes:
        *   Player/character identifiers.
        *   S_Src's S2S communication endpoint.
        *   Basic character data (`CharacterBaselineDataForHandoff`) fetched from the database to allow S_Tgt to make initial preparations.
    *   S_Tgt (specifically, its `UOWSPlayerReconstructionManager` via an Instance Launcher proxy or direct RabbitMQ client) consumes this message.

3.  **Player State Reconstruction and Token Generation (S_Tgt):**
    *   S_Tgt's `UOWSPlayerReconstructionManager`, upon being alerted (conceptually after RabbitMQ message consumption and S2S data arrival):
        *   Receives the full, dynamic player state (`FPlayerStateDataForHandoff`) from S_Src via a direct S2S HTTP call (handled by `UOWSS2SCommsManager`).
        *   Spawns a new character for the player (placeholder implementation).
        *   Applies the received state to this character (placeholder for full dynamic state, basic transform/stats applied).
        *   Generates a unique, short-lived `HandoffSessionToken` (FString GUID).
        *   Stores this token locally, associated with the `CharacterName` and the newly reconstructed pawn (e.g., in `UOWSPlayerReconstructionManager::PendingHandoffData`).

4.  **S2S Acknowledgment with Token (S_Tgt to S_Src):**
    *   S_Tgt's `UOWSS2SCommsManager`, after `UOWSPlayerReconstructionManager` has processed the state and generated the token:
        *   Retrieves the `HandoffSessionToken` from `UOWSPlayerReconstructionManager`.
        *   Sends an S2S HTTP acknowledgment message back to S_Src's `/handoff/acknowledge_preparation` endpoint.
        *   **Intended Design (Implementation Incomplete):** This JSON acknowledgment payload is designed to include `{"status": "ready", "handoffToken": "TOKEN_HERE", "characterName": "CHAR_NAME_HERE"}`.

5.  **Token Reception and Client RPC Trigger (S_Src):**
    *   S_Src's `UOWSS2SCommsManager` receives the acknowledgment and parses the `handoffToken` and `characterName`.
    *   It broadcasts this information via its `OnS2SAcknowledgmentReceived` delegate.
    *   `UHandoffComponent` on S_Src, listening to this delegate:
        *   **Intended Design (Implementation Incomplete):** Stores the received `handoffToken`.
        *   Calls the `ClientRPC_ExecuteSeamlessHandoff` on the client's `AOWSPlayerController`. This RPC is intended to pass S_Tgt's connection details (IP:Port) and the `handoffToken`.

6.  **Client Travels to S_Tgt with Token:**
    *   The client, upon receiving the RPC, executes `UOWSGameInstanceSubsystem::ConnectToTargetMeshedServer`.
    *   **Intended Design (Implementation Incomplete):** This function constructs a travel URL to S_Tgt including `?CharacterName=...&HandoffToken=...&bIsHandoff=true`.
    *   The client disconnects from S_Src and connects to S_Tgt using this URL.

7.  **Handoff Authentication at S_Tgt:**
    *   S_Tgt's GameMode (`AOWSGameModeBase` or equivalent), in `PreLogin` or `Login`:
        *   **Intended Design (Implementation Incomplete):** Parses the incoming client URL for `bIsHandoff`, `CharacterName`, and `HandoffToken`.
        *   If `bIsHandoff` is true, it calls `UOWSPlayerReconstructionManager::ConsumeHandoffDataForCharacter()` with the `CharacterName` and `HandoffToken` from the URL.
        *   `UOWSPlayerReconstructionManager` validates the token against its stored tokens. If valid, it provides the pre-spawned pawn and associated `UserSessionGUID`.
        *   The GameMode bypasses standard OWS authentication and allows the client to possess the reconstructed pawn.

## 2. Intended Client-Side Connection Logic

*   **`AOWSPlayerController::ClientRPC_ExecuteSeamlessHandoff`:**
    *   Defined in `AOWSPlayerController.h`.
    *   Intended parameters: `(const FString& TargetServerIP, int32 TargetServerPort, const FString& CharacterNameToHandoff, const FString& TokenFromS_Tgt)`.
    *   Implementation (Conceptual): Calls `UOWSGameInstanceSubsystem::ConnectToTargetMeshedServer` with these details.
*   **`UOWSGameInstanceSubsystem::ConnectToTargetMeshedServer`:**
    *   Defined in `UOWSGameInstanceSubsystem.h`.
    *   Intended parameters: `(const FString& TargetServerIP, int32 TargetServerPort, const FString& CharacterNameToHandoff, const FString& TokenForClient)`.
    *   Implementation (Conceptual):
        *   Constructs travel URL: `FString::Printf(TEXT("%s:%d/Game/Maps/YourDefaultMapEntry?CharacterName=%s?HandoffToken=%s?bIsHandoff=true"), ...)`
        *   Uses `ClientTravel()` to connect the client to S_Tgt.

## 3. Intended GameMode Handoff Authentication (S_Tgt)

*   Target GameMode: `AOWSGameModeBase` (or identified OWS equivalent).
*   **Override `PreLogin` (or `Login`):**
    *   Parse `OptionsString` from incoming client connection for `bIsHandoff`, `CharacterName`, and `HandoffToken`.
    *   If `bIsHandoff` is true:
        *   Retrieve `UOWSPlayerReconstructionManager`.
        *   Call `ConsumeHandoffDataForCharacter(CharacterNameFromURL, TokenFromURL, OutPawn, OutUserSessionGUID)`.
        *   If valid: Approve login, store `OutPawn` and `OutUserSessionGUID` for `PostLogin` (e.g., in a temporary map keyed by PlayerController or in a custom PlayerState).
        *   Else: Reject login.
    *   Else: Proceed with standard OWS authentication.
*   **Override `PostLogin` (or `HandleStartingNewPlayer`/`InitNewPlayer`):**
    *   If player was approved for handoff:
        *   Retrieve the stored `OutPawn` and `OutUserSessionGUID`.
        *   Ensure `APlayerState` is correctly set up with `UserSessionGUID`.
        *   Possess `OutPawn`.
    *   Else: Standard OWS player initialization.

## 4. Current Status of C++ Components

*   **`FPlayerStateDataForHandoff` (C++ struct - `OWSPlayerStateData.h`):**
    *   Defined with fields mirroring the C# DTO.
    *   Includes `ToJsonString()` and `FromJsonString()` methods for JSON serialization via `FJsonObjectConverter`.
*   **`UOWSS2SCommsManager` (C++ UObject - `OWSS2SCommsManager.h/.cpp`):**
    *   Implemented as a `UGameInstanceSubsystem`.
    *   Capable of starting/stopping HTTP listeners (`IHttpRouter`) for S2S messages on configurable ports.
        *   Handles `/handoff/receive_state` (S_Tgt): Deserializes `FPlayerStateDataForHandoff` from JSON, broadcasts `OnPlayerStateReceived` delegate.
        *   Handles `/handoff/acknowledge_preparation` (S_Src): Parses JSON acknowledgment.
    *   Capable of sending HTTP POST requests for S2S messages.
        *   `SendPlayerState()` (S_Src): Serializes `FPlayerStateDataForHandoff` and POSTs it.
        *   `SendAcknowledgePreparation()` (S_Tgt): Sends a status JSON back to S_Src.
    *   **Token Propagation in Ack:** Designed for `SendAcknowledgePreparation` to include the handoff token. The delegate `OnS2SAcknowledgmentReceived` signature is updated for this. *Full implementation and testing of this token path is incomplete.*
*   **`UHandoffComponent` (C++ UActorComponent - `UHandoffComponent.h/.cpp`):**
    *   Role: Orchestrates handoff on S_Src.
    *   `BeginPlay()`: Gets `UOWSAPISubsystem` and `UOWSS2SCommsManager`. Binds to `OnS2SAcknowledgmentReceived`.
    *   `CheckForBoundaryTransition()`: Placeholder logic to simulate handoff need.
    *   `InitiateHandoffPreparation()`: Calls backend `/api/Handoff/RequestHandoffPreparation`. Handles response and waits for S2S acknowledgment.
    *   `TriggerPlayerStateSend()`: Called after S_Tgt acknowledgment. Populates (currently with placeholder data) and sends `FPlayerStateDataForHandoff` using `UOWSS2SCommsManager`.
    *   `FinalizeHandoffOnSource()`: Placeholder for player removal; intended to call `ClientRPC_ExecuteSeamlessHandoff`.
    *   **Token Handling:** Designed to receive token from `OnS2SAcknowledgmentReceived` and pass to client RPC. *Implementation incomplete.*
*   **`UOWSPlayerReconstructionManager` (C++ UGameInstanceSubsystem - `UOWSPlayerReconstructionManager.h/.cpp`):**
    *   Role: Manages player reconstruction on S_Tgt.
    *   `Initialize()`: Gets `UOWSS2SCommsManager`, subscribes to `OnPlayerStateReceived`, starts S2S listener.
    *   `HandlePlayerStateReceived()`:
        *   Logs received player data.
        *   Spawns a default character at the specified transform.
        *   Applies basic stats (placeholder for full state application from snapshots).
        *   Generates `HandoffSessionToken` (GUID).
        *   Stores token, spawned pawn, and UserSessionGUID in `PendingHandoffData` map, keyed by `CharacterName`.
    *   `GetHandoffTokenForCharacter()`: Non-consuming getter for the token.
    *   `ConsumeHandoffDataForCharacter()`: Validates token and provides pawn/UserSessionGUID, removing the entry.

## 5. Outstanding C++ Implementation and Key Challenges

The foundational C++ classes and S2S communication pathways are partially established. However, critical C++ logic remains incomplete or placeholder:

*   **Full Handoff Token Propagation:**
    *   Reliable transmission of the token from S_Tgt's `UOWSPlayerReconstructionManager` back to S_Src's `UHandoffComponent` via the `UOWSS2SCommsManager` acknowledgment.
    *   Passing this token to the client via `ClientRPC_ExecuteSeamlessHandoff`.
*   **Client-Side Execution:**
    *   Full implementation of `AOWSPlayerController::ClientRPC_ExecuteSeamlessHandoff`.
    *   Full implementation of `UOWSGameInstanceSubsystem::ConnectToTargetMeshedServer` to correctly use the token in the travel URL.
*   **GameMode Authentication on S_Tgt:**
    *   Complete implementation of `PreLogin` and `PostLogin` (or equivalents) in the OWS GameMode to handle the handoff URL parameters, validate the token, and possess the reconstructed pawn.
*   **Dynamic Data Serialization/Deserialization:**
    *   The C++ `FPlayerStateDataForHandoff` currently uses placeholder data. Robustly serializing live `APlayerState`, `ACharacter`, `UCharacterMovementComponent`, and other custom components into the `TArray<uint8>` snapshots on S_Src.
    *   Robustly deserializing these snapshots on S_Tgt and applying them to the newly spawned character and its components. This is a complex task involving deep knowledge of Unreal Engine's object model and serialization.
*   **Error Handling and Resilience:** Adding comprehensive error handling, retries for S2S communication, and handling for scenarios like S_Tgt failing after S_Src has removed the player.
*   **Precise Boundary Detection:** Replacing placeholder boundary detection in `UHandoffComponent` with actual logic based on world grid data.
*   **Configuration:** Exposing necessary configurations (e.g., S2S ports, handoff timeouts) in a manageable way.

The primary challenge during development has been the reliable application of iterative C++ code changes using the available tooling and turn limits, especially for interconnected components.
