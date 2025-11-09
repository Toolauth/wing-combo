import time
import board
import digitalio
import busio
import pwmio
import wifi
import socketpool
import os
import adafruit_rtttl
import adafruit_ade7953
import adafruit_pn532.i2c
from adafruit_httpserver import Server, Request, Response, GET

print("Booting CircuitPython network-enabled test...")

# --- Pin Definitions (from YAML) ---
pin_sda = board.GP3
pin_scl = board.GP4
# pin_irq = board.GP11  # Not used by the ADE library
pin_red_led = board.GP5
pin_green_led = board.GP6
pin_reset_relay = board.GP17
pin_set_relay = board.GP18
pin_buzzer = board.GP16
pin_bypass_key = board.GP8
pin_estop_1 = board.GP13
pin_estop_2 = board.GP12
pin_bin_cur = board.GP15
pin_relay_feedback = board.GP9

# --- Connect to WiFi (from YAML) ---
try:
    print(f"Connecting to WiFi: {os.getenv('CIRCUITPY_WIFI_SSID')}")
    wifi.radio.connect(os.getenv("CIRCUITPY_WIFI_SSID"), os.getenv("CIRCUITPY_WIFI_PASSWORD"))
    print(f"IP Address: {wifi.radio.ipv4_address}")
    pool = socketpool.SocketPool(wifi.radio)
    server = Server(pool, debug=True)
except Exception as e:
    print(f"Failed to connect to WiFi or start server: {e}")
    # In a real app, you might start the AP fallback here.
    # For this test, we'll stop if WiFi fails.
    while True: pass 

# --- Initialize I2C and Peripherals ---
i2c = busio.I2C(pin_scl, pin_sda)

try:
    ade = adafruit_ade7953.ADE7953_I2C(i2c)
    print("ADE7953 Initialized.")
except Exception as e:
    print(f"Failed to init ADE7953: {e}")
    ade = None

try:
    pn532 = adafruit_pn532.i2c.PN532_I2C(i2c, debug=False)
    fw_version = pn532.firmware_version
    print(f"Found PN532! Firmware ver. {fw_version[0]}.{fw_version[1]}")
except Exception as e:
    print(f"Failed to init PN532: {e}")
    pn532 = None
    
# --- Initialize Buzzer ---
buzzer = pwmio.PWMOut(pin_buzzer, frequency=440, duty_cycle=0, variable_frequency=True)
mario_song = "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6"

# --- Initialize Outputs ---
outputs = {
    "red": digitalio.DigitalInOut(pin_red_led),
    "green": digitalio.DigitalInOut(pin_green_led),
    "set": digitalio.DigitalInOut(pin_set_relay),
    "reset": digitalio.DigitalInOut(pin_reset_relay)
}
for out in outputs.values():
    out.direction = digitalio.Direction.OUTPUT

# --- Initialize Inputs (Matching YAML) ---
inputs = {
    "bypass_key": digitalio.DigitalInOut(pin_bypass_key),
    "estop_1": digitalio.DigitalInOut(pin_estop_1),
    "estop_2": digitalio.DigitalInOut(pin_estop_2),
    "bin_cur": digitalio.DigitalInOut(pin_bin_cur),
    "relay_feedback": digitalio.DigitalInOut(pin_relay_feedback)
}
for name, in_pin in inputs.items():
    in_pin.direction = digitalio.Direction.INPUT
    if name == "relay_feedback":
        in_pin.pull = digitalio.Pull.UP # Set pullup per YAML

# --- Web Server Routes ---

@server.route("/", GET)
def handle_root(request: Request):
    html = """<html><body><h1>Wing Combo IO Test (CircuitPython)</h1>
    <p><a href='/sensors'>Get Sensor JSON</a></p>
    <h2>Outputs</h2>
    <p>LEDs: <a href='/output/red/on'>Red ON</a> | <a href='/output/red/off'>Red OFF</a> | <a href='/output/green/on'>Green ON</a> | <a href='/output/green/off'>Green OFF</a></p>
    <p>Relays: <a href='/output/set/on'>Set ON</a> | <a href='/output/set/off'>Set OFF</a> | <a href='/output/reset/on'>Reset ON</a> | <a href='/output/reset/off'>Reset OFF</a></p>
    <h2>Actions</h2>
    <p><a href='/nfc'>Scan NFC Tag (blocking)</a></p>
    <p><a href='/playmario'>Play Mario Song (blocking)</a></p>
    </body></html>"""
    return Response(request, html, content_type="text/html")

@server.route("/sensors", GET)
def handle_sensors(request: Request):
    data = {}
    
    # Read binary sensors (All Active-LOW per YAML)
    for name, pin in inputs.items():
        data[name] = not pin.value # "not" provides the inverted logic
        
    # Read ADE7953
    if ade:
        try:
            data["ade7953"] = {
                "voltage": ade.voltage_rms,
                "current_a": ade.current_a,
                "current_b": ade.current_b,
                "active_power_a": ade.active_power_a,
                "active_power_b": ade.active_power_b
            }
        except Exception as e:
            data["ade7953"] = f"Error reading: {e}"
    else:
        data["ade7953"] = "Not initialized"
        
    return Response(request, str(data), content_type="application/json")

@server.route("/nfc", GET)
def handle_nfc(request: Request):
    if not pn532:
        return Response(request, "NFC Reader not initialized", status=500)
    
    msg = "NFC Scan: "
    try:
        uid = pn532.read_passive_target(timeout=0.1)
        if uid:
            msg += f"Tag found! UID: {[hex(i) for i in uid]}"
        else:
            msg += "No tag found."
    except Exception as e:
        msg += f"Error: {e}"
    return Response(request, msg, content_type="text/plain")

@server.route("/playmario", GET)
def handle_play_mario(request: Request):
    print("Playing Mario... (This will block other requests)")
    adafruit_rtttl.play(buzzer, mario_song)
    print("Done playing.")
    return Response(request, "Played Mario song.", content_type="text/plain")

@server.route("/output/<target>/<state>", GET)
def handle_output(request: Request, target: str, state: str):
    if target in outputs:
        outputs[target].value = (state == "on")
        # Redirect back to the root page
        return Response(request, "", headers={"Location": "/"})
    else:
        return Response(request, "Not Found", status=404)

# --- Start Server and Main Loop ---
print("Starting web server...")
server.start(str(wifi.radio.ipv4_address))

while True:
    try:
        # This single call handles all incoming requests
        server.poll()
    except Exception as e:
        print(f"Error in server loop: {e}")