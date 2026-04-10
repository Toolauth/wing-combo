# Workshop Tool Control System (Firmware)

This firmware runs on ESP32-S3 hardware to gate industrial machinery via an electronic self-latching relay system. It integrates RFID authorization, physical E-Stop monitoring, and a centralized management API.

## Core Logic & Safety
1.  **Self-Latching Architecture**: The ESP32 does not "hold" the tool on. It pulses a `SET` relay to allow a user to start the tool by pressing a green (Normally-OFF) button and pulses a `RESET` relay to force it off if needed (timeout). Typically a user will press a red (Normally-ON) button to stop the tool's operation.
2.  **E-Stop Monitoring**: Physical E-Stops are wired to the hardware. The ESP32 monitors these and will actively block `SET` pulses and report status if an E-Stop is depressed.
3.  **Bypass Mode**: A physical key switch allows shop managers to bypass the RFID/Network requirement for maintenance or in the event of firmware/network failures.
4.  **Network Resilience**: All events (scans, status changes) are stored in a 30-item queue. If the local server is down, the tool remains functional (if authorized) or retries the connection without blocking the main loop.

## File Structure
* `hal.cpp/h`: Hardware Abstraction Layer. All `digitalRead` and `pinMode` calls live here.
* `services.cpp/h`: Network, OTA, and RFID logic.
* `config_check.h`: Logic to ensure build flags from `platformio.ini` are applied correctly.
* `main.cpp`: The State Machine.

---

## Strategies to Improve Reliability

### 1. Strategy: Prioritized Pruning
When the queue reaches a "Pressure Threshold" (e.g., 25 out of 30 slots), the system follows these rules:
* **Discard Low Priority:** If the new event is a Heartbeat and the queue is heavy, drop it immediately.
* **Evict for High Priority:** If a critical event (E-Stop or Auth) arrives and the queue is full, search for and delete the oldest Heartbeat to make room.

#### Summary of Priority Levels
| Data Type | Path | Priority | Action on Congestion |
| :--- | :--- | :--- | :--- |
| **Emergency** | `/status` (estop) | **Critical** | Evict Heartbeats to fit. |
| **Access** | `/auth` | **High** | Evict Heartbeats to fit. |
| **Usage** | `/status` (relay/curr) | **Medium** | Standard FIFO. |
| **Diagnostics** | `/heartbeat` | **Low** | Drop if queue > 20 items. |

### 2. Event Coalescing (State Deduplication)
In your current `main.cpp`, you already check `if(now.relayLatched != last.relayLatched)`. This is excellent "Edge Triggering." To take it further, you can check the queue itself. If the network is down and the relay flips `ON -> OFF -> ON`, you have three messages queued. The server only needs to know the tool is currently `ON`.
* **The Trick:** Before adding a status update, check if the *last* item in the queue is for the same `key`. If it is, update the `value` of that existing item instead of adding a new one.

### 3. The "Offline Mode" Heartbeat
Currently, your `sendHeartbeat()` triggers every 30 minutes. 
* **The Trick:** If `WiFi.status() != WL_CONNECTED`, don't just return. Increment a "failure counter." If that counter hits a certain threshold (e.g., 2 hours of no connectivity), use `ESP.restart()`. Sometimes the internal WiFi stack can get into a state that only a hardware reset can fix.


---

## Configuration & Scalability

This project uses **PlatformIO Environments** to manage a fleet of different tools from a single codebase.

### Adding a New Tool
To add a new tool to the workshop, you do not need to change the source code. Simply add a new `[env:tool_name]` block to `platformio.ini`.

```ini
[env:lathe_01]
extends = room_metal           ; Inherits metalshop API keys and flags
build_flags = 
    ${room_metal.build_flags}
    -D TOOL_ID=\"lathe_01\"    ; Unique ID for the DB
    -D NUM_ESTOPS=1            ; Physical config
    -D HAS_NFC=1               ; Does this tool have a reader?
upload_port = lathe_01.local   ; Target for OTA updates
```

### Build Flag Reference
| Flag | Description | Default |
| :--- | :--- | :--- |
| `TOOL_ID` | String used in JSON payloads to identify the machine. | `unknown` |
| `NUM_ESTOPS` | Number of physical E-stop inputs to monitor (0, 1, or 2). | `1` |
| `HAS_NFC` | Set to `1` to enable PN532 RFID features, `0` to disable. | `1` |
| `ENABLE_DELAY` | How long (ms) the "Start" window stays open after auth. | `120000` (2m) |

---

## Deployment & OTA Updates

### Initial Flash (Wired)
1. Connect the ESP32 via USB.
2. Run: `pio run -e tool_name -t upload`

### Remote Update (OTA)
Once the tool is on the network, you can update it wirelessly without removing it from the machine. The firmware uses its `TOOL_ID` as its network hostname.

1. Ensure your computer is on the same local subnet as the tools.
2. Run the OTA command:
   ```bash
   # Example: Updating the Bridgeport Mill wirelessly
   pio run -e bridgeport_mill -t upload --upload-port bridgeport_mill.local
   ```

### Scalability Strategy for "Dozens"
For a large workshop, use the `[env]` base block in `platformio.ini` to define common settings (WiFi SSID, API URL, Library versions). 


* **Group by Room**: Use intermediate blocks (e.g., `[room_woodshop]`) to set environment-specific API paths.
* **Centralized Control**: Since `API_BASE_URL` is a build flag, you can point the entire fleet to a new server IP by changing one line and pushing an OTA update to all envs.
