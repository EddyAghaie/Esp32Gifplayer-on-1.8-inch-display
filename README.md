# Esp32Gifplayer-on-1.8-inch-display
⚠️ Please read the README before using this project.

A high-speed ESP32 GIF player with Wi-Fi upload, optimized playback, 1.8" ST7735 TFT support, and LittleFS storage.
# ESP32 High-Speed GIF Player


This project is designed as one of the **fastest and simplest ways to play GIF animations on a standard ESP32 over Wi-Fi**, without requiring an SD card or an internet connection.

GIFs are uploaded from a smartphone over Wi-Fi, processed directly in the browser, converted into an optimized custom binary format, and stored in **LittleFS**. The ESP32 then reads the optimized frames directly from flash memory and plays them continuously on the TFT display.

---

# Features

* ⚡ High-speed GIF playback
* 📺 1.8-inch ST7735 TFT display
* 🖥️ 160×128 landscape resolution
* 📡 Built-in ESP32 Wi-Fi Access Point
* 📱 Upload GIFs directly from a smartphone
* 💾 Store animations in LittleFS
* 🚫 No SD card required
* 🌐 No internet connection required
* 🚀 Optimized SPI display communication
* 🧩 Custom lightweight `GFA1` animation format
* 🔄 Continuous GIF playback
* 📷 Automatic QR code generation
* 📱 Mobile-friendly upload interface
* 📲 PWA support / Add to Home Screen
* 🔄 Factory reset using 5 power cycles
* 💻 GIF decoding is performed in the browser using pure JavaScript
* 📦 Maximum 30 output frames per GIF

---

# Hardware

## Required Hardware

* Standard ESP32 development board
* 1.8-inch ST7735 SPI TFT display
* USB cable / suitable power supply
* Jumper wires

The project is designed around a standard ESP32 and does not require an ESP32-S3, ESP32-C3, or other specialized variant.

---

# TFT Pinout

The following pin configuration is used by the firmware:

| ST7735 Pin | ESP32 GPIO | Function       |
| ---------- | ---------: | -------------- |
| VCC        |       3.3V | Power          |
| GND        |        GND | Ground         |
| SCK / SCL  |    GPIO 18 | SPI Clock      |
| SDA / MOSI |    GPIO 23 | SPI MOSI       |
| CS         |     GPIO 5 | Chip Select    |
| DC / A0    |    GPIO 21 | Data / Command |
| RST / RES  |     GPIO 2 | Display Reset  |
| LED / BL   |     GPIO 4 | Backlight      |

### Wiring

```text
ESP32                  ST7735 1.8"
----------------------------------
3.3V       ----------> VCC
GND        ----------> GND
GPIO18     ----------> SCK / SCL
GPIO23     ----------> SDA / MOSI
GPIO5      ----------> CS
GPIO21     ----------> DC / A0
GPIO2      ----------> RST / RES
GPIO4      ----------> LED / BL
```

> **Important:** The display logic is configured for 3.3V. Do not apply 5V directly to 3.3V-only display pins.

---

# Display Configuration

The firmware is configured for:

```text
Display:    ST7735
Resolution: 160 × 128
Orientation: Landscape
```

The default rotation is:

```cpp
tft.setRotation(1);
```

If the image is upside down, try:

```cpp
tft.setRotation(3);
```

---

# SPI Configuration

The default ESP32 SPI pins used by the project are:

```text
MOSI = GPIO23
SCK  = GPIO18
```

Display control:

```text
CS  = GPIO5
DC  = GPIO21
RST = GPIO2
BL  = GPIO4
```

---

# Arduino IDE Configuration

Before uploading the firmware, configure Arduino IDE correctly.

## Board

Select:

```text
Tools
→ Board
→ ESP32 Arduino
→ ESP32 Dev Module
```

## Recommended Settings

