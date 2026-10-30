# KaihongOS 桌面版资料结论（5.0.1 / Meta 6.1）

> 资料研读日期：2026-10-30。  
> 目的：把“开鸿桌面版 5.0.1”“KaihongOS Meta 6.1”“OpenHarmony SDK”“华为鸿蒙 PC”
> 分开，避免再拿一个版本去假设另一个版本。

## 0. 一句话结论

**KaihongOS 桌面版 5.0.1 的官方北向开发路径不是 DevEco 单干，而是
VS Code + ArkTS Project Manager + ARM64 Linux 容器 + `kaihongos-devkit`；
默认 SDK 是 API 11/API 14。当前 DHB HAP 的 `targetSdkVersion 20` /
`runtimeOS: HarmonyOS` 不能当作 KaihongOS 5.0.1 的构建配置。**

## 1. 资料分层

| 资料 | 版本/日期 | 能证明什么 | 不能证明什么 |
|---|---|---|---|
| `KaihongOS桌面版 5.0.1 ArkTS插件使用指导.pdf` | 01 / 2026-01-14 | 5.0.1 的 VS Code 插件、Linux 容器、devkit、API 11/14、自动签名/构建/部署 | 6.1 的行为 |
| `KaihongOS桌面版 北向应用开发指导书.pdf` | 02 / 2025-05-09 | HAP/Stage/FA、Hvigor、签名材料、hdc 安装的通用流程 | 其正文仍出现 API 8/9；**不能当作 5.0.1 的当前 API 说明** |
| `KaihongOS桌面版 API参考.zip` | 2025-03-27 | 文件实际是 **KaihongOS 4.1.3 API 参考** | 不能代表 5.0.1/6.1 |
| `KaihongOS桌面版 源码编译指导书.pdf` | 01 / 2025-03-30 | 系统镜像/OTA 编译，Ubuntu 20 环境 | 不是应用开发/SDK 指南 |
| `KaihongOS Meta 6.1.0 产品说明书`（超管平台/应用市场） | 01 / 2026-07-30 | Meta 平台的管理/应用市场功能 | 不是 6.1 桌面 OS 的 API/SDK 文档 |
| `kaihongos-devkit-5.0.1.35-linux-arm64.deb` | 5.0.1.35 / arm64 | 5.0.1 北向开发套件是 **ARM64** | 不能在 x86_64 WSL 里直接安装运行 |

## 2. KaihongOS 5.0.1 的实际开发链

根据 5.0.1 ArkTS 插件指导：

```text
KaihongOS PC / BotBook / BotMini（ARM64）
  └── Linux 容器（开鸿桌面应用引擎）
        ├── VS Code
        ├── ArkTS Project Manager / ArkTS 插件
        └── kaihongos-devkit-5.0.1.35-linux-arm64.deb
              └── OpenHarmony SDK：API 11 / API 14
```

关键点：

- **不是必须用 DevEco Studio**；官方 5.0.1 指南主推 VS Code 插件。
- devkit 是 **linux-arm64**，安装目标是开鸿电脑里的 Linux 容器。
- 实际解包核对（不安装）：`/opt/ohsdk/11` 与 `/opt/ohsdk/14` 两个 SDK 目录；
  另有 `/opt/node`、`/opt/ohpm-1.4.0`、aarch64 的 JDK 17 和 musl aarch64 loader。
  这印证了“API 11/14 + ARM64”的结论。
- 新建工程时可选 API 11 / API 14；默认镜像自带这两个 SDK。
- 插件提供：工程同步、自动签名、工程构建、一键部署/运行、设备选择。
- 生成的是标准 HAP；通过 hdc 安装/拉起。
- 签名是 OpenHarmony 体系（`.p12` + `.cer` + `.p7b`），不是华为 AGC。

## 3. 北向指导书里的 HAP 流程（通用部分）

即使它的 API 版本偏旧，以下流程仍然适用：

- Stage 模型是主推模型；FA 是旧模型。
- HAP 是安装单位，APP 是上架单位。
- Hvigor + hvigor-ohos-plugin 构建。
- `build-profile.json5` 包含 `compileSdkVersion`、`compatibleSdkVersion`、
  `signingConfigs`、`products`。
- HAP 签名需要：
  - `.p12` 密钥库；
  - `.cer` 证书；
  - `.p7b` Profile；
  - `hap-sign-tool.jar` 可做 profile/HAP 签名。
- `hdc install` / `hdc shell` 用于安装和调试。
- 指南里“每个 HAP 不超过 10MB”是旧版限制，当前 DHB HAP 约 209KB，不构成问题。

