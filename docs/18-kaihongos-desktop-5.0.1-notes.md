# KaihongOS 桌面版资料结论（5.0.1 / Meta 6.1）

> 资料研读日期：2026-10-30。  
> 目的：把“开鸿桌面版 5.0.1”“KaihongOS Meta 6.1”“OpenHarmony SDK”“华为鸿蒙 PC”
> 分开，避免再拿一个版本去假设另一个版本。

## 0. 一句话结论

**KaihongOS 桌面版 5.0.1 有两条官方北向路径：DevEco Studio + Kaihong Public SDK
（主路线，SDK 已有），以及 VS Code + ArkTS Project Manager + ARM64 Linux 容器 +
`kaihongos-devkit`。默认 SDK 是 API 11/API 14。当前 DHB HAP 的
`targetSdkVersion 20` / `runtimeOS: HarmonyOS` 不是 Kaihong 的官方 profile，
但 HAP 是标准安装单位，仍应先尝试安装，失败再切 API14 profile。**

## 1. 资料分层

| 资料 | 版本/日期 | 能证明什么 | 不能证明什么 |
|---|---|---|---|
| `KaihongOS桌面版 5.0.1 ArkTS插件使用指导.pdf` | 01 / 2026-01-14 | 5.0.1 的 VS Code 插件、Linux 容器、devkit、API 11/14、自动签名/构建/部署 | 6.1 的行为 |
| `KaihongOS桌面版 北向应用开发指导书.pdf` | 02 / 2025-05-09 | HAP/Stage/FA、Hvigor、签名材料、hdc 安装的通用流程 | 其正文仍出现 API 8/9；**不能当作 5.0.1 的当前 API 说明** |
| `KaihongOS桌面版 API参考.zip` | 2025-03-27 | 文件实际是 **KaihongOS 4.1.3 API 参考** | 不能代表 5.0.1/6.1 |
| `KaihongOS桌面版 源码编译指导书.pdf` | 01 / 2025-03-30 | 系统镜像/OTA 编译，Ubuntu 20 环境 | 不是应用开发/SDK 指南 |
| `KaihongOS Meta 6.1.0 产品说明书`（超管平台/应用市场） | 01 / 2026-07-30 | Meta 平台的管理/应用市场功能 | 不是 6.1 桌面 OS 的 API/SDK 文档 |
| `kaihongos-devkit-5.0.1.35-linux-arm64.deb` | 5.0.1.35 / arm64 | 5.0.1 北向开发套件是 **ARM64** | 不能在 x86_64 WSL 里直接安装运行 |

## 2. KaihongOS 5.0.1 的两条官方开发链

资料里其实有两条都成立的北向路径，不能只留一条：

### A. DevEco Studio + KaihongOS Public SDK

```text
Windows / DevEco Studio
  └── Customize > SDK > OpenHarmony
        └── KaihongOS Public SDK（API 11 / API 14）
              └── Hvigor 构建 HAP
```

- 北向应用开发指导书就是这条路线：HAP、Stage、Hvigor、签名、hdc。
- SDK 由 Kaihong 提供，装到 DevEco 的 OpenHarmony SDK 位置。
- 构建主机可以是 x86_64 Windows；**安装/运行才需要 Kaihong 设备**。
- 这条路线不要求当前 WSL 安装 arm64 devkit。

### B. VS Code + ArkTS Project Manager + ARM64 Linux 容器

```text
KaihongOS PC / BotBook / BotMini（ARM64）
  └── Linux 容器（开鸿桌面应用引擎）
        ├── VS Code
        ├── ArkTS Project Manager / ArkTS 插件
        └── kaihongos-devkit-5.0.1.35-linux-arm64.deb
              └── OpenHarmony SDK：API 11 / API 14
```

- 5.0.1 ArkTS 插件指导是这条路线：工程同步、自动签名、构建、一键部署、设备选择。
- devkit 是 **linux-arm64**，安装目标是开鸿电脑里的 Linux 容器。
- 实际解包核对（不安装）：`/opt/ohsdk/11` 与 `/opt/ohsdk/14` 两个 SDK 目录；
  另有 `/opt/node`、`/opt/ohpm-1.4.0`、aarch64 的 JDK 17 和 musl aarch64 loader。
  这印证了“API 11/14 + ARM64”的结论。

