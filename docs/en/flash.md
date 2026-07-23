# Web Flasher

InkSight provides browser-based firmware flashing for most users.

![Web flasher screenshot](/images/docs/flash-en.png)

The current flasher page shows the step-by-step guide, firmware source selector, and console logs in one screen.

## Video Tutorial

- Flashing walkthrough: [`Bilibili: InkSight Web Flasher demo`](https://www.bilibili.com/video/BV1aWcQzQE3r/?spm_id_from=333.1387.homepage.video_card.click&vd_source=166ea338ef8c38d7904da906b88ef0b7)

## 1. Requirements

- Chrome or Edge (WebSerial support)
- HTTPS site or `localhost`
- USB data cable (not charge-only)

## 2. Steps

1. Open the **Web Flasher** page
2. Connect device and authorize serial port
3. Select firmware version (you can refresh the list)
4. Click flash and wait for completion
5. Observe the serial logs after flashing to confirm normal boot

## 3. Firmware Sources & Proxy

The WebApp fetches firmware release info via:

- `GET /api/firmware/releases`
- `GET /api/firmware/releases/latest`
- `GET /api/firmware/validate-url?url=...`

If `NEXT_PUBLIC_FIRMWARE_API_BASE` is not set, the frontend defaults to using the same-origin API Route proxy to `INKSIGHT_BACKEND_API_BASE`.

## 4. Troubleshooting

### Serial port not found

- Confirm you are using a data cable, not a charge-only cable
- Try a different USB port or reconnect the device
- Check your OS device manager to ensure the serial port exists

### Release list fails to load

- Check backend availability
- Check if the backend hit GitHub API rate limits
- Click "Refresh Versions" on the page to retry

### Flash interrupted / failed

- Keep the USB connection stable
- Switch to manual URL mode and validate the link first
- Reboot the device and try the flashing process again

## 5. Waveshare ESP32-S3-RLCD-4.2 initial firmware

This hardware port is in `ESP32-S3-RLCD4.2/` and uses pure ESP-IDF 5.5,
without Arduino or PlatformIO. Until it is published in a formal Release
and added to the Web Flasher list, install and activate ESP-IDF, then build
and flash from the repository root:

```bash
source "$HOME/esp/esp-idf/export.sh"
idf.py -C ESP32-S3-RLCD4.2 set-target esp32s3
idf.py -C ESP32-S3-RLCD4.2 build
idf.py -C ESP32-S3-RLCD4.2 -p /dev/cu.usbmodemXXXX flash monitor
```

Replace `/dev/cu.usbmodemXXXX` with the actual port. If only one ESP
device is connected, omit `-p` and let the tool detect it. Press `Ctrl+]`
to exit the serial monitor.

On the first boot without saved configuration, the device creates an
`InkSight-XXXXXX` provisioning access point. Connect to it and open
`http://192.168.4.1` to enter Wi-Fi, the InkSight server URL, and the
refresh interval. Holding the onboard **KEY** (`GPIO18`) for about
0.5 seconds during a normal boot forces provisioning. While the device
is sleeping, a short KEY press wakes it and requests the next mode;
holding it for about 1.5 seconds opens provisioning instead.

If the board does not enter download mode automatically, hold **BOOT**,
tap **RESET**, release **BOOT** after the serial port appears, and run
the flash command again. To clear old settings, first run
`idf.py -C ESP32-S3-RLCD4.2 -p /dev/cu.usbmodemXXXX erase-flash`; this removes
saved Wi-Fi and InkSight configuration from the device.
