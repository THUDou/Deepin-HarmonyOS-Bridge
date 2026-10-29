# DHB 系统架构

> 目标：用一套协议、两种传输、两档能力，把 OpenHarmony/KaihongOS 与 Deepin/UOS 连成可用的"超级桌面"。
> 本文描述 **v0.1 已实现** 与 **v1.0 计划**，明确标注状态，避免把设计当现实。

## 1. 设计原则

1. **协议与传输解耦**：DHB 帧协议不依赖 TCP；今天跑 TCP，明天跑 Klink/dsoftbus，业务代码不改。
2. **能力协商，优雅降级**：HELLO 交换 `caps`；没有屏幕采集就只开文件，没有 dsoftbus 就退回 UDP+TCP。
3. **本地优先，无云**：MVP 不引入账号、云中转、遥测上传；配对与信任留在设备侧。
4. **系统能力单独成档**：零售三方应用能做的（Core）与系统签名能做的（Pro）在代码、打包、文档上分开，不混淆宣传。
5. **安全默认开启**：PSK/HMAC 起步，TCP 生产版强制 TLS 1.3；文件路径净化 + 哈希校验；最小权限。
6. **可测试**：核心逻辑用内存传输做端到端测试，不依赖真机；真机验证只测适配层。

## 2. 逻辑组件

```
┌────────────────────────────────────────────────────────────────────┐
│                         DHB Core Library (C++17)                   │
│                                                                    │
│  crypto        protocol        session          discovery          │
│  SHA-256       Frame codec     HELLO/AUTH       UDP broadcast      │
│  HMAC/HKDF     control msgs    multiplex         responder          │
│  CRC-32C       file msgs       file engine                         │
│                                                                    │
│  transport:  ITransport ── TcpTransport (now)                      │
│                          ├─ MemoryTransport (tests)                │
│                          ├─ KlinkTransport (M2)                    │
│                          └─ DsoftbusTransport (M3, OHOS native)    │
└────────────────────────────────────────────────────────────────────┘
        │                                   │
        ▼                                   ▼
┌───────────────────────┐         ┌───────────────────────────────┐
│ dhb CLI / dhbd        │         │ HarmonyOS HAP                 │
│ serve/send/discover   │         │ ArkTS UI + BridgeClient       │
│ DDE tray / systemd    │         │ TCP/TLS now; NAPI dsoftbus M3 │
└───────────────────────┘         └───────────────────────────────┘
```

### 2.1 Linux 侧

| 组件 | 状态 | 说明 |
|---|---|---|
| `dhb_core` | ✅ v0.1 | 协议、会话、文件、发现、加密；无 Qt/DBus 依赖 |
| `dhb` CLI | ✅ v0.1 | `serve` / `send` / `discover` / `doctor` / `selftest` |
| `dhbd` 守护进程 | ⏳ M1 | 复用 core；D-Bus 接口、托盘、开机自启 |
| DDE 控制中心面板 | ⏳ M2 | 设备列表、配对码、连接状态、开关 |
| PipeWire 采集 | ⏳ M2 | xdg-desktop-portal + PipeWire；X11 回退 |
| VAAPI 编码 | ⏳ M2 | H.264 baseline/low-latency；软编回退 |
| uinput 注入 | ⏳ M2 | polkit 辅助进程；Wayland/X11 通用 |
| Klink 适配 | ⏳ M2 | `ITransport` 实现；复用 Klink 发现/认证/会话 |
| deb / systemd | ⏳ M1 | `packaging/` |

### 2.2 HarmonyOS 侧

| 组件 | 状态 | 说明 |
|---|---|---|
| `BridgeProtocol.ets` | ✅ v0.1 | 与 C++ 等价的帧/消息/CRC/SHA-256 |
| `BridgeClient.ets` | ✅ v0.1 | TCP 连接、握手、文件双向、剪贴板 |
| `Discovery.ets` | ✅ v0.1 | UDP 广播发现（可选） |
| `Index.ets` | ✅ v0.1 | 设备/连接/文件/日志 UI |
| `distributedDeviceManager` | ⏳ M2 | 公开 API 发现 + PIN 绑定；失败则手动 IP |
| `libdhb_native.z.so` | ⏳ M3 | NAPI + dsoftbus session + avcodec |
| 系统签名 HAP / SA | ⏳ M3 | Pro 档；KaihongOS ACL |

## 3. 传输矩阵

| 传输 | 适用 | 认证 | 加密 | 状态 |
|---|---|---|---|---|
| TCP（开发） | 同机回环、内网调试 | DHB PSK/HMAC | ❌（仅开发） | ✅ |
| TCP + TLS 1.3 | 零售 NEXT Core | PSK + 证书/TOFU | ✅ | ⏳ M1 |
| Klink/dsoftbus | Deepin + KaihongOS Pro | HiChain PIN | ✅ dsoftbus 会话 | ⏳ M2 |
| dsoftbus（OHOS native） | Pro | HiChain | ✅ | ⏳ M3 |
| 内存 | 单元/集成测试 | 无 | 无 | ✅ |

