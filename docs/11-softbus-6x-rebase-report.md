# KLink → OpenHarmony 6.x 软总线 rebase 报告

> 目的：把“KLink 4.x 主基座 + OpenHarmony 6.0 现代协议”从口号变成可执行的补丁计划。
> 数据来源：本地浅克隆（2026-10-27），不是估算。

## 0. 结论

- KLink 的 **Linux host 适配层**（CMake、非 binder、ifname/SN、认证 UI）是最有价值的资产，必须保留。
- OpenHarmony 6.0 的 dsoftbus 相对 KLink 4.x fork 差异很大：**3388 个文件条目、1369 个修改、1555 个新增、318 个删除**。
- 认证栈更重：KLink `base/security/device_auth` ↔ OpenHarmony 6.0 `security_device_auth` 差异 **994 个文件、+125620/−16962 行**。
- 因此不能“把 KLink 升个版本”就得到 6.x；正确做法是 **KLink host 适配层 + OH 6.0 核心 + 分阶段裁剪**。
- 第一阶段不要 port 全量 6.x：先 **CoAP/TCP + HiChain auth**，关闭 BLE/BR/P2P/wifi_direct_cpp/dfx 等非必需模块。

## 1. 输入与提交

| 组件 | 提交 | 日期 | 用途 |
|---|---|---|---|
| KLink | `5f0d585` | 2025-08-01 | 4.x Linux host 基线 |
| OpenHarmony `communication_dsoftbus` | `e2b2e65` | 2025-09-10 | 6.0-Release 现代核心 |
| OpenHarmony `security_device_auth` | `63a796a` | 2025-09-03 | 6.0 HiChain |

复现：

```bash
scripts/fetch-softbus-sources.sh
python scripts/analyze-softbus-diff.py third_party/klink/dsoftbus third_party/dsoftbus-6.0
git diff --no-index --shortstat \
  third_party/klink/base/security/device_auth \
  third_party/device-auth-6.0
```

## 2. dsoftbus 差异规模

`git diff --no-index` 摘要：

| 指标 | 数量 |
|---|---:|
| 总差异条目 | 3388 |
| 新增（OH 6.0 有、KLink 无） | 1555 |
| 删除（KLink 有、OH 6.0 无） | 318 |
| 修改 | 1369 |
| 重命名 | 146 |

KLink 侧变化（按顶层目录）：

| 目录 | 条目 |
|---|---:|
| core | 677 |
| tests | 635 |
| components | 227 |
| sdk | 177 |
| adapter | 97 |
| interfaces | 8 |
| dsoftbus_enhance | 2 |

OH 6.0 侧新增（按顶层目录）：

| 目录 | 条目 |
|---|---:|
| tests | 918 |
| core | 440 |
| interfaces | 129 |
| **dfx** | 109 |
| sdk | 43 |
| adapter | 37 |
| **br_proxy** | 15 |
| components | 7 |

其中 KLink 独有文件 318 个，**62 个是 CMakeLists.txt**；其余是 4.x 的 Linux/历史实现。OH 6.0 新增的 `dfx`、`br_proxy`、`core/connection/wifi_direct_cpp` 是主要新模块。

## 3. KLink host 适配层：必须保留的部分

KLink `dsoftbus/CMakeLists.txt`（231 行）是当前唯一的 Linux host 构建入口：

- `host_system=ON` 时定义 `DEFAULT_NET_INF_NAME_FOR_LINUX`；
- 定义 `NSTACKX_WITH_HMOS_LINUX`、`FILLP_LINUX`、`FILLP_POWER_SAVING_LINUX`；
- 用 `lninclude.sh` 建立 include 树，绕开 OHOS GN 的 include 布局；
- 62 个 `CMakeLists.txt` 逐目录组织 core/components/sdk；
- 提供 `adapter/default_config`、`adapter/common/include/OS_adapter_define/linux` 等宿主适配。

这些正是 OH 6.0 缺失的（OH 6.0 只有 `BUILD.gn`，没有 CMake host build）。

### 3.1 关键 feature flag（KLink 当前默认）

| flag | 默认 | 含义 |
|---|---|---|
| `dsoftbus_feature_disc_coap` | ON | CoAP 发现 |
| `dsoftbus_feature_disc_ble` | **OFF** | BLE 发现未启用 |
| `dsoftbus_feature_conn_ble` | **OFF** | BLE 连接未启用 |
| `dsoftbus_feature_conn_br` | **OFF** | BR 连接未启用 |
| `dsoftbus_feature_conn_tcp` | ON | TCP |
| `dsoftbus_feature_conn_p2p` | ON | Wi-Fi P2P |
| `support_bluetooth` | ON | 编译蓝牙相关代码，但不等于启用 BLE 发现/连接 |

**结论**：KLink host 默认是 **CoAP + TCP/P2P**，不是“手机靠近就能发现”的 BLE 方案。Gate A 可以用 CoAP/手动发现先过认证；如果产品要求“靠近发现”，BLE 适配是额外工作，不能假装已有。

## 4. OpenHarmony 6.0 的新模块

| 模块 | 作用 | 对 Linux port 的影响 |
|---|---|---|
| `dfx/` | 日志、trace、watchdog、统计 | 可先裁剪；保留 hilog/适配层 |
| `br_proxy/` | BR 代理、NAPI、HAP 卸载观察 | Linux 无 NAPI/Ability，先关闭 |
| `core/connection/wifi_direct_cpp/` | Wi-Fi Direct C++/dbinder 重构 | Linux 无 dbinder，先关闭 P2P 或保留旧 C 实现 |
| `linkEnhance`（API 20） | 蓝牙多通道应用级连接 | **不是 dsoftbus 认证**；三方应用可用但要求同 bundleName，不能替代 HiChain |
| `adapter` 新文件 | boot_event、netmanager、SLE 星闪 | Linux 需要逐项裁剪/适配 |

