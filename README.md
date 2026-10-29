# DHB · Deepin-HarmonyOS-Bridge

**让 Deepin/UOS 和鸿蒙设备之间，传文件、同步剪贴板、配对连接，像在同一张桌面上。**

DHB 是一个开源的跨端协同桥：Linux 侧是原生 C++17 CLI/daemon，鸿蒙侧是 ArkTS/HAP。
它不依赖 ADB、不要求开发者模式，也不把“兼容鸿蒙”做成一句口号——**同一套 DHB 协议，今天跑 TCP，明天可以跑 dsoftbus；软总线走不通时，自己的协议仍然能工作。**

> **状态：`v0.1.0-alpha`，未完成。**
> 文件/剪贴板/发现/PSK 认证/双端骨架已跑通；HAP 已编译但未签名、未真机运行；
> KLink 4.x 已做到 Linux host build + 双容器 COAP 发现 + CLOUD_SN lab 认证；
> **Gate A（KLink ↔ OH 4.x）和 Gate B（OH 6.0 ↔ KaihongOS 6.x）尚未通过。**

---

## 现在能做什么

| 能力 | 状态 | 说明 |
|---|---|---|
| **双向文件流转** | ✅ 可用 | `serve`/`send`/`recv`，断点续传、`.part`、SHA-256 校验 |
| **PSK 认证 / 配对** | ✅ 可用 | 双 nonce + challenge + HMAC-SHA256；`dhb pair` 只认证不传文件 |
| **局域网发现** | ✅ 可用 | UDP 47822 广播；失败可手动填 IP |
| **文本剪贴板** | ✅ 协议与客户端代码完成 | HAP UI 已接，真机权限/行为待验证 |
| **Linux CLI** | ✅ 可用 | `serve` / `send` / `pair` / `recv` / `discover` / `doctor` / `selftest` |
| **HarmonyOS HAP** | ✅ 编译通过（unsigned） | DevEco 26 / SDK 26；未签名、未安装 |
| **KLink 4.x lab** | 🧪 实验可用 | Linux host build、双容器 COAP 发现、CLOUD_SN lab 认证 |
| **dsoftbus 6.x** | 📐 研究中 | 差异报告与 rebase 计划完成；Linux host build 未开始 |
| **TLS 1.3** | 🚧 未实现 | 当前 TCP 明文，仅限开发/可信局域网 |
| **屏幕协同 / 输入回控** | 🚧 路线图 | 采集、编码、回控均未实现 |
| **通知 / 分布式硬件** | 🚧 路线图 | 仅有架构设计 |

## 为什么不是“又一个投屏软件”

- **双端可安装**：Deepin 侧是 CLI/daemon，鸿蒙侧是普通 HAP；不依赖 ADB、不要求开发者模式。
- **协议开放**：DHB 帧格式、认证、文件流程都在仓库里，可审计、可替换传输层。
- **软总线不是唯一路径**：KLink 4.x/OH 6.x 能走就走；走不通时，DHB 自有 TCP 协议保底。
- **Linux 原生**：纯 C++17，无 Qt 依赖；后续接 PipeWire/uinput，而不是套一层 Electron。
- **不假装兼容**：Gate A/B 没通过就写没通过；HAP 没签名就写没签名。

## 一个桥，三条落地路径

| 路径 | 面向 | 当前状态 |
|---|---|---|
| **App / TCP** | 零售 HarmonyOS NEXT + Deepin/UOS | ✅ 可跑；HAP 已编译，待签名/真机 |
| **KLink 4.x legacy** | HarmonyOS 4.x / OpenHarmony 4.x / KaihongOS 4.x | 🧪 Linux host build + COAP 发现 + lab 认证；PIN/真机待验证 |
| **OH 6.x modern** | OpenHarmony 6.0 / KaihongOS 6.x / 纯血 HarmonyOS 6.x（系统权限） | 📐 差异/rebase 报告完成；Linux host build 未开始 |

> **纯血鸿蒙 PC 的“超级终端”是华为系统级能力，不是三方 HAP 的公开 API。** DHB 先做标准 HAP + TCP；PC 显示/超级终端接入属于 S3 研究，详见 [平台边界矩阵](docs/17-platform-taxonomy.md)。

