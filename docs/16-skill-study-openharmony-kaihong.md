# SKILL 研读：以 KaihongOS / OpenHarmony 为主，Huawei HarmonyOS 为辅

> 研读时间：2026-10-30。  
> 当前 WSL、DevEco、Docker 不可用，所以这份文档只记录**技能边界与选型**，不代表任何命令已经执行。

## 0. 结论

1. **`deveco-cli` 和 `harmonyos-agent-skills` 都是华为 HarmonyOS 工具链视角**，可以用来构建/调试通用 HAP，但不能拿来“构建 KaihongOS/OpenHarmony 系统”，也不能替代 dsoftbus/device_auth 的源码适配。
2. **DHB 的 HAP 只使用标准 ArkTS/ArkUI**；华为 Kit、AGC、元服务、ArkUI-X、账号/支付/推送等能力不进入 Core 依赖。
3. **Deepin 侧优先 DTK6 + DDE tray**，Linyaps 只作为分发层；不要把桌面 UI 做成华为生态绑定。
4. **Linyaps 的首选路径是 `linyaps-debian-rules-to-linyaps`**：仓库已有 `packaging/debian/`，不是“只有二进制”的项目，所以暂不用 `linyaps-app-packaging-generator`。
5. **KaihongOS/OpenHarmony 的系统级验证仍以 OpenHarmony 6.0 源码为准**；DevEco/deveco-cli 不参与 dsoftbus/HiChain 的构建。

## 1. 技能分层与取舍

| Skill | 定位 | DHB 用法 | 判断 |
|---|---|---|---|
| `deveco-cli` | DevEco/hvigor/ohpm/hdc/emulator 的 CLI 包装 | 可选：构建 HAP、查设备、拉日志 | **谨慎**，只做 HAP 工具，不碰系统层 |
| `harmonyos-agent-skills` | 115 个华为 HarmonyOS 专项技能的总入口 | 只取 ArkTS/ArkUI/Hvigor/hdc/hilog/测试中通用部分 | **强过滤**，华为 Kit/AGC/元服务不引入 |
| `dtk-development` | Deepin/UOS v25 DTK5/DTK6 桌面应用 | `dhb-gui`、配对窗口、设置页 | **采用**，固定 DTK6/Qt6 |
| `dde-tray-development` | dde-tray-loader 托盘/快捷面板插件 | 托盘状态、打开主窗、暂停接收 | **优先采用**，MVP 第一个 UI |
| `dde-control-center-development` | 控制中心设置插件 | 自启动/接收目录/发现开关 | **第二阶段** |
| `dde-shell-development` | Dock/顶栏/侧栏 Applet、LayerShell | 不做 | **暂不采用**，避免扩大插件面 |
| `linyaps-development` | Linyaps 构建/运行/发布 | `linglong.yaml`、`${PREFIX}`、Base/Runtime | **采用**，但先包 CLI |
| `linyaps-debian-rules-to-linyaps` | 从 `debian/` 规则生成玲珑配置 | 复用 `packaging/debian` | **首选转换路径** |
| `linyaps-app-packaging-generator` | 二进制 deb/tar/AppImage 转玲珑工程 | 当前有源码，不用 | **暂不使用** |
| `linyaps-packaging-runner` | 已有模板的批量出包/CI | 发布流水线 | **后期采用** |

## 2. 华为 HarmonyOS 的边界

`deveco-cli` 本机是 **1.2.0-stable**，实际可用 11 个子命令（`build/run/update/device/emulator/skills/log/create/init/serve/docs`）；上游 1.3.x 的 `check/ui/verify/signature/auth` 本机不可用。  
这意味着：

- 可以用它构建/安装/看 HAP，但**不要**把 `signature/auth` 当成已经可用；签名需要 AGC/DevEco 图形界面，且只适用于华为 HarmonyOS。
- `devecocli build` 走的是 hvigor；它不会编 `communication_dsoftbus`、`security_device_auth`，也不会生成 KaihongOS 系统签名。
- 当前工程的 `compileSdkVersion=26.0.0.105`、`targetSdkVersion=6.0.0(20)` 是 **DevEco 26 / HarmonyOS SDK** 的事实，不等于 OpenHarmony 6.0 的 API/ABI 已经验证。

`harmonyos-agent-skills` 的 115 个技能里，**只取以下通用类**：

- `hmos-arkui-develop-skill`、`hmos-arkts-knowledge-retriever`：ArkTS/ArkUI 语法与组件；
- `deveco-studio-hvigor`、`deveco-studio-hilog`、`deveco-studio-codelinter`：构建、日志、静态检查；
- `hmos-local-test` / `hmos-instrument-test`：测试方法；
- 多设备适配/性能/内存中与 OpenHarmony 共通的通用原则。