### 共同点

- 两条路都产出标准 HAP；都通过 hdc 安装/拉起。
- 新建工程时可选 API 11 / API 14；不要用 API 20 的默认 HarmonyOS profile。
- 签名是 OpenHarmony 体系（`.p12` + `.cer` + `.p7b`），不是华为 AGC。
- 如果 Windows 上已有 DevEco + Kaihong SDK，**优先用 A**；B 是 Kaihong PC 上的另一条路。

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

它面向华为 HarmonyOS 5/6，不是 KaihongOS 5.0.1（API 11/14）的官方 profile。
但 `compatibleSdkVersion=12` 低于 API14，manifest 上并非必然被拒；
**先实测安装，不能凭 target 20 断定“不能装”，也不能凭 manifest 断定“已兼容”。**

### 4.2 如果标准 HAP 装不上，再切 Kaihong profile

标准 HAP 先试；只有安装/运行失败，才需要单独的 Kaihong 5.0.1 profile。
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

### 4.3 当前环境能做什么、不能做什么

- 当前 WSL 是 **x86_64 Deepin 25**；
- devkit 是 **arm64**，不能直接装进当前 WSL；
- 但 **DevEco + Kaihong Public SDK 的路线不要求 WSL**：构建在 Windows 上，
  安装/运行才需要 Kaihong 设备；
- 之前磁盘里的 Kaihong SDK 现在不可读；在磁盘恢复前，不能声称已经用
  Kaihong SDK 编译过。

可行顺序：

1. 磁盘恢复后，先在 DevEco 里确认 Kaihong SDK 的 API 11/14 路径和版本；
2. 用 DevEco 新建/编译一个 API 14 空 HAP，确认 Hvigor 和签名链；
3. 有 Kaihong 设备时 `hdc install`；没有设备时只到构建/签名，不吹安装；
4. 只有 Kaihong PC 时，再走 VS Code + ArkTS 插件的 ARM64 路线；
5. x86 尝鲜版等镜像/硬盘回来再单独评估，不混进当前 Gate。

华为鸿蒙 PC 则是 HarmonyOS HAP/DevEco 路线，不能与 Kaihong 5.0.1 的 SDK 混用。

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
B0  KaihongOS 5.0.1 先尝试安装当前标准 HAP（不先重编 API14）
B1  若装不上，再用 API14 profile 构建/签名/安装/启动
B2  HAP 标准 TCP + 文件/剪贴板（DHB 自有协议）
B3  KaihongOS 的 dsoftbus/device_auth 版本确认
B4  OH 6.0 ↔ KaihongOS 对应版本 的认证/session
```

B1/B2 之前，不应该先承诺 B3/B4。

## 6. 待办

1. 先用当前标准 HAP 尝试 `hdc install`（有设备时），不要先重编；
2. 若装不上/启动失败，再恢复旧盘 Kaihong SDK，用 API14 profile 重编；
3. 用 DevEco 新建 API 14 空工程做对照，确认 Hvigor/签名的差异；
4. 先只验证 `INTERNET` + TCP，再验证 `DISTRIBUTED_DATASYNC`；
5. 最后才谈 dsoftbus/HiChain 和 Gate B4；x86 尝鲜版另行评估。

## 7. 参考

- 本地：`C:\迅雷下载\KaihongOS桌面版 5.0.1 ArkTS插件使用指导.pdf`
- 本地：`C:\迅雷下载\KaihongOS桌面版 北向应用开发指导书.pdf`
- 本地：`C:\迅雷下载\kaihongos-devkit-5.0.1.35-linux-arm64.deb`
- 官方文档入口：<https://developer.kaihong.com/document/kaihong-developer-documents/kaihongos-guides/document/community/northboundApplication.html>
- 华为鸿蒙 PC 入门：<https://developer.huawei.com/consumer/cn/multidevice/pc/get-started/>
