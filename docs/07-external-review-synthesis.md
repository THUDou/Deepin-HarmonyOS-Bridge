# 外部评审交叉核对（2026-10-27）

> 输入：两份外部研究材料（子智能体产出，未纳入本仓库）：
> - **材料 A**：`PROJECT-CHARTER.md`（openEuler 主基座 + Miracast 接收端 + DDE）
> - **材料 B**：`DHB-项目技术方案.md`（KLink 主基座 + DHB Proto 双栈 + T2 零安装）
>
> 约束：本文件只记录交叉核对后的判断。**不改写 `docs/01`–`docs/06` 的研究结论，也不改变既有技术主线。**

## 0. 一句话结论

两份材料没有推翻主线，反而补了三个洞：**KLink 的桌面成熟度证据、T2 零安装投屏路径、命名/许可风险**。

我的主线保持不变：

> **KLink（Linux 宿主，Apache-2.0）+ OHOS 原生 dsoftbus（Pro）+ DHB 自有协议（跨传输统一会话）；Core/Pro 两档；文件/剪贴板先跑通；屏幕先做 Linux→鸿蒙 App 级副屏，再做鸿蒙→Linux 镜像回控。**

只调整三处**表述与范围**：

1. Core 的设备发现不再把 dsoftbus 当作默认能力，改为 **UDP/手动配对为主，dsoftbus 仅系统档/待验证**；
2. **应用接续移出近中期范围**；
3. 新增 **T2 零安装接收**为 P2 研究项，不进入 MVP。

---

## 1. 三方主线对照

| 维度 | 我的主线（已落地 v0.1） | 材料 A | 材料 B | 裁决 |
|---|---|---|---|---|
| Linux 基座 | **KLink**（dsoftbus 的 Linux 分支） | openEuler dsoftbus 为主，KLink 只借认证 | **KLink** 为主，openEuler 作上游 | **保持 KLink 主基座**；openEuler 作上游/CI |
| OHOS 侧 | 系统签名走原生 dsoftbus；零售走 TCP/TLS | 系统 dsoftbus | 系统 dsoftbus + NAPI | 一致 |
| 屏幕方向 | Linux→鸿蒙 **App 级副屏**；鸿蒙→Linux 镜像（Pro） | 只做鸿蒙→Linux（Miracast 被投）；不做 Linux→鸿蒙 | T2 零安装接收 + 自有视频流 | **保持双向**，但把“系统 Cast+ Source”明确排除 |
| 自有协议 | 二进制帧 + TCP/TLS + SHA-256 + 256 KiB 块 | 以 dsoftbus 为主，DHB 协议兜底 | QUIC + SPAKE2 + BLAKE3 + 1 MiB 块 | **v1 不改**；v2 再评估 QUIC/PAKE |
| 产品分层 | Core / Pro | 一个产品三层部署 | T1/T2/T2+ 生态阶梯 | 兼容：Core≈T2+，Pro≈T1；T2 单列 P2 |
| 应用接续 | 此前写成“可选” | 明确不做 | 未强调 | **移出近中期** |
| 命名 | `org.dhb.bridge`，deb `dhb` | 警告 Deepin/HarmonyOS 商标；拆 `dhb-busd`/`dhb-gui` | D-Bus 命名空间 | 采纳商标提示与后续拆包 |

---

## 2. 共识（可以直接执行）

1. **基座同源**：KLink 是 OpenHarmony dsoftbus 的 Linux 分支，openEuler 是同一套代码的发行版打包；不是两套可并列的底座。
2. **openEuler 不适合做桌面主基座**：依赖 binder / isula，IPC/SN 是样例，CoAP-only、摸不到 BLE；适合做上游协议特性与 CI 范式。
3. **KaihongOS 是 OHOS 侧首选 T1 对端**：官方演示过与 HarmonyOS Next 的跨生态互联；x86 桌面版可做开发验证。
4. **零售 HarmonyOS 的系统级协同不向三方开放**：华为 6.0 文档明确“多端协同任务管理能力尚未具备……不支持三方应用接入”，示例里 `createDeviceManager` 也标注为系统 API。
5. **文件/剪贴板先跑通**；不伪装华为设备、不做私有协议逆向、不做跨公网穿透。
6. **D-Bus + DDE + ArkTS HAP** 是桌面产品的正确外壳；`org.dhb.bridge` 作为包标识。
7. **安全终态**是 TLS 1.3 + PAKE/TOFU；v0.1 的 PSK/HMAC 只是开发期形态。