**同一会话协议**：无论底层是什么，`Session` 只依赖 `ITransport::sendAll/recvExact`。Klink 的 `OnBytesReceived` 回调把数据喂给同一个 `readFrame`；dsoftbus 的 stream 通道用于视频，bytes 通道用于控制/文件。

## 4. 关键数据流

### 4.1 文件：Linux → 鸿蒙

```
Linux                         HarmonyOS
  │ FILE_OFFER(id,size,sha) ─────▶ │
  │ ◀──── FILE_ACCEPT(id,offset)   │  (断点续传：offset = .part 大小)
  │ FILE_CHUNK(id,off,256KiB) ───▶ │  write + incremental SHA-256
  │ ...                            │
  │ FILE_DONE(id,sha) ───────────▶ │  compare, rename .part, send result
  │ ◀──── FILE_RESULT(id,ok,sha)   │
  │ progress callback              │  onFileReceived(path)
```

- 接收端先写 `.part`；校验通过才 `rename` 到最终名。
- 文件名净化：取 basename、去掉控制字符、拒绝 `..`、截断 180 字节；重名自动 `name (1).ext`。
- 断点续传：若 `.dhb-<sha256>.part` 存在且 ≤ offer.size，从该偏移继续；最终 SHA 不匹配则删除并报错。用内容哈希命名，重连后新 transfer id 也能找回旧 `.part`。
- 分块 256 KiB；MVP 串行传多文件；后续可并行多 transfer id。

### 4.2 文件：鸿蒙 → Linux

同一协议反向；鸿蒙侧 `BridgeClient` 读 picker URI → 计算 SHA-256 → OFFER → 等 ACCEPT → 发 CHUNK → DONE → 等 RESULT。Linux `dhb serve` 的 `onFileReceived` 打印落盘路径。

### 4.3 屏幕：Linux → 鸿蒙（Core，M2）

```
Deepin: PipeWire frame ─▶ VAAPI H.264 ─▶ DHB SCREEN_FRAME(stream 2)
                                              │
HarmonyOS: AVCodec decode ─▶ XComponent surface
HarmonyOS touch ─▶ DHB INPUT_EVENT(stream 3) ─▶ Linux uinput
```

- 只传"变化区域"是 M3 优化；M2 全帧 + 关键帧请求。
- 编码参数：H.264 baseline, 1080p30, 8–12 Mbps, GOP 30, no B-frame, `zerolatency`。
- 时延预算：采集 10ms + 编码 10ms + 网络 10ms + 解码 10ms + 显示 16ms ≈ 56ms；目标 < 80ms。

### 4.4 屏幕：鸿蒙 → Linux（Pro，M3）

```
KaihongOS: avScreenCapture ─▶ AVCodec H.264 ─▶ dsoftbus stream
                                                   │
Deepin: dsoftbus recv ─▶ VAAPI/FFmpeg decode ─▶ SDL/GTK window
Deepin mouse/keyboard ─▶ DHB INPUT_EVENT ─▶ OHOS multimodal input inject
```

- 不直接复用 `distributed_screen` 的 SA 框架（依赖 OHOS SAMGR/IPC），但复用它定义的 Source/Sink 分层与 `SoftbusAdapter` 思路。
- 长期可做"Linux 作为 distributed_screen 的原生 sink"，直接接收 OpenHarmony 系统投屏，无需设备端装 DHB；这是 M4 的研究项。

## 5. 线程与生命周期

- **一个 Session 一个 reader 线程**：阻塞读帧、分发；文件接收在 reader 线程内完成（MVP），因此大文件落盘会短暂阻塞 ping；M2 改为有界队列 + worker。
- **写操作**由 `write_mutex_` 串行化；`sendFile` 由 `send_file_mutex_` 串行化。
- **握手同步**：connect/accept 后先 HELLO/AUTH，再启动 reader；握手超时 10s。
- **关闭**：`close()` 幂等；shutdown socket 唤醒 reader；join；回调不得在 reader 线程里调用 `close()`（检测 self join）。
- **重连**：M1 由 daemon 负责指数退避；Session 本身不自动重连，避免隐藏状态。

## 6. 能力协商

`Hello.caps` 位图：

| 位 | 名称 | 含义 |
|---|---|---|
| 0 | `kCapFile` | 支持 FILE_* |
| 1 | `kCapClipboard` | 支持 CLIPBOARD_SET |
| 2 | `kCapScreenSink` | 能显示远端屏幕 |
| 3 | `kCapScreenSource` | 能采集并发送屏幕 |
| 4 | `kCapInput` | 能注入输入 |
| 5 | `kCapNotify` | 能发布/订阅通知 |
| 6 | `kCapDiscovery` | 支持 DHB 发现 |

