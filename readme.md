# Voltage Protector: ESP32 Simulation

A voltage and overcurrent protection controller built for the Frontier Semiconductor Bangladesh FSMB Recruitment 2026 (Phase 1) assessment. It runs on an ESP32 in the Wokwi simulator. Two potentiometers stand in for the voltage and current sensors. When either reading leaves its safe range, the controller opens a relay to disconnect the simulated load.

This is a simulation only. It has not been built or tested on hardware, and it must not be connected to mains power.

## What it does

- Reads the two simulated inputs every 100 ms with the 12 bit ADC and scales them to 0 to 300 V and 0 to 10 A.
- Default safe limits are 220 to 240 V and 1 to 3 A.
- On a fault it opens the relay, turns on the red LED and logs which fault occurred.
- After readings return inside the limits it waits 3 seconds, then reconnects the load and turns on the green LED.
- The OLED shows voltage, current, relay state and the number of voltage and current fault events.
- Limits can be changed at runtime through the serial monitor.

## Repository layout

```text
voltage-protector/
├── src/                 firmware (sketch.ino)
├── schematics/          circuit screenshot and system block diagram
├── test_evidence/       screenshots of the four test scenarios
├── project_files/       Wokwi export (diagram.json, sketch.ino, libraries.txt, wokwi-project.txt)
├── design_document.pdf  one page project report
└── readme.md
```

## How to run the simulation

1. Open the Wokwi project: https://wokwi.com/projects/477395800857832449
2. Click the green Start button.
3. Turn the top potentiometer to change the voltage and the bottom one to change the current.
4. Watch the OLED, the LEDs and the serial monitor.

To run it from this repo instead, create a new ESP32 project on https://wokwi.com and replace `sketch.ino`, `diagram.json` and `libraries.txt` with the files in `project_files/`.

Required libraries: Adafruit SSD1306 and Adafruit GFX Library.

## Serial commands

Open the serial monitor at 115200 baud and send a command followed by a newline.

| Command | Action |
|---|---|
| `HELP` | List the commands |
| `STATUS` | Print readings, limits, fault state, relay state and fault counts |
| `SET VMIN <value>` | Set the minimum voltage |
| `SET VMAX <value>` | Set the maximum voltage |
| `SET IMIN <value>` | Set the minimum current |
| `SET IMAX <value>` | Set the maximum current |

A value is rejected if it puts a minimum above its maximum or goes past 300 V or 10 A. The firmware also prints a status line every second and logs each fault and recovery. Limits are stored in RAM, so a reset brings back the defaults.

## Pin map

| Function | ESP32 pin |
|---|---|
| Voltage input (potentiometer) | GPIO 34 |
| Current input (potentiometer) | GPIO 35 |
| Relay module | GPIO 23 |
| Green LED (load connected) | GPIO 21 |
| Red LED (fault) | GPIO 22 |
| OLED SDA and SCL | GPIO 19 and GPIO 18 |

The yellow LED sits on the relay's normally closed contact and represents the protected appliance.

## Test evidence

The four scenarios are in `test_evidence/`. Readings are taken from the OLED in each screenshot.

| Scenario | Reading | Relay | Faults (V / I) |
|---|---|---|---|
| Normal | 224.3 V, 2.89 A | Connected | 0 / 0 |
| Voltage fault | 273.3 V, 2.55 A | Disconnected | 2 / 1 |
| Current fault | 224.3 V, 5.63 A | Disconnected | 0 / 1 |
| Combined fault | 223.3 V, 2.70 A | Disconnected | 2 / 2 |

The combined fault screenshot was taken after both faults had been logged, so the readings on screen are already back inside the limits while the relay is still open. The evidence is qualitative.

## Limitations

- The potentiometers are not calibrated sensors, so sensing accuracy is untested.
- Response time and relay behavior under load were not measured.
- The firmware treats any current below 1 A as a fault, so a lightly loaded appliance would be disconnected.
- A real version would need proper voltage and current sensing with isolation, a relay rated for the load, and low voltage bench testing first.
