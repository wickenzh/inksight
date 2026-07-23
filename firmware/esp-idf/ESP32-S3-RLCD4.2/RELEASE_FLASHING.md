# ESP32-S3-RLCD4.2 Release Firmware

本固件适用于 Waveshare ESP32-S3-RLCD-4.2，使用纯 ESP-IDF 5.5.3
构建。当前仍是基础适配版本，尚未完整测试全部板载外设和长期运行场景。

This firmware targets the Waveshare ESP32-S3-RLCD-4.2 and is built with
pure ESP-IDF 5.5.3. It remains a basic port; not all onboard peripherals
or long-running scenarios have been fully tested.

## 烧录合并固件 / Flash the merged image

下载 `ESP32-S3-RLCD4.2-full.bin` 后执行：

After downloading `ESP32-S3-RLCD4.2-full.bin`, run:

```bash
esptool.py --chip esp32s3 \
  --before default_reset \
  --after hard_reset \
  write_flash 0x0 ESP32-S3-RLCD4.2-full.bin
```

合并固件会写入引导程序、分区表、OTA 数据分区和应用程序，并清除原有
NVS 配置。烧录后需要重新配置 Wi-Fi 和服务器地址。

The merged image writes the bootloader, partition table, OTA data, and
application, and clears the existing NVS configuration. Configure Wi-Fi
and the server address again after flashing.

## 烧录分立固件 / Flash the individual images

解压 `ESP32-S3-RLCD4.2-firmware.zip`，进入解压后的目录并执行：

Extract `ESP32-S3-RLCD4.2-firmware.zip`, enter the extracted directory,
and run:

```bash
esptool.py --chip esp32s3 \
  --before default_reset \
  --after hard_reset \
  write_flash @flash_args
```
