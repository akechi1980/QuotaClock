# 服务端运行与迁移

## 快速启动（Windows）

1. 安装Python 3.9+；双击 `progress_service/start.cmd`，首次创建该目录的 `.venv`。
2. 若没有私密配置，会从 `config.example.json` 生成 `config.private.json` 并提示填写。只在本机填写，不提交Git。
3. 再双击 `start.cmd`：先安全关闭本目录旧服务，再后台无窗口启动，默认 `0.0.0.0:8766`。不自动修改防火墙，不是开机自启。
4. 关闭用 `progress_service/stop.cmd`；日志位于被Git忽略的 `.runtime/`。

配置字段：OpenAI `access_token` 为Codex登录访问令牌，`account_id`为对应账号ID（不是API Key）；Cursor `session_token`为网页 `WorkosCursorSessionToken` 的原始值。Token过期需手动更新，不自动读取/轮换其他应用凭据。

设备管理页填写 `http://电脑局域网IP:8766/api/progress`，默认每5分钟请求。不要使用localhost；例 `192.168.1.100` 只是示例地址。

## 接口与刷新

- `GET /health`：进程健康，不代表上游认证成功。
- `GET /api/progress`：三项0–100整数，顺序固定为OpenAI 5小时、OpenAI每周、Cursor月度Auto，`simulation:false`表示真实来源。
- 服务启动即后台取数，成功每5分钟刷新、失败每1分钟重试；设备请求只读缓存，不等外部接口。
- 未准备好/刷新失败/缓存超过10分钟返回503；设备保留旧值并按原规则判过期，不能把失败当0%。
- 使用非公开仪表盘接口，服务可能随上游变更而失效；仅用于自己的授权账号和可信局域网，不开放公网。没有认证、数据写接口或联网OTA。

## 测试与迁移

```powershell
py -3 -m unittest discover -s .\progress_service -p 'test_*.py' -v
```

迁移只需源码/模板，不复制 `.venv`、缓存和日志；新机启动重建环境。私密配置自行安全传递或重新填写，绝不能放进公开包。IP变化后自行更新设备URL。
