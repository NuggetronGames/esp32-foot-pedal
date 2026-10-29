# ESP32 Foot Pedal (up to 2 pedals)

A Bluetooth (BLE) foot pedal keyboard built on an ESP32. Each pedal presses a configurable key or key combo on whatever device it's paired with. Keybinds and presets are changed from a small web page served by the pedal itself, and firmware can be updated over Wi-Fi (OTA).

The same sketch supports **one or two pedals** and runs in either of two builds: **USB powered** or **battery powered**.

**Setup instructions (after you've built it and powered it on): https://foot-pedal.vercel.app/**

## Choose your build

| | USB powered | Battery powered |
| --- | --- | --- |
| Power | USB cable into the ESP32 | 18650 cell, charger board and 3.3 V converter |
| Wiring | Switches only | Switches plus power circuit |
| Enclosure | MakerWorld link coming soon | MakerWorld link coming soon |
| Good for | Desk setups, simplest build | Wireless use anywhere |

Both builds use the same firmware and the same switch wiring. Only the power side differs.

## Features

- One or two pedals, each with its own active keybind
- BLE keyboard output (no Wi-Fi needed for the pedals to work)
- Modifier combos, e.g. `CTRL+ALT+SHIFT+F12`, `CTRL+SHIFT+t`, `ALT+SPACE`
- Case-sensitive single characters (`t` sends lowercase, `T` sends uppercase)
- Five saved presets per pedal, switchable from the web UI
- First-boot Wi-Fi setup through a captive portal. No credentials are compiled into the firmware
- Settings persist across reboots (ESP32 NVS)
- OTA support in the firmware (needs a partition scheme with an OTA slot, see below)
- Handles both pedals held at once without stuck modifiers

### One pedal or two

This is an "up to 2 pedals" sketch. The web interface always shows keybind and preset settings for both pedals, even if you only built one. For a single-pedal build, wire the switch to Pedal 1 (GPIO14) and ignore the Pedal 2 settings. Leaving GPIO25 unconnected is fine, because the internal pull-up keeps it idle.

## Firmware

Use `esp32_foot_pedal_wifi_setup_guided.ino`, attached to each [release](../../releases). It is the same file for both builds.

The other `.ino` files in this repo are earlier iterations and are kept for reference.

**Arduino IDE settings**

| Setting | Value |
| --- | --- |
| Board | ESP32 Dev Module |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| ESP32 board package | esp32 by Espressif Systems (3.x, built with 3.3.12) |
| Library: ESP32 BLE Keyboard | by T-vK, installed from a ZIP (see below) |
| Library: WiFiManager | by tzapu, installed from the Library Manager |

### Installing the libraries

Everything else the sketch includes (`WiFi`, `WebServer`, `ArduinoOTA`, `Preferences`, `BLE` and so on) ships with the ESP32 board package. Don't install those separately.

**1. ESP32 board package** (skip if `ESP32 Dev Module` is already in your board list)

1. Open **File > Preferences** and add this to "Additional boards manager URLs":
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Open **Tools > Board > Boards Manager**, search for `esp32` and install **esp32 by Espressif Systems**.

**2. WiFiManager**

1. Open the Library Manager (`Ctrl+Shift+I`).
2. Search for `WiFiManager` and install the one **by tzapu**. Choose **Install All** if it asks about dependencies.

**3. ESP32 BLE Keyboard (T-vK)**

This library isn't reliably available in the Library Manager, and similarly named libraries from other authors don't provide `BleKeyboard.h`, so install the original from GitHub.

1. Go to https://github.com/T-vK/ESP32-BLE-Keyboard and choose **Code > Download ZIP**.
2. In the Arduino IDE, choose **Sketch > Include Library > Add .ZIP Library...** and select the ZIP.
3. Restart the IDE.

### Patching BleKeyboard.cpp (required on ESP32 core 3.x)

Version 3.x of the ESP32 board package changed the BLE library to take Arduino `String` instead of `std::string`. T-vK's library still passes `std::string`, so it fails to compile with errors like:

```
BleKeyboard.cpp:106:19: error: cannot convert 'std::string' to 'String'
BleKeyboard.cpp:117:32: error: no matching function for call to 'BLECharacteristic::setValue(std::string&)'
```

Two lines need to be changed. Open this file in any text editor:

```
C:\Users\<you>\Documents\Arduino\libraries\ESP32_BLE_Keyboard\BleKeyboard.cpp
```

Around line 106, change:

```cpp
BLEDevice::init(deviceName);
```

to:

```cpp
BLEDevice::init(String(deviceName.c_str()));
```

Around line 117, change:

```cpp
hid->manufacturer()->setValue(deviceManufacturer);
```

to:

```cpp
hid->manufacturer()->setValue(String(deviceManufacturer.c_str()));
```

Save the file and compile again. The patch lives in your local library folder, so reinstalling or updating the library overwrites it and you'll need to reapply it.

If you'd rather not patch, downgrading the board package to 2.0.17 lets the stock library compile, but the sketch's other libraries target the 3.x core, so patching is the simpler path.

**First compile is slow.** On a fresh install the first build can take several minutes while the IDE compiles the ESP32 core. Later builds reuse the cache and take seconds.

**The sketch must be flashed with the Huge APP partition scheme** (Tools > Partition Scheme in the Arduino IDE). BLE, Wi-Fi, the web server and WiFiManager together are too large for the default partition, so with the default scheme the upload fails with a "sketch too big" error.

Huge APP has no spare partition for over-the-air updates, so flash over USB. See the OTA note under Usage notes.

The Arduino IDE expects a sketch to live in a folder with the same name. Put the file in a folder called `esp32_foot_pedal_wifi_setup_guided/` before opening it.

## Wiring the pedals (both builds)

Each pedal switch connects one GPIO pin to GND. The firmware enables the ESP32's internal pull-up on both pins, so **no external resistors are needed**. A pressed switch pulls the pin LOW.

| Pedal | ESP32 pin | Other switch terminal |
| --- | --- | --- |
| Pedal 1 | GPIO14 | GND |
| Pedal 2 (optional) | GPIO25 | GND |

If you want different pins, change `PEDAL_1_PIN` and `PEDAL_2_PIN` near the top of the sketch. The classic ESP32-WROOM has no GPIO24, so pick another valid GPIO.

## Build 1: USB powered

### Bill of materials

| Qty | Part | Notes |
| --- | --- | --- |
| 1 | ESP32 Dev Module (classic ESP32-WROOM) | GPIO14 and GPIO25 are used for the pedals |
| 1-2 | Mechanical keyboard switch | One per pedal |
| 1 | USB cable | Powers the ESP32 and is used for flashing |
| 1 | 3D-printed pedal enclosure | MakerWorld link coming soon |
| - | Hookup wire, solder | |

### Wiring

```mermaid
flowchart LR
    USB["USB power"] --> ESP["ESP32 Dev Module"]
    SW1["Pedal 1 switch"] -->|"GPIO14"| ESP
    SW1 -->|"GND"| ESP
    SW2["Pedal 2 switch (optional)"] -->|"GPIO25"| ESP
    SW2 -->|"GND"| ESP
```

## Build 2: Battery powered

### Bill of materials

| Qty | Part | Notes |
| --- | --- | --- |
| 1 | ESP32 Dev Module (classic ESP32-WROOM) | GPIO14 and GPIO25 are used for the pedals |
| 1-2 | Mechanical keyboard switch | One per pedal |
| 1 | 18650 Li-ion cell | Power source |
| 1 | 18650 charging board | Charges the cell over USB |
| 1 | XL63020-3.3 buck/boost converter module | Steps the battery voltage to a steady 3.3 V for the ESP32 |
| 1 | 3D-printed pedal enclosure (top and bottom) | Cavities for the battery, charger and converter. MakerWorld link coming soon |
| - | Hookup wire, solder | |

> Check the part list against your own build before relying on it, especially the charging board model and where the converter output connects to the ESP32.

### Wiring

```mermaid
flowchart LR
    USB["USB charge input"] --> CHG["18650 charging board"]
    CELL["18650 cell"] <--> CHG
    CHG --> BUCK["XL63020-3.3 buck/boost converter"]
    BUCK -->|"3.3 V"| ESP["ESP32 Dev Module"]
    BUCK -->|"GND"| ESP
    SW1["Pedal 1 switch"] -->|"GPIO14"| ESP
    SW1 -->|"GND"| ESP
    SW2["Pedal 2 switch (optional)"] -->|"GPIO25"| ESP
    SW2 -->|"GND"| ESP
```

## First-time setup

1. Flash the firmware to the ESP32.
2. Power it on, then pair **ESP32 Foot Pedal** from your computer's or phone's Bluetooth settings. The pedals work as soon as it connects.
3. To use the web interface, join the **ESP32-Pedal-Setup** Wi-Fi network from a phone or laptop and enter your home Wi-Fi details in the portal that opens. The portal stays open for 3 minutes.
4. Follow the guide at https://foot-pedal.vercel.app/ for the rest.

## Usage notes

**Defaults**

- Pedal 1 sends `t`
- Pedal 2 sends `CTRL+ALT+SHIFT+F12`

**Changing keybinds.** Once the pedal is on your Wi-Fi, open its IP address in a browser. The IP is shown in the Serial Monitor at 115200 baud. Set the active keybind for each pedal there, or save up to five named presets per pedal and activate them with one click.

**Keybind syntax.** Join keys with `+`.

- Modifiers: `CTRL`, `SHIFT`, `ALT`, `WIN` (also `CONTROL`, `WINDOWS`, `GUI`)
- Special keys: `SPACE`, `ENTER`, `TAB`, `ESC`, `BACKSPACE`, `DELETE`, `INSERT`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`
- Function keys: `F1` to `F12`
- Any single printable character, case preserved

**Resetting Wi-Fi.** Hold both pedals while powering on and keep holding for about 5 seconds. The saved credentials are erased and the setup portal opens again. Releasing early keeps them. This reset needs both pedals, so a single-pedal build can't use it. Reflash the firmware, or temporarily short GPIO25 to GND while powering on.

**Wi-Fi is optional.** If the portal times out or no network is available, Bluetooth still works. The web UI and OTA only start once Wi-Fi is connected. Reboot to try setup again.

**OTA updates.** The sketch includes ArduinoOTA and the device appears as `esp32-pedal` on your network. OTA only works if the board was flashed with a partition scheme that includes an OTA slot. With Huge APP (required for the first flash) there is no OTA slot, so update the firmware over USB.

**Security.** The web interface and OTA have no password. Only use the pedal on a network you trust.

## Roadmap

- MakerWorld links for the USB and battery enclosures
- Integrate into the PrintPlease website

## License

No license has been chosen yet.