> **软总线版本策略**：KLink 只负责 **4.x legacy**；**6.x modern** 用 **OpenHarmony 6.0-Release** 的 `communication_dsoftbus`（openEuler 公开包是 3.2，只作 CI/legacy 参考）。两代用独立 helper daemon 隔离，不链进同一进程；**不假设 4.x↔6.x 原生互通**。详见 [软总线 profile](docs/09-softbus-profiles.md)、[认证优先的 MVP](docs/10-auth-first-mvp.md)、[ADR-0005](docs/adr/0005-softbus-profiles.md)。

## 当前实现边界（重要）

- **HAP 是 unsigned**：`entry-default-unsigned.hap` 已生成，但没有签名配置、没有真机安装/运行。
- **没有 OHOS/KaihongOS 真机认证**：KLink 只在 Linux↔Linux 容器里跑通发现和 CLOUD_SN lab 认证；PIN 仍失败。
- **TCP 是明文**：`--insecure` 只用于本机回环；生产版要 TLS 1.3，尚未实现。
- **屏幕/输入/通知未实现**：README 里标 🚧 的能力都还在路线图，不是现成功能。
- **没有应用接续/流转**：平台限制多，不作为 MVP 承诺。

## 架构（当前代码路径）

```
┌──────────────────────────────┐         ┌──────────────────────────────┐
│ HarmonyOS / KaihongOS        │         │ Deepin / UOS (Linux)         │
│                              │         │                              │
│ ArkTS UI (HAP)               │         │ dhb CLI / dhbd (C++17)       │
│   │                          │         │   │                          │
│ BridgeClient.ets             │  DHB    │ Session / FileEngine         │
│   ├── TCP (PSK) ─────────────┼─────────┼── TCP transport              │
│   └── softbus helper (lab) ──┼─────────┼── KLink 4.x / OH6 adapter    │
│                              │         │   │                          │
│ AVCodec / XComponent (M2)    │         │ PipeWire / uinput (M2)       │
└──────────────────────────────┘         └──────────────────────────────┘
```

- **一个协议，多种传输**：20 字节头 + CRC-32C 的 DHB 帧，当前跑 TCP；设计上可跑 KLink/dsoftbus 的 byte stream。
- **能力协商**：HELLO 交换 `caps`；当前是 `file|clipboard`，屏幕/输入是后续扩展。
- **认证**：PSK/HMAC 挑战应答已实现；HiChain PIN 是 Gate A/B 的目标，不是现状。
- **文件**：`FILE_OFFER → FILE_ACCEPT(offset) → FILE_CHUNK* → FILE_DONE → FILE_RESULT`，256 KiB chunk，端到端 SHA-256。

## 五分钟跑起来（Linux 侧）

依赖：CMake ≥ 3.20、C++17 编译器、pthread。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

本机回环，不需要第二台设备：

```bash
./build/dhb serve --port 47821 \
  --psk 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff \
  --dir /tmp/dhb-recv &

./build/dhb send --host 127.0.0.1 --port 47821 \
  --psk 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff \
  ./README.md
```

只验证认证：

```bash
./build/dhb pair --host 127.0.0.1 --port 47821 --psk <pairing-key>
```

发现局域网设备：

```bash
./build/dhb discover
```

## HarmonyOS 侧

`ohos/` 是 HarmonyOS 工程：

- `compatibleSdkVersion=5.0.0(12)`
- `targetSdkVersion=6.0.0(20)`
- 已验证编译：DevEco Studio 26.0.0.851 / SDK 26.0.0.105

1. 用 DevEco Studio 打开 `ohos/`，等待工程同步。
2. 运行 `python3 scripts/gen-assets.py` 生成/刷新占位图标（仓库已带一份）。
3. 在 `Index` 页填 Deepin 主机 IP、端口、pairing key；连接后可发文件、收文件、同步文本剪贴板。
4. 真机/模拟器需要 `ohos.permission.INTERNET`；UDP 发现需要局域网广播权限。

CLI 构建（把路径换成你的 DevEco 安装目录）：

```powershell
$deveco = '<DevEco Studio>'
$env:NODE_PATH = "$deveco\tools\hvigor"
$env:JAVA_HOME = "$deveco\jbr"
Set-Location ohos
& "$deveco\tools\hvigor\bin\hvigorw.bat" assembleApp `
  --mode project -p product=default -p buildMode=debug --no-daemon
