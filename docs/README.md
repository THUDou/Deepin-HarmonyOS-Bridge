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
16. [SKILL 研读：OH/KaihongOS 优先](16-skill-study-openharmony-kaihong.md) — deveco-cli/harmonyos-agent-skills 的边界，DTK/DDE/Linyaps 取舍。
17. [平台边界矩阵](17-platform-taxonomy.md) — 华为鸿蒙 4.x / 纯血鸿蒙 PC / HarmonyOS Connect / KaihongOS / OHOS / 国产 Linux。
18. [GCL / 控制面](../CONTROL-PLANE.md) — 外部 Agent 的唯一入口、绝对路径、任务队列。
19. [外部 Agent 提示词](../HANDOFF-PROMPT.md) — 可直接粘贴的交接提示词。
20. [KaihongOS 桌面版 5.0.1 资料结论](18-kaihongos-desktop-5.0.1-notes.md) — VS Code/ARM64 devkit、API 11/14、HAP 签名与 Gate B 顺序。
21. [HAP 可移植性](19-hap-portability.md) — 先按标准 HAP/APK 开发，厂商 profile 后置。

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
