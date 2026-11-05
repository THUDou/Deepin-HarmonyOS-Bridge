# ADR-0005：软总线按版本分 profile，用独立 helper daemon 隔离

- 状态：Accepted
- 日期：2026-10-27
- 相关：ADR-0001（开放 dsoftbus 谱系）、ADR-0002（Core/Pro）

## 背景

KLink 的公开说明兼容到鸿蒙软总线 **4.x**，而 KaihongOS 5.x / 纯血 HarmonyOS 6.x 已经是更新一代的 dsoftbus。**6.x 的协议源是 OpenHarmony `communication_dsoftbus` 6.0-Release；openEuler 公开包的基线仍是 3.2，比 KLink 更老，不能当 6.x 用。** 两者不是“同一个库换个版本号”那么简单：

- 发现/认证/session 头在不同代之间有差异；
- openEuler 自己的 3.1 与 3.2 也明确不跨版本通信；
- KLink 与 OpenHarmony 上游 dsoftbus 都带全局状态（device_auth、softbus_client、网络回调），把两代链接进同一个进程容易出现符号冲突、ABI 冲突和认证状态串台；
- 用户明确要求：**KLink 做基座，但必须尽量用 openEuler / OpenHarmony 上游补新协议，兼容 4.X 与纯血 6.X**。

## 决策

**一个 DHB 会话协议，两个软总线 profile，两个独立 helper daemon。**

```
                    ┌────────────────────────────┐
                    │ dhbd (D-Bus, 设备/文件/屏幕)│
                    └─────────────┬──────────────┘
                                  │ Unix socket (DHB frames)
              ┌───────────────────┴───────────────────┐
              ▼                                       ▼
   ┌──────────────────────┐              ┌──────────────────────────┐
   │ dhb-softbus4d        │              │ dhb-softbus6d            │
   │ KLink 4.x lineage    │              │ OpenHarmony 5.x/6.x      │
   │ (Apache-2.0)         │              │ 或 openEuler dsoftbus    │
   └──────────┬───────────┘              └────────────┬─────────────┘
              ▼                                       ▼
      HarmonyOS 4.x / OH 4.x                HarmonyOS 6.x / KaihongOS 5.x+
```

- **Legacy profile**：KLink（4.x 谱系），服务 HarmonyOS 4.x / OpenHarmony 4.x 设备。
- **Modern profile**：OpenHarmony `communication_dsoftbus` 6.0-Release（5.x 同理），服务 KaihongOS 5.x+ / 纯血 HarmonyOS 6.x（系统签名/SA）。openEuler 3.2 只做 CI/legacy 参考，不做 6.x 主基座。
- **App profile**：DHB 自有 TCP/TLS，服务零售 NEXT 三方应用，不依赖软总线。
- `dhbd` 只依赖 `ITransport`；helper 通过 Unix socket 暴露字节流，`dhbd` 在上面跑同一套 DHB 帧/文件/剪贴板逻辑。
- `HELLO.flags` 携带 profile 位（4x/5x/6x），用于诊断和对端能力展示，不用于替代传输选择。
- 版本事实与认证优先的验收见 [docs/10](../10-auth-first-mvp.md)；差异规模与 rebase 计划见 [docs/11](../11-softbus-6x-rebase-report.md)。
- **不假设 4.x 与 6.x 原生软总线互通**。如果需要跨代文件/剪贴板，由 `dhbd` 做应用层 relay（P2），不做系统级“联邦”。

## 为什么不把两代链进一个进程

| 方案 | 问题 |
|---|---|
| 只链 KLink | 只能覆盖 4.x，6.x 无解 |
| 只链上游 6.x | 4.x 设备全部掉线 |
| 两代同进程 | 符号/ABI/全局状态冲突；认证回调可能串台；崩溃互相影响；升级一个库要重编全部 |
| **两个 helper** | 进程隔离、可独立打包/升级/降级；代价是本地 IPC 与多一个 systemd unit |

## 后果

**正面**

- 4.x 与 6.x 可以并存，互不污染。
- KLink 继续做“能跑、能配对”的成熟 legacy 基座；上游 dsoftbus 负责新协议。
- 每个 profile 可以单独 CI、单独灰度、单独回滚。
- `dhbd` 的业务代码不需要知道对端是 4.x 还是 6.x。

**负面**

- 多两个进程/包（`dhb-softbus4d`、`dhb-softbus6d`），安装体积和 systemd 配置增加。
- helper 与 `dhbd` 之间需要定义本地帧/生命周期/心跳；Unix socket 权限要收紧。
- 两套 dsoftbus 的设备 ID / networkId / 认证 UI 需要分别适配。
- 4.x↔6.x 跨代仍不能原生组网；只能应用层 relay。

## 备选方案

- **单 KLink + 自己补 6.x 协议**：等于重写上游，维护成本高。拒绝。
- **单上游 + 放弃 4.x**：违背“兼容 4.X”的目标。拒绝。
- **容器/namespace 隔离而不是 helper**：可作为部署备选，但桌面用户体验差；helper 更轻。

## 复审条件

- KLink 能在合理工作量内直接兼容 6.x（则 legacy/modern 可合并）；
- 上游 dsoftbus 提供稳定的跨版本兼容层；
- helper 的本地 IPC 成为性能/稳定性瓶颈（届时评估同进程 `dlmopen` 或静态符号隔离）。