```

产物：

```text
ohos/entry/build/default/outputs/default/entry-default-unsigned.hap
```

**当前是 unsigned**，真机安装前需在 DevEco 配置自动签名。HAP 编译通过不等于“已可运行”。

## 仓库结构

```text
apps/dhb-cli/        Linux CLI：serve / send / pair / recv / discover / doctor / selftest
include/dhb/         公共头：协议、会话、传输、发现、加密
src/                 核心实现（纯 C++17，无 Qt 依赖）
tests/               加密向量、帧编解码、内存传输、端到端文件/剪贴板测试
ohos/                HarmonyOS ArkTS 客户端（DevEco 工程）
docs/                研究、架构、协议、路线图、验证、lab、建仓计划
scripts/             构建、KLink lab、OHOS 校验、图标生成
deepin/              Deepin 25 客户端计划与 D-Bus 接口（未实现/未编译）
patches/             KLink 4.x host bring-up patch（lab-only）
packaging/           deb / systemd 骨架（M1）
.github/             CI、issue/PR 模板
```

## 文档

- [研究方向与可行性判断](docs/01-research.md)
- [系统架构](docs/02-architecture.md)
- [DHB 线协议 v1](docs/03-protocol.md)
- [路线图与验收指标](docs/04-roadmap.md)
- [风险登记册](docs/05-risks.md)
- [KLink / dsoftbus 接入笔记](docs/06-klink-dsoftbus-notes.md)
- [外部评审交叉核对](docs/07-external-review-synthesis.md)
- [验证记录：已证明/未证明](docs/08-verification-log.md)
- [软总线版本 profile](docs/09-softbus-profiles.md)
- [认证优先的 MVP](docs/10-auth-first-mvp.md)
- [KLink → OH 6.x rebase 报告](docs/11-softbus-6x-rebase-report.md)
- [KLink 4.x Linux lab](docs/12-klink-linux-lab.md)
- [建仓计划（CI / 标签 / release）](docs/13-repo-plan.md)
- [AliOS 分布式软总线研究](docs/14-alios-softbus-notes.md)
- [Deepin 25 客户端计划（DTK6 / DDE tray / D-Bus / Linyaps）](deepin/README.md)
- [SKILL 研读：OH/KaihongOS 优先](docs/16-skill-study-openharmony-kaihong.md)
- [平台边界矩阵（华为鸿蒙 / 纯血鸿蒙 PC / HarmonyOS Connect / KaihongOS / OHOS / 国产 Linux）](docs/17-platform-taxonomy.md)
- [变更记录](CHANGELOG.md)
- ADR：[dsoftbus 谱系](docs/adr/0001-dsoftbus-base.md) · [两档能力](docs/adr/0002-two-tier.md) · [二进制帧](docs/adr/0003-wire-protocol.md) · [C++/ArkTS](docs/adr/0004-cpp-arkts.md) · [4.x/6.x 双 profile](docs/adr/0005-softbus-profiles.md)

## 安全

- 默认要求 32 字节 PSK（64 hex）；PSK 不直接上线，走 HMAC 挑战应答。
- 文件落盘前做路径净化（拒绝 `..`、绝对路径、控制字符），先写 `.part`，SHA-256 校验后再原子重命名。
- 当前 TCP **明文**，只用于开发/可信局域网；生产版必须 TLS 1.3（M1）。
- KLink/dsoftbus 的 HiChain/session 保护是设计目标；当前 lab 只验证到发现和 CLOUD_SN 放行，**不能当生产安全方案**。
- 漏洞报告见 [SECURITY.md](SECURITY.md)。

## 参与

DHB 现在是“可运行骨架 + 明确路线图”，最需要：

1. **KLink 4.x ↔ OH 4.1/HarmonyOS 4.x**、**OH 6.0 ↔ KaihongOS 6.x** 两条认证链路的真机日志；
2. Wayland/PipeWire 采集与 `uinput` 回控实测；
3. HarmonyOS NEXT 三方应用权限边界验证；
4. deb/DDE 集成与 UOS 兼容性测试。

提交前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。目前建仓/CI/发布计划见 [docs/13-repo-plan.md](docs/13-repo-plan.md)。

## 命名与商标

**DHB** 是产品名；仓库名中的 **Deepin / HarmonyOS** 仅描述兼容目标，不表示商标授权或与统信/华为的官方关联。HAP bundle 使用 `org.dhb.bridge`；deb 包计划拆为 `dhb-busd`（服务）与 `dhb-gui`（界面）。

`kylin-connectivity` 是 GPL-3.0：只作为架构参考，不复制/链接进本 Apache-2.0 项目。

## License

Apache-2.0。第三方组件与商标声明见 [NOTICE](NOTICE)。