---

## 3. 关键分歧与裁决

### 3.1 主基座：材料 A 说 openEuler，材料 B 说 KLink，我选 KLink

材料 A 的“一主一参”在**桌面产品**语境下自相矛盾：它自己承认 openEuler 需要 binder/isula、IPC/SN 是样例、只有 CoAP、摸不到 BLE。对 Deepin/UOS 这种“普通用户 apt 安装”的产品，这些是硬伤。

KLink 的优势是材料 A 也承认的：

- `cmake -Dhost_system=ON` 的 **非 binder** Linux 宿主构建；
- 完整的 PIN 认证 UI 钩子（展示/输入/取消/重试/信任策略）；
- `/opt/dsoftbus` 的部署形态，天然适合 systemd；
- 同源的 `kylin-connectivity` 桌面栈（D-Bus、discovery、projection、filetransfer、pcscreenmanage），可作为 **架构参考**；
- Gitee 页面显示 568 次提交、71 个标签，仍有持续打包分支；“4.x 已停更”的判断至少需要复核。

**但 KLink 的 4.x 协议代差仍是第一风险**。裁决：

- KLink 做 Linux 宿主与认证/会话接入；
- openEuler 做上游协议特性、CI 与打包参考；
- M0 必须验证 KLink ↔ KaihongOS 5.x；失败则评估 rebase 到 OpenHarmony 5.x，或 Pro 档先退回 DHB 协议。

### 3.2 屏幕：材料 A 说“不做 Linux→鸿蒙”，我保留 App 级副屏

材料 A 的“不做”指的是 **Cast+ Source / 系统投屏入口 / 超级终端设备列表**——这些确实要华为资质或系统权限，不该做。

但 DHB Core 的“Deepin→鸿蒙副屏”是另一件事：

- Linux 采集自己的屏幕（合法、无需手机权限）；
- 编码后通过 DHB 会话发给鸿蒙上的 **DHB 自己的 HAP**；
- HAP 在自己的 `XComponent` 里显示，触控回传 Linux。

这不依赖 Cast+ Source，也不冒充系统投屏，更不进“超级终端”。所以**保留**，但把边界写死：不做系统级 Cast+ 发起、不做“超级终端”入列、不做设备伪装。

材料 B 的 T2（手机系统“无线投屏”直接发现 Deepin）是**反方向**的零安装路径，和 Core 不冲突；见 3.4。

### 3.3 协议：材料 B 的 QUIC/SPAKE2/BLAKE3 很好，但 v1 不改

材料 B 的 Stack B 设计更“终态”：QUIC 多路复用、SPAKE2、Ed25519、X25519、ChaCha20-Poly1305、BLAKE3、1 MiB 块。方向正确，但不适合现在改：

- ArkTS 侧 QUIC 支持与跨语言实现成本高；
- SPAKE2/Ed25519/ChaCha20 在 C++ 与 ArkTS 双端都要有经过审计的实现；
- 当前二进制帧 + TCP + SHA-256 + 256 KiB 已经跑通测试，先换协议会把 MVP 拖回底座；
- 安全短板（明文 TCP）应该用 **TLS 1.3** 先补，而不是同时换传输/加密/哈希。

裁决：**v1 保持**；M1 上 TLS 1.3；v2 评估 QUIC/SPAKE2/BLAKE3。块大小 256 KiB 保持不变（控制帧延迟更低），可配置。

### 3.4 T2 零安装：这是两份材料补上的最大盲点，但只做 P2

