# Workshop Tool Control System (Firmware)

This firmware runs on ESP32-S3 hardware to gate industrial machinery via an electronic self-latching relay system. It integrates RFID authorization, physical E-Stop monitoring, and a centralized management API with a focus on high reliability in "dirty" network environments.

## Core Logic & Safety
1.  **Self-Latching Architecture**: The ESP32 does not "hold" the tool on. It pulses a `SET` relay to allow a user to start the tool and pulses a `RESET` relay to force it off (e.g., on timeout).
2.  **E-Stop Monitoring**: Physical E-Stops are wired to the hardware. The ESP32 monitors these, actively blocks `SET` pulses, and reports status immediately if an E-Stop is depressed.
3.  **Bypass Mode**: A physical key switch allows managers to bypass the RFID/Network requirement for maintenance or during network failures.
4.  **Self-Healing Network**: The system monitors API success. If the device is disconnected from the server for longer than `MAX_OFFLINE_MS`, it performs a software reboot to clear the network stack.

## File Structure
* `hal.cpp/h`: Hardware Abstraction Layer. All `digitalRead` and `pinMode` calls live here.
* `services.cpp/h`: Network, OTA, and RFID logic. Includes the coalesced event queue.
* `watchdog.cpp/h`: Persistent reset tracking and software-triggered reboot logic.
* `config_check.h`: Logic to ensure build flags from `platformio.ini` are applied correctly.
* `main.cpp`: The State Machine, signal debouncing, and main execution loop.

---

## Strategies for Industrial Reliability

### 1. Event Coalescing (State Deduplication)
To prevent the 30-item network queue from filling with redundant data (e.g., a relay toggling rapidly), the system uses a **Last-Write-Wins** strategy.
* **How it works:** When a new event is queued, the system searches the existing queue for a matching `path` and `key`.
* **The Result:** If an update is already pending, the old value is overwritten and the timestamp is refreshed. This ensures only the *latest* state is sent, saving bandwidth and preventing buffer overflows.

### 2. Temporal Accuracy (Negative Time-Delta)
Network delays shouldn't corrupt your logs. Each event is timestamped at the moment of occurrence (`queuedAt`).
* **Offset Calculation:** When an event is finally transmitted, the ESP32 calculates the "age" of the event (`millis() - queuedAt`) and sends it as `offset_ms`.
* **The Result:** The server can subtract this offset from the receive time to record the **exact** millisecond the physical event happened, even if it was delayed by a 5-minute reboot cycle.

### 3. Signal Debouncing & Heartbeats
Electrical noise from industrial motors can cause "ghost" triggers. All physical inputs are processed through a `monitorSignal` helper.
* **Debounce:** Signals must remain stable for 50ms before a change is registered or reported.
* **Still-Active Heartbeat:** For critical states (like "Current Detected"), the system re-reports the "ON" state every 30 seconds to ensure the server knows the tool is still actively in use.

### 4. The "Self-Healing" Watchdog
The ESP32 uses RTC (Real-Time Clock) memory to track its own health across reboots.
* **Persistent Tally:** A `bootCount` survives software restarts.
* **API Check:** Connectivity isn't just "WiFi bars"; it is defined by successful HTTP 200 responses from the API.
* **The Reaper:** If no API contact occurs for 5 minutes, the device increments its `bootCount` and reboots itself. On the next successful connection, it reports its reboot history to the server for diagnostics.

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

### Network & MQTT Credentials
Define these in your `platformio.ini` or a global header:
* `WIFI_SSID` / `WIFI_PASS`
* `MQTT_SERVER` (e.g., `192.168.1.50`)
* `MQTT_USER` / `MQTT_PASS`
* `OTA_PASSWORD`

### Flags that change for each tool
| Flag | Description | Default |
| :--- | :--- | :--- |
| `TOOL_ID` | String used for MQTT topics and HA entity naming (e.g., `lathe_01`). | `unknown` |
| `NUM_ESTOPS` | Number of physical E-stop inputs to monitor (0, 1, or 2). | `1` |
| `HAS_NFC` | Set to `1` to enable PN532 RFID features, `0` to disable. | `1` |
| `ENABLE_DELAY` | Duration (ms) the "Start" window stays open after authorization. | `120000` (2m) |
| `MAX_OFFLINE_MS` | Max time allowed without MQTT contact before a self-reboot. | `300000` (5m) |
| `SCAN_COOLDOWN` | Milliseconds before the same RFID card can trigger again. | `2500` |

## Deployment & OTA Updates

### Remote Update (OTA)
The firmware uses its `TOOL_ID` as its network hostname for easy wireless updates.
```bash
# Example: Updating the Bridgeport Mill wirelessly
pio run -e bridgeport_mill -t upload --upload-port bridgeport_mill.local
```

## Home Assistant Integration
This firmware uses **MQTT Discovery**. When a tool powers on, it automatically creates entities in Home Assistant under the name provided in `TOOL_ID`.

### Entity Mapping
| Feature | Entity Type | MQTT Topic |
| :--- | :--- | :--- |
| **Enable Pulse** | `switch` | `cmnd/TOOL_ID/set_relay` |
| **Timeout Pulse** | `button` | `cmnd/TOOL_ID/reset_relay` |
| **RFID Card** | `sensor` | `stat/TOOL_ID/rfid` |
| **Tool Current** | `binary_sensor` | `stat/TOOL_ID/bincur` |
| **Relay Feedback** | `binary_sensor` | `stat/TOOL_ID/relay` |

### Adapting Legacy ESPHome Automations
Because this firmware is no longer a native ESPHome node, you must update your YAML automations to use standard Service calls.

**1. Authorization (RFID Scan)**
Change your trigger to watch the new MQTT sensor:
```yaml
trigger:
  - platform: state
    entity_id: sensor.compoundmitersaw_rfid
```

**2. Enabling the Tool (Success Response)**
Replace the legacy `esphome.xxxx_enable` action with a switch toggle:
```yaml
action:
  - service: switch.turn_on
    target:
      entity_id: switch.compoundmitersaw_set_relay
```

**3. Tool Timeout (Abandoned Tool)**
Replace the legacy `esphome.xxxx_latch_reset` action with the force-reset button:
```yaml
action:
  - service: button.press
    target:
      entity_id: button.compoundmitersaw_reset_relay
```