`linkEnhance` 容易被误认为“6.x 软总线新 API”；它不是。它走蓝牙 BLE 广播/连接，给应用传 ArrayBuffer，和 dsoftbus 的发现/组网/HiChain 是两套东西。

## 5. 认证栈差异：Gate B 的真正难点

KLink 的 `device_manager`（PIN UI）和 `base/security/device_auth` 是 4.x 实现；OpenHarmony 6.0 的 `security_device_auth` 已经独立演进：

- 994 个文件差异；
- +125620 / −16962 行；
- OH 6.0 新增 `common_lib`、`deps_adapter`、`frameworks`、`interfaces`、`services` 结构；
- KLink `device_manager` 的 UI 回调（`OnShowAuthInfo`/`OnInputAuthInfo`）需要重新映射到 OH 6.0 的认证回调。

**判断**：Gate B（与 KaihongOS 6.x/OH 6.x 认证）不是“编过 dsoftbus 就行”，认证栈是独立的 2–4 人周工作量；如果只做“Linux 两个实例发现”而不接 OH 设备，可以先用 KLink 的 4.x auth 跑通本机双实例。

## 6. 分阶段 rebase 计划

### P0 · 让 OH 6.0 在 Linux 上编出 client/server（1–2 人周）

- 以 KLink 的 62 个 CMakeLists 为模板，给 OH 6.0 生成 CMake host build。
- 先只编译：`core/bus_center`、`core/discovery/coap`、`core/connection/tcp`、`core/transmission`、`sdk`。
- 关闭：BLE、BR、P2P、`wifi_direct_cpp`、`br_proxy`、`dfx` 非必要部分。
- 产物：`libsoftbus_client` + `softbus_server`（或最小 server 进程）。
- 验收：两个 Linux 进程/两台 Linux 机器通过 CoAP 发现，`GetAllNodeDeviceInfo` 有对端。

### P1 · 合并 Linux 适配层（1–2 人周）

- 保留 KLink 的 `OS_adapter_define/linux`、ifname、SN/UDID、日志适配；
- 补 OH 6.0 新增的 `netmanager`/`sle` 的 **空实现/裁剪**；
- 保证重启后设备 ID 稳定（`/etc/SN` 或 machine-id 派生）；
- 验收：`softbus_server` 稳定运行 24h，不崩、不漏 fd。

### P2 · 认证栈移植（2–4 人周）

- 以 OH 6.0 `security_device_auth` 为准，替换 KLink 4.x `device_auth`；
- 保留 KLink `device_manager` 的 PIN 展示/输入逻辑，映射到 OH 6.0 回调；
- 先做 **PAKE/PIN 点对点**，不做账号/云；
- 验收：Linux ↔ OH 6.0 设备 PIN 认证成功，可信列表出现对端。

### P3 · Gate B 真机（1–2 人周）

- 对端：KaihongOS 6.x / OpenHarmony 6.0 dayu200（系统签名/SA）。
- 记录 `deviceTypeId`、`networkId`、认证错误码、session 是否能开。
- 通过：trusted list 有对端 + session 打开；不要求文件。

**认证-only 总计：约 5–10 人周**（不含真机等待）。文件/屏幕再叠加。

## 7. 为什么先裁剪，而不是全量 port

- OH 6.0 的 `dfx`/`br_proxy`/`wifi_direct_cpp` 依赖 OHOS IPC/NAPI/Ability，Linux 没有；
- BLE/BR/星闪需要 BlueZ/内核适配，KLink 自己默认都没开；
- 先让 `CoAP + TCP + auth` 跑通，才能证明“Linux 能作为 6.x 的可信对端”；其他模块可以后补；
- 全量 port 会把 Gate B 拖到 3–6 个月，失去“短周期迭代”的意义。

## 8. 风险与退出条件

| 风险 | 触发 | 退出 |
|---|---|---|
| OH 6.0 CMake host build 无法收敛 | P0 超过 3 人周 | 只保留 KLink legacy + App TCP；6.x 走 App/系统投屏 |
| 6.0 device auth 与 KLink UI 无法对接 | P2 超过 4 人周 | 先做 Linux 双实例认证；OH 6.x 认证延后 |
| KaihongOS 6.x 拒绝 Linux 作为可信对端 | Gate B 失败 | 产品退到 App 层 + T2 投屏；Pro 档只做 KaihongOS 侧代理 |
| BLE 发现必须 | 产品验收要求“靠近发现” | 单独立项 BlueZ/BLE 适配，不计入 Gate A/B |

## 9. 与当前代码的关系

- `Hello.flags` 已能携带 4x/5x/6x profile 位，`SessionConfig.hello_flags` / `peerFlags()` 已测试；
- `dhb pair` 已能只做 App 层认证（不传文件）；
- `scripts/analyze-softbus-diff.py` 可重复生成本报告的数据；
- `scripts/fetch-softbus-sources.sh` 已能取 KLink / OH 6.0 dsoftbus / OH 6.0 device auth。

KLink 4.x 侧的 Linux host 构建与两容器 COAP 发现已经单独验证，见
[KLink 4.x Linux lab](12-klink-linux-lab.md)；它证明的是 4.x legacy 链路，不是 6.x 的 P0。

下一步不是再写架构，而是：

```bash
scripts/fetch-softbus-sources.sh
# 按 P0 生成 OH 6.0 的 CMake host build
# 先让两个 Linux 实例 CoAP 发现
# 再接 HiChain PIN
```
