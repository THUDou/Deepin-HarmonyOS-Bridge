# DHB 最终目标与总路线（Goal Mode）

> 本文是 DHB 的目标定义、最终成品状态、组件边界和分阶段验收。
> 后续每轮以本文为基线推进；每轮必须有可验证产出，不写“纯计划”收尾。
> 遇到许可证、真机、签名、内核/权限等阻塞时暂停询问，不猜。

## 1. 项目简介

**DHB = Deepin-HarmonyOS-Bridge**。

它不是一个替代 dsoftbus 的新总线，也不是一个只会在命令行传文件的 demo，而是：

> **在 Deepin/UOS 与 KaihongOS/OpenHarmony/HarmonyOS 设备之间，建立“发现 → 信任 → 协同”的跨生态组件桥。**

核心主阵营：

```text
Deepin/UOS x86  ↔  KaihongOS x86
```

随后扩展：

```text
华为 HarmonyOS PC / 2in1（ARM）
OpenHarmony 开发者手机 / 平板
HarmonyOS 手机 / 华为智选设备
其他国产 Linux（openEuler/星光麒麟等）
```

DHB 的差异化：

- Linux 原生（D-Bus/DTK/DDE/Linyaps），不是 Electron 套壳；
- KaihongOS/OpenHarmony 标准 HAP；
- 自有 DHB 协议作为 **跨生态 fallback**；
- 同时适配 SoftBus 6.x 和 KLink 4.x，而不是假设所有鸿蒙设备同一个协议；
- 先文件/剪贴板/信任，再屏幕/键鼠，不把 KVM 当 MVP。

## 2. 最终成品状态（Definition of Done）

DHB 1.0 达到以下状态才算“功能交付”：

### 2.1 用户可见能力

- [ ] Deepin/UOS 与 KaihongOS 在同一局域网自动发现；
- [ ] 首次配对支持 PIN/二维码/确认码，不需要手填 PSK；
- [ ] 可信设备长期保存，可查看/撤销；
- [ ] 双向文件发送、接收目录、进度、断点续传、SHA-256 校验；
- [ ] 双向文本剪贴板（权限允许时）；图片/文件拖拽；
- [ ] 屏幕共享：至少一条可用链路（查看/控制分开标注）；
- [ ] 键鼠跨屏：鼠标跨屏、键盘跟随、修饰键/中文输入不丢；
- [ ] 可选音频/摄像头/麦克风流转（不作为 1.0 阻塞项）；
- [ ] 断线重连、失败重试、清晰错误提示；
- [ ] HAP、DDE/DTK 前端、tray/控制中心入口一致。

### 2.2 技术底座

- [ ] **SoftBus 6.x**：OpenHarmony 6.0+ dsoftbus 在 Linux host 上可编译、可启动、可 CoAP 发现；
- [ ] **openEuler 3.2 → 6.x**：完成 openEuler 打包 rebase，产出可构建的 RPM/容器验证；
- [ ] **KLink 4.x**：作为独立 4.x legacy 栈保留，与华为 4.x 设备互通；
- [ ] **DHB 协议**：自有 TCP + PSK/TLS fallback，不依赖 SoftBus 也能工作；
- [ ] **D-Bus/FDBus/AF_BUS**：Linux 侧服务、跨进程、低层总线适配层边界清晰；
- [ ] **dde-cooperation**：官方 DDE 协作项目接入，不重复造它的设备/文件/剪贴板入口；
- [ ] **HarmonyOS Connect / 厂商适配**：以可选 adapter 形式存在，闭源组件不伪造开源实现；
- [ ] TLS 1.3/Noise、可信设备状态、审计与撤销；
- [ ] 可在干净环境复现构建与测试。

### 2.3 交付形态

```text
Deepin/UOS:
  dhb CLI + dhbd + DTK/DDE 前端 + D-Bus service + Linyaps/deb

KaihongOS/OpenHarmony:
  entry-default-signed.hap（测试证书/生产证书分开）

openEuler:
  dsoftbus 6.x source package + spec + 容器验证

文档:
  协议、架构、验证日志、许可证边界、现场报告
```

