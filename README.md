# ESP32 IDF BLE Library

This library offers an object oriented wrapper around the ESP32 IDF BLE libraries. It is
primarily designed to allow the sending and receiving of arbitrary data over BLE.

Additionally, the library utilized "modern C++" concepts and guidelines such as smart pointers and
single stage initialization.

## Prerequisites
You must ensure that you have installed:
    - esp32-utilities
    - esp32-abseil-cpp

## Setup
Simply add the repository to your ESP IDF Project's component directory:
```bash
    mkdir -p components && git submodule add <url> components/esp32-ble-redux
```

# License
This library is licensed under MPLV2, if the LICENSE file is unavailable you can obtain a copy at:
http://mozilla.org/MPL/2.0/
