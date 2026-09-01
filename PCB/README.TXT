# PCB Design

The PCB design for this project is provided in the `PCB` directory.

The PCB should be treated as the hardware implementation of the wiring and pin configuration documented in this repository.

## PCB Documentation

All PCB-related work should be based on:

* The wiring diagram provided with this project
* The hardware documentation in this README
* The files inside the `PCB` directory
* The actual firmware GPIO configuration
* The exact hardware components listed in this repository

Do not change GPIO assignments or invent additional connections without verifying them against the project documentation.

## Main PCB Components

The PCB is designed around the following hardware:

* **Wemos D1 Mini ESP32**
* **1.8-inch ST7735 SPI TFT LCD**
* **650mAh 3.7V Li-ion battery**
* **TP4056 Li-ion charging module with built-in USB connector**
* **ON/OFF slide switch**

## TFT Connections

The TFT uses the following connections:

| TFT Pin    | Wemos D1 Mini ESP32 |
| ---------- | ------------------- |
| VCC        | 3.3V                |
| GND        | GND                 |
| SCK / SCL  | GPIO18              |
| MOSI / SDA | GPIO23              |
| CS         | GPIO5               |
| DC / A0    | GPIO21              |
| RST / RES  | GPIO2               |
| LED / BL   | GPIO4               |

These GPIO assignments must remain unchanged unless the firmware is also modified accordingly.

## Power & Charging

The TP4056 is used as the Li-ion battery charging module.

The battery is connected to the TP4056 battery terminals:

```text
650mAh 3.7V Li-ion Battery
        │
        ├── + → TP4056 B+
        │
        └── − → TP4056 B−
```

Charging is performed through the **USB connector already integrated into the TP4056 module**.

```text
USB-C 5V Charger
        │
        ▼
   TP4056 USB Port
        │
        ▼
  Li-ion Battery
```

There is no separate USB charging module.

There is no separate USB connector added to the PCB.

## ON/OFF Switch

The ON/OFF switch is placed in the positive power path of the device.

The switch controls the power supplied to the ESP32 system.

```text
Battery
   │
   ▼
ON/OFF Switch
   │
   ▼
ESP32 Power
```

## Important PCB Design Rules

When modifying or manufacturing the PCB:

1. Use the exact components documented in this repository.
2. Follow the wiring diagram exactly.
3. Do not change the TFT GPIO assignments.
4. Do not add components that are not part of the documented circuit.
5. Do not add a separate USB charging connector; the TP4056 already contains the USB charging port.
6. Do not connect the battery directly to the TFT signal pins.
7. Do not connect 5V directly to 3.3V-only TFT logic pins.
8. Verify every PCB trace against the corresponding wiring connection before manufacturing.
9. Check polarity of the Li-ion battery and TP4056 connections carefully.
10. Confirm that the ON/OFF switch is installed in the correct power path.

## PCB Files

The latest PCB-related files and design assets are located in:

```text
PCB/
```

Use the files in this directory together with the wiring documentation when reviewing, modifying, or manufacturing the board.

The repository is the primary source of truth for this project. When there is a conflict between an external drawing and the actual project files or firmware, verify the connection against the firmware and project documentation before proceeding.

## Repository

Project repository:

[Esp32Gifplayer-on-1.8-inch-display](https://github.com/EddyAghaie/Esp32Gifplayer-on-1.8-inch-display/tree/main?utm_source=chatgpt.com)