```text
Board:             ESP32 Dev Module
CPU Frequency:     240MHz (WiFi/BT)
Flash Frequency:   80MHz
Flash Mode:        QIO
Upload Speed:      921600
Partition Scheme:  2MB APP / 2MB Filesystem
```

> The exact names of some options may differ depending on your ESP32 Arduino Core version.

---

# Partition Scheme

The recommended partition layout is:

```text
+-----------------------------+
|       APP / Firmware        |
|           2 MB              |
+-----------------------------+
|     LittleFS / Filesystem   |
|           2 MB              |
+-----------------------------+
```

Select a partition scheme that provides approximately:

```text
2 MB APP
2 MB Filesystem
```

Depending on the ESP32 Arduino Core version, the filesystem may be displayed as:

```text
SPIFFS
```

or:

```text
Filesystem
```

Even if the menu says **SPIFFS**, this project uses the **LittleFS API** in the firmware.

The filesystem partition is used to store:

```text
/current.anim
```

and optionally:

```text
/icon.png
```

---

# Required Libraries

Install the following libraries before compiling.

## Adafruit

```text
Adafruit GFX Library
Adafruit ST7735 and ST7789 Library
Adafruit BusIO
```

## ESPAsyncWebServer

Install:

```text
ESPAsyncWebServer
AsyncTCP
```

Use the **ESP32Async** versions compatible with the ESP32 Arduino Core.

## Included with ESP32 Core

These are normally already included:

```text
SPI
WiFi
FS
LittleFS
Preferences
```

## QR Code

The project requires:

```text
QRCodeGen.h
QRCodeGen.c
```

Place both files next to the `.ino` file.

The QR implementation is based on the `ricmoo/QRCode` project with modifications required by this project.

---

# Project Structure

Your project folder should look like:

```text
GifFrame/
│
├── GifFrame.ino
├── QRCodeGen.h
└── QRCodeGen.c
```

---

# How the Project Works

The complete process is:

```text
ESP32
  │
  ├── Creates Wi-Fi Access Point
  │
  ▼
QR Code 1
  │
  ▼
Phone connects to ESP32 Wi-Fi
  │
  ▼
QR Code 2
  │
  ▼
http://192.168.4.1/
  │
  ▼
Upload GIF / Image
  │
  ▼
Browser processes the file
  │
  ├── Decode GIF
  ├── Resize
  ├── Crop / Cover
  ├── Convert RGB888 → RGB565
  └── Build GFA1
  │
  ▼
Upload GFA1 to ESP32
  │
  ▼
LittleFS
  │
  ▼
/current.anim
  │
  ▼
ESP32 reads frames
  │
  ▼
ST7735
  │
  ▼
Continuous playback
```

---

# First Boot

When the ESP32 does not have an animation stored in LittleFS, it starts an open Wi-Fi Access Point.

The SSID is automatically generated from the ESP32 MAC address.

Example:

```text
TABLOA1B2
```

The display shows a QR code containing the Wi-Fi configuration.

Scan this QR code with your smartphone to connect to the ESP32.

The Wi-Fi network is open and does not require a password.

---

# Opening the Upload Page

After a smartphone connects to the ESP32 Access Point, the display automatically changes to a second QR code.

This QR code contains:

```text
http://192.168.4.1/
```

Scan it to open the upload page.

You can also manually enter:

```text
http://192.168.4.1
```

into the browser.

No internet connection is required.

---

# Uploading GIFs

The web interface allows you to select an image or GIF from your phone.

Supported image types depend on the browser, including common formats such as:

```text
JPG
PNG
WEBP
BMP
GIF
```

The original GIF is **not** stored on the ESP32.

Instead, the browser processes the GIF before uploading it.

---

# Browser-Side GIF Processing

The ESP32 does not need the `AnimatedGIF` library.

GIF decoding is performed using pure JavaScript running inside the smartphone browser.

The browser performs:

