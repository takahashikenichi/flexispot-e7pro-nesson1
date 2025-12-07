# Flexispot Web Controller (M5Unified / Arduino Nesso N1)

![IMG_1237](https://github.com/user-attachments/assets/2d6b620c-cdfd-4e2e-9b4c-b81906e4d92c)

> [!WARNING]  
> **Important:** Working with electronic devices can be dangerous.  
> Follow this guide at your own risk.

---
[日本語のREADMEはこちら](README_ja.md)

## Overview

This project enables control of a Flexispot standing desk using an M5Unified-compatible device (such as the **Arduino Nesso N1**) that emulates the original desk controller.

Most Flexispot desks—including the **E7 Pro**—use controller boards manufactured by **LoctekMotion**.  
This project communicates with the desk’s control box through its RJ45 port via serial communication, allowing you to operate the desk from your M5Unified device.

Additionally, the desk’s height (7-segment display output) is decoded through the serial interface and displayed on:

- the M5Unified device’s built-in display  
- a web UI accessible on the same local network

This project has been **tested only on the Arduino Nesso N1**.

---

## Features

### Desk Control via Serial Communication
- Full up/down motion control for Flexispot E7 Pro
- Wakeup, Memory, and Preset operations

### Physical Controls on the M5Unified Device
- **Button A:** Wakeup (activates the desk controller)
- **Button B:** Preset 4 (commonly used as sitting height)
- Live height display on device screen

### Web Browser Control & Monitoring
Accessible via `http://<device-ip>/`

- Wakeup (activate controller and read current height)
- Up / Down (moves only while pressed; commands sent every ~108 ms)
- Memory (cmd_mem)
- Presets 1 / 2 / 3 / 4
- Height auto-updates every 1 second
- Displays `Sleeping...` when the desk is idle

---

## Hardware Configuration

### Supported Desk
- **Flexispot E7 Pro**

### Controller Device
- **Arduino Nesso N1** (tested & verified)

### Connection Between Flexispot E7 Pro and Arduino Nesso N1

| RJ45 Pin | Cable Color (T-568B) | Description | Nesso N1 Pin |
|---------|-----------------------|-------------|--------------|
| 1 | White Orange | - | - |
| 2 | Orange | - | - |
| 3 | White Green | - | - |
| 4 | Blue | Set HIGH for 1 second to wake controller | D3 |
| 5 | White Blue | RX (from remote) | D2 |
| 6 | Green | TX (to remote) | D1 |
| 7 | White Brown | GND | GND |
| 8 | Brown | VDD (5V) | VIN |

![wiring](https://github.com/user-attachments/assets/2cc1f20f-4027-4ce3-8875-643718602d34)

---

## Serial Communication Settings

Flexispot / LoctekMotion controller uses:

- **Baud rate:** 9600 bps  
- **Data bits:** 8  
- **Stop bits:** 1  
- **Parity:** none  

Use the following configuration:

```cpp
Serial1.begin(9600, SERIAL_8N1, D2, D1);
```

## Web UI

Once the Wi-Fi SSID and password are configured in the code and the device is powered on,  
the M5Unified device will connect to Wi-Fi and start an HTTP server on port **80**.

The assigned IP address is shown on:

- the Serial Monitor  
- the device's display  

Access the controller from a browser: http://<device-ip>/


### Web UI Functions

- **Wakeup**  
  Activates the controller and enables height monitoring.

- **Height Display**  
  Updated once per second.

- **Up / Down**  
  Moves only while the button is held (press-and-hold behavior).

- **Memory**  
  Press once to enter memory-set mode.  
  Then press a preset button to store the current height.

- **Preset 1–4**  
  Moves desk to the stored height.

---

## Setup Instructions

1. Prepare and modify a standard RJ45 (Ethernet) cable.  
2. Connect the cable to the Flexispot controller box. (Some Flexispot models have **two RJ45 ports**, allowing coexistence with the original remote.)
3. Open `flexispot_e7pro_nesson1.ino` in Arduino IDE.
4. Update Wi-Fi credentials:
```cpp
   const char* ssid = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";
```
5. Build and upload the firmware to the Arduino Nesso N1.
6. Check the assigned IP address (Serial Monitor or device screen):
  - Example:
```
WiFi connected.
IP address: 192.168.x.y
```
  - Device screen:
![IMG_1238](https://github.com/user-attachments/assets/34019079-53df-4635-a90c-db0bb567817c)
7. Open the Web UI:
```
http://192.168.x.y/
```
<img width="631" height="434" alt="image" src="https://github.com/user-attachments/assets/09dcf8de-a8dd-4a82-9209-76c9e7a2f77c" />

## Caution
- This is an unofficial, hobby-grade project, not affiliated with Flexispot or LoctekMotion.
- Be especially careful with wiring. Use a multimeter to verify correct voltage levels before connecting to the desk.
- Ensure no objects or people are under the desk while it is moving. Use entirely at your own risk.

## License

This project is released under the MIT License.
