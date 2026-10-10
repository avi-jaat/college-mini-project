# Smart Waste Management & Bin-Level Monitoring

ESP32 + HC-SR04 ultrasonic sensor -> ThingSpeak dashboard + IFTTT email/SMS alert.

## Files
- `smart_bin.ino` - complete Arduino sketch (open this in Arduino IDE)

## Wiring
| HC-SR04 | ESP32 |
|---|---|
| VCC | VIN (5V) |
| GND | GND |
| TRIG | GPIO 5 |
| ECHO | GPIO 18 (through a voltage divider) |

ECHO outputs 5V but ESP32 pins are 3.3V-safe. Use a 1k ohm resistor from ECHO to GPIO 18,
and a 2k ohm resistor from GPIO 18 to GND.

## Setup
1. Arduino IDE -> Boards Manager -> install "esp32 by Espressif". Select "ESP32 Dev Module".
2. ThingSpeak: create a channel with 3 fields (Fill %, Distance cm, Alert flag).
   Copy the Write API Key into `TS_API_KEY`.
3. IFTTT: create an applet, trigger = Webhooks, event name `bin_full`, action = send email.
   Use {{Value1}} for bin ID and {{Value2}} for fill %. Copy your Webhooks key into `IFTTT_KEY`.
4. Calibrate: measure the empty-bin and full-bin sensor distances (cm) with a ruler and set
   `EMPTY_DISTANCE_CM` and `FULL_DISTANCE_CM`.
5. Enter WiFi name/password, upload, and open Serial Monitor at 115200 baud.

## Multiple bins
Flash the same code with a different `BIN_ID` and use a separate ThingSpeak channel per bin.
