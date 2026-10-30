# DHB External Agent Handoff Prompt

> 用途：把 DHB 交给另一个 Agent/工作站，在它自己的工作区复制、实现、验证、提交。  
> 控制面绝对路径：`C:\Deepin-HarmonyOS-Bridge\CONTROL-PLANE.md`  
> 完整源码 bundle：`C:\Deepin-HarmonyOS-Bridge\build\DHB-full.bundle`（生成后）

## 可直接粘贴的提示词

```text
你是 DHB（Deepin-HarmonyOS-Bridge）的外部实现 Agent。不要直接修改源工作区
C:\Deepin-HarmonyOS-Bridge；先把项目复制到你自己的工作区，例如 D:\work\dhb-handoff。

第一步：读控制面
- 优先读 C:\Deepin-HarmonyOS-Bridge\CONTROL-PLANE.md
- 如果该路径不可见，从 GitHub 克隆：
  https://github.com/THUDou/Deepin-HarmonyOS-Bridge
  或从 bundle：
  C:\Deepin-HarmonyOS-Bridge\build\DHB-full.bundle
- 再按顺序读：
  docs/17-platform-taxonomy.md
  docs/08-verification-log.md
  deepin/README.md
  deepin/org.dhb.Bridge.xml
  docs/16-skill-study-openharmony-kaihong.md
  docs/13-repo-plan.md
  README.md

第二步：确认环境并选一个任务（不要同时开三条大线）
- H1（默认，需 Deepin 25 + DTK6）：在 deepin/dhb-tray 实现 DTK6 DDE tray，
  并给 dhb-daemon 加 D-Bus MVP：GetStatus/ListDevices/GetReceiveDir/
  SetReceiveDir/Pair/SendFile/CancelTransfer。tray 只通过 D-Bus 调 daemon，
  不在 GUI 里重写协议。
- H2（需 DevEco/hvigor + OH/Kaihong 设备）：HAP 只用标准 ArkTS/ArkUI，
  不引入华为 Kit/AGC；在 OpenHarmony/KaihongOS 上做安装/启动验证。
- H3（需 Linux + Docker/KLink）：补 KLink lab 的 GetAllNodeDeviceInfo、
  OpenSession、字节通道验证；只写 Linux↔Linux，不冒充 Gate A。
- H4（任何 Linux）：TLS 1.3 或传输加密，基于 ITransport，保持现有测试。

主线约束：
- Core = HAP/ArkTS + DHB TCP；Pro = KaihongOS/OpenHarmony 系统签名 +
  dsoftbus/device_auth。
- KLink 4.x 只做 legacy 4.x；OpenHarmony 6.0 做 modern 6.x；不假设
  4.x↔6.x 原生互通。
- KaihongOS 是厂商 OpenHarmony 发行版，不是 OHOS 原版；不假设 API/
  dsoftbus/签名完全一致。
- 纯血鸿蒙 PC 的“超级终端”是华为系统能力，不是三方 HAP API；不要写成
  DHB 已支持。
- 不引入华为 Kit/AGC/元服务/ArkUI-X 作为 Core 依赖。
- 不提交 third_party/build/HAP/证书/PSK/真实 UDID。

工作方式：
1. 在自己的工作副本建分支 handoff/<task>；
2. 先写最小可验证实现，不扩架构；
3. 本地跑：
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ctest --test-dir build --output-on-failure
   ./build/dhb selftest
4. H1 额外跑 busctl introspect 和 dde-tray-loader 加载验证；
   H2 额外跑 hvigor 构建与 OH/Kaihong 安装；
   H3 额外记录 KLink 发现/认证/session 日志；
   H4 额外跑 TLS 互操作测试。
5. 更新 docs/08-verification-log.md：只写实际跑过的结果，未跑写“未执行”；
6. 提交 commit，给出 diff 摘要、测试命令/结果、未验证项、下一步（≤3 条）。

禁止：
- 不要直接改 C:\Deepin-HarmonyOS-Bridge；
- 不要用 G: 盘上的 DevEco 路径作为构建前提；
- 不要把 “HAP 编译通过” 写成 “HAP 已安装/运行”；
- 不要把 “Linux↔Linux lab 认证” 写成 “Gate A/B 通过”；
- 不要为了过 CI 删除测试或降低断言。

交付：
- 分支/commit；
- 修改文件列表；
- 实际执行的命令与结果；
- 未验证项及原因；
- 是否影响 Gate A/B/超级终端口径。
```

## 最小路径

如果外部 Agent 只能做一件事，优先 **H1**：  
`deepin/dhb-tray`（DTK6） + `dhb-daemon` D-Bus MVP。  
这是当前产品化最缺、且不依赖 OH/Kaihong 真机的一环。
