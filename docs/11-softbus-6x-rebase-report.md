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

## 10. 2026-11-07 P0 实际尝试（未通过）

在 Deepin 25 WSL 上做了第一次真实 overlay，而不是只写计划：

```text
/root/softbus6
  = KLink 4.x host skeleton (CMake/build scripts/config)
  + OpenHarmony 6.0 dsoftbus subtree (replaces KLink's 4.x dsoftbus)
  + KLink 4.x CMakeLists overlaid on the 6.0 tree
```

复现脚本：

```bash
scripts/setup-softbus6-lab.sh
```

配置阶段已经越过：

```text
-- Found OpenSSL 3.2.4
host_system is linux ...
devicetype is "PC"
ifname is "eth0"
```

但停在 **4.x CMake 的源文件清单与 6.0 目录结构不兼容**：

- `core/transmission/common/src/softbus_message_open_channel.c` 等 4.x 路径在 6.0 已消失/迁移；
- `core/authentication/ipc/src/auth_client_proxy_standard.cpp`、
  `sdk/authentication/ipc/src/auth_server_proxy.cpp` 不存在于 6.0；
- `adapter`、`components/nstackx` 的 CMakeLists 在 6.0 目录下没有可枚举源文件；
- 6.0 缺少 `core/common/dfx/hisysevent_adapter/include`、
  `core/connection/wifi_direct/channel` 等 4.x include 目录。

结论：

> **KLink 的 143 个 CMakeLists 不能整体挪到 OH 6.0。**
> 它们是 4.x 的“目录 + 源文件清单”，不是通用的 6.x 构建描述。

P0 的下一步必须按 OH 6.0 的 `BUILD.gn` 重新生成源清单，而不是继续修 4.x 路径：

1. 解析 `third_party/dsoftbus-6.0/**/BUILD.gn`，导出 `target -> sources/includes/deps`；
2. 先只生成 `core/common`、`components/nstackx`、`adapter/linux` 三个最小库；
3. 用 KLink 的 host flags（`host_system`、`DEFAULT_NET_INF_NAME_FOR_LINUX`）编译；
4. 再补 CoAP/TCP；认证栈单独按 6.0 `security_device_auth` 走，不混 4.x。

openEuler 3.2 的升级也必须等这层源清单稳定后再改 spec；现在把 3.2 spec 直接指向 6.0
源码，只会在打包阶段重复同一批路径错误。

### 10.1 P0 源清单（已生成）

```bash
python scripts/export-softbus6-manifest.py
```

当前结果（包含 tests）：

```text
OH6 BUILD.gn 源文件: 959
adapter/common:      48
components/nstackx:  57
core/authentication: 47
core/bus_center:    214
core/connection:    142
core/frame:         131
core/common:         19
sdk/*:                7
tests/*:            819（不应进产品 target）
```

同时确认 KLink 4.x CMake 引用的路径有 **348 个在 OH6 不存在**（含变量展开前的模板路径）。
这不是“补几个文件”，而是构建描述需要按 OH6 target 重建。

下一刀：用 GN target 导出 `sources/include_dirs/deps`，先只生成
`core/common` + `components/nstackx` + `adapter/common` 三个 host target；不碰
`tests/*`、`dfx/dumper`、`br_proxy`、`wifi_direct_cpp`。

### 10.2 P0 `core/common` 静态库（已编出）

```bash
scripts/build-softbus6-common-p0.sh
```

不是整棵 6.0 server，而是先验证“6.0 源码可以在 Linux host 上用独立 CMake 编译”：

```text
softbus_bitmap.c.o
softbus_json_utils.c.o
message_handler.c.o
softbus_network_utils.c.o
softbus_queue.c.o
softbus_feature_config.c.o
softbus_tlv_utils.c.o
softbus_utils.c.o
sqlite3_utils.c.o
-> libsoftbus6_common_p0.a  (159924 bytes)
+ libhilog_stub.a + log_smoke
```

- 脚本：`scripts/build-softbus6-common-p0.sh`
- 产物副本：`G:\DHB-Lab\artifacts\libsoftbus6_common_p0.a`
- SHA-256：`a395a13f684003c0b0d3e7b23bae7038dc0851a3b09f09f6b03905ef64f226c6`
- `log_smoke` 已实际执行，输出 `hilog stub smoke`；说明 6.0 的
  `COMM_LOGE` 日志宏可以被 host stub 接住。
- 仍需解决：静态库仍有 **111 个未定义符号**（sqlite/cJSON/securec/pthread 等），
  还没有链接成一个可运行的 SoftBus server；`adapter/nstackx/CoAP` 尚未接。
- 结论：**P0 从“0 个 6.0 host target”推进到“第一个 6.0 common 静态库可复现编译”**，
  但 full host server 仍未通过。

### 10.3 P0 `adapter/common` Linux 静态库（已编出）

同一脚本新增 `softbus6_adapter_p0`：