**不进入 DHB Core**：

- Account Kit、Map Kit、Push Kit、Payment Kit、Scan Kit、Ads Kit、Live View；
- AppGallery Connect 上架/AGC 元数据；
- 元服务/ASCF、Taro/uni-app 转元服务；
- ArkUI-X/Android/iOS 迁移；
- 任何依赖华为账号、云证书、AGC 权限的签名/上架链路。

## 3. KaihongOS / OpenHarmony 优先

| 层 | 以谁为准 | 说明 |
|---|---|---|
| HAP UI/协议 | 标准 ArkTS/ArkUI | 不依赖华为 Kit；bundle `org.dhb.bridge` |
| HAP 构建 | hvigor/DevEco 仅作工具 | 产物需在 KaihongOS/OH 上复验，不能只看 DevEco 编译通过 |
| 系统服务/dsoftbus | OpenHarmony 6.0 `communication_dsoftbus` | DevEco/deveco-cli 不参与 |
| 认证/HiChain | OpenHarmony 6.0 `security_device_auth` | 与华为 AGC 签名无关 |
| 系统签名 | KaihongOS/OH 系统签名 | 不使用华为 AGC profile |
| Linux 桌面 | DTK6/DDE + Linyaps/deb | 与华为工具链无关 |

对 KaihongOS 的验证顺序应是：

1. HAP 用标准 API 构建；
2. KaihongOS 侧用系统签名安装/运行；
3. 系统级 dsoftbus/device_auth 由 OH 6.0 源码适配；
4. **不要**用 `deveco-cli signature` 或 AGC 证书冒充 KaihongOS 签名。

## 4. Deepin 25 客户端路由

```text
dhb-daemon（现有 C++ 核心）
   │ D-Bus: org.dhb.Bridge
   ├── dhb-tray       dde-tray-development（第一个 UI）
   ├── dhb-gui        dtk-development（DTK6/Qt6）
   └── dde-control-center 模块（第二阶段）
```

- `dtk-development`：先确认 DTK6/Qt6；用 `DApplication`、DConfig、D-Bus、DCI 图标；不要 DTK5/Qt5 混编。
- `dde-tray-development`：`PluginsItemInterfaceV2`，`Type_Tray`/`Type_Quick` 互斥；不要用 `QSystemTrayIcon` 冒充 DDE 托盘。
- `dde-control-center-development`：`DCC_FACTORY_CLASS` + `DdeControlCenter` target；不要把旧版插件基类套进来。
- `dde-shell-development`：`DApplet`/`DPanel` 与 tray 是两套体系；MVP 不做，避免同时踩两套插件框架。

## 5. Linyaps 路线

仓库已有 `packaging/debian/`，所以：

```text
首选：linyaps-debian-rules-to-linyaps
  → 解析 debian/control + rules
  → 生成 linglong.yaml
  → ll-builder build

后续：linyaps-packaging-runner
  → 版本迭代/批量出包/CI
```

硬约束：

- 所有产物安装到 `${PREFIX}`，不能写 `/usr`；
- Base/Runtime 默认 `org.deepin.base/25.2.2` + `org.deepin.runtime.dtk/25.2.2`，必须成对；
- `sources` 固定 commit/digest，不用分支名；
- 首个包先打 `dhb` CLI；`dhb-tray`/`dhb-gui` 等 DTK 工程稳定后再进玲珑；
- `linyaps-app-packaging-generator` 只在“没有源码、只有 deb/AppImage/tar”时使用，当前不适用。

## 6. 使用这些 Skill 的规则

1. 先读对应 `SKILL.md`，只读当前任务需要的 reference，不一次加载 115 个专项。
2. 华为 Kit/AGC 相关技能默认不选；只有明确做华为商店分发时才考虑。
3. 任何 HAP 结论必须注明是 DevEco 编译、模拟器运行还是 KaihongOS/OH 真机运行。
4. 任何 DTK/Linyaps 结论必须注明是否在 Deepin 25 上实际构建/运行。
5. 缺 WSL/DevEco/设备时直接写“未执行”，不拿静态阅读冒充验证。

## 7. 当前未执行

- `devecocli` 实际 build/run：未执行（DevEco/环境不可用）。
- `harmonyos-agent-skills` 专项技能：仅研读路由与边界，未执行 115 个子技能。
- DTK6 tray：**最小插件已编译并由 trayplugin-loader 加载**；完整 DDE 面板/`dhb-gui` 未完成。
- Linyaps：`ll-builder` 1.14.3 已实际完成 `dhb` CLI 的 build/export（`--skip-run-container`）；runtime check 未过。
- KaihongOS/OH 真机：无设备。

这些是环境前置，不是代码结论。
