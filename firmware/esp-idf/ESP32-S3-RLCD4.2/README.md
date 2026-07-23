# InkSight for Waveshare ESP32-S3-RLCD-4.2

This is a pure ESP-IDF 5.5 port for the Waveshare
ESP32-S3-RLCD-4.2. It does not use Arduino or PlatformIO.

The first version includes:

- native ST7305 SPI initialization and 400×300 landscape rendering;
- open provisioning AP and a setup page at `http://192.168.4.1`;
- NVS-backed Wi-Fi, backend URL, device token, and refresh interval;
- InkSight `/api/device/{mac}/token` and `/api/render` integration;
- 1-, 8-, 24-, and 32-bit uncompressed BMP decoding;
- battery measurement, KEY wake/next mode, and timer deep sleep;
- ST7305 low-power scan mode while the ESP32-S3 sleeps.

Audio, RTC, SHTC3, microSD, OTA, and live voice features are not part
of this first hardware-validation version.

## Build and flash

Install and activate ESP-IDF 5.5, then run from the repository root:

```bash
idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 set-target esp32s3
idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 build
idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 -p /dev/cu.usbmodemXXXX flash monitor
```

On first boot, connect to the `InkSight-XXXXXX` Wi-Fi network and open
`http://192.168.4.1`. On a normal boot, holding KEY for about half a
second opens setup. A short KEY press while sleeping wakes the board and
requests the next InkSight mode; keeping it held for about 1.5 seconds
opens setup instead.

Pin mappings are centralized in `main/board_config.h`.
