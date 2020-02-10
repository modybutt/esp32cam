# Lagermanagement: Station 

Creates a http server and listen to `GET` requests at `http://[board-ip]/jpg` as well as for `POST` forms `http://[board-ip]/start_led`. When the request is triggered, it returns a UXGA JPEG image from the camera. Additionally, a handshake message is send via multicast address to enable linking with the ControllerStation.

## Instructions

In order to run this example, you need the `esp-idf` repository with the `esp32-camera` as additional component library.

> git clone https://github.com/espressif/esp-idf --recursive esp-idf

> git clone https://github.com/espressif/esp32-camera --recursive esp32-camera

Then you need to configure your WiFi SSID and Password via `make menuconfig` or directly in [Kconfig.projbuild](./main/Kconfig.projbuild) file. Alternativly when working with IDE (Eclipse) you can configure the `sdkconfig` in the root directory.

All camera pins are configured by default accordingly to [this A.I. Thinker document](../../assets/ESP32-CAM_Product_Specification.pdf) and you can check then inside [Kconfig.projbuild](./main/Kconfig.projbuild).

## Notes

Make sure to read [sdkconfig.defaults](./sdkconfig.defaults) file to get a grasp of required configurations to enable `PSRAM` and set it to `64MBit`.

Multicast can be enabled and the device id used in the system via the corresponding `mulcast.h` in the projects `driver` directory.

## Demo

By default, the resolution is `UXGA` and bellow is a real photo taken by the module using this example.