```text
adapter/common/kernel/posix/
  lnn_ip_utils_adapter.c
  softbus_adapter_file.c
  softbus_adapter_mem.c
  softbus_adapter_socket.c
  softbus_adapter_thread.c
  softbus_adapter_timer.c
adapter/common/openssl/
  softbus_adapter_crypto.c
  softbus_aes_encrypt.c
adapter/common/perf/softbus_adapter_perf.c
adapter/common/range/softbus_adapter_range.c
-> libsoftbus6_adapter_p0.a (129760 bytes)
```

- SHA-256：`2ada27cad888438de6c0a0ccbcb1f4126884334e3832e5c68bd719b23e894cbc`
- 产物副本：`G:\DHB-Lab\artifacts\libsoftbus6_adapter_p0.a`
- 编译警告：OpenSSL 3.0 对 AES/HMAC 旧 API 的 deprecated 警告，不是错误。
- 仍有 **192 个未定义符号**（securec、sqlite、cJSON、OHOS access token、部分 POSIX 适配）。
- 结论：**6.0 的 Linux POSIX adapter 已能独立编译成库**；下一步是把
  common + adapter + securec/cJSON/sqlite 链接成一个可运行的 `softbus_server`。

### 10.4 P0 host 依赖库（已编译并实测）

同一脚本新增：

```text
libsecurec_p0.a   157134 bytes  sha256 9de2c45d...
libcjson_p0.a      42050 bytes  sha256 4e9ab15c...
libsqlite_p0.a   1485764 bytes  sha256 294d8880...
```

并新增 `deps_smoke`，实际执行结果：

```text
securec_rc=0 json_object=1 sqlite=3.37.2
```

说明 securec、cJSON、SQLite 三套 host 依赖均可编译、链接和运行。

- 产物副本：`G:\DHB-Lab\artifacts\libsecurec_p0.a` /
  `libcjson_p0.a` / `libsqlite_p0.a`
- 仍未完成：`softbus6_common_p0` + `softbus6_adapter_p0` + deps 的
  完整可执行链接；6.0 server 应用层/CoAP 尚未接。

### 10.5 P0 第一个可运行 host 二进制（已通过）

新增 `link_smoke`，链接：

```text
softbus6_common_p0 + softbus6_adapter_p0
+ securec + cJSON + sqlite + hilog stub
```

实际执行：

```text
softbus_time_ms=1794057253853
```

- 二进制：`link_smoke`，35352 bytes
- SHA-256：`cbb7d9b1d7f9a00b11b9097bec8c8703971f9012582814486954e4307d4b8578`
- 副本：`G:\DHB-Lab\artifacts\softbus6_link_smoke`
- 意义：**OH6 common + Linux POSIX adapter + host deps 已经能链接成一个
  真正执行的二进制**，而不是只躺在 `.a` 里。
- 限制：只拉起了 `SoftBusGetSysTimeMs` 所需的 timer/thread 对象，
  不是完整 server；nstackx/CoAP/TCP 尚未接。

### 10.6 P0 `nstackx_util` Linux 库（已编出）

同一脚本新增：

```text
nstackx_util/core/      8 个 .c
nstackx_util/platform/unix/  7 个 .c
-> libsoftbus6_nstackx_util_p0.a
-> 109416 bytes
SHA-256: bb3e84af1c02ec0d4b09d19ad1892d693133bab5d992d9c15561f6426f2b5819
```

- 产物副本：`G:\DHB-Lab\artifacts\libsoftbus6_nstackx_util_p0.a`
- 发现并补上的 host 兼容问题：OH6 的 `sys_event.c` 假定构建环境已暴露
  `fcntl.h`；CMake 加 `-include fcntl.h` 后通过。
- 剩余警告：`CPU_ZERO/CPU_SET` 隐式声明（`_GNU_SOURCE`/sched 适配），不影响编译。
- `nm -u` 仍有 **185 个未定义符号**；`nstackx_ctrl`/CoAP 尚未接。
- 结论：**nstackx 的 util 层可以在 Linux host 编译成库**，但还不是可运行的发现服务。

### 10.7 P0 `nstackx_ctrl` 核心库（已编出）

同一脚本继续新增：

```text
nstackx_ctrl/core/ 12 个 .c
（json_payload / device / device_local / device_remote /
  dfinder_* / inet / smartgenius / statistics）
-> libsoftbus6_nstackx_ctrl_p0.a
-> 198736 bytes
SHA-256: 5016140b2d35ca5ef4ad08305e1271c5195dd177998b3bdf6fc31b9f6641fb12
```

- 产物副本：`G:\DHB-Lab\artifacts\libsoftbus6_nstackx_ctrl_p0.a`
- 补齐 `interfaces/kits/nstackx`（`nstackx_struct.h`）和
  `third_party/coap/include`（`coap3/coap.h`）后编译通过。
- `nm -u` 仍有 **226 个未定义符号**；`coap_discover` 源文件、
  libcoap 链接、实际发现循环都还没接。
- 结论：**nstackx_ctrl core 已经能作为 host 静态库编译**，
  但“发现服务可运行”仍未证明。
