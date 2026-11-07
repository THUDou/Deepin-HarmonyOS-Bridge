# Oink 参考与许可证边界

> 参考项目：<https://github.com/oinkvm/oink>（2026-11-07 查看）
> 结论先行：**可以借鉴产品功能和交互，不复制代码，也不复制其私有线协议。**

## 1. 许可证

Oink 是 **GPL-3.0-only**，并带有 GPLv3 §7 附加条款：

- 分发/修改版必须保留 Oink 原始署名；
- 修改版必须显著标记“已修改”，不能冒充原版；
- 不得用 Oink 名称背书或推广衍生产品。

DHB 当前是 **Apache-2.0**。因此：

- **不能**把 Oink 的 Rust/C++/Swift 源码直接拷进 DHB；
- **不能**把 Oink 的代码翻译/改写后当作 DHB 自有实现；
- 可以独立实现相同**功能**、参考其公开的交互与架构思想；
- 如果未来确实要复用其代码，只能把相关部分做成独立的 GPL-3.0 组件并
  完整遵守 GPL，不能和 Apache-2.0 核心混成一个许可不明的二进制。

本仓库没有引入 Oink 代码或二进制。

## 2. 它是什么

Oink（Oink Is Not only a KVM）是 Windows/macOS 之间的对等 KVM：

| 能力 | Oink | DHB 当前 |
|---|---|---|
| 设备发现 | 局域网自动发现 | UDP 47822 + 手动 IP |
| 拓扑 | P2P，无固定控制端 | HAP 作为 client，Linux 作为 server |
| 键鼠控制 | 支持 | 未实现 |
| 剪贴板 | 自动同步 | 协议/接收侧有，发送受 Kaihong 权限限制 |
| 文件 | 拖拽/文件传输 | TCP 文件传输，实传待验证 |
| 音频 | 多设备音频转发 | 未实现 |
| 加密 | 默认加密 | TCP + PSK，**未 TLS** |
| 平台 | Windows x64 / macOS arm64 | Linux x86 + KaihongOS x86 HAP |
| 协议 | 自研 P2P，不兼容 Deskflow | DHB v1 自研 TCP |

它证明了一个产品判断：

> 用户要的不是“某个总线”，而是“设备像一台电脑一样可用”：
> 发现、信任、输入、剪贴板、文件、音频，统一在一个界面里。

但 Oink **不覆盖 Linux/HarmonyOS**，也没有 dsoftbus/软总线；它不能替代
DHB 的跨生态桥，只能作为产品体验和 P2P 架构的参照。

## 3. 可以借鉴的部分

### 3.1 对称 P2P，而不是固定 host/client

Oink 明确不要求指定控制端。DHB 现在是“HAP 连 Linux server”，适合先把
文件链路跑通；下一步应把 `serve/send` 抽象成**对称 peer**：

```text
PeerSession:
  discovery -> pairing -> capability exchange -> encrypted channel
  roles: controller / controlled / file-peer  (可同时存在)
```

这不会改变 DHB v1 的线协议，只是把“谁 listen、谁 connect”从产品语义里解耦。

### 3.2 默认加密，不是“开发模式明文”

Oink 的默认加密是产品底线。DHB 目前只有 PSK/HMAC 认证，TCP 仍明文；
这不能作为正式版。下一步优先级应高于屏幕/输入：

- TLS 1.3 或 Noise IK；
- PSK 只用于首次配对，之后用设备长期密钥；
- 传输层加密与 DHB 文件校验（SHA-256）分开。

### 3.3 发现后立刻给用户“可操作的结果”

Oink 的界面不是“发现列表 + 连接按钮”就结束，而是直接进入可用状态。
DHB HAP UI v2 已有“我的设备/附近设备/协作”，下一步要补：

- 首次配对二维码/PIN（而不是只填 PSK）；
- 可信设备长期列表；
- 拖拽文件到设备卡；
- 接收目录/进度/失败重试；
- 断线自愈。

### 3.4 功能分层，不把 KVM 当 MVP

Oink 把键鼠、剪贴板、文件、音频做成同一网络上的不同能力。DHB 应该按
**用户任务**而不是按协议模块排期：

| 优先级 | 用户任务 | 依赖 |
|---|---|---|
| P0 | 找到设备、配对、传文件、校验 | 现有 DHB TCP + PSK |
| P1 | 双向剪贴板、可信设备、自动重连 | 权限 + 信任状态 |
| P2 | 屏幕共享/远程查看 | 采集/编码/传输 |
| P3 | 键鼠回控、音频 | 输入注入/音频通道 |

屏幕/输入不应阻塞 P0/P1；Oink 的“全功能”是成熟产品结果，不是第一版范围。

## 4. 不能借鉴的部分

- **代码**：GPL-3.0-only + 附加条款，不能进入 Apache-2.0 核心。
- **线协议**：Oink 自研 P2P 协议，不兼容 Deskflow；DHB 也不应假装兼容。
- **名称/品牌**：不能使用 Oink 名称或暗示官方兼容。
- **平台假设**：Oink 的 Windows/macOS 输入注入、音频插件、Tauri UI
  不能直接映射到 Linux/KaihongOS；这些必须各自实现。

## 5. 对 DHB 路线的影响

短期不改变主战线：

1. 先把 **Kaihong x86 HAP ↔ Deepin x86** 的文件实传和 SHA-256 做完；
2. 再补 **TLS + 可信设备状态**；
3. 再谈屏幕/输入；
4. dsoftbus 6.x 仍按 `docs/11` 的 P0/P1/P2 推进，不因 Oink 改线协议。

Oink 的最大价值是提醒我们：

> **桥的终点不是“协议通了”，而是“用户看到设备、信任设备、把文件拖过去”。**

DHB 的差异化仍然是：Linux + KaihongOS/OpenHarmony + 自有 TCP fallback +
未来 dsoftbus 适配，而不是复制一个 Windows/macOS KVM。