规则：发送方在发起屏幕/输入前检查对端 caps；不满足则 UI 灰掉并给出原因。Core 的鸿蒙侧声明 `ScreenSink | Input | File | Clipboard`；Linux 声明 `ScreenSource | Input | File | Clipboard`。

## 7. 安全模型

### 7.1 威胁

- 同 LAN 的攻击者伪装成 DHB 对端、窃听文件/屏幕、路径穿越写文件、恶意超大文件/分块、重放握手。
- 本机低权限进程调用 CLI/DBus 发起传输。

### 7.2 控制

- **认证**：PSK 32 字节（或口令经 SHA-256 派生）+ 双 nonce + challenge nonce 的 HMAC-SHA256；常量时间比较；拒绝降级（有 PSK 时收到 AUTH_OK 直接失败）。
- **传输加密**：dsoftbus 会话自带加密；TCP 生产版 TLS 1.3 + 证书 pinning；当前明文 TCP 仅限 `--insecure` 本机调试。
- **授权**：文件接收目录固定；路径净化；单文件大小上限（M2 可配）；分块偏移必须连续。
- **完整性**：每帧 CRC-32C；整文件 SHA-256；`.part` 校验后重命名。
- **最小权限**：uinput 通过 polkit 辅助进程；daemon 不以 root 常驻；HAP 只申请 INTERNET/GET_NETWORK_INFO（Core）。
- **审计**：连接、认证失败、文件落盘、屏幕开始/结束写结构化日志；不记录剪贴板内容。

### 7.3 待补

- PAKE（SPAKE2/OPAQUE）替代 PSK 明文输入；二维码/近场配对。
- TLS 证书生命周期与吊销。
- 防重放窗口（当前 nonce 已防跨会话重放，但同一会话内无序号窗口）。
- 模糊测试（帧解析、文件状态机）。

## 8. 打包与桌面集成

- **deb**：`dhb`（CLI + core）、`dhbd`（daemon，M1）、`dhb-dde`（控制中心插件，M2）。
- **systemd user unit**：`dhb.service`，`Restart=on-failure`，`NoNewPrivileges=yes`，`ProtectSystem=strict`，仅允许读写 `~/DHB` 与配置目录。
- **DDE**：控制中心"跨端协同"页；托盘菜单；文件管理器"发送到鸿蒙设备"；剪贴板走 `wl-paste`/DDE clipboard DBus。
- **UOS**：商业签名、安全中心白名单、应用商店；M4。
- **HarmonyOS**：HAP 签名；Core 走应用市场；Pro 走 KaihongOS 预装/系统签名。

## 9. 性能预算（M2 验收）

| 指标 | 目标 | 测量 |
|---|---|---|
| 屏幕时延（glass-to-glass） | < 80 ms @1080p30 | 高速相机/计时码 |
| 屏幕 CPU（i5-1240P） | < 15% 单核 | `pidstat` |
| 文件吞吐（Wi-Fi 6, 80MHz） | > 80 MB/s | 1 GiB 文件 |
| 文件 CPU | < 10% 单核 | `pidstat` |
| 配对时间 | < 10 s | 手动计时 |
| 断线重连 | < 3 s | 拔网线/关 Wi-Fi |
| 内存常驻 | < 120 MB（无屏幕） | `ps_mem` |

## 10. 与代码的映射

| 设计 | 代码 |
|---|---|
| 帧格式/控制消息 | `include/dhb/protocol.h`, `src/protocol.cpp` |
| 会话/握手/文件引擎 | `include/dhb/session.h`, `src/session.cpp` |
| 传输抽象/TCP/内存 | `include/dhb/transport.h`, `src/transport.cpp` |
| 发现 | `include/dhb/discovery.h`, `src/discovery.cpp` |
| 加密 | `include/dhb/crypto.h`, `src/crypto.cpp` |
| CLI | `apps/dhb-cli/main.cpp` |
| 鸿蒙客户端 | `ohos/entry/src/main/ets/core/*` |
| 测试 | `tests/dhb_tests.cpp` |

## 11. 已知取舍

- **TCP 明文**：为快速验证协议；生产必须 TLS。文档与 CLI 明确标注。
- **文件接收在 reader 线程**：实现简单，但大文件会延迟控制帧；M2 改 worker。
- **没有自动重连**：避免库层隐藏状态；daemon 负责。
- **没有并行文件**：MVP 串行；协议已用 transfer id，可扩展。
- **没有流控**：依赖 TCP 背压；dsoftbus stream 需要 M3 加 credit/关键帧请求。
- **Pro 档依赖系统签名**：不是纯技术问题，需要 KaihongOS 合作或自行烧录系统镜像。