材料 B 指出：商业 HarmonyOS 可以走系统“无线投屏”发现接收端，手机零安装。KaihongOS 官方 2024-06 的演示也证明：HarmonyOS Next 手机打开“无线投屏”能发现 KaihongOS 会议中控台并组网。

这确实补上了我此前研究的一个盲点：**零售鸿蒙的“手机→Linux”镜像不一定非要装 App**。但要注意：

- 演示的对端是 **KaihongOS 设备**，不是通用 Linux；不能直接推导 Deepin 也能被 Next 发现；
- Miracast sink 在 Linux 上要接管 NetworkManager/wpa_supplicant，可能断网；miraclecast 是 GPL，Wayland 兼容性有坑；
- Cast+ 接收端要华为 SDK 审批；DLNA 只适合媒体，不是完整镜像；
- 这条路径与 dsoftbus 认证无关，属于“标准投屏接收端”产品线。

裁决：**列为 P2 研究项，不进入 MVP**。M1 之后用 1 周做 spike：miraclecast sink 在 Deepin 25 上能否被 HarmonyOS Next 发现；不能就放弃，不阻塞主线。

### 3.5 应用接续：移出近中期

材料 A 明确“不做”，理由是跨系统应用签名/分布式任务调度无解。我的原 README 写了“公开 API 可用时 ✅”，过于乐观。裁决：**Core/Pro 近中期都不承诺应用接续**；如果未来平台开放，再作为独立 ADR。

### 3.6 命名与许可：采纳材料 A 的提醒

- 产品名用 **DHB**；`Deepin-HarmonyOS-Bridge` 只作描述性仓库名，不作商标/官方关联。
- HAP bundle 已经是 `org.dhb.bridge`（此前已避开 `org.deepin.*`）。
- deb 目前是 `dhb`；M1 拆为 `dhb-busd`（服务）与 `dhb-gui`（界面）时再改。
- **`kylin-connectivity` 是 GPL-3.0**：只能读架构、不能复制/链接进 Apache-2.0 的 DHB。这一点材料 B 没有强调，必须写进贡献规范。
- KLink 是 Apache-2.0，可放心作为基座；保留 NOTICE。

---

## 4. 外部材料补上的证据/盲点

