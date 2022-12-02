# ESP32 WiFi credential setup over BLE

> This repository is based on the ESP32 WiFi BLE project, available on Bitbucket [here](https://bitbucket.org/beegee1962/esp32_wifi_ble_esp32.git) by [Bernd Giesecke](https://bitbucket.org/beegee1962/).

Setup your ESP32 WiFi credentials over BLE from an Android phone or tablet.
Sometimes you do not want to have your WiFi credentials in the source code, specially if it is open source and maybe accessible as a repository on Github.

There are already solution like [WiFiManager-ESP32](https://github.com/zhouhan0126/WIFIMANAGER-ESP32) that give you the possibility to setup the WiFi credentials over a captive portal. But I wanted to test the possibility to setup the ESP32's WiFi over Bluetooth Low Energy.    

## Development platform
PlatformIO, but as the whole code is in a single file it can be easily copied into a .ino file and used with the Arduino IDE

## Used hardware
- [Nodemcu Dev board](https://naylampmechatronics.com/espressif-esp/384-nodemcu-32-30-pin-esp32-wifi.html)		
- Any Android phone or tablet that is capable of BLE.

## SW practices used
- Use of BLE for sending and receiving data
