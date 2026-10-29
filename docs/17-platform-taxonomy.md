# 平台边界：华为鸿蒙、纯血鸿蒙 PC、HarmonyOS Connect、KaihongOS、OpenHarmony 与国产 Linux

> 目的：把“鸿蒙”拆成可执行的平台矩阵，避免把华为商业鸿蒙、OpenHarmony、
> KaihongOS、HarmonyOS Connect 和国产 Linux 混成同一个兼容性承诺。
>
> 结论先行：**DHB Core 只依赖标准 HAP/ArkTS + TCP；Pro 才碰系统级
> dsoftbus/超级终端；KaihongOS 按厂商发行版对待，不按 OHOS 原版假设；
> Linux 侧用一套 C++ CLI，DTK/DDE 只作为 Deepin/UOS 的可选 UI。**

## 1. 名词先分清

| 名称 | 是什么 | 不是什么 |
|---|---|---|
| **华为 HarmonyOS 4.x** | 华为商业鸿蒙的旧代，仍有兼容层/旧 API 负担 | 不是纯血 NEXT，不等于 OpenHarmony 4.x |
| **华为纯血鸿蒙（HarmonyOS NEXT / 5 / 6 / 7）** | 华为商业发行版，HAP/ArkTS，去除 AOSP 兼容层 | 不是 OpenHarmony 原版；API/SDK 由华为定义 |
| **鸿蒙电脑（HarmonyOS PC）** | 运行 HarmonyOS 5/6/7 的华为 PC，支持超级终端 | 不是 Windows/Linux PC；三方 HAP 不能当系统协同服务 |
| **HarmonyOS Connect / 鸿蒙智选** | 华为生态接入/认证品牌，覆盖 IoT 设备；所谓“智选 PC”也要先看它实际跑什么 OS | 不是一套统一 PC OS，也不是通用 Linux 主机 SDK；Connect 认证 ≠ HarmonyOS PC |
| **OpenHarmony / OHOS** | 开放原子开源基金会下的开源基座 | 不包含华为账号、AGC、华为云服务 |
| **KaihongOS** | 基于 OpenHarmony 的商业发行版，厂商会改内核/服务/签名/认证 | **不是 OHOS 原版**；不能假设 API、dsoftbus、签名与 OH 完全一致 |
| **国产 Linux** | Deepin/UOS、银河麒麟、OpenKylin、OpenAnolis、OpenEuler、中兴新支点等 | 不是同一个包格式/桌面栈；只能抽公共 CLI/daemon |

## 2. 平台矩阵

| 平台 | 应用形态 | DHB 可做的事 | 不能假设的事 | 优先级 |
|---|---|---|---|---|
| 华为 HarmonyOS 4.x | HAP/ArkTS（旧 API） | KLink 4.x Linux ↔ 4.x 的 legacy 认证/文件；HAP 需单独兼容构建 | 不能拿 API 12+ HAP 直接覆盖 4.0；不能假设与 OH 4.1 同源 | P2 |
| 纯血鸿蒙手机/平板 | HAP/ArkTS | DHB HAP + TCP：文件、剪贴板、发现/配对 | 三方 HAP 不能加入“超级终端”成为系统设备 | **P1** |
| 纯血鸿蒙 PC（HarmonyOS 5/6/7） | HAP/ArkTS；系统级超级终端 | 标准 HAP 文件/剪贴板；研究 PC 作为显示/接收端 | 不能调用系统“超级终端”发现/组网 API；不能整机录屏/注入输入 | **P0 研究** |
| HarmonyOS Connect 设备 / “智选 PC” | 认证设备/HiLink；PC 可能仍是 Windows/Linux/HarmonyOS | 只作为生态研究；先确认机型实际 OS | 没有公开 Linux 主机 SDK；Connect 认证不等于 HarmonyOS PC 的超级终端 | P3 |
| OpenHarmony 4.x | OH 系统/HAP | KLink 4.x Gate A（发现/PIN/可信列表） | 不能假设华为商业鸿蒙 4.x 与 OH 4.x 完全互通 | P1 |
| OpenHarmony 6.x | OH 系统/HAP/SA | dsoftbus 6.x + device_auth；Gate B | 不能拿 openEuler 3.2 代替 OH 6.0 | P1 |
| KaihongOS 5.x/6.x | 厂商系统/HAP/SA | Pro 档：系统签名 HAP/SA，走厂商 dsoftbus/认证 | 不能假设它是 OHOS 原版；不能直接用华为 AGC 签名 | P1（Pro） |
| Deepin / UOS | Linux 桌面 | CLI/daemon + DTK6/DDE tray/GUI + Linyaps/deb | 不是 RPM 系；DTK 只适用于 DDE | P0 |
| 银河麒麟 / OpenKylin | Linux 桌面（deb 系为主，需查文档） | CLI/daemon；可选 Qt/UKUI 集成 | 不能假设 DDE 插件可加载 | P1 |
| OpenAnolis / OpenEuler | Linux 服务器/桌面（rpm 系） | CLI/daemon + RPM/systemd | 不能假设 deb/Linyaps 可用 | P1 |
| 中兴新支点 NewStartOS | 国产 Linux（包格式/桌面以文档为准） | CLI/daemon；按文档做包 | 不下载镜像逐个试；先读开发文档 | P2 |