| 证据 | 来源 | 对 DHB 的意义 |
|---|---|---|
| 华为 6.0 文档：多端协同“只能通过开发系统应用获取设备列表，不支持三方应用接入”；`createDeviceManager` 标注系统 API | [OpenHarmony 6.0 多端协同](https://raw.gitcode.com/openharmony/docs/raw/OpenHarmony-6.0-Release/zh-cn/application-dev/application-models/hop-multi-device-collaboration.md) | **修正**我此前对 Core“dsoftbus 发现/PIN”的乐观表述；Core 默认走 UDP/手动配对 |
| KaihongOS 与 HarmonyOS Next 跨生态互联：手机“无线投屏”发现 KaihongOS 会议中控台并组网 | [KaihongOS 新闻](https://www.kaihong.com/stgj/527.html) | T2 零安装路径的强证据；但仅对 KaihongOS，不外推 Linux |
| `kylin-connectivity` 桌面栈：dbus/discovery/projection/filetransfer/pcscreenmanage/ui，**GPL-3.0** | [仓库](https://gitee.com/openkylin/kylin-connectivity) | 架构参考价值高；**代码不能进 Apache-2.0 项目** |
| KLink 仓库仍有 568 次提交、71 个标签、持续打包分支 | [Klink](https://gitee.com/openkylin/klink) | “4.x 已停更”需复核；但 4.x↔5.x 代差仍是事实 |
| openEuler 3.2 起组件化启动、依赖 binder/isula、CoAP-only、3.1/3.2 不互通 | [openEuler 软总线](https://pages.openeuler.openatom.cn/embedded/docs/build/html/openEuler-25.03/features/distributed_softbus.html)（材料 B 引用） | 支持“openEuler 不作桌面主基座”的判断 |
| KLink 产品级验证：星光麒麟 1.0 的手机投屏/反控/文件互传 | 材料 A/B 转述，待产品文档复核 | 若属实，KLink 的桌面成熟度高于 openEuler；仍不能替代 M0 真机验证 |

---

## 5. 采纳/不采纳清单

| 建议 | 采纳？ | 落地方式 |
|---|---|---|
| KLink 主基座、openEuler 上游 | ✅ | 保持现有 ADR-0001；M0 验证 |
| 设备伪装/逆向华为 | ❌ | 继续禁止 |
| 应用接续 | ❌（近中期） | README 改为 P2 评估 |
| Linux→鸿蒙 App 级副屏 | ✅ | 保留 Core；排除 Cast+ Source |
| 手机→Linux Miracast/Cast+ 接收 | ⏸ P2 | M1 后 1 周 spike，失败即弃 |
| QUIC/SPAKE2/BLAKE3 | ⏸ v2 | 先 TLS 1.3；协议 v1 不改 |
| D-Bus 接口面 | ✅ | M1 daemon 按 `org.dhb.Device/File/Clip/Input/Screen/Pair` 设计 |
| `dhb-busd` / `dhb-gui` 拆包 | ⏸ M1 | 当前 `dhb` 单包，先不折腾 |
| 商标/包名规范 | ✅ | README 加“命名与商标”说明 |
| kylin-connectivity 代码 | ❌ | GPL-3.0，只读架构，不复制 |

---

## 6. 对 M0/M1 的具体修改

**M0（不变，但加三项检查）**

1. KLink 在 Deepin 25 上 `host_system=ON` 构建（确认无 binder 依赖）。
2. KLink ↔ KaihongOS 5.x：发现、PIN、session、bytes 四步日志。
3. 核对 `distributedDeviceManager.createDeviceManager` 在目标 HarmonyOS 版本上是否真的对三方开放；不通过就把 Core 发现固定为 UDP/手动。
4. Deepin 25：PipeWire 抓帧 + uinput 注入各一个 demo。

**M1（范围收紧）**

- Core 不做应用接续；不做 dsoftbus 发现承诺。
- daemon 按 D-Bus 接口面设计，但先实现 Device/File/Clip/Pair 四组。
- TLS 1.3 优先于任何协议升级。
- 商标/包名说明进 README。

**P2（不排期，先记录）**

- miraclecast/Cast+ 接收端 spike（1 周，验收：HarmonyOS Next 能发现 Deepin；退出后网络 100% 恢复）。
- mDNS `_dhb._tcp` 与 multicast 发现（替换/补充 UDP 广播）。
- v2 协议评估（QUIC/SPAKE2/BLAKE3）。

---

## 7. 复审触发条件

出现以下任一情况，重新评估主线：

1. KLink 在 Deepin 25 无法构建，或 M0 认证/会话无法互通，且 2 人周内无法修复；
2. 华为/OpenHarmony 关闭 KaihongOS 与 Next 的跨生态组网；
3. 零售 HarmonyOS 对 `distributedDeviceManager` 的三方限制进一步收紧（Core 失去发现能力）；
4. Miracast/Cast+ 接收端在 Deepin 25 上被验证为稳定可用（T2 可升级为 P1）；
5. KLink 上游停止维护超过 6 个月且无社区接手。

---

## 8. 结论

**主线不变，边界更清楚。** 两份材料最大的价值不是让我换基座，而是：

- 用华为官方文档把“零售 NEXT 三方协同”的幻想彻底关掉；
- 用 KaihongOS 的跨生态演示打开了“T2 零安装接收”这条侧路；
- 用 `kylin-connectivity` 提醒我们：桌面集成有现成架构可学，但许可证是 GPL，不能抄代码；
- 用材料 A 的商标提醒，让我们在起名字和发包时少踩一个坑。

下一步仍然是 M0：**KLink 在 Deepin 25 上编出来，并和 KaihongOS 5.x 说上话。** 这比再写十页架构更重要。