```text
GIF decoding
     ↓
Frame extraction
     ↓
Frame composition
     ↓
Resize / Cover
     ↓
160×128 output
     ↓
RGB888 → RGB565
     ↓
GFA1 generation
     ↓
Wi-Fi upload
```

This significantly reduces the processing required by the ESP32 during playback.

---

# Image Resolution

The target resolution is:

```text
160 × 128
```

Images are automatically resized using a cover-style algorithm.

The goal is to fill the display while avoiding unwanted empty areas.

For animated GIFs, the browser limits the output to:

```text
MAX_FRAMES = 30
```

If the original GIF contains more than 30 frames, frames are sampled to reduce the final file size.

---

# LittleFS Storage

The processed animation is stored as:

```text
/current.anim
```

The ESP32 reads the animation directly from LittleFS.

The animation remains stored after power is removed.

When the ESP32 boots again, it automatically detects:

```text
/current.anim
```

and starts playing it.

---

# GFA1 Format

The project uses a custom binary format called:

```text
GFA1
```

Instead of decoding GIF files on the ESP32, the GIF is converted into raw RGB565 frames.

## File Header

```text
Byte 0-3    : "GFA1"
Byte 4-5    : Width
Byte 6-7    : Height
Byte 8-9    : Frame Count
```

Each frame contains:

```text
2 bytes              Frame delay
width × height × 2   RGB565 pixel data
```

All header integers use little-endian format.

Pixel data is stored in the byte order required by the ST7735 transfer.

---

# Frame Size

Each 160×128 RGB565 frame requires:

```text
160 × 128 × 2
```

which equals:

```text
40,960 bytes
```

approximately:

```text
40 KB per frame
```

Therefore, a 30-frame animation can require roughly:

```text
1.2 MB
```

plus headers and frame delays.

This is why the recommended filesystem partition is:

```text
2 MB
```

---

# Playback

After a successful upload, the ESP32 opens:

```text
/current.anim
```

and starts playing it.

The animation loops continuously:

```text
Frame 1
   ↓
Frame 2
   ↓
Frame 3
   ↓
...
   ↓
Last Frame
   ↓
Frame 1
   ↓
Repeat
```

The display is updated using SPI and the frames are read directly from LittleFS.

---

# Factory Reset

A factory reset is available without a physical reset button.

Power-cycle the ESP32:

```text
ON → OFF
ON → OFF
ON → OFF
ON → OFF
ON → OFF
```

**5 boots within 10 seconds** triggers the factory reset.

The firmware then:

1. Formats LittleFS
2. Deletes the stored animation
3. Resets the boot counter
4. Displays `Factory Reset...`
5. Restarts the ESP32
6. Returns to the initial Wi-Fi QR screen

After this, the device behaves like a new device.

---

# Web Interface

The complete web interface is embedded directly inside the ESP32 firmware.

It does not require:

* Internet
* External web hosting
* CDN
* External JavaScript libraries
* Cloud services

The HTML, CSS and JavaScript are stored in the firmware.

The interface also includes PWA support, allowing the user to add the upload page to the smartphone Home Screen.

---

# PWA

The project includes:

```text
/manifest.json
```

and supports:

```text
Add to Home Screen
```

The upload page can therefore behave more like a small local application.

The project also supports an optional:

```text
/icon.png
```

stored in LittleFS.

---

# ST7735 Offset Configuration

Some 1.8-inch ST7735 displays use different internal GRAM offsets.

The firmware includes:

```cpp
#define COL_START 0
#define ROW_START 0

#define X_OFFSET 0
#define Y_OFFSET 0
```

The default values should normally remain:

```text
COL_START = 0
ROW_START = 0
X_OFFSET  = 0
Y_OFFSET  = 0
```

If your particular display has an incorrectly positioned image, these values can be adjusted.

Do not change them unless necessary.

---

# ST7735 Initialization

The current configuration uses:

```cpp
tft.initR(INITR_BLACKTAB);
```

