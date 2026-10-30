# HAP 可移植性：先按“标准 APK”开发，再做厂商 profile

> 结论：**不要把 KaihongOS/华为鸿蒙/OpenHarmony 的厂商差异，变成 HAP 开发的开工门槛。**
> HAP 是安装单位，标准 ArkTS/ArkUI + 标准系统 API 是共同层；厂商 SDK、API level、
> 签名、系统权限是打包/验证分支。

## 1. 类比

| Android | HarmonyOS/OpenHarmony |
|---|---|
| APK | HAP |
| Android SDK / API level | OpenHarmony SDK / API level |
| 厂商 ROM 差异 | 华为 HarmonyOS、KaihongOS、其他 OH 发行版 |
| 厂商推送/账号/系统接口 | 华为 Kit、Kaihong 系统服务、dsoftbus/HiChain |
| `minSdkVersion` | `compatibleSdkVersion` |
| `targetSdkVersion` | `targetSdkVersion` |

和“不能因为耀米OV接口不同就不开发 APK”一样：

- 文件、剪贴板、TCP、UDP、基础 UI 应该先按标准 HAP 做；
- 厂商专属能力（超级终端、系统投屏、dsoftbus、系统签名）放到 Pro/厂商分支；
- 不为每一个厂商先做一套 App。

## 2. 当前 DHB HAP 的实际状态

- 标准 ArkTS/ArkUI Stage HAP；
- 没有 `@hms.*`、华为账号、AGC、华为 Kit 依赖；
- 使用 `@kit.AbilityKit`、`@kit.CoreFileKit`、`@kit.NetworkKit`、
  `@kit.CryptoArchitectureKit`、`@kit.BasicServicesKit`、`@kit.ArkUI`；
- `compatibleSdkVersion: 5.0.0(12)`，`targetSdkVersion: 6.0.0(20)`；
- `runtimeOS: HarmonyOS`；
- 已在 DevEco 26 / SDK 26 编译通过（unsigned），**未在 KaihongOS 或 OH 真机安装**。

`compatibleSdkVersion=12` 意味着 manifest 的最低 API 是 12；Kaihong 5.0.1 的
API 14 在数字上不低于它。但 `targetSdkVersion=20`、`runtimeOS`、`@kit` 别名、
签名/权限仍可能导致安装或运行问题，**必须实测，不能凭 manifest 推断**。

## 3. 三种 HAP profile

| profile | 用途 | 状态 |
|---|---|---|
| `default`（当前） | 华为 HarmonyOS 5/6，DevEco 26/SDK 26 | 编译通过，unsigned |
| `kaihong-5.0.1`（候选） | Kaihong 5.0.1 API 11/14，OpenHarmony runtimeOS | 未构建 |
| `openharmony-portable`（候选） | 标准 OH HAP，优先兼容 OH/Kaihong | 未构建 |

候选文件：

- `ohos/profiles/kaihong-5.0.1.build-profile.json5`
- `ohos/profiles/README.md`

## 4. 先做什么

1. **不等待 SDK**：继续开发标准 HAP 的 UI/协议/TCP/文件/剪贴板；
2. 用当前 DevEco 26 保持 `default` 可编译；
3. 有 Kaihong/OH 设备时，**先尝试安装当前标准 HAP**（不先重编 API14）；
4. 只有安装/运行失败，才切 `kaihong-5.0.1` 或 portable profile；
5. 系统级 dsoftbus/超级终端永远另开分支，不塞进 Core HAP。

## 5. 兼容性风险清单

- `@kit.*` 别名在 Kaihong/OH SDK 中是否齐全；
- `targetSdkVersion 20` 是否被 API14 设备接受；
- `runtimeOS: HarmonyOS` 是否影响签名/安装；
- `DISTRIBUTED_DATASYNC` 的授权行为；
- `2in1`/`tablet` deviceTypes 是否被 Kaihong 桌面识别；
- HAP 签名链是否必须换成 Kaihong/OpenHarmony 证书。

## 6. 已落地的防线

`scripts/check-ohos-portability.py` 已加入 CI：

- 扫描 `ohos/**/*.ets` / `*.ts`；
- 如果 Core HAP 引入 `@hms.*`、`@huawei.*`、`@hw.*`、`@agconnect`、
  `@hmscore`，CI 直接失败；
- `@kit.*` / `@ohos.*` 允许，但计数输出，提醒后续做 API14 兼容检查。

这样“厂商能力是锦上添花”不只是口头原则，而是 CI 约束。

## 7. 结论

- **标准 HAP 是可移植层，先做；厂商 profile 是适配层，后做。**
- Kaihong 5.0.1 的 API 11/14 是“可能需要单独 profile”的理由，
  不是“HAP 不能开发/不能尝试安装”的理由。
- 当前没有 Kaihong 设备，所以只能写“未验证”，不能写“不能装”，
  也不能写“已装”。