## 3. 分层架构

```text
┌────────────────────────────────────────────────────────────┐
│ 前端                                                       │
│  HAP UI · DDE 控制中心/托盘 · dde-cooperation · CLI        │
├────────────────────────────────────────────────────────────┤
│ 协同能力                                                   │
│  文件 · 剪贴板 · 屏幕 · 键鼠 · 音频/相机（可选）           │
├────────────────────────────────────────────────────────────┤
│ 会话/信任                                                  │
│  发现 · 配对 · 可信设备 · 权限 · TLS · DHB session         │
├────────────────────────────────────────────────────────────┤
│ 协议适配                                                   │
│  DHB v1 │ SoftBus 6.x │ KLink 4.x │ HarmonyOS Connect     │
├────────────────────────────────────────────────────────────┤
│ Linux 底座                                                 │
│  D-Bus · FDBus · AF_BUS · dde-cooperation · PipeWire/uinput│
├────────────────────────────────────────────────────────────┤
│ 系统/打包                                                  │
│  Deepin/UOS · KaihongOS · OpenHarmony · HarmonyOS · RPM    │
└────────────────────────────────────────────────────────────┘
```

原则：

- **总线不等于传输**：D-Bus/FDBus/AF_BUS 负责本机服务/进程/内核边界；
  SoftBus/KLink 负责跨设备发现/组网；DHB 负责跨生态业务会话。
- **协议适配器可插拔**：SoftBus6、KLink4、HarmonyOS Connect、DHB v1
  是并列 adapter，不互相伪装。
- **前端先行**：先定义用户看到的设备卡、信任级别、传输状态，再补后端。

## 4. 组件矩阵

### 4.1 必须融入

| 组件 | 角色 | 说明 |
|---|---|---|
| D-Bus | Linux 服务总线 | dhbd/tray/控制中心/权限 |
| FDBus | 跨进程/服务框架 | 作为可选 IPC adapter，不替代 D-Bus |
| AF_BUS | 内核态总线 | 仅在有内核支持的分支启用；无则明确降级 |
| Deepin/UOS 底层服务 | 系统集成 | 控制中心、托盘、文件管理器、剪贴板 |
| **dde-cooperation** | 官方 DDE 协作入口 | 设备/文件/剪贴板 UI 与服务优先复用/对接 |
| SoftBus 6.x | 鸿蒙现代发现/组网 | OH6 dsoftbus + openEuler 打包 |
| KLink 4.x | 华为 4.x legacy | 星光麒麟 KLink，独立栈，不混 6.x |
| HarmonyOS Connect / 厂商组件 | 特定 OS 发现/互联 | 闭源/半闭源，按 adapter 封装，不伪造 |
| DHB 协议 | 自有 fallback | TCP + PSK/TLS；跨生态可控 |

### 4.2 屏幕共享候选池（先评估，后选 2 条）

RustDesk、Remmina、xrdp、scrcpy、QtScrcpy、Escrcpy、Sunshine、Moonlight、
Bananas、Piik、Screego、Mirror-space、CrossDesk、MiniRTC、
lamco-rdp-server、TigerVNC（含 w0vncserver）、WayVNC、RemotePhone、
MyWebScrcpy、tango-mirror、hnscrcpy、dsh-hos-scrcpy、UxPlay、
PlayBridge、MiracastReceiver、LeanCast、CastBay。

选择原则：

- x86 Linux → HAP：优先 **DHB 原生 WebRTC/低延迟流**，VNC 作保底；
- HAP → Linux：先支持“查看”，再做“控制”；
- 投屏/音频（UxPlay/Miracast）单独分类，不混进屏幕控制；
- 许可证、Wayland/X11、硬件编码、HAP 权限逐项记录。

### 4.3 键鼠跨屏

参考：

