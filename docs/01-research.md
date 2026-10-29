# DHB 方向研究与可行性判断

> 调研时间：2026-10-27 · 范围：OpenHarmony / KaihongOS / HarmonyOS NEXT、Deepin / UOS、dsoftbus / Klink、屏幕协同与文件流转
> 方法：公开文档、源码仓库、发行版公告、API 声明文件交叉验证；区分"已证实事实"与"工程推断"。

## 0. 一句话结论

**方向成立，而且窗口期很好；但不能把"零售 HarmonyOS 手机镜像到 Deepin"当作 MVP 承诺。**

正确的打法是两条产品线：

1. **DHB Core**：零售 HarmonyOS NEXT 三方应用 + Deepin/UOS。做**发现/配对、双向文件流转、剪贴板、通知、以及"Deepin 作为主机、鸿蒙设备作为无线副屏 + 触控板"**。这条线今天就能开发、能上架、能验证。
2. **DHB Pro**：KaihongOS / OpenHarmony 系统签名应用或 SA。做**鸿蒙→Linux 镜像回控、hmdfs 文件视图、分布式相机/麦克风、应用接续**。这条线是护城河，但需要系统权限/厂商合作。

底座用 **Klink（Linux 侧）+ OpenHarmony dsoftbus（鸿蒙侧）** 是对的；但 Klink 目前公开说明兼容到**鸿蒙软总线 4.x**，而 KaihongOS 5.x 基于 OpenHarmony 5.x，**第一周必须做协议/认证互通 spike**，这是 Go/No-Go  gate。

---

## 1. 事实基线（可引用的硬信息）

