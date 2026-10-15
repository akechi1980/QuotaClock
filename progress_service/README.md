# 简易进度 JSON 服务

## 一键启动 / 迁移（Windows）

- 双击 `start.cmd`：首次自动创建本目录 `.venv`，之后先关闭本目录旧服务，再以 `pythonw.exe` 后台无窗口启动；启动窗口可关闭。
- 双击 `stop.cmd`：只关闭本目录服务；未运行也可执行。日志保存在 `.runtime/`，不自动删除；启动失败时窗口提示。
- 默认监听 `0.0.0.0:8766`，仅可信局域网使用；端口被其他/无法识别进程占用时拒绝启动，不误杀。
- 安装Python 3.9+（推荐当前受支持版本）；仅标准库，`requirements.txt` 无第三方依赖。
- 迁移时复制 `progress_service/`（排除 `.venv/`、`.runtime/`、`__pycache__/`），在新机双击启动重建环境。私密配置需自行安全转移或重填，禁止公开打包/上传。无需复制整个固件工作区。后台运行不等于开机自启，电脑重启后需再次启动。
- 新机IP变化后，自行更新设备服务URL；不自动改防火墙。迁移包/旧归档不含Token。

```powershell
.\progress_service\start.ps1 -SetupOnly   # 只创建/检查虚拟环境，不动运行中服务
.\progress_service\start.ps1 -BindAddress 192.168.1.100 -Port 8766
```

## 真实额度（已接通）

使用 `--config` 真实模式，返回 `simulation:false`；设备接入状态与验证范围见根目录 `HANDOFF.md`。

复制 `config.example.json` 为 `config.private.json`，仅在本机填写：
- `openai.access_token`：Codex/ChatGPT 登录 access token，**不是 sk- API Key**；`account_id` 选填，多账号时填对应账号 ID。
- `cursor.session_token`：网页 `WorkosCursorSessionToken` Cookie 的原始值，不含 Cookie 名称或其他 Cookie。

```powershell
py -3 .\progress_service\server.py --host 192.168.1.100 --port 8766 --config .\progress_service\config.private.json
```

同端口只运行一个服务。Token手动更新，不自动续期/读写其他应用凭据；私密配置不提交、不归档。OpenAI可从本机Codex的 `tokens.access_token` / `tokens.account_id` 复制对应值。三行依次为OpenAI5小时（18000秒）、每周（604800秒）、Cursor月度 `individualUsage.plan.autoPercentUsed`；均为已用百分比、四舍五入截到100，不含额外按量消费。缺字段报错，不猜分母或替换口径。

使用非公开HTTPS仪表盘接口，禁止重定向转发凭据；上游超时10秒、响应上限1MB。启动后台立即取数，**按北京时间UTC+8：10:00（含）至次日03:00（不含）每2分钟，03:00–10:00每5分钟**，时段边界会提前唤醒；失败仍每1分钟重试。设备请求只读内存缓存，不等待上游；设备自身拉取周期由用户配置，Python调度不会修改它。尚无缓存、刷新失败或缓存超过10分钟立即返回503，设备保留旧值并按原规则判过期。不会发送Token或上游原文；连续多个真实周期仍需观察。

Python 标准库实现，只提供只读 HTTP，不安装依赖或修改防火墙。

```powershell
# 本机调试，返回明确的测试值
py -3 .\progress_service\server.py --values 25 50 75
# 设备访问：明确绑定 LAN；只在可信局域网使用
py -3 .\progress_service\server.py --host 0.0.0.0 --demo
# 真实数据：每次请求读取文件中的 {"progress":[...]}，支持随时更新
py -3 .\progress_service\server.py --host 0.0.0.0 --file .\progress_service\progress.example.json
```

ROM 网页填写 `http://电脑局域网IP:8766/api/progress`，不要填 localhost。需要用户自行检查网络/防火墙可达性，不自动开放端口。

响应：`{"progress":[25,50,75],"simulation":true}`。严格三项 0–100 整数，不接受小数或布尔值。固定测试值与 `--demo` 标为 simulation，设备显示 SIM；文件输入标为非模拟。原子替换数据文件可避免读取半写入 JSON；文件损坏/超限返回 503，设备保留旧值。

`GET /health` 为健康检查；不提供数据写入接口、认证或公网部署能力。测试：`py -3 -m unittest discover -s .\progress_service -p 'test_*.py' -v`。
