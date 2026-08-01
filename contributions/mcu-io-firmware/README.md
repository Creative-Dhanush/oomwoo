# mcu-io-firmware — Motor + Sensor Safety Demo

## Why this module first

Out of everything on the oomwoo RFC list, this is the one I picked deliberately, not just because it was "ready to start work."

A robot vacuum spends its entire life moving around a house full of people, pets, furniture, and stairs. Every other feature — mapping, cleaning modes, scheduling, the app — only matters if the robot doesn't crash into things or hurt someone while doing it. The MCU firmware is where that protection actually lives: it's the layer sitting directly between the software's intentions and the motors physically moving. If this layer isn't solid, nothing built on top of it can be trusted.

So before touching navigation, mapping, or any of the higher-level modules, I wanted to prove out the most safety-critical piece first: **can the firmware reliably detect an obstacle and cut motor power in real time, without depending on anything else in the stack?**

This repo is that proof, built small and honest — one motor driver, two motors, one sensor, on a breadboard, not the full robot.

## What it does

- Two DC motors run continuously, simulating the drive wheels
- An ultrasonic sensor continuously watches for obstacles in front of the robot
- The moment something gets within range, the motors are force-stopped — immediately, not on the next loop cycle
- An LED lights up as a visual indicator whenever the safety system has taken over
- A serial interface lets you send commands (`START`, `STOP`, `STATUS`) from a laptop, standing in for where the main robot computer will eventually talk to this board over the "custom serial to CPU" link mentioned in the spec

## Why FreeRTOS

The oomwoo spec calls for FreeRTOS specifically because a vacuum's firmware has to do several things *at once*, reliably: drive the motors, read the sensor, and be ready to override everything the instant there's danger. A normal single-threaded loop can't guarantee that — if one part is busy, the safety check waits too, and "the safety check waited" is exactly the failure mode you can't afford in a moving robot.

So this demo runs three independent FreeRTOS tasks:

- **Motor task** — decides whether to drive or stop, based on shared state
- **Sensor task** — polls the ultrasonic sensor on its own schedule, independent of everything else
- **Safety task** — the highest-priority task in the system. It has one job: watch the sensor data and force an obstacle flag the moment something gets too close. The motor task always checks this flag first, before anything else.

This mirrors what the real firmware will eventually need at a much larger scale — more sensors, more safety conditions, more communication with the main computer — but the core pattern (independent tasks + safety having final say) is already proven here.

## Hardware used

| Component | Role |
|---|---|
| ESP32 DevKit-C (WROOM-32) | Runs the firmware — prototyping on ESP32 first since it's easy to source/flash; plan is to port to STM32G473 / Nucleo-G474RE to match the spec exactly |
| L298N motor driver | Drives the two motors from the ESP32's low-current logic pins |
| 2x TT gear motors | Stand-in for the drive wheels |
| 4xAA battery pack | Separate power supply for the motors, isolated from ESP32's USB power |
| HC-SR04 ultrasonic sensor | Obstacle detection — this is the safety input |
| LED + 220Ω resistor | Visual indicator when safety stop is active |
| 1kΩ + 2kΩ resistors | Voltage divider on the sensor's ECHO line — HC-SR04 outputs 5V, ESP32 GPIO is only rated for 3.3V, so this brings it down safely |

*(Note: ENA/ENB are currently set via jumper caps on the L298N, running the motors at fixed full power. PWM-based variable speed control is a planned next step — it doesn't change the safety architecture, just adds finer motor control on top of it.)*

## Photos

**The full setup:**
![Full build](./images/full-setup.jpg)

**ESP32 wiring:**
![ESP32 wiring](./images/esp32-wiring.jpg)

**L298N + motors:**
![Motor driver](./images/l298n-motors.jpg)

**HC-SR04 sensor:**
![Sensor](./images/sensor.jpg)

## Demo video

[Link to demo video] — shows the motors running, an obstacle being introduced in front of the sensor, the motors cutting off instantly, the LED lighting up, and the motors resuming once the obstacle is cleared. Serial monitor is visible throughout, showing live status.

## Serial commands

Connect at 115200 baud.

| Command | Effect |
|---|---|
| `START` | Resume motor operation (if not blocked by an obstacle) |
| `STOP` | Force motors off, regardless of sensor state |
| `STATUS` | Print current motor state, sensor distance, and safety status |

Status also prints automatically once per second, without needing a command — useful for watching behavior live during testing.

## What's next

- PWM-based variable speed control on ENA/ENB, instead of the current fixed-speed jumper cap setup
- Port to STM32G473 / Nucleo-G474RE to match the spec's target chip
- Define the actual serial protocol contract with whatever will run on the CPU side (ROS2 bridge)
- Expand from one sensor to the fuller safety picture the real robot will need (multiple sensors, bumper input, etc.)

## Discussion

Continuing the conversation from [Discussion #49](https://github.com/makerspet/oomwoo/discussions/49) — feedback on the task structure or serial protocol design is welcome before this goes further.