- Oink：P2P、无固定控制端、默认加密；GPL-3.0-only，**只借鉴功能，不复制代码/协议**；
- Deskflow：成熟 KVM 输入/屏幕布局参考；许可证单独核对。

DHB 自己实现输入会话：

```text
pointer move/click/scroll + keyboard + modifier + clipboard focus
```

先做“同一局域网、同一用户、可信设备”，不做跨公网。

### 4.4 文件/剪贴板/音频参考

LocalSend、ClipSunc、RGM、deviceShare、Synergy-core、SonoBus、
EchoWarp、REMOTE-AU 作为功能/协议参考；不直接复制不兼容许可证代码。

## 5. SoftBus 双栈策略

### 5.1 事实校正

- OpenHarmony 上游：`openharmony/communication_dsoftbus`，本地基线
  `e2b2e65`（6.0-Release）。
- openEuler GitHub 镜像：`openeuler-mirror/dsoftbus_standard`，
  Apache-2.0；观察到 tags `v3.1.2-release`、`v3.1.2.2-release`，
  master 最后提交 2022-09；**没有看到 v3.2 tag**。
- openEuler Embedded 22.03-LTS 文档把它描述为“嵌入式软总线”：
  仅 CoAP 局域网发现、样例 IPC/SN、双节点、client-server 一对一；
  与 OH6 全量 dsoftbus 不是同一复杂度。
- 因此“欧拉 V3.2”要落到**具体发行版 package/spec**，不能把
  `dsoftbus_standard` 的 tag 直接称为 3.2。

### 5.2 目标

```text
openEuler 3.2 发行版 dsoftbus
        ↓ rebase
OpenHarmony 6.x dsoftbus（host build）
        ↓ 保留
KLink 4.x 独立 legacy 栈（华为 4.x 互通）
        ↓ 并列
DHB v1 fallback
```

### 5.3 P0 已发现的真实阻塞

第一次 overlay：

```text
KLink 4.x host skeleton + OH6 dsoftbus + KLink CMake
```

配置越过 OpenSSL 3.2.4，但失败于：

- 4.x 源文件路径在 6.0 不存在；
- `adapter/components/core/sdk` 的 6.0 目录没有对应 CMakeLists；
- OH6 缺少 4.x 的 dfx/wifi_direct include 目录。

已生成 OH6 源清单（959 个 BUILD.gn 源文件），确认 KLink 4.x CMake
有 348 个路径在 OH6 不存在。

**下一步**：从 OH6 `BUILD.gn` 导出 `target -> sources/includes/deps`，
先编 `core/common + components/nstackx + adapter/common` 三个 host target；
不碰 tests/dfx/br_proxy/wifi_direct_cpp。

## 6. 信任与安全

分两层：

1. **自有设备群组**：首次 PIN/二维码 → 长期设备密钥 → 后续只做会话密钥协商；
2. **临时/他人设备**：每次都带外确认，不进入长期可信列表。

要求：

- PSK 只用于首次配对或开发模式；
- 正式链路 TLS 1.3 或 Noise；
- 设备撤销、密钥轮换、失败锁定；
- 屏幕/键鼠权限单独授权，不与文件权限捆绑；
- 所有跨端传输有审计事件。

## 7. 分阶段路线

### Phase 0 · 真实文件闭环（当前优先）

- HAP ↔ Deepin WSL/Deepin VM 文件传输；
  - Linux → Kaihong HAP：**已通过**（18 bytes，SHA-256 对账）；
  - HAP → Linux：**已通过**（19 bytes，SHA-256 对账，keepalive）；
- SHA-256 对账、断点续传、失败重试；
- 2in1 模拟器与 Kaihong x86 都跑同一 HAP；
- 产出：传输日志、校验和、验证记录、`alpha.3` HAP。

### Phase 1 · 产品前端与信任

- HAP UI：设备卡/附近/可信/传输队列；
- DDE/DTK + dde-cooperation 入口调研；
- PIN/二维码配对，替代手填 PSK；
- 产出：可交互 HAP + DDE 原型 + 信任状态存储。

