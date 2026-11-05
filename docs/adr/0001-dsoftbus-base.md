# ADR-0001：基于开放 dsoftbus 谱系，而不是重写或逆向

- 状态：Accepted
- 日期：2026-10-27
- 决策者：DHB 初始团队

## 背景

DHB 需要设备发现、可信认证、会话、字节/文件/流传输。可选路径：

1. 从零实现一套跨端协议；
2. 逆向华为"多屏协同"私有协议；
3. 复用 OpenHarmony dsoftbus 的开源实现与 Linux 兼容层（Klink / openEuler dsoftbus）。

## 决策

选择 **3**：

- Linux 侧以 **Klink**（Apache-2.0，兼容鸿蒙软总线 4.x）作为 Pro 档主传输基座；openEuler `dsoftbus` 作为参考与备选打包。
- 鸿蒙侧使用 **OpenHarmony dsoftbus** 原生接口（系统签名/SA）。
- 零售 HarmonyOS 三方应用走 **DHB 自己的 TCP/TLS 通道**，dsoftbus 只用于公开的发现/PIN 绑定。
- **不**逆向、不复制华为闭源"多屏协同"协议或二进制。

## 理由

- Klink 已经解决 Linux 上最难的发现、HiChain 认证、会话与文件/流传输，且是 Apache-2.0，与 DHB 许可证兼容。
- openEuler 官方文档已验证 Linux 与 OpenHarmony 3.1 的 dsoftbus 互通，说明"Linux 作为对端"成立。
- OpenHarmony 的 `distributed_screen`/`hmdfs` 都是 Apache-2.0，长期可做原生互操作。
- 逆向私有协议法律风险高、版本脆弱、无法进入信创采购。

## 后果

**正面**

- 站在成熟底座上，MVP 聚焦产品层（文件、屏幕、输入、桌面集成）。
- 与鸿蒙生态的协议演进同向，长期可互操作。
- 许可证清晰，可审计。

**负面**

- 依赖 Klink 的维护节奏；其公开说明只到 4.x，KaihongOS 5.x 需要验证/可能 rebase。
- dsoftbus 的系统权限/信任链不是纯代码能解决，Pro 档需要厂商合作。
- 零售 NEXT 仍拿不到任意字节通道，必须维护第二条 TCP/TLS 路径。

## 备选方案

- **纯自研协议**：可控但重复造轮子，且无法与鸿蒙原生组网互通。拒绝。
- **纯 TCP/TLS**：零售可达，但失去"软总线"生态叙事与系统级能力。作为 Core 兜底，不作为唯一路线。
- **Miracast/WiDi**：只解决"看"，不解决文件/输入/认证。作为屏幕备选。

## 复审条件

- Klink 停止维护，或 6 个月内无法适配 OpenHarmony 5.x；
- 华为/OpenHarmony 改变 dsoftbus 授权或协议封闭；
- 真机验证发现零售 NEXT 完全拒绝 Linux 对端，且 Pro 合作无法推进。
