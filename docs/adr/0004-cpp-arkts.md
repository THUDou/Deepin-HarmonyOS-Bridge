# ADR-0004：C++17 核心 + ArkTS 应用，daemon 不引入 Qt

- 状态：Accepted
- 日期：2026-10-27

## 背景

- dsoftbus/Klink 是 C/C++，系统组件、编解码、PipeWire/VAAPI 都是 C/C++ 生态。
- Deepin/UOS 桌面是 C++/Qt，但 daemon 不应绑定 Qt 版本（Qt5/Qt6 并存）。
- HarmonyOS 应用层是 ArkTS；原生能力通过 NAPI/C API 接入。
- 团队需要快速迭代、可测试、可打包。

## 决策

- **核心库/daemon/CLI 用 C++17**，CMake 构建，无 Qt 依赖；只依赖 pthread/OpenSSL（TLS 阶段）。
- **HarmonyOS UI/业务用 ArkTS**；Pro 档原生能力用 NAPI + C++ 封装 dsoftbus/avcodec。
- **协议在两端各实现一份**，用同一份文档 + 测试向量约束；不引入跨语言 IDL。
- **DDE 集成**通过 D-Bus/控制中心插件，不把 Qt 拉进核心。

## 理由

- C++ 与底座同构，FFI 成本最低；性能可控。
- 无 Qt 依赖让 deb 包更小、可在服务器/容器/其他桌面复用。
- ArkTS 是鸿蒙唯一官方应用语言；NAPI 是系统能力的标准桥。
- 两份协议实现换来构建简单、调试直接；协议本身很小。

## 后果

**正面**

- daemon 可独立测试、可在无桌面环境运行。
- 不被 Qt5/Qt6 或 DDE 版本绑架。
- Pro 档原生模块与 Core 共用 C++ 核心。

**负面**

- 协议/加密逻辑双实现，存在漂移风险；需要 CI 对照测试（M2 加跨语言向量）。
- UI 迭代需要鸿蒙/前端技能。
- NAPI 层需要处理线程/生命周期，容易出 bug。

## 备选方案

- **全 Rust**：内存安全更好，但 dsoftbus/Klink/avcodec 的 FFI 与团队栈不匹配；暂不选。
- **Qt 全家桶**：UI 快，但 daemon 被 Qt 绑定，打包与服务器复用差；拒绝。
- **Flutter/Electron UI**：鸿蒙不支持/体验差；拒绝。

## 复审条件

- 协议实现漂移导致线上故障；
- 团队转为 Rust 栈；
- 需要支持 Windows/macOS 客户端（届时评估跨平台 UI）。
