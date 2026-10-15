# 开发环境与编译

## 硬件目标

ESP32-S3，8MB flash + 8MB OPI PSRAM，ST7789 240×240。LCD：MOSI1、SCLK18、DC17、RST6、背光2（低有效）、CS=-1；SPI mode3/20MHz，反色开启。音频ES8311/NS4150B及引脚见 `clock_rom/AUDIO_BOARD.md`。不同板卡不要直接套用。

## Windows依赖

- Python 3.9+；固件工具 `esptool==5.5.0` 安装到 `py -3` 使用的解释器。
- Arduino CLI，构建脚本当前使用 `C:\Program Files\Arduino CLI\arduino-cli.exe`。
- 隔离Arduino-ESP32 **2.0.12**；FQBN和8个配套库版本见根目录 `ARCHIVE_INFO.json`。配套库源码/TFT配置已包含，不需要复制旧工具链。
- 已包含 `ui_glyphs.h`，普通编译无需Pillow；重新生成中文字形才需要Pillow 12.2.0及脚本指定字体，发布新字形前须确认字体授权。

在本目录根部运行：

```powershell
py -3 -m pip install esptool==5.5.0
powershell -NoProfile -ExecutionPolicy Bypass -File .\maintenance\restore_dev.ps1 -InstallCore
powershell -NoProfile -ExecutionPolicy Bypass -File .\clock_rom\build.ps1
py -3 -m unittest discover -s .\clock_rom -p 'test_*.py' -v
```

恢复脚本只在 `vendor/clock_rom/` 重新生成本机配置、安装隔离core，不改全局Arduino core、不连接设备。不要直接运行旧 `prepare.py`：它是原工作区的依赖导入脚本，独立目录应使用 `maintenance/restore_dev.ps1`。

每次编译新建 `build/clock_rom/run_时间戳/`，不复用旧分区缓存；校验S3、8MB、应用摘要和已验证huge_app分区精确SHA256/MD5。脚本不上传固件。绝对路径/编译时间可能导致重新编译哈希不同，不能误认为与打包ROM逐字节可复现。

## 设计与实施

分别打开 `clock_rom/hourly_web_preview.html` 与 `clock_rom/hourly_firmware_preview.html`，固件预览共用 `hourly_design.html`。先确认视觉/交互，再改固件与API；代码和预览保持同步。需更新ROM时先验证为无配置干净镜像，再重新生成提交清单；不要拿实机8MB读回文件替换公开成品。

WEB预览当前由实际 `web_page.html` 加内存模拟API生成，保留真实CSS/控件；可模拟切换“天气时钟”和“灰阶时钟 / 三行进度”并保存，不访问设备。新方案先做预览确认，实施后再校准到真实页面。

```powershell
# 重新校准当前页面的离线预览
py -3 .\maintenance\update_web_preview.py
# 可选：安装Pillow后，用Chrome/Edge生成真实HTML截图（仅示例数据）
py -3 .\maintenance\capture_web_preview.py
```

截图脚本使用隔离临时浏览器配置、仅监听本机的短期预览服务，结束自动关服务；不读取浏览器账号或实机页面。布局变化后要人工检查截图裁切，不能用手绘示意冒充当前管理页截图。