## 3. 纯血鸿蒙 PC + 超级终端：能做什么，不能做什么

### 3.1 官方事实

华为官方支持页明确：鸿蒙电脑可通过“超级终端”与手机/平板/智慧屏/耳机等协同；
电脑↔手机是多屏协同，电脑↔平板/智慧屏是无线投屏，电脑↔耳机/音箱是音频流转。
前置条件包括同一华为账号、WLAN/蓝牙（或星闪）等，且部分能力要求 HarmonyOS 6.1+。

来源：[华为支持：鸿蒙电脑如何通过超级终端实现多设备协同](https://consumer.huawei.com/cn/support/content/zh-cn16048446/)

这说明：

- **超级终端是华为系统级能力**，不是三方 HAP 的公开 API；
- PC 可以作为协同的发起端/接收端，但“被发现/被连接”由系统服务控制；
- 三方应用能稳定做的是**标准网络 + 文件/剪贴板**，以及应用内的投屏接收/解码（如果系统允许）。

### 3.2 DHB 的可执行路径

| 路径 | 可行性 | 说明 |
|---|---|---|
| HAP 文件/剪贴板 | ✅ | 标准 TCP + DHB；不依赖超级终端 |
| HAP 作为“显示接收端” | 🧪 研究 | 需要系统投屏/编码解码 API；三方权限待验证 |
| 加入超级终端设备列表 | ❌ 三方不可 | 需华为系统签名/系统服务；不是公开 HAP API |
| PC 作为 DHB 主机被手机发现 | 🟡 应用层 | 手机装 DHB HAP 可发现 PC；不等于系统“超级终端” |
| 模拟 Cast+/Miracast sink | 🧪 P2 | 协议/授权/DRM 限制多；不计入 MVP |

**关键判断**：不要把“纯血鸿蒙 PC 能接入超级终端”写成 DHB 已支持。  
DHB 能做的是：**在 PC 上装 HAP，用 DHB 自己的发现/传输；或者研究系统投屏接收端**。  
超级终端本身是华为生态的护城河，不是我们可以直接调用的 SDK。

### 3.3 需要调研的系统底层

1. 纯血鸿蒙 PC 是否对三方 HAP 开放 `distributedDeviceManager` / `dsoftbus` 的任意字节通道；
2. PC 的“无线投屏”接收端是否允许三方应用注册 Cast sink；
3. `AVCodec`/`XComponent` 在 PC 上的硬件解码与低延迟能力；
4. `ohos.permission.DISTRIBUTED_DATASYNC` 在 PC 上的实际授权行为；
5. 系统签名 HAP/SA 在鸿蒙 PC 上是否可安装（非华为应用市场渠道）。

没有这些证据前，Gate 只到 App/TCP，不把超级终端列为可交付能力。

## 4. KaihongOS：按“厂商发行版”对待

KaihongOS 不是 OpenHarmony 原版，也不能简单等同于华为 HarmonyOS。它至少可能在：

- 内核/BSP/驱动；
- dsoftbus/device_auth 的版本与补丁；
- 系统签名、权限模型、预装服务；
- HAP 运行时/API 级别；

上做厂商适配。因此：

- **Gate B 的基线仍是 OpenHarmony 6.0 源码**，KaihongOS 作为对端验证，不把 KaihongOS 私有改动当上游；
- KaihongOS 侧必须使用厂商系统签名/文档，不使用华为 AGC 证书；
- 不能假设 KaihongOS 6.x 与 OH 6.0 的 dsoftbus 二进制/协议完全一致，必须真机抓日志；
- KaihongOS 5.x/4.x 走 Pro/KLink legacy 的对应路线，不混用 6.x helper。

## 5. 国产 Linux：一套核心，多种包

### 5.1 共同点

- POSIX/C++17、TCP/UDP、文件系统、systemd；
- CLI/daemon 可无 GUI 运行；
- 可选 TLS 1.3、可选 D-Bus。

### 5.2 差异点

| 发行版 | 包格式 | 桌面/工具链 | DHB 策略 |
|---|---|---|---|
| Deepin / UOS | deb | DDE/DTK6、Linyaps | 主目标；CLI + DTK tray/GUI + deb/Linyaps |
| 银河麒麟 | deb（以文档为准） | UKUI/Qt | CLI/daemon；可选 Qt 托盘，不假设 DDE |
| OpenKylin | deb（以文档为准） | UKUI/Qt | CLI/daemon；可选 Qt |
| OpenAnolis | rpm | GNOME/服务器 | CLI/daemon + RPM/systemd |
| OpenEuler | rpm | 服务器/桌面 | CLI/daemon + RPM/systemd；dsoftbus 公开包是 3.2，仅参考 |
| 中兴新支点 | 以文档为准 | 国产桌面 | CLI/daemon；先读开发文档，不逐个下镜像 |

**原则**：

- `dhb` 核心不依赖 Qt/DTK；GUI 是可选层；
- Deepin/UOS 用 DTK6/DDE 插件；其他发行版不强行套 DDE；
- 包格式用 deb/rpm 两套适配，Linyaps 只作为 Deepin/UOS 的分发选项；
- 不要求每个发行版都下载镜像；以开发文档 + 一个 Deepin 实机/容器为准，其余做 CI/打包检查。

## 6. 对当前代码/文档的影响

- HAP：`compatibleSdkVersion=5.0.0(12)`、`targetSdkVersion=6.0.0(20)`，面向纯血鸿蒙 5/6 更合适；**HarmonyOS 4.x 需要单独兼容构建**，不能直接复用。
- `deveco-cli`/DevEco 只用于 HAP 构建/设备/日志，不用于 OH/KaihongOS 系统层。
- KLink 4.x 只对 legacy 4.x；OH 6.x 用 OpenHarmony 6.0 源码；KaihongOS 是厂商对端，不是基线。
- 超级终端是**S3 研究项**，不是 Gate A/B，也不是 Core 承诺。
- HarmonyOS Connect 是生态认证/IoT 路线，不是 Linux 主机 SDK。

## 7. 还有哪些证据要补

1. 纯血鸿蒙 PC 的三方 HAP 权限/投屏 API 官方文档；
2. KaihongOS 6.x 的 dsoftbus/device_auth 版本与签名文档；
3. 银河麒麟/OpenKylin/OpenAnolis/OpenEuler/NewStartOS 的包格式与桌面栈文档；
4. 华为 HarmonyOS 4.x 与 5/6 的 HAP API 兼容矩阵。

在这些证据补齐前，文档只写“路径/待验证”，不写“已兼容”。
