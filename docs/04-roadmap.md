# DHB 路线图

> 起点：2026-10。时间按 1–2 名全职工程师估算；硬件与厂商合作是最大变量。
> 原则：**每个里程碑都有可演示的纵向切片**，不做"半年后一次集成"。

## M0 · 可行性 gate（2 周）

目标：证明三条关键链路至少两条能跑。

| # | 任务 | 产出 | 通过标准 |
|---|---|---|---|
| 0.1 | **Legacy 4.x**：KLink ↔ OpenHarmony 4.1 / HarmonyOS 4.x | Klink probe 日志 | 发现 + PIN + 双向字节，至少一项 |
| 0.2 | **Modern 6.x**：OpenHarmony 6.x / KaihongOS 6.x ↔ 6.x | native probe 日志 | 发现 + HiChain + session + bytes |
| 0.3 | 零售 NEXT 发现 Linux（保守） | HAP 日志 | `distributedDeviceManager` 是否对三方开放；失败则固定 UDP/手动 |
| 0.4 | Deepin 25 采集/注入 | 两个 demo | PipeWire 抓一帧 + uinput 移动一次鼠标 |
| 0.5 | 本仓库骨架 | `ctest`/MSYS2 测试 | 文件/剪贴板/发现/TCP 双向（已通过 POSIX 回环） |

**Gate**：0.1/0.2/0.3 中 ≥2 通过 → 进入 M1；仅 1 通过 → 收缩为"文件+剪贴板"；0 通过 → 暂停 Pro，转纯应用层工具。4.x 与 6.x 分别评估，不互相绑定。

## M1 · Core Alpha（4–6 周）

目标：Deepin ↔ 零售 HarmonyOS 的"文件口袋 + 剪贴板"可用。

- TLS 1.3（OpenSSL）替换明文 TCP；证书 TOFU + pairing key。
- `dhbd` 守护进程：D-Bus 接口、托盘、开机自启、断线重连。
- `dhb` CLI 完善：`pair`、`devices`、`send`、`recv`、`status`。
- HAP：手动 IP + pairing key、文件选择/接收、剪贴板、连接状态、错误提示。
- deb 包 + systemd user unit；Deepin 25 / UOS 25 测试。
- 文档：用户手册、故障排查、威胁模型。
- 验收：1 GiB 文件双向传输 SHA 一致；断网重连 < 5s；72h 不崩。

## M2 · 副屏与传输基座（6–8 周）

目标：**Deepin 作为主机，鸿蒙设备作为无线副屏 + 触控板**；双 profile 接入。

- PipeWire/xdg-desktop-portal 采集；X11 回退。
- VAAPI H.264 低延迟编码；关键帧请求；自适应码率（初版）。
- HAP：`XComponent` + `AVCodec` 解码显示；触摸/笔事件回传。
- Linux：uinput 注入；坐标映射、HiDPI、多显示器。
- **Legacy helper**（`dhb-softbus4d`）：KLink 4.x，发现/认证/`SendBytes`/`SendStream`；服务 4.x 设备。
- **Modern helper**（`dhb-softbus6d`）：OpenHarmony 6.0-Release；服务 KaihongOS/6.x 设备（openEuler 3.2 只作 CI/legacy 参考）。
- 两个 helper 通过 Unix socket 暴露 `ITransport`，与 TCP 共用 DHB Session；不假设 4.x↔6.x 原生互通。
- DDE 控制中心"跨端协同"页；文件管理器右键"发送到鸿蒙"。
- 验收：1080p30 端到端 < 80ms；CPU < 15%；连续 1h 不漂移。

## M3 · Pro 镜像回控（8–10 周）

目标：KaihongOS/OpenHarmony 系统级能力。

- OHOS 原生 NAPI 模块：`libsoftbus_client.z.so` + `avScreenCapture` + `AVCodec` + 输入注入。
- 鸿蒙 → Linux 镜像 + 回控；多窗口/应用窗口选择（若系统 API 允许）。
- 音频（Opus）双向；麦克风/扬声器流转。
- hmdfs PoC：Linux 挂载/浏览鸿蒙分布式目录（或反向）。
- 系统签名 HAP / SA；KaihongOS ACL 配置；安装包。
- 验收：1080p30 镜像 + 回控 < 100ms；文件拖动落盘；72h 稳定。

## M4 · 产品化（持续）

- 多设备、设备分组、权限管理、企业策略。
- UOS 商业签名/商店/安全中心；KaihongOS 预装合作。
- 跨网段/中继（可选，端到端加密）；IPv6。
- 分布式相机/麦克风、通知双向；应用接续仍不承诺（平台限制）。
- 国际化、无障碍、遥测（opt-in）。

## 版本节奏

| 版本 | 内容 | 目标日期 |
|---|---|---|
| `v0.1.0-alpha` | 协议 + CLI + HAP 骨架 + 文档 | 2026-11 |
| `v0.2.0` | TLS + daemon + deb + 文件/剪贴板可用 | 2026-12 |
| `v0.3.0` | 副屏 + 触控 + Klink | 2027-02 |
| `v0.4.0` | Pro 镜像回控 PoC | 2027-04 |
| `v1.0.0` | 产品化、UOS/KaihongOS 认证 | 2027-Q3 |

## 资源与分工（建议）

| 角色 | 人数 | 职责 |
|---|---|---|
| Linux/系统 | 1 | Klink、PipeWire、uinput、deb/systemd、DDE |
| 鸿蒙/应用 | 1 | ArkTS、NAPI、avcodec、系统签名 |
| 测试/交付 | 0.5 | 设备矩阵、性能、打包、文档 |
| 设计/产品 | 0.5 | 交互、控制中心、场景验证 |

## 关键依赖

- KaihongOS x86/ARM 测试机（至少 1 台 x86 + 1 台 RK3588）。
- Deepin 25 / UOS 25 测试机（Wayland + X11 各一台）。
- HarmonyOS NEXT 真机（零售）用于 Core 验证。
- Klink 维护者/麒麟社区沟通渠道（协议升级、问题反馈）。
- H.264 编码合规评估。

## 度量

- 北星指标：**每周成功跨端文件传输次数 / 副屏使用时长**。
- 质量：崩溃率、配对成功率、传输成功率、P95 时延。
- 社区：issue 响应、外部贡献者、设备兼容列表。