### Phase 2 · SoftBus 6.x / openEuler

- GN target manifest → CMake host targets；
- CoAP + TCP + 最小 host server；
- openEuler spec rebase / RPM 构建；
- KLink 4.x 独立栈回归，不与 6.x 混链；
- 产出：可编译 host server、openEuler 构建日志、Gate 结果。

### Phase 3 · 屏幕共享

- 选 2 条后端（DHB 原生 + VNC/WebRTC 保底）；
- 查看优先，控制后置；
- 产出：2in1/Kaihong → Deepin 的屏幕画面与延迟数据。

### Phase 4 · 键鼠跨屏与音频（可选）

- 输入事件协议、屏幕布局、修饰键/IME；
- 音频/相机作为独立能力，不阻塞文件/屏幕；
- 产出：双机跨屏 demo + 兼容性矩阵。

### Phase 5 · 打包与发布

- deb/Linyaps/RPM/HAP 签名；
- 干净环境复现、CI、release notes；
- 产出：可安装包 + 验证报告。

## 8. 每轮工作协议

每轮必须包含：

1. 一个明确目标（对应本文 Phase/验收项）；
2. 一次真实构建/运行/测试或源码改动；
3. 一条可复核证据（日志、截图、哈希、测试输出）；
4. 文档/状态更新；
5. 若阻塞：写清阻塞点、已尝试、需要用户做什么，然后暂停询问。

禁止：

- 用“已设计/已规划”冒充“已实现”；
- 把模拟器写成实体机；
- 把 TCP/PSK 认证写成文件传输完成；
- 把 KLink 4.x 写成 SoftBus 6.x；
- 复制 GPL 代码进 Apache-2.0 核心。

## 9. 当前基线（2026-11-07）

已交付：

- HAP UI v2 安装/启动于 KaihongOS 6.1.1.11（API 23，x86_64）；
- HAP UI v2 安装/启动于 DevEco MateBook Pro 2in1 模拟器
  （HarmonyOS 7.0.0，API 26，ARM）；
- Linux ↔ Kaihong TCP/PSK 认证；
- **Linux ↔ Kaihong HAP 双向文件实传**：
  Linux→HAP 18 bytes、HAP→Linux 19 bytes，SHA-256 均对账通过；
  server 增加 3s keepalive，修复空闲会话被对端关闭；
- GitHub `v0.1.0-alpha.2`（旧 UI 的签名/未签名 HAP + SHA256SUMS）；
- SoftBus6/KLink P0 overlay + 源清单 + openEuler 打包边界。

未交付：

- 双向剪贴板发送；
- 屏幕/键鼠/音频；
- SoftBus6 host build；
- openEuler RPM；
- 华为实体 2in1。

## 10. 许可证边界

| 项目 | 许可证 | 处理 |
|---|---|---|
| DHB | Apache-2.0 | 主许可证 |
| OpenHarmony dsoftbus | Apache-2.0 | 可适配/派生，保留 NOTICE |
| openEuler dsoftbus_standard | Apache-2.0 | 可打包/派生 |
| KLink | 需逐个文件核对 | 独立栈；不混许可 |
| Oink | GPL-3.0-only + 附加条款 | 只借鉴功能；不复制代码/协议/名称 |
| dde-cooperation | 接入前核对 | 官方项目优先对接，不复制 |
| VNC/RustDesk/Sunshine 等 | 各自核对 | 作为独立后端/进程，不静态混链 |

## 11. 最终验收（一句话）

> 在干净的 Deepin/UOS 与 KaihongOS 机器上，用户打开 DHB，能看到彼此、
> 完成一次可信配对、双向传文件并校验、同步剪贴板；在同一网络下能看到
> 对方屏幕并跨屏键鼠操作；SoftBus6/KLink4/DHB 三条链路至少各有一条
> 可复现证据，所有未验证项在文档中明确标注。