| # | 事实 | 证据 | 对 DHB 的含义 |
|---|---|---|---|
| 1 | 麒麟软件 2024-06 在 openKylin 开源 **Klink**，Apache-2.0，定位"通用 Linux 端间互联组件"，明确"兼容鸿蒙软总线 4.x 协议"，提供发现、组网、HiChain 认证、字节/消息/流/文件传输 SDK | [Klink 仓库](https://gitee.com/openkylin/klink)、[Klink README](https://gitee.com/openkylin/klink/blob/upstream/README.md)、[openKylin 新闻](https://www.openkylin.top/news/3423-cn.html)、[麒麟新闻](https://www.kylinos.cn/about/news/1943512226291322881.html) | Linux 侧不必从零实现 dsoftbus；Klink 的 `SendFile`/`SendStream`/`SendBytes` 正好覆盖 DHB 的文件与屏幕通道 |
| 2 | openEuler 22.03 LTS SP4 提供 `dsoftbus` 软件包（`dnf install dsoftbus`），含 `softbus_server_main`、`/usr/include/dsoftbus/`、`libsoftbus_client.z`；文档给出与 OpenHarmony 3.1 RK3568 的发现、HiChain PIN 认证、字节传输完整 demo | [openEuler 文档](https://docs.openeuler.openatom.cn/zh/docs/22.03_LTS_SP4/server/development/distributed/expanding_the_ecosystem_through_distributed_soft_bus.html)、[dsoftbus.spec](https://atomgit.com/src-openeuler/dsoftbus/blob/master/dsoftbus.spec) | "Linux 作为 dsoftbus 对端"不是理论，已有发行版级验证；但 openEuler 包偏嵌入式，Deepin 需要 Klink 或自行打包 |
| 3 | OpenHarmony `distributed_screen` 是 Apache-2.0 的屏幕虚拟化组件，含 Source/Sink、`SoftbusAdapter`、`ScreenTransport`、编解码与代理显示客户端；`distributed_hardware_fwk` 统一管理虚拟屏幕 | [distributed_screen README](https://atomgit.com/openharmony/distributed_screen) | "屏幕协同"在 OpenHarmony 里有官方系统级实现可参考；Linux sink 是空白，也是 DHB Pro 的机会 |
| 4 | OpenHarmony `hmdfs` 通过 dsoftbus 组网，提供跨设备文件访问；`distributedfile_daemon` 监听设备上下线，应用可用 `ohos.file.fs` 统一访问本地/分布式文件 | [Distributed FS Overview](https://raw.gitcode.com/openharmony/docs/raw/weekly_20250707/en/application-dev/file-management/distributed-fs-overview.md) | Pro 档"文件流转"可以不止于传文件，而是挂载式文件视图；MVP 先用应用层分块传输更稳 |
| 5 | `@ohos.distributedDeviceManager` 的 `startDiscovering` / `bindTarget` / `unbindTarget` 是**公开 API**（要求 `ohos.permission.DISTRIBUTED_DATASYNC`）；`bindType=1` 表示 PIN 认证，系统弹窗完成绑定 | [API 声明](https://raw.gitcode.com/openharmony/interface_sdk-js/raw/master/api/@ohos.distributedDeviceManager.d.ts) | 零售 NEXT 三方应用**可以**做 dsoftbus 发现与 PIN 绑定；但绑定后没有公开的任意字节 socket API，需要自建 TCP/TLS 通道 |
| 6 | KaihongOS 桌面版（x86）已发布 **V5.0.2.01**（2026-04），开放 ISO 制作与安装工具源码，预置 Docker，支持部分 Intel 声卡 | [KaihongOS 公告](https://www.kaihong.com/cpxg/1004.html) | 没有 ARM 开发板也能做系统级验证：x86 笔记本/虚拟机装 KaihongOS，另一台装 Deepin 25 |
| 7 | KaihongOS 标准系统以 OpenHarmony 为底座，含分布式软总线、分布式数据、分布式任务调度、多模输入等子系统；系统应用与三方应用有 ACL 权限差异 | [KaihongOS 架构文章](https://bbs.elecfans.com/jishu_2486219_1_1.html)、[KaihongOS x86 ACL 文章](http://mp.weixin.qq.com/s?__biz=Mzk2ODA2Mjk5NQ==&mid=2247484781&idx=1&sn=d62b0472af31fb194e9d4ee1a71159fb) | Pro 档的能力（录屏、注入、hmdfs）需要系统签名/ACL；这是合作与交付门槛，不是纯代码问题 |
| 8 | Deepin 25.2.2（2026-09）以 **Treeland**（Wayland 合成器，wlroots 0.20）为主力，支持截图/录屏、pointer-constraints 等 | [Deepin 25.2.2](https://www.deepin.org.cn/zh/deepin-25-2-2-update/)、[Deepin 技术双周报](https://www.deepin.org.cn/zh/deepin-technical-bi-weekly-issue-6/) | Linux 侧屏幕采集要走 xdg-desktop-portal/PipeWire；输入注入不能依赖 XTest，需要 uinput + polkit |
| 9 | Deepin 社区长期有"能否与手机多屏协同"的需求；现有 UOS 助手被用户评价为"不好用"；有社区成员指出第三方应用拿不到手机高权限 | [Deepin 论坛帖](https://bbs.deepin.org.cn/post/264734) | 需求真实；但"手机镜像"的零售三方路线确实受限，印证两条产品线判断 |
| 10 | 华为"多屏协同"主要覆盖 Windows PC（华为电脑管家），Linux 没有官方版本 | 华为 PC 管家产品页/公开资料 | Deepin/UOS 是明确的空白；但不要低估华为生态的锁定与协议封闭性 |

> 说明：第 10 条是产品层面的公开事实归纳，不是对华为内部路线图的判断。

**一个重要澄清**：不要把 Klink 和 dsoftbus 当成两套并列的基座。Klink 的 README 明确写的是 "fork from openharmony dsoftbus"，它是同一套 dsoftbus 代码的 Linux 适配/发行分支；openEuler 的 `dsoftbus` 包也是同一套代码在 openEuler 的打包。DHB 的实际结构是：**Linux 端复用 Klink 的 Linux 适配，OHOS 端用原生 dsoftbus，中间用 DHB 协议衔接**。这比"同时集成两个基座"简单得多，也避免了两套发现/认证/会话状态机。

---

## 2. 能力边界：三方应用 vs 系统应用

这是整个项目最需要说清楚的一张表。**不要把系统能力当成应用能力卖。**

| 能力 | 零售 HarmonyOS NEXT 三方应用 | KaihongOS/OpenHarmony 系统应用/SA | Deepin/UOS |
|---|---|---|---|
| dsoftbus 设备发现 | ✅ `startDiscovering`（公开） | ✅ | ✅ Klink |
| PIN 绑定/认证 | ✅ `bindTarget(bindType=1)`（公开） | ✅ HiChain | ✅ Klink `AuthenticateDevice` |
| 任意字节通道 | ❌ 无公开 dsoftbus socket API；需自建 TCP/TLS | ✅ `CreateSessionServer`/`OpenSession`/`SendBytes` | ✅ Klink session |
| 整机屏幕采集 | ⚠️ 无公开证据；按系统能力处理 | ✅ `avScreenCapture` + `CAPTURE_SCREEN` | ✅ PipeWire/X11 |
| 向其他应用注入输入 | ❌ 无公开证据 | ✅ 多模输入注入 | ✅ uinput |
| 应用内画面/触控 | ✅（自己的 XComponent/UI） | ✅ | ✅ |
| 文件选择/分享 | ✅ picker / share | ✅ | ✅ |
| 后台持续连接 | ⚠️ 受后台/长时任务限制 | ✅ 系统服务 | ✅ systemd user service |
| 剪贴板 | ⚠️ 前台受限；后台读取受限 | ✅ | ✅ |
| 通知 | ✅ 发布；❌ 读取其他应用通知 | ✅ 订阅 | ✅ D-Bus |
| 分布式文件 hmdfs | ❌ | ✅ | ⚠️ 需移植客户端 |
| 分布式相机/麦克风 | ❌ | ✅ | ⚠️ 需虚拟设备 |

**结论**：

- **Core 档**能做的最大差异化不是"镜像手机"，而是"**把鸿蒙设备变成 Deepin 的副屏/触控板/文件口袋**"。这个方向在零售 NEXT 上完全合法可用，且和"手机镜像到 PC"是互补而非替代。
- **Pro 档**才是"鸿蒙→Linux 镜像回控 + hmdfs + 分布式硬件"，必须走 KaihongOS 系统签名或厂商合作。

---

## 3. 竞品与参考项目

| 项目/产品 | 定位 | 对 DHB 的价值 | 不直接用它的原因 |
|---|---|---|---|
| 华为多屏协同 | Windows/华为生态闭环 | 体验标杆：靠近发现、一碰传、拖拽、副屏 | 协议封闭、系统级绑定、无 Linux |
| UOS 助手 | 统信官方跨端尝试 | 证明需求存在；可对比功能缺口 | 社区反馈体验一般；闭源/生态窄 |
| Klink | Linux 端 dsoftbus 兼容层 | **Linux 侧基座**：发现/认证/文件/流 | 不是桌面产品；4.x 协议；无 UI/屏幕管线 |
| openEuler dsoftbus | 发行版 dsoftbus 包 | 验证可行性、HiChain/softbus 参考 | 嵌入式取向；RPM；无桌面集成 |
| OpenHarmony distributed_screen | 系统级屏幕虚拟化 | Pro 档协议/管线参考；长期可做 native sink | SA 框架重；依赖 system Ability，不适合 MVP |
| hmdfs | 分布式文件系统 | Pro 档文件视图 | 内核模块 + daemon；跨 Linux 移植成本高 |
| KDE Connect / GSConnect | 跨设备协同（剪贴板/通知/文件/遥控） | 功能模型、UX、插件化 | 无 dsoftbus，不能与鸿蒙原生组网 |
| scrcpy | Android 镜像/回控 | 采集-编码-传输-注入的工程范式 | Android ADB，不适用 OHOS |
| RustDesk | 跨平台远程桌面 | 低延迟管线、NAT/中继、编解码 | 通用远程桌面，不是生态级协同 |
| Sunshine/Moonlight | 低延迟串流 | 编码/时延调优 | 面向游戏串流 |
| castengine_wifi_display | OpenHarmony Wi-Fi Display | 纯投屏备选（Miracast/WiDi） | 只解决"看"，不解决文件/输入/生态 |
| ScreenCast (frankzhangv5) | Qt6 跨 Android/OpenHarmony 投屏 | 采集/编码参考 | 应用级、非 dsoftbus |

**空白点**：没有一个项目同时做到"**Deepin/UOS 桌面集成 + 鸿蒙 dsoftbus 谱系 + 文件/屏幕/输入一体 + 开源可审计**"。DHB 的机会在这里。

---

## 4. 技术路线比较

| 路线 | 描述 | 兼容性 | 工作量 | 零售可达 | 系统能力 | 结论 |
|---|---|---|---|---|---|---|
| A. Klink/dsoftbus 原生 | Linux 跑 Klink，鸿蒙走 dsoftbus session | 取决于协议版本/信任链 | 中 | 低（需系统权限） | 强 | **Pro 档主路线** |
| B. 应用层 TCP/TLS | 鸿蒙 app 自建 socket，dsoftbus 只做发现/绑定 | 高（跨版本稳） | 低-中 | 高 | 弱 | **Core 档主路线 / 兜底** |
| C. Miracast/WiDi | Linux 做标准投屏 sink | 中（设备支持不一） | 中 | 中 | 仅屏幕 | 备选，不做主路线 |
| D. 混合 | B 做 Core，A 做 Pro，同一 DHB 协议 | 高 | 高（但可渐进） | 高 | 强 | **推荐** |

**推荐 D**：协议层与传输层解耦，第一版只实现 B，A 作为 `ITransport` 的第二个实现接入；C 只在"只求看画面"的客户场景做 PoC。

---

## 5. 关键未知与验证计划（按优先级）

### U1. Klink 4.x 能否与 KaihongOS 5.x 的 dsoftbus 互通？

- **风险**：协议版本、设备认证（HiChain/信任环）、组网发现、session 名/权限。
- **验证**：两台 x86 机器/虚拟机，一台 KaihongOS 5.0.2.01，一台 Deepin 25 + Klink；用 Klink 的 `devicemanager` 发现、PIN 认证、`SendBytes` 发一条 "hello"。
- **通过标准**：发现 + 认证 + 双向字节；日志可复现。
- **失败预案**：把 Klink rebase 到 OpenHarmony 5.x `communication_dsoftbus`；或 Pro 档先走应用层 TCP。

### U2. 零售 HarmonyOS NEXT 是否接受 Linux 作为 dsoftbus 对端？

- **风险**：即使 `bindTarget` 公开，系统信任链/厂商白名单可能拒绝非鸿蒙设备。
- **验证**：HarmonyOS NEXT 真机装 DHB Core，调用 `startDiscovering`，看能否发现 Klink 设备并弹出 PIN；绑定后能否拿到 `networkId`。
- **通过标准**：PIN 弹窗 + 绑定成功。
- **失败预案**：Core 档退回"手动 IP/二维码 + TCP/TLS"，dsoftbus 只做可选优化。

### U3. 信任链与跨厂商认证

- **风险**：HiChain 的信任环/证书根、账号体系可能绑定华为/OpenHarmony 生态。
- **验证**：Klink 的 `AuthenticateDevice` 对 KaihongOS 的 PIN 流程；查看 `device_manager` 日志与错误码。
- **失败预案**：Pro 档与 KaihongOS 厂商合作预置信任；Core 档自建 PAKE/TLS。

### U4. Wayland 采集与输入注入

- **验证**：Deepin 25 Treeland 下 `xdg-desktop-portal` + PipeWire 抓屏；`uinput` 创建虚拟鼠标/键盘，polkit 提权；XWayland 回退。
- **通过标准**：1080p30 采集 < 15% CPU（i5-12 代），输入端到端 < 30ms。
- **失败预案**：X11 会话优先；或 GNOME/KDE portal 分支。

### U5. 编码与许可

- **验证**：VAAPI H.264 低延迟（baseline, no B-frame, VBV），对比 openh264 软编。
- **风险**：H.264 专利池、发行版编解码器合规。
- **预案**：默认硬件编码；软编可选；法务评估。

### U6. UOS 商业发行版

- **风险**：应用签名、安全中心、商店审核、预装合作。
- **验证**：UOS 25 试用镜像 + 开发者签名；与统信生态合作沟通。
- **预案**：先 Deepin 开源版，再 UOS 认证。

---

## 6. 为什么是现在

1. **生态叙事对齐**：openEuler 与 OpenHarmony 的"1+1>2"是官方叙事，dsoftbus 是明确的技术抓手；KaihongOS 把 OpenHarmony 推到 x86 桌面，正好覆盖"PC ↔ 终端"场景。
2. **底座成熟**：Klink 把 Linux 侧最难的发现/认证/会话做完了；OpenHarmony 的屏幕/文件组件开源可参考；不需要从零造协议。
3. **需求真空**：Deepin/UOS 用户没有官方鸿蒙协同；UOS 助手覆盖有限；华为不管 Linux。
4. **信创采购场景**：政企桌面替换中，"国产 PC + 国产手机/平板"的跨端协同是高频诉求，且愿意为"可控、可审计、可私有化"买单。
5. **时间窗口**：HarmonyOS NEXT 生态仍在扩张，OpenHarmony PC 生态刚起步；现在做"桥"，比等厂商做完再切入更有位置。

---

## 7. 明确不做（至少 MVP 不做）

- ❌ 不承诺"零售 HarmonyOS 手机完整镜像到 Deepin"——系统权限不在三方应用手里。
- ❌ 不做云中转、账号体系、远程公网穿透——MVP 只做局域网，减少隐私与合规面。
- ❌ 不复制/逆向华为私有"多屏协同"协议——法律与稳定性风险都高。
- ❌ 不做 Android 兼容层/模拟器——那是另一条产品线。
- ❌ 不做"支持所有 Linux 发行版"——先 Deepin 25 / UOS，再 openKylin/Ubuntu。
- ❌ 不在 MVP 阶段做 hmdfs 内核移植——应用层分块传输先跑通。

---

## 8. 结论与 Go/No-Go

**GO**，但设一个 2 周 M0 gate：

- ✅ Klink ↔ KaihongOS 5.x：发现 + PIN + 双向字节，至少一项跑通；
- ✅ 零售 NEXT：`startDiscovering` 能发现 Linux 设备并弹出绑定（若失败，Core 改手动配对）；
- ✅ Deepin 25：PipeWire 抓屏 + uinput 注入各有一个可运行 demo。

三项中至少两项通过 → 全速开发；只通过一项 → 收缩为"文件+剪贴板"工具；全部失败 → 暂停 Pro，转做纯应用层跨端工具。

**一句话**：这不是"再做一个投屏软件"，而是**在信创桌面和鸿蒙生态之间，补一层开放、可审计、可被集成的系统级互操作层**。方向对，节奏要稳，先把"文件流 + 副屏"做成刚需，再谈"镜像回控"。
