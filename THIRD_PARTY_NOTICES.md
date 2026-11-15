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
| [ESP32 BLE Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard) (T-vK) | Bluetooth HID keyboard | No license published (see below); patched copy included |

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

`patched-libraries/ESP32-BLE-Keyboard/BleKeyboard.cpp` is a copy of
`BleKeyboard.cpp` from https://github.com/T-vK/ESP32-BLE-Keyboard with two
lines changed so it compiles on ESP32 Arduino core 3.x (the `std::string`
arguments to `BLEDevice::init` and `setValue` are wrapped in `String(...)`).
All other code in that file is T-vK's and the original authors' work. The rest
of the library is not included here; install it from the upstream repository.

The upstream repository has not published a license, and a 2020 discussion
there suggests GPLv3 may be intended, but no license has been granted that I
can point to. This copy and any firmware built with it are provided on that
basis. If a license is added upstream, its text and copyright notices will be
added here, and anything that requires, such as offering matching source for
firmware binaries, will be followed. If you are a copyright holder and want
this removed, open an issue.
