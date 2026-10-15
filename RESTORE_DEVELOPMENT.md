# 非敏感开发恢复包 · 0.2.2

只包含源码、示例配置、配套库源码/板卡配置、设计预览与干净ROM。无Token、真实Wi-Fi配置、MAC/账号ID、NVS实机镜像、日志、原始构建/操作记录、工具链或虚拟环境。源码中的本机用户名和局域网地址已改为示例；ROM是原始干净构建，NVS已验证全FF，未改动二进制。

## 恢复开发
1. 完整解压，安装Python 3.9+和Arduino CLI（C:\Program Files\Arduino CLI\arduino-cli.exe）。`py -3`解释器安装esptool==5.5.0；已有中文字形头文件，正常编译无需Pillow，重新生成字形时才需Pillow==12.2.0和Windows微软雅黑。
2. 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\maintenance\restore_dev.ps1 -InstallCore`，生成本机路径并安装隔离esp32 core 2.0.12。配套库源码/TFT配置已保留，不要替换成最新版本。
3. 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\clock_rom\build.ps1`，再运行 `py -3 -m unittest discover -s .\clock_rom -p 'test_*.py' -v`。每次生成新的run_构建目录，不包含上传动作。
4. 服务端双击 `progress_service/start.cmd`，自动重建.venv；自行填写从模板生成的config.private.json，stop.cmd关闭。IP变化后自行设置设备URL；不自动改防火墙。私密配置不应放入后续archives包。

## 固定设计流程
分别打开 clock_rom/hourly_web_preview.html（完整WEB管理）与 clock_rom/hourly_firmware_preview.html（主画面及各状态页，使用hourly_design.html）。先展示、反馈并确认设计，再实施代码、编译测试；刷机另需当次授权、新双读备份及校验，部署后实机确认。预览明确为模拟，浏览器试听不等于设备实测。

## 边界
本包用于恢复源码开发，不含原厂恢复基线和受保护设备快照，不可直接当成完整恢复工具包。任何写入仍须原工作区的安全流程、新授权和新快照。干净ROM会清除本机配置；不要自动刷写。重新编译可能因时间戳/路径而改变ROM哈希。
