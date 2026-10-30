# GCL / DHB Control Plane

> 这是 DHB 外部协作的**唯一控制面入口**。  
> 仓库里原本没有名为 `GCL` 的文件；这里把 “GCL” 当作 **governance/control layer** 的代号。  
> 绝对路径：`C:\Deepin-HarmonyOS-Bridge\CONTROL-PLANE.md`

## 0. 绝对路径清单

| 用途 | 绝对路径 |
|---|---|
| 主工作区 | `C:\Deepin-HarmonyOS-Bridge` |
| Git 元数据 | `C:\Deepin-HarmonyOS-Bridge\.git` |
| 控制面（本文件） | `C:\Deepin-HarmonyOS-Bridge\CONTROL-PLANE.md` |
| 外部 Agent 提示词 | `C:\Deepin-HarmonyOS-Bridge\HANDOFF-PROMPT.md` |
| 验证记录（唯一事实表） | `C:\Deepin-HarmonyOS-Bridge\docs\08-verification-log.md` |
| 平台边界矩阵 | `C:\Deepin-HarmonyOS-Bridge\docs\17-platform-taxonomy.md` |
| SKILL 取舍 | `C:\Deepin-HarmonyOS-Bridge\docs\16-skill-study-openharmony-kaihong.md` |
| 建仓/CI/发布计划 | `C:\Deepin-HarmonyOS-Bridge\docs\13-repo-plan.md` |
| Deepin 客户端计划 | `C:\Deepin-HarmonyOS-Bridge\deepin\README.md` |
| D-Bus 契约 | `C:\Deepin-HarmonyOS-Bridge\deepin\org.dhb.Bridge.xml` |
| 完整源码 bundle（生成后） | `C:\Deepin-HarmonyOS-Bridge\build\DHB-full.bundle` |
| GitHub 远端 | `https://github.com/THUDou/Deepin-HarmonyOS-Bridge` |

## 1. 当前快照

- 本地 HEAD：以 `git -C C:\Deepin-HarmonyOS-Bridge log -1 --format='%H %s'` 为准。
- GitHub 远端：`THUDou/Deepin-HarmonyOS-Bridge`，默认分支 `main`。
- 现有 release：`v0.1.0-alpha`（prerelease，**不是**完整产品版）。
- 完整源码 bundle 由 `git bundle create build\DHB-full.bundle --all` 生成；外部 Agent 可直接 clone。

## 2. 阅读顺序

1. `docs/17-platform-taxonomy.md` — 华为鸿蒙 4.x / 纯血鸿蒙 PC / HarmonyOS Connect / KaihongOS / OHOS / 国产 Linux 的边界；
2. `docs/08-verification-log.md` — 哪些已证明、哪些没有；
3. `deepin/README.md` + `deepin/org.dhb.Bridge.xml` — Linux 客户端/D-Bus 契约；
4. `docs/16-skill-study-openharmony-kaihong.md` — 可用/禁用的 skill 与华为工具边界；
5. `docs/13-repo-plan.md` — CI、标签、发布；
6. `README.md` — 对外功能与限制。

## 3. 不可破坏的主线

- **Core = HAP/ArkTS + DHB TCP**（文件、剪贴板、发现/配对）；不依赖华为 Kit、AGC、ADB。
- **Pro = KaihongOS/OpenHarmony 系统签名 + dsoftbus/device_auth**；KaihongOS 按厂商发行版对待，不按 OHOS 原版假设。
- **KLink 4.x = legacy 4.x**；**OpenHarmony 6.0 = modern 6.x**；不假设 4.x↔6.x 原生互通。
- **纯血鸿蒙 PC 的超级终端是华为系统能力**，不是三方 HAP API；只能做 S3 研究，不能写成 Core 已支持。
- **Linux 核心不依赖 Qt/DTK**；DDE/DTK 只作为 Deepin/UOS 可选 UI；其他发行版走 CLI/daemon + deb/rpm。
- 所有 HAP 结论必须注明是 DevEco 编译、模拟器运行，还是 KaihongOS/OH 真机运行。
- 不提交 `third_party/`、`build/`、HAP、证书、PSK、真实设备 UDID。

## 4. 当前已验证（摘要）

- Windows/MinGW + MSYS2 POSIX：`ctest 1/1`、`dhb selftest`、真实 TCP 文件、UDP 发现。
- **Deepin 25 WSL**：CMake 构建、`ctest 1/1`、`dhb selftest` 通过。
- Ubuntu 22.04 容器：CMake/ctest/TCP 回环/UDP 发现。
- HAP：DevEco 26 / SDK 26 编译通过，**unsigned**，未安装运行。
- KLink 4.x：Linux host build + 两容器 COAP 发现 + CLOUD_SN lab 认证；PIN 未过、session 未验。
- 未完成：Gate A/B、HAP 安装、DTK6 tray/GUI、Linyaps、TLS/屏幕/输入。

## 5. 外部 Agent 工作队列

按环境选一个，不要同时开三条大线：

### H1（默认，需 Deepin 25 + DTK6）
在 `deepin/dhb-tray` 实现 DTK6 DDE tray 插件 + `dhb-daemon` 的 D-Bus MVP：
`GetStatus`、`ListDevices`、`GetReceiveDir`、`SetReceiveDir`、`Pair`、`SendFile`、`CancelTransfer`。
验收：`busctl introspect` 可见接口；tray 被 `dde-tray-loader` 加载；文件发送走 daemon 而非 GUI 重写协议。

### H2（需 DevEco/hvigor + OH/Kaihong 设备）
HAP 兼容性收口：只使用标准 ArkTS/ArkUI，不引入华为 Kit；在 OpenHarmony/KaihongOS 上验证安装/启动；把结果写 `docs/08`。
**不要**用华为 AGC 证书冒充 KaihongOS 系统签名。

### H3（需 Linux + Docker/KLink 源码）
补 KLink lab 的 `GetAllNodeDeviceInfo` / `OpenSession` / 字节通道验证；基于 `docs/12` 与 `scripts/klink-lab-*`。
不要求 OH 真机；但必须明确“Linux↔Linux”而非 Gate A。

### H4（任何 Linux）
TLS 1.3 或传输加密；基于 `ITransport`，不破坏现有 TCP/内存传输测试；补 `docs/08` 与安全说明。

## 6. 外部 Agent 复制方式

```powershell
# 方式一：GitHub（网络可用时）
git clone https://github.com/THUDou/Deepin-HarmonyOS-Bridge.git D:\work\dhb-handoff

# 方式二：本地 bundle（网络不可用时）
git clone C:\Deepin-HarmonyOS-Bridge\build\DHB-full.bundle D:\work\dhb-handoff
```

然后：

```powershell
cd D:\work\dhb-handoff
git switch -c handoff/<task>
```

不要直接改 `C:\Deepin-HarmonyOS-Bridge`；所有实现、测试、提交在外部工作副本完成。

## 7. 回报格式

- 分支/commit；
- 改了哪些文件；
- 跑了哪些命令、结果；
- 哪些没验证、为什么；
- 是否触碰 Gate A/B/超级终端口径；
- 下一步建议（不超过 3 条）。
