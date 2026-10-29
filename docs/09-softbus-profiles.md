# 软总线版本 profile：4.X / 5.X / 6.X

> 目标：**KLink 做 4.x legacy 基座，openEuler / OpenHarmony 上游补 5.x/6.x modern 协议**；一个 DHB 会话，两套传输适配器，互不污染。
> 关联：[ADR-0005](adr/0005-softbus-profiles.md) · [Klink 接入笔记](06-klink-dsoftbus-notes.md) · [外部评审](07-external-review-synthesis.md)

## 0. 一句话

**不要在“KLink 还是上游”之间二选一；按对端版本选 profile。**

- 对 **HarmonyOS 4.x / OpenHarmony 4.x**：KLink legacy profile。
- 对 **KaihongOS 5.x+ / 纯血 HarmonyOS 6.x**：**OpenHarmony 6.0-Release** 的 modern profile。
- 对 **零售 NEXT 三方应用**：DHB App profile（TCP/TLS），不走软总线。
- **4.x 与 6.x 之间不假设原生互通**；需要时由 `dhbd` 做应用层 relay。

> 版本纠正：openEuler 公开的 `dsoftbus` 包基线是 **OpenHarmony 3.2**，不是 6.x；它比 KLink 4.x 更老。6.x 的现代协议只能从 OpenHarmony `communication_dsoftbus` 5.x/6.x 取。详见 [认证优先的 MVP](10-auth-first-mvp.md)。

## 1. 为什么必须分 profile

| 维度 | KLink（4.x 谱系） | OpenHarmony 5.x/6.x（`communication_dsoftbus`） |
|---|---|---|
| 来源 | `openkylin/klink`，fork from OpenHarmony dsoftbus | `openharmony/communication_dsoftbus`（6.0-Release 分支）；openEuler 3.2 只作 CI/legacy 参考 |
| 发现 | CoAP / 局域网；BLE 支持待实测 | CoAP + BLE/BR/星闪（版本/设备相关） |
| 认证 | HiChain + PIN，KLink 提供完整 UI 钩子 | HiChain；系统 UI/SA |
| 会话头 | 4.x 格式 | 5.x/6.x 格式，可能与 4.x 不同 |
| 宿主 | Linux `host_system=ON`，非 binder | OHOS 原生在系统内；openEuler 3.2 依赖 binder/isula |
| 许可 | Apache-2.0 | Apache-2.0（openEuler 打包另有 MulanPSL-2.0 部分） |

关键事实：

- openEuler 3.1 与 3.2 明确**不跨版本通信**（openEuler 文档）；4.x 与 6.x 更不应假设互通。
- KLink 的公开说明只到 **4.x**；最新提交 2025-08-01，仍在维护，但协议代际没变。
- 上游 `communication_dsoftbus` 有 `OpenHarmony-5.0.x/5.1.0/6.0-Release` 分支；6.0 是 modern 的唯一正确源，不能拿 openEuler 3.2 冒充。
- 两代都带全局状态（device_auth、softbus_client、网络回调），同进程链接有符号/ABI/认证串台风险。

## 2. 进程结构

```
dhbd (org.dhb.Daemon)
  │  Unix socket: /run/user/<uid>/dhb/softbus4.sock
  ├──────────────► dhb-softbus4d  (KLink 4.x)
  │                  └── HarmonyOS 4.x / OH 4.x
  │
  │  Unix socket: /run/user/<uid>/dhb/softbus6.sock
  └──────────────► dhb-softbus6d  (OpenHarmony 6.0-Release)
                     └── KaihongOS 5.x+ / HarmonyOS 6.x (系统签名)
```

- helper 只做“软总线 → 字节流”的适配：发现、认证、session、`SendBytes`/`OnBytesReceived`。
- `dhbd` 在字节流上跑 DHB 帧协议；文件/剪贴板/屏幕逻辑不重复。
- helper 通过 systemd user unit 启动；未安装时 `dhbd` 自动降级到可用的 profile。
- `HELLO.flags` 位：
  - bit0 `SOFTBUS_4X`
  - bit1 `SOFTBUS_5X`
  - bit2 `SOFTBUS_6X`
  - 其余保留。

## 3. 兼容矩阵（待真机填）

| 对端 | Profile | 发现 | 认证 | Session | 状态 |
|---|---|---|---|---|---|
| HarmonyOS 4.x 手机/平板 | legacy4 (KLink) | CoAP/BLE? | HiChain PIN? | 4.x | ❌ 未测 |
| OpenHarmony 4.1 / dayu200 | legacy4 (KLink) | CoAP | HiChain PIN | 4.x | ❌ 未测 |
| KaihongOS 5.x / 6.x | modern6（OH 6.0-Release） | 原生 | HiChain | 5.x/6.x | ❌ 未测（官方有 Next 互联演示） |
| 纯血 HarmonyOS 6.x | modern6（系统签名/SA） | 原生 | HiChain | 6.x | ❌ 未测；三方应用不可用 |
| 零售 HarmonyOS NEXT 三方应用 | app (TCP/TLS) | UDP/手动 | PSK | DHB v1 | ✅ POSIX 回环已测 |
| Deepin/UOS | legacy4 或 app | UDP/KLink | PSK/HiChain | DHB v1 | ⚠️ KLink 构建待测；TCP 已测 |

> “官方有 Next 互联演示”指 KaihongOS 设备作为投屏接收端被 HarmonyOS Next 发现；不等于 Linux + KLink 也能被 Next 发现。