Some ST7735 modules may require:

```cpp
INITR_GREENTAB
```

or:

```cpp
INITR_144GREENTAB
```

If the display is not initialized correctly, try the appropriate initialization mode for your specific ST7735 module.

---

# Serial Monitor

Open the Arduino Serial Monitor at:

```text
115200 baud
```

During startup, you should see information similar to:

```text
LittleFS: XXXX / XXXX bytes used
AP started: TABLOXXXX
IP: 192.168.4.1
```

After uploading an animation, you should see information about the LittleFS usage.

---

# Important Notes

### 1. Read the README first

This project has specific wiring, partition and library requirements.

**Read the entire README before uploading or modifying the firmware.**

### 2. Use the correct partition

Recommended:

```text
2 MB APP
2 MB LittleFS / Filesystem
```

### 3. Use 3.3V for the display

The project is designed around a 3.3V ESP32 and 3.3V display logic.

### 4. GIF size matters

Large GIFs can produce large `GFA1` files.

If the filesystem becomes full, upload a smaller GIF or one with fewer frames.

### 5. Wi-Fi is open

The ESP32 Access Point intentionally uses no password.

Do not use this configuration for sensitive networks.

---

# Limitations

```text
Display resolution:     160 × 128
Maximum output frames:  30
Storage:                LittleFS
Display interface:      SPI
Wi-Fi:                  ESP32 Access Point
External SD card:       Not required
Internet:                Not required
```

The actual maximum animation size depends on the filesystem partition and available LittleFS space.

---

# Technologies

This project uses:

* ESP32
* Arduino Framework
* C++
* SPI
* ST7735
* Adafruit GFX
* ESPAsyncWebServer
* AsyncTCP
* Wi-Fi AP
* LittleFS
* Preferences / NVS
* HTML
* CSS
* JavaScript
* QR Code generation
* Custom GFA1 binary format
* RGB565

---

# Why GFA1?

The main reason for creating the `GFA1` format is performance.

Instead of asking the ESP32 to:

```text
Read GIF
   ↓
Decode GIF
   ↓
Process frames
   ↓
Resize
   ↓
Convert colors
   ↓
Display
```

the phone performs the expensive processing first:

```text
Phone
  ↓
Decode
  ↓
Resize
  ↓
Convert
  ↓
GFA1
  ↓
ESP32
  ↓
Read frame
  ↓
Display
```

The ESP32 therefore has a much simpler job during playback: read RGB565 frame data from LittleFS and send it to the ST7735 over SPI.

This approach is specifically designed to achieve very fast and lightweight GIF playback on a standard ESP32.

---

# Quick Start

```text
1. Read this README completely
          ↓
2. Install Arduino IDE
          ↓
3. Install ESP32 Arduino Core
          ↓
4. Select ESP32 Dev Module
          ↓
5. Select 2MB APP + 2MB Filesystem
          ↓
6. Install required libraries
          ↓
7. Add QRCodeGen.h and QRCodeGen.c
          ↓
8. Connect the ST7735 using the pinout above
          ↓
9. Upload the firmware
          ↓
10. Open Serial Monitor at 115200
          ↓
11. Connect your phone to the TABLOXXXX Wi-Fi
          ↓
12. Scan the second QR code
          ↓
13. Select an image or GIF
          ↓
14. Wait for browser processing
          ↓
15. Upload the generated GFA1 file
          ↓
16. ESP32 stores it in LittleFS
          ↓
17. GIF starts playing automatically
```

---

# Project Goal

The goal of **GifFrame** is to provide a fast, simple and standalone way to turn an ESP32 and a small ST7735 TFT display into a Wi-Fi controlled GIF frame.

The project moves GIF processing to the smartphone and keeps the ESP32 playback system lightweight and optimized for speed.

**ESP32 + ST7735 + Wi-Fi + LittleFS = Fast standalone GIF Frame**
