# DHB HarmonyOS 客户端（Core 档）

这是 `DHB Core` 的 HarmonyOS NEXT 客户端骨架：用 TCP + DHB 协议连接 Deepin/UOS 上的 `dhb serve`，完成双向文件流转与文本剪贴板同步。

## 构建

环境（已验证）：

- DevEco Studio **26.0.0.851**
- SDK **26.0.0.105**（API 26）
- 工程：`compatibleSdkVersion=5.0.0(12)`，`targetSdkVersion=6.0.0(20)`
- hvigor **6.26.8**（DevEco 自带，`oh-package.json5` 不再声明 registry 插件）

1. 生成/刷新占位图标（仓库内已带一份占位图，可跳过）：

   ```bash
   python3 scripts/gen-assets.py
   ```

2. DevEco 打开 `ohos/`；`hvigor/hvigor-config.json5` 的 `modelVersion` 必须是 `6.0.0`，与 `oh-package.json5` 一致。
3. 真机签名：`File > Project Structure > Signing Configs > Automatically generate signature`。
4. 运行 `entry`。

CLI 构建（无需 `ohpm install`，使用 DevEco 自带 hvigor/plugin）：

```powershell
$deveco = 'G:\DevEco Studios\DevEco Studio'
$env:NODE_PATH = "$deveco\tools\hvigor"
$env:JAVA_HOME = "$deveco\jbr"
Set-Location C:\Deepin-HarmonyOS-Bridge\ohos
& "$deveco\tools\hvigor\bin\hvigorw.bat" assembleApp `
  --mode project -p product=default -p buildMode=debug --no-daemon
```

产物：

```
entry/build/default/outputs/default/entry-default-unsigned.hap
```

已验证 `BUILD SUCCESSFUL`（209247 字节，`org.dhb.bridge`，minAPI 12 / targetAPI 20）。
当前**未签名**（`No signingConfig found`）；真机安装前需配置签名。`hdc list targets` 为空，尚未做真机运行。

## 使用

1. 在 Deepin/UOS 上启动：

   ```bash
   dhb serve --dir ~/DHB
   ```

   记下输出的 64 位 hex `pairing key`。
2. 手机与电脑在同一 Wi-Fi / 局域网。
3. 在 App 里填写电脑 IP、端口 `47821`、pairing key，点"连接"。
4. "发送文件"从系统文件选择器选文件；"发送剪贴板"同步当前文本。
5. 电脑端 `dhb serve` 收到文件会打印落盘路径。
6. 验证电脑 → 手机：用 `dhb serve --send-on-connect ~/Pictures/demo.jpg` 启动，手机连上后会收到该文件。

## 已知限制（v0.1）

- **仅 TCP 明文**：当前用于局域网开发验证；生产版必须 TLS 1.3。不要在不可信网络使用。
- **没有后台常驻**：App 退到后台后连接可能被系统回收；M2 用长时任务/前台服务。
- **发现功能实验性**：依赖 `@ohos.net.socket` 的 UDP 广播与 `remoteInfo`；部分设备/权限下不可用，可手动填 IP。
- **没有 dsoftbus**：零售 NEXT 三方应用没有公开的任意字节 dsoftbus API；Pro 档走系统签名 + NAPI，见 `docs/02-architecture.md`。
- **没有屏幕协同**：Core 档的"Deepin → 鸿蒙副屏"在 M2；本版本只有文件/剪贴板。
- **设备 ID 不持久**：当前由 `deviceInfo.productModel/brand` 派生，同一型号可能冲突；M2 用 preferences 持久化。
- **未签名**：HAP 编译通过但未签名；`hdc list targets` 为空，真机安装/运行未验证。

## 代码结构

```
entry/src/main/ets/
├─ core/Protocol.ets     帧/消息/CRC/SHA-256/HMAC（与 C++ 等价）
├─ core/BridgeClient.ets TCP、握手、双向文件、剪贴板
├─ core/Discovery.ets    UDP 发现
├─ pages/Index.ets       UI
└─ entryability/         UIAbility
```

## 与 C++ 的协议同步

`Protocol.ets` 必须与 `include/dhb/protocol.h`、`src/protocol.cpp` 保持一致。修改协议时：

1. 先改 `docs/03-protocol.md`；
2. 改 C++ 与 ArkTS 两端；
3. 跑 `ctest`；
4. 真机对传一次 1 GiB 文件，校验 SHA-256。