## 4. “兼容 4.X 与 6.X”的准确含义

### 4.1 能做到

- 同一个 `dhbd` 同时管理两个 helper，分别对接 4.x 和 6.x 设备。
- 对 4.x 设备走 legacy，对 6.x 设备走 modern；UI 上统一显示为“DHB 设备”。
- 文件/剪贴板可以在两个会话上各自工作。
- 可选：`dhbd` 做 **应用层 relay**——4.x 设备发文件给 `dhbd`，`dhbd` 再转给 6.x 设备；剪贴板同理。这是 DHB 的能力，不是软总线原生互通。

### 4.2 做不到 / 不承诺

- 不承诺 4.x 设备与 6.x 设备在系统“超级终端”里互相发现。
- 不承诺 4.x 的分布式屏幕/相机能被 6.x 原生调用。
- 不承诺应用流转/接续；这本来就是弱项，且跨系统签名/任务调度无解。
- 不承诺零售 HarmonyOS 6.x 三方应用直接连 dsoftbus（华为 6.0 文档明确多端协同不支持三方）。

## 5. 构建与依赖策略

### 5.1 Legacy：KLink

```bash
scripts/bootstrap-klink.sh          # clone + cmake host_system=ON
# 产物仍是 KLink 的 /opt/dsoftbus 布局
```

- 只链 KLink 的 `softbus_client` / `devicemgrservice`。
- 认证 UI 由 `dhb-softbus4d` 暴露给 `dhbd`（D-Bus `org.dhb.Pair`）。
- 4.x 设备 ID/networkId 规则按 KLink 的约定，不强行套用 OHOS 6.x。

### 5.2 Modern：OpenHarmony 6.0-Release

```bash
# 现代协议源：OpenHarmony 6.0-Release 分支
git clone -b OpenHarmony-6.0-Release \
  https://gitee.com/openharmony/communication_dsoftbus.git \
  third_party/dsoftbus-6.0

# openEuler 3.2 只做 CI/对比，不是 6.x 源
# dnf install dsoftbus
```

- 只链现代 dsoftbus 的 client/SA 接口；**不与 KLink 同进程**。
- 6.x 的认证/会话头以对端系统为准；`dhb-softbus6d` 只做最小适配，不在 DHB 里重写 HiChain。
- openEuler 版是 3.2，偏嵌入式（binder/isula、CoAP、双节点样例），适合做 CI/协议参考，不做桌面 6.x 主基座。

### 5.3 App：TCP/TLS

- 零售 NEXT 的 HAP 走这条；不依赖任何软总线权限。
- 当前 PSK/HMAC；M1 上 TLS 1.3。
- 与 legacy/modern 共用 DHB 帧、文件引擎、剪贴板。

## 6. M0 验证清单（按 profile 分开）

### Legacy 4.x

1. KLink 在 Deepin 25 上 `host_system=ON` 构建（无 binder）。
2. KLink ↔ OpenHarmony 4.1 / dayu200：发现 → PIN → session → bytes。
3. KLink ↔ HarmonyOS 4.x 真机（若可得）：同上。
4. 记录 `deviceTypeId`、`networkId`、session 头、认证错误码。

### Modern 6.x

1. `dhb-softbus6d` 能在 OpenHarmony 6.x / KaihongOS 6.x 的 native 环境编译（系统签名/SA 或 NAPI）。
2. 6.x 设备 ↔ 6.x 设备：发现 → HiChain → session → bytes（先用系统 demo 验证，不接 DHB）。
3. 6.x 设备 ↔ `dhb-softbus6d`：session 打开，DHB `HELLO` 通过。
4. 纯血 HarmonyOS 6.x：确认三方应用是否仍被拒绝；若拒绝，只做系统签名/合作机型。

### 双 profile 并存

1. `dhbd` 同时连一个 4.x 和一个 6.x 会话。
2. 文件分别双向传输成功。
3. 可选：4.x → `dhbd` → 6.x relay 成功（应用层）。
4. 杀掉任一 helper，另一 profile 不受影响。

## 7. 验收与放弃条件

**通过**

- 两个 profile 各自完成发现/认证/bytes；
- `dhbd` 能同时管理两个会话；
- 文件传输 SHA-256 一致；
- 4.x 或 6.x 任一 profile 失败不影响另一个。

**放弃/降级**

- KLink 无法在 Deepin 25 构建且 2 人周内无法修复 → legacy 先放弃，只做 6.x + App。
- 6.x 无法获得系统权限/合作 → modern 只做 KaihongOS 开发板，零售继续 App。
- 双 helper 的 IPC 稳定性不达标 → 评估容器/namespace 隔离或单 profile 交付。

## 8. 对当前代码的影响

已落地（v0.1）：

- `SessionConfig.hello_flags`，`HELLO.flags` 携带 profile 位；
- `Session::peerFlags()` 可读对端 profile；
- 测试覆盖 4x/6x 标志的互通；
- 传输层仍是 TCP/内存；KLink/modern helper 尚未实现。

下一步（M1/M2）：

- `dhb-softbus4d`：KLink `ITransport` 适配；
- `dhb-softbus6d`：OpenHarmony 6.x native 适配；
- `dhbd`：Unix socket transport + helper 生命周期；
- 打包：`dhb-softbus4d` / `dhb-softbus6d` 可选包，避免强制安装两套。
