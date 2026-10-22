# 开发入口

请先读根目录 RESTORE_DEVELOPMENT.md，依赖/目标见 ARCHIVE_INFO.json。先独立预览WEB与固件并确认，再实施、测试和另获授权部署。

当前0.2.6保留界面/用户设置，修复Wi-Fi恢复，天气下载/校验移到后台，统一异步SNTP并限制网络传输；健康指标与剩余限制见 `../docs/RELIABILITY.md`。不要把任务隔离理解为所有第三方库调用都完全无阻塞。
