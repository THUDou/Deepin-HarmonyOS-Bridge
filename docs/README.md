# DHB 文档

建议阅读顺序：

1. [研究方向与可行性判断](01-research.md) — 为什么做、边界在哪、Go/No-Go。
2. [系统架构](02-architecture.md) — 组件、传输、数据流、安全、打包。
3. [DHB 线协议 v1](03-protocol.md) — 字节级格式，改代码前先读。
4. [路线图](04-roadmap.md) — M0–M4 的交付与验收。
5. [风险登记册](05-risks.md) — 可能杀死项目的 12 件事。
6. [Klink / dsoftbus 接入笔记](06-klink-dsoftbus-notes.md) — 底座怎么接。
7. [外部评审交叉核对](07-external-review-synthesis.md) — 两份外部材料带来的修正、分歧与裁决。
8. [验证记录](08-verification-log.md) — 已证明/未证明的边界与复现命令。
9. [软总线版本 profile](09-softbus-profiles.md) — 4.x legacy / 5.x/6.x modern 的分层与验收。
10. [认证优先的 MVP](10-auth-first-mvp.md) — KLink 4.x / OpenHarmony 6.0 / openEuler 3.2 的版本真相与认证 Gate。
11. [KLink → OH 6.x rebase 报告](11-softbus-6x-rebase-report.md) — 3388 个差异、认证栈 994 个文件，分阶段计划。
12. [KLink 4.x Linux lab](12-klink-linux-lab.md) — host 构建、两容器 COAP 发现、PIN 失败与 CLOUD_SN lab 认证。
13. [建仓计划](13-repo-plan.md) — 仓库身份、CI、标签、里程碑、release checklist。
14. [AliOS 分布式软总线研究](14-alios-softbus-notes.md) — 官方口径、Fusion/SOME-IP/DDS、与 dsoftbus 的边界。
15. [Deepin 25 客户端计划](../deepin/README.md) — DTK6/DDE tray/D-Bus/Linyaps 的分阶段设计。

架构决策：

- [ADR-0001](adr/0001-dsoftbus-base.md)：基于开放 dsoftbus 谱系。
- [ADR-0002](adr/0002-two-tier.md)：Core / Pro 两档能力。
- [ADR-0003](adr/0003-wire-protocol.md)：自定义二进制帧。
- [ADR-0004](adr/0004-cpp-arkts.md)：C++17 核心 + ArkTS 应用。
- [ADR-0005](adr/0005-softbus-profiles.md)：4.x/6.x 双 profile，独立 helper daemon。

其他：

- [真机验证模板](field-reports/README.md)
- [安全策略](../SECURITY.md)
- [贡献指南](../CONTRIBUTING.md)
