# 认证优先的 MVP，以及一个必须纠正的版本判断

> 2026-10-27 联网核对。结论先放前面：
>
> **KLink 是 4.x，openEuler 公开包是 3.2，真正的新协议在 OpenHarmony 6.0-Release。**
> 所以“KLink 主基座 + openEuler 补新协议”这个组合，实际是 **4.x + 3.2**，不是 4.x + 6.x。
> 要兼容纯血 6.X，现代 profile 必须取 **OpenHarmony `communication_dsoftbus` 6.0**；openEuler 继续做 CI/3.2 参考，不做 6.x 主基座。

## 1. 版本真相（有证据）

| 来源 | 协议代际 | 证据 | 在 DHB 里的角色 |
|---|---|---|---|
| KLink | **4.x** | 官方说明“兼容鸿蒙软总线 4.x”；Gitee API 最新提交 **2025-08-01** | Legacy 4.x Linux 宿主 |
| openEuler `dsoftbus` | **3.2** | `src-openeuler/dsoftbus.spec` 固定 `OpenHarmony-v3.2-Release`；24.03 文档写 3.2、binder、CoAP | CI/3.2 参考，**不是 6.x** |
| OpenHarmony `communication_dsoftbus` | **5.x / 6.x** | `OpenHarmony-6.0-Release` 分支最新提交 **2025-09-10** | Modern 6.x 协议源 |
| HarmonyOS 6 | 6.x | [2025-10-23 发布](https://m.cnfin.com/gs-lb//zixun/20251023/4321716_1.html) | 目标对端；系统能力 |

关键纠正：

- **KLink 没有“2024 前停更”**。Gitee 公开镜像最新提交是 2025-08-01；但它的协议仍然停在 4.x，这点没变。
- **openEuler 不是“纯血鸿蒙之后的新软总线”**。公开的 openEuler Embedded 软总线是 3.2，启动方式从 `softbus_server_main` 改为 `start_services.sh softbus`，依赖 binder/isula、CoAP-only、3.1/3.2 不互通；它比 KLink 4.x 更老，不是更新。
- **6.x 的现代协议只能从 OpenHarmony 6.0-Release 取**。`communication_dsoftbus` 有 `OpenHarmony-5.0.x/5.1.0/6.0` 分支；6.0 Release 还新开了 `linkEnhance`（蓝牙多通道增强连接，API 20，三方应用可用但要求同 bundleName），注意它不是 dsoftbus 设备认证，不能拿来替代 HiChain/软总线。

证据链接：

- [KLink 仓库](https://gitee.com/openkylin/klink) · [Gitee commits API](https://gitee.com/api/v5/repos/openkylin/klink/commits?per_page=5)
- [openEuler dsoftbus.spec](https://gitee.com/src-openeuler/dsoftbus/blob/master/dsoftbus.spec) · [openEuler 软总线文档](https://pages.openeuler.openatom.cn/embedded/docs/build/html/openEuler-25.03/features/distributed_softbus.html)
- [OpenHarmony 6.0 Release Notes](https://raw.gitcode.com/openharmony/docs/raw/master/zh-cn/release-notes/OpenHarmony-v6.0-release.md)
- [OpenHarmony 6.0 linkEnhance 指南](https://raw.gitcode.com/openharmony/docs/raw/OpenHarmony-6.0-Release/zh-cn/application-dev/distributedservice/linkEnhance_development-guide.md)

## 2. “至少认证”就是 MVP 的第一道门

你说得对：先不追文件/屏幕，**先证明能跟 KaihongOS/OHOS 设备建立可信关系**。认证通过，后面 session/文件/屏幕才有意义。

### Gate A · Legacy 4.x

- 环境：Deepin/UOS + KLink；对端 OpenHarmony 4.1（dayu200）或 HarmonyOS 4.x。
- 通过标准：发现设备 → PIN → `GetTrustedDeviceList()` 出现对端，`networkId`/`deviceType` 可读。
- 不要求：session、文件、屏幕。
- 失败处理：记录发现/PIN/认证错误码；换 openEuler 3.2 只用于对比，不改主基座。
- Lab 进展：KLink 4.x host build + **Linux↔Linux COAP 发现**已通过；PIN 认证在 Linux CLI 上以 `status 5 / -20006` 失败；**CLOUD_SN lab 路径**以 `status 7 / reason 0` 建立可信关系并拿到 `networkId`。**这不等于 Gate A 通过**：对端不是 OH 4.1/HarmonyOS 4.x，且 lab 放行了 SN 校验。详见 [KLink 4.x Linux lab](12-klink-linux-lab.md)。

### Gate B0 · KaihongOS 5.0.1 HAP baseline

> 在谈 6.x 软总线前，先把 Kaihong 桌面版 5.0.1 的 HAP 基线跑通。
> 官方 SDK 是 API 11/14，不是 target 20；可用 DevEco + Kaihong Public SDK，
> 或 VS Code + ARM64 devkit。详见 [KaihongOS 资料结论](18-kaihongos-desktop-5.0.1-notes.md)。

- B0.1：先尝试安装当前标准 HAP；失败再切 API14 profile 构建/签名/安装。
- B0.2：标准 TCP 文件/剪贴板（DHB 自有协议）通过。
- B0.3：确认 KaihongOS 的 dsoftbus/device_auth 版本，再决定 5.x 还是 6.x helper。
- 通过标准：应用启动、能连 Deepin `dhb serve`、文件真实到达；不要求软总线。
- 注意：当前 HAP 是 `runtimeOS: HarmonyOS` / target 20，manifest 上未必要先重编；
  **先装再判断**，不要因为厂商文档不同就停掉标准 HAP 开发。

### Gate B · Modern 6.x

- 环境：OpenHarmony 6.0-Release / KaihongOS 6.x（B0 之后）；系统签名应用或 SA；`dhb-softbus6d` native 适配。
- 通过标准：HiChain 可信群组建立，`deviceManager`/trusted list 能看到对端；session 能打开。
- 不要求：文件、屏幕。
- 注意：纯血 HarmonyOS 6.x 三方应用仍受“多端协同仅系统应用”限制；如果拿不到系统权限，Gate B 只对 KaihongOS/开发板成立。

### Gate C · App / TCP

- 环境：零售 NEXT HAP + Deepin `dhb serve`。
- 通过标准：PSK 握手成功，双方 `peer()` 信息正确；`dhb send`/`recv` 已能证明。
- 这条不依赖任何软总线权限，是产品下限。

**MVP 判定**：A 或 B 任一通过 → “与 OHOS 设备认证打通”；C 通过 → “普通用户可安装的双端 App 可用”。三条都失败才是真的失败。

## 3. 被发现/被内置的四条路径

| 路径 | 机制 | 现状 | 定位 |
|---|---|---|---|
| S1 dsoftbus native | KLink 4.x / OH 6.x | 认证待真机 | 主路径，Gate A/B |
| S2 HarmonyOS Connect / OHOS 设备 | 鸿蒙智联设备认证、HiLink SDK、BLE/Wi-Fi | 公开资料以 IoT 设备/HiLink 为主，**没有公开的 Linux 主机端 SDK** | 研究项；若可行是加分，不作为认证前提 |
| S3 系统无线投屏 | 手机“无线投屏”发现接收端 | KaihongOS 官方演示过 Next 手机发现 KaihongOS 会议中控台 | T2 零安装接收；只解决“看” |
| S4 DHB 自有协议 | 双端都装 DHB，TCP/TLS | 已跑通文件/剪贴板 | 产品下限；任何一条软总线路径失败都保底 |

对 S2 的态度：**不否定，但必须验证**。如果 HarmonyOS Connect 的认证要求是 HiLink 模组/芯片或华为审核，那么普通 Linux 安装包无法完成；如果是标准 BLE 外设 + 公开协议，可以做一个 Linux 侧广播器。这个验证放在 M0 之后，不阻塞 A/B。

## 4. 产品下限：就算软总线全挂，DHB 仍然成立

- 双端安装 DHB：文件/剪贴板已跑通；Linux→鸿蒙副屏、鸿蒙→Linux（系统档）是 M2/M3 目标。
- 零售 NEXT：不依赖 dsoftbus，TCP/TLS + 手动/二维码配对。
- 就算只做到“文件口袋 + 剪贴板”，也**不是 UOS 助手那种 ADB 假兼容**：不依赖开发者模式、不依赖 USB 调试、不依赖 Android 兼容层。
- 这就是“降级一百次也仍然可用”的底线；C 端可安装、可解释、可审计。

## 5. 下一步：把“融合第二个基座”变成工程动作

### 5.1 取源

```bash
scripts/fetch-softbus-sources.sh
# third_party/klink                 4.x Linux host
# third_party/dsoftbus-6.0          OpenHarmony 6.0-Release
# third_party/device-auth-6.0       OpenHarmony 6.0-Release HiChain
```

### 5.2 做 rebase，而不是重写

1. 找 KLink 相对 OpenHarmony 4.x 的 **host 适配补丁**（netdev、非 binder IPC、SN/UDID、auth UI、CMake）。
2. diff OpenHarmony 4.x → 6.0 的 `communication_dsoftbus`（发现、auth、session 头、trans 模块）。
3. 把 host 补丁重放到 6.0，形成 `patches/000x-*.patch` 队列。
4. 构建 `dhb-softbus6d`，先只跑 **auth**（Gate B），不接文件。
5. 与 KLink legacy helper 并存，验证互不影响。

> 差异规模与分阶段计划见 [KLink → OH 6.x rebase 报告](11-softbus-6x-rebase-report.md)。

### 5.3 认证探针要记录什么

- 设备 ID/networkId/UDID 生成规则；
- `deviceTypeId`（PC/PHONE）是否被对端正确识别；
- PIN 弹窗方向、错误码、重试次数；
- HiChain 群组 ID、可信列表；
- 失败时是“发现不到”“PIN 不弹”“认证拒绝”还是“session 打不开”——四类分开记录。

## 6. 不承诺

- 不承诺 4.x 与 6.x 原生软总线互通；跨代只做 DHB 应用层 relay。
- 不承诺零售 HarmonyOS 6.x 三方应用直接入网。
- 不承诺应用流转/接续；这不是 DHB 的胜负手。
- 不把 HarmonyOS Connect 当作已可用路径；先验证认证门槛。

## 7. 一句话

**主基座仍是 KLink（4.x legacy）；现代协议不是 openEuler 3.2，而是 OpenHarmony 6.0；先用“认证”把 A/B 两道门推开，再用 DHB 自有协议保底。**
