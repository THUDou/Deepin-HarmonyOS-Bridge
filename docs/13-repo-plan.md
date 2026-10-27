# DHB 建仓计划（v0.1.0-alpha）

> 目标：把当前本地仓库变成一个可公开、可协作、可复现实验的
> `deepin-harmonyos-bridge` 仓库，而不是把半成品直接推上去。

## 1. 仓库身份

| 项 | 建议值 |
|---|---|
| 仓库名 | `deepin-harmonyos-bridge` |
| 产品名 | DHB / 深鸿桥 |
| 包名 | `org.dhb.bridge`（HAP 已使用） |
| 许可证 | Apache-2.0（根目录已有 `LICENSE`/`NOTICE`） |
| 主分支 | `main` |
| 主语言 | 中文文档；代码/提交信息英文 |
| 托管 | Gitee 主仓 + GitHub 镜像（二选一做 CI 主仓） |

组织名不要猜：把 `<org>` 换成实际 Gitee/GitHub 组织或个人命名空间。

## 2. 仓库里放什么

**提交：**

- `include/`、`src/`、`apps/`、`tests/`：C++17 核心
- `ohos/`：HAP 工程（不含 `oh_modules`、`build`、`local.properties`）
- `docs/`：研究、架构、协议、验证、lab、rebase 报告
- `scripts/`：构建、发现、KLink lab、OHOS 校验
- `patches/`：KLink host bring-up patch（明确标注 lab-only）
- `packaging/`：deb/systemd 骨架
- `.github/`：CI、issue/PR 模板

**不提交：**

- `third_party/`（KLink/OH6 的本地克隆）
- `build/`、`build-linux/`、`ohos/build/`、`ohos/oh_modules/`
- HAP/APP 产物（unsigned 也不作为 release asset）
- 证书、`*.p12`、`*.cer`、`*.pem`、`local.properties`
- PSK、token、设备 UDID、真实设备日志
- 磁盘探测/排障脚本（本次已从仓库根目录清理）

## 3. 建仓步骤

```bash
# 1) 在 Gitee/GitHub 创建空仓库（不要勾选 README/LICENSE，避免首提交冲突）
# 2) 绑定远端
git remote add origin git@gitee.com:<org>/deepin-harmonyos-bridge.git
git push -u origin main

# 3) 打第一个 tag（确认 CI 绿、docs/08 已更新）
git tag -a v0.1.0-alpha -m "DHB v0.1.0-alpha: protocol, file, discovery, HAP build, KLink lab"
git push origin v0.1.0-alpha
```

若同时维护 GitHub 镜像：

```bash
git remote add github git@github.com:<org>/deepin-harmonyos-bridge.git
git push github main --tags
```

## 4. 分支与保护

- **trunk-based**：`main` 常绿；功能分支 `feat/<topic>`、修复 `fix/<topic>`。
- `main` 保护：禁止直接 push；至少 1 个 review；CI 必须绿。
- 发布用 annotated tag；不建长期 `develop`，避免两个真相。
- `docs/08-verification-log.md` 是“已证明/未证明”的唯一入口；PR 改能力时同步更新。

## 5. CI 设计

当前 `.github/workflows/ci.yml` 已有 Linux + Windows 核心构建/测试。建仓时补：

1. Linux job 增加 `python3 scripts/check-ohos-config.py`（OHOS 工程结构/JSON 校验）。
2. Windows job 保持 CMake/CTest；不跑 HAP。
3. HAP 构建**不进 GitHub-hosted CI**：DevEco Studio/SDK 不是公开 CI 镜像；由维护者本地 `hvigorw assembleApp`，把命令和结果写进 `docs/08`。
4. KLink lab 也**不进默认 CI**：需要 Linux 文件系统符号链接、Docker、`/opt/dsoftbus`，成本高；用 `workflow_dispatch` + self-hosted 或本地脚本。
5. 可选：`codespell`/`markdownlint` 只做告警，不阻塞首个 alpha。

## 6. 标签（Labels）

| 标签 | 用途 |
|---|---|
| `area:protocol` | DHB 帧/加密/会话 |
| `area:linux` | Deepin/UOS CLI、打包、systemd |
| `area:harmonyos` | ArkTS/HAP/DevEco |
| `area:softbus` | KLink/dsoftbus/OH6 |
| `gate:A` / `gate:B` / `gate:C` | 认证门 |
| `type:bug` / `type:feature` / `type:docs` | 类型 |
| `good first issue` | 文档、测试、打包 |
| `lab-only` | 不可进产品的实验代码/补丁 |

## 7. 里程碑

| 里程碑 | 判定 |
|---|---|
| **M0 骨架** | C++/ArkTS 双端、协议、文件、发现、PSK；已达成 |
| **M1 MVP** | HAP 可安装运行 + Linux deb + 文件/剪贴板；进行中 |
| **Gate A** | KLink 4.x ↔ OH 4.1/HarmonyOS 4.x 可信列表；未达成 |
| **Gate B** | OH 6.0 ↔ KaihongOS 6.x HiChain/session；未达成 |
| **M2 屏幕** | 采集/编码/解码/回控；未开始 |

## 8. 第一个 alpha 的 Release Checklist

- [ ] `ctest` Linux + Windows 全绿
- [ ] `./build/dhb selftest` 通过
- [ ] `python3 scripts/check-ohos-config.py` 通过
- [ ] `docs/08` 与 `docs/12` 的“已证明/未证明”一致
- [ ] HAP 构建命令/产物 hash 记录在 `docs/08`（不提交 HAP）
- [ ] 仓库无证书/PSK/`local.properties`/`third_party`/`build`
- [ ] `LICENSE`、`NOTICE`、`SECURITY.md`、`CONTRIBUTING.md` 存在
- [ ] README 明确写：Gate A/B 未通过、HAP unsigned、TCP 明文仅开发
- [ ] tag `v0.1.0-alpha`，release notes 用 `CHANGELOG.md`

Release assets 建议只放：

- `dhb` Linux x86_64 二进制（可选，附 SHA-256）
- `dhb_0.1.0_amd64.deb`（可选；打包完成后再放）
- 源码 tarball（平台自动生成）

**不要**把 unsigned HAP 当 release asset；签名/安装流程另开 issue。

## 9. 建仓后第一批 issue

1. `gate:A` KLink 4.x ↔ dayu200（OH 4.1）PIN + trusted list
2. `gate:B` OH 6.0 CMake host build（按 docs/11 P0）
3. HAP 自动签名 + 真机安装/运行
4. KLink PIN 状态机：`OnInputAuthInfo` 未触发 / status 5
5. Linux TLS 1.3（替换明文 TCP）
6. deb/systemd 打包与 Deepin 25 实机验证
7. 英文 README / 贡献指南
8. HAP 剪贴板 `READ_PASTEBOARD` 权限与运行时申请

## 10. 口径

- 不写“已兼容 HarmonyOS 6.x”；只写“Gate B 未通过”。
- 不写“KLink 已被我们吃透”；只写“host build + Linux↔Linux discovery + CLOUD_SN lab auth”。
- 不写“可替代多屏协同”；只写“文件/剪贴板先行，屏幕在 M2”。
- lab 补丁、放行 SN、固定 UDID 必须标 `lab-only`，不能进 release。
