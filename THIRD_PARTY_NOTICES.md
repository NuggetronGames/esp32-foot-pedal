# Third-party notices

The sketches in this repository are MIT licensed (see `LICENSE`). Compiled
firmware built from them also contains code from the projects below, which
keep their own licenses. This file is a good-faith summary, not legal advice.
Check each project's own license text for the authoritative terms.

| Component | Used for | License |
|---|---|---|
| [arduino-esp32](https://github.com/espressif/arduino-esp32) (Espressif) | Arduino core: `WiFi`, `WebServer`, `ArduinoOTA`, `Preferences`, `HTTPClient`, BLE | LGPL-2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) (Espressif) | Bootloader, FreeRTOS, Wi-Fi and Bluetooth stacks inside the core | Apache-2.0 |
| [WiFiManager](https://github.com/tzapu/WiFiManager) (tzapu) | Wi-Fi setup portal | MIT |
| [ESP32 BLE Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard) (T-vK) | Bluetooth HID keyboard | No license published (see below) |

## LGPL-2.1 (arduino-esp32)

The firmware statically links the Arduino ESP32 core. The complete
corresponding source for the sketch is in this repository, and the core is
available from the link above, so anyone can rebuild and reflash the firmware
with a modified core. The full LGPL-2.1 text is at
https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.

## Apache-2.0 (ESP-IDF)

Licensed under the Apache License, Version 2.0. Text and NOTICE files:
https://github.com/espressif/esp-idf/blob/master/LICENSE

## MIT (WiFiManager)

Copyright (c) 2015 tzapu. Permission is granted under the MIT License, whose
text must accompany copies of the library:
https://github.com/tzapu/WiFiManager/blob/master/LICENSE

## ESP32 BLE Keyboard (T-vK)

This library is not distributed with this repository. Users install it
themselves, and the README describes a small compile-time patch. Because the
upstream repository has not published a license, no permission to
redistribute it has been granted. Published firmware binaries, which include
it, rely on the author's tacit permission until a license is added upstream.
