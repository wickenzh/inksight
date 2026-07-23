# Web 在线刷机

InkSight 提供基于浏览器的在线刷机能力，适合绝大多数用户。

![在线刷机页面截图](/images/docs/flash-zh.png)

上图就是当前刷机页的实际界面：左侧先看操作步骤，下面选择固件来源，最后点击“刷写固件”并观察日志。

## 视频教程

- 刷机视频教程：[`Bilibili：墨鱼AI墨水屏刷机演示`](https://www.bilibili.com/video/BV1nSNcziE7q/?spm_id_from=333.1387.homepage.video_card.click&vd_source=54a8e4fd085f18b62270015f45fabcc7)

## 1. 使用条件

- 浏览器：Chrome / Edge（支持 WebSerial）
- 访问环境：`localhost` 或 HTTPS 域名
- 连接方式：USB 数据线（必须支持数据传输）

## 2. 基本流程

1. 打开**在线刷机**页面
2. 连接设备并授权串口
3. 选择固件版本（可刷新版本列表）
4. 点击刷写并等待完成
5. 刷写后观察串口日志确认启动正常

## 3. 版本来源与代理

WebApp 会通过以下接口获取固件版本信息：

- `GET /api/firmware/releases`
- `GET /api/firmware/releases/latest`
- `GET /api/firmware/validate-url?url=...`

当未配置 `NEXT_PUBLIC_FIRMWARE_API_BASE` 时，前端默认走同域 API Route 代理到 `INKSIGHT_BACKEND_API_BASE`。

## 4. 故障排查

### 无法识别串口设备

- 确认是数据线，不是仅充电线
- 尝试更换 USB 口或重插设备
- 在系统设备管理中确认串口是否存在

### 版本列表加载失败

- 检查后端是否可访问
- 检查后端是否触发 GitHub API 频率限制
- 在页面内点击“刷新版本”重试

### 刷写中断/失败

- 保持 USB 连接稳定
- 切换到手动 URL 模式并先做链接校验
- 重启设备后再次进入刷机流程

## 5. Waveshare ESP32-S3-RLCD-4.2 首版固件

该硬件适配位于 `firmware/esp-idf/ESP32-S3-RLCD4.2/`，使用纯 ESP-IDF 5.5，不依赖
Arduino 或 PlatformIO。可以从
[自动构建的最新测试版](https://github.com/wickenzh/inksight/releases/tag/esp32-s3-rlcd4.2-latest)
下载完整固件，也可以安装并激活 ESP-IDF，然后从仓库根目录构建和烧录：

```bash
source "$HOME/esp/esp-idf/export.sh"
idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 set-target esp32s3
idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 build
idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 -p /dev/cu.usbmodemXXXX flash monitor
```

将 `/dev/cu.usbmodemXXXX` 替换为实际串口；只有一台 ESP 设备时也可省略
`-p` 让工具自动检测。按 `Ctrl+]` 退出串口监视器。

首次启动且没有保存配置时，设备会创建 `InkSight-XXXXXX` 配网热点。
连接该热点后打开 `http://192.168.4.1`，填写 Wi-Fi、InkSight 服务地址和刷新间隔。
服务器地址默认预填为官方后端 `https://web.inksight.site`；使用自建服务时可直接修改。
正常上电时按住板载 **KEY**（`GPIO18`）约 0.5 秒可强制进入配网。
设备深度睡眠期间短按 KEY 会唤醒并请求下一个模式；持续按住约 1.5 秒则进入配网。

若设备没有自动进入下载模式，按住 **BOOT**，短按 **RESET**，电脑识别到串口后
松开 **BOOT**，再执行烧录命令。需要清除旧配置时，可先执行
`idf.py -C firmware/esp-idf/ESP32-S3-RLCD4.2 -p /dev/cu.usbmodemXXXX erase-flash`；此操作会删除设备中
已保存的 Wi-Fi 和 InkSight 配置。