## 4. 对 DHB 的直接影响

### 4.1 当前 HAP 不是 Kaihong 5.0.1 profile

`ohos/build-profile.json5` 现在是：

```json5
compatibleSdkVersion: "5.0.0(12)"
targetSdkVersion: "6.0.0(20)"
runtimeOS: "HarmonyOS"
```

它面向华为 HarmonyOS 5/6，不是 KaihongOS 5.0.1（API 11/14）。
**不能把现在的 unsigned HAP 直接说成“KaihongOS 可安装”。**

### 4.2 需要单独的 Kaihong 5.0.1 构建 profile

候选片段（**未在 Kaihong SDK 上验证**，只是把差异写清楚）：

```json5
{
  "app": {
    "products": [
      {
        "name": "kaihong-5.0.1",
        "compatibleSdkVersion": 14,
        "targetSdkVersion": 14,
        "runtimeOS": "OpenHarmony"
      }
    ]
  }
}
```

注意：

- 具体版本字符串/数值要以 Kaihong SDK 的 `build-profile` 模板为准；
  不要直接把 `5.0.0(12)` 改成 `14` 就宣称可用。
- `runtimeOS` 必须是 `OpenHarmony`，不是 `HarmonyOS`。
- API 14 下 `@kit.*` 导入是否全部可用，要在 Kaihong SDK 上实际编译。
- `DISTRIBUTED_DATASYNC` 的授权行为必须真机验证，不能从权限名推断。

### 4.3 为什么不能在当前 WSL 里直接做

- 当前 WSL 是 **x86_64 Deepin 25**；
- devkit 是 **arm64**；
- 官方安装路径是开鸿电脑的 Linux 容器，不是 x86 WSL；
- 因此本轮只能做文档/配置分析，不能把 devkit 装进当前 WSL 并宣称构建成功。

可选路径：

1. 有 KaihongOS ARM64 机器：按官方 VS Code 插件链路构建/签名/安装（首选）。
2. 只有 x86 机器：找 Kaihong 的 x86 SDK/容器（若有），或用受支持的 ARM 构建机；
   不把 QEMU 跑 arm64 devkit 当成官方验证。
3. 华为鸿蒙 PC：走 HarmonyOS HAP/DevEco 路线，不能与 Kaihong 5.0.1 混用。

### 4.4 Meta 6.1 不要误读

`KaihongOS Meta 6.1.0` 的两份文档是：

- 超管平台；
- 应用市场。

它们是平台侧产品说明，不是 6.1 桌面 OS 的北向 SDK/API 文档。
在没有 6.1 桌面 devkit/API 文档前，不能把 Meta 6.1 当作“API 20 的 Kaihong 桌面”。

## 5. Gate B 的顺序要改

之前的 Gate B 写成“OH 6.0 ↔ KaihongOS 6.x 认证/session”。  
现在更准确的顺序是：

```text
B1  KaihongOS 5.0.1（API 14）HAP 构建/签名/安装/启动
B2  HAP 标准 TCP + 文件/剪贴板（DHB 自有协议）
B3  KaihongOS 的 dsoftbus/device_auth 版本确认
B4  OH 6.0 ↔ KaihongOS 对应版本 的认证/session
```

B1/B2 之前，不应该先承诺 B3/B4。

## 6. 待办

1. 找 Kaihong 5.0.1 的 **x86_64 devkit**（若有）或一台 ARM64 Kaihong 机器；
2. 用官方 ArkTS 插件新建 API 14 空工程，确认 HAP 能构建/安装；
3. 把 DHB HAP 降到 API 14 profile，先只验证 `INTERNET` + TCP；
4. 再验证 `DISTRIBUTED_DATASYNC` 权限；
5. 最后才谈 dsoftbus/HiChain 和 Gate B4。

## 7. 参考

- 本地：`C:\迅雷下载\KaihongOS桌面版 5.0.1 ArkTS插件使用指导.pdf`
- 本地：`C:\迅雷下载\KaihongOS桌面版 北向应用开发指导书.pdf`
- 本地：`C:\迅雷下载\kaihongos-devkit-5.0.1.35-linux-arm64.deb`
- 官方文档入口：<https://developer.kaihong.com/document/kaihong-developer-documents/kaihongos-guides/document/community/northboundApplication.html>
- 华为鸿蒙 PC 入门：<https://developer.huawei.com/consumer/cn/multidevice/pc/get-started/>
