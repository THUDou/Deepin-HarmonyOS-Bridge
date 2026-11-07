# DHB 验证记录

> 目的：记录“哪些已经证明、哪些还没有”，避免把设计当现实。
> 最近更新：2026-10-30

## 1. 环境

| 项 | 值 |
|---|---|
| 主机 | Windows 11 (22631) |
| 构建 A | CMake 3.29 + Ninja + MinGW g++ 16.1（Windows 目标） |
| 构建 B | MSYS2 POSIX g++ 16.2.0（`x86_64-pc-msys`，用于验证 POSIX 分支） |
| HarmonyOS | DevEco Studio **26.0.0.851** + SDK **26.0.0.105**；HAP 编译通过（unsigned）；无真机连接 |
| G 盘工具 | `G:\DevEco Studios\command-line-tools`：hvigor **6.26.9**、SDK **26.0.0.38**；`G:\DHB-Lab` 为唯一新增工作目录 |
| VM | VMware Workstation 17.5.2：Deepin-25.2.0（192.168.128.11）与 KaihongOS-6.1（192.168.128.10）在跑；Kaihong 的 hdc/SSH/guest ops 未连通 |
| Linux 容器 | Ubuntu 22.04 / g++ 11.4 / CMake 3.22（Docker Desktop，Linux 6.6 WSL2 内核）；**构建/测试/传输/发现已跑通** |
| Deepin 25 WSL | **Deepin 25 (crimson)**，g++ 12.3.0 / CMake 3.31.4；**构建 + ctest + selftest 已通过** |
| Linux 真机 | **未执行**（Deepin/UOS 实机、内核/PipeWire/uinput 未验） |
| KLink / KaihongOS | KLink 4.x host build + 两容器 COAP 发现已验证；**KaihongOS/OH 真机未执行** |
| 版本基线 | KLink 4.x（最新提交 2025-08-01）；OpenHarmony 6.0-Release（2025-09-10）；openEuler 公开包 3.2 |

## 2. 已通过

### 2.1 Windows / MinGW（核心与内存传输）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\dhb.exe selftest
python scripts/check-ohos-config.py
```

结果：

- 构建通过（`dhb_core`、`dhb`、`dhb_tests`）。
- `ctest`：**1/1 通过**。
- `dhb selftest`：通过。
- OHOS：20 个必需文件、11 个 JSON/JSON5 配置解析通过。
- **断点续传**：预置 `.dhb-<sha256>.part`（700 KiB 文件的前 204807 字节），服务端接受
  `FILE_ACCEPT(offset=204807)`，最终内容逐字节一致，`.part` 清理；测试 `testResumeFileTransfer` 通过。

> Windows 下 `TcpTransport` 是 stub，所以这一步只覆盖协议、加密、内存传输、文件引擎。

### 2.2 MSYS2 POSIX（真实 socket + 双向文件）

编译（MSYS2 需要显式 POSIX feature macro，Linux 默认不需要）：

```bash
g++ -std=c++17 -D_POSIX_C_SOURCE=200809L -O0 -g -Iinclude \
  src/crypto.cpp src/protocol.cpp src/transport.cpp src/session.cpp src/discovery.cpp \
  tests/dhb_tests.cpp -pthread -o build/msys/dhb_tests.exe
```

结果：**`all DHB tests passed`**。覆盖：

- SHA-256 / HMAC / HKDF / CRC-32C 标准向量；
- 帧编解码、损坏帧、超长帧、截断帧；
- 文件名净化（`../`、反斜杠、控制字符）、重名处理；
- 内存传输：握手、错误 PSK、剪贴板、PING、双向文件、`HELLO.flags` 4x/6x profile 位；
- **真实 TCP**：`TcpListener` + `TcpTransport` 握手、剪贴板、客户端→服务端文件、服务端→客户端文件，内容逐字节比较。

额外用 `-D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS` 重编译运行，仍全部通过。

### 2.3 CLI 真实 TCP 回环 + UDP 发现

```bash
# 终端 A
dhb serve --port 47921 --discovery-port 47922 --psk <64-hex> --dir /tmp/dhb-recv2

# 终端 B
dhb discover --discovery-port 47922 --timeout 1500
dhb send --host 127.0.0.1 --port 47921 --psk <64-hex> README.md
cmp README.md /tmp/dhb-recv2/README.md
```

结果：

- `discover` 找到 `127.0.0.1:47921`（主机名、设备 ID 正确）；
- `send` 完成 PSK 认证并传完约 9.8 KB 的 README；
- `cmp` 通过（逐字节一致）。

### 2.4 反向推送：`serve --send-on-connect` + `dhb recv`

```bash
# 终端 A：客户端一连上就推 README.md
dhb serve --port 47941 --psk <64-hex> --send-on-connect README.md

# 终端 B：作为客户端接收
dhb recv --host 127.0.0.1 --port 47941 --psk <64-hex> --dir /tmp/recv4 --timeout 20
cmp README.md /tmp/recv4/README.md
```

结果：

- `serve` 打印配对 key 与连接日志；
- `recv` 收到 `README.md`，落盘路径正确；
- `cmp` 通过。

这条路径对 HAP 同样适用：HAP 就是“带 UI 的 recv”。

### 2.5 认证-only：`dhb pair`

```bash
dhb serve --port 47951 --psk <64-hex> --dir /tmp/pair-recv
dhb pair --host 127.0.0.1 --port 47951 --psk <64-hex>
```

结果：

```
authenticated: <host> (Deepin/UOS Linux)
  device id: ...
  caps     : file|clipboard
  profile  : app/unspecified
pair_rc=0
```

这条命令只做 HELLO + PSK 认证，不传文件；对应“至少先认证”的 App 保底门。

### 2.6 HarmonyOS HAP 编译

```powershell
$deveco = 'G:\DevEco Studios\DevEco Studio'
$env:NODE_PATH = "$deveco\tools\hvigor"
$env:JAVA_HOME = "$deveco\jbr"
Set-Location C:\Deepin-HarmonyOS-Bridge\ohos
& "$deveco\tools\hvigor\bin\hvigorw.bat" assembleApp `
  --mode project -p product=default -p buildMode=debug --no-daemon
```

结果：**BUILD SUCCESSFUL**（42 tasks）。

- 产物：`ohos/entry/build/default/outputs/default/entry-default-unsigned.hap`（209247 字节）。
- SHA-256：`EA9BE43EAC78B5F788B7314BC95345FDEAD7BE48F2675620B8D0506F81BDD5B1`。
- HAP `module.json`：bundle `org.dhb.bridge`，`compileSdkVersion=26.0.0.105`，`minAPIVersion=50000012`，`targetAPIVersion=60000020`，debug。
- 编译修复：`ByteWriter` 字段/方法重名、`deviceInfo.model` → `productModel`、`openSync()` 返回 `File` 取 `.fd`。
- 未签名：`No signingConfig found`；`hdc list targets` 为空，未做真机安装/运行。

### 2.7 Linux 容器：DHB 构建/测试/传输/发现

Docker `ubuntu:22.04` + g++ 11.4 + CMake 3.22：

```bash
docker run --rm -v "$PWD:/work" -w /work ubuntu:22.04 \
  bash -lc 'apt-get update && apt-get install -y g++ cmake && \
  cmake -S . -B build-linux && cmake --build build-linux -j4 && \
  ctest --test-dir build-linux --output-on-failure'
```

结果：

- `dhb` / `dhb_tests` 构建通过；
- `ctest` **1/1 passed**；
- `scripts/dev-loopback.sh`：`README.md` 10838 字节逐字节校验通过；
- UDP 发现：`dhb discover` 找到 `127.0.0.1:47961`，设备 ID/主机名正确。

### 2.8 KLink 4.x Linux host build + 双容器发现

```bash
# 在 Linux 文件系统（不能是 Windows 挂载盘）上：
scripts/build-klink-lab.sh
```

结果：

- `BUILD_RC=0`，产出 `connectivity_softbus_server`、`connectivity_devicemgr_cmd` 和 dsoftbus/devicemgr/base 库；
- 服务端日志：`network interface name is 1:eth0`；
- 两个 Docker 容器（`klink-a`/`klink-b`）各自启动 server + discovery：
  - A 看到 `klink-b(2222...)[172.19.0.3]`
  - B 看到 `klink-a(1111...)[172.19.0.2]`
- 这说明 **KLink 4.x 的 COAP 发现、deviceId/peerAddr 交换在 Linux 上可用**。

认证尝试：

- **PIN 路径未通过**：A 发起 `AuthenticateDevice(PIN)` 返回 0；B 显示 `Auth PinCode:[398754]`；A 随后 `OnAuthResult status: 5 reason: -20006`（`ERR_DEV_CENTER_INPUT_PARA_INVALID`）。判断是 Linux CLI 的 PIN/UI 状态机未走到 `OnInputAuthInfo`。
- **CLOUD_SN lab 路径通过**：A `AuthenticateDevice(type 0)`，B `OnCheckDeviceSNVaild` 放行，A `OnAuthResult status: 7 reason: 0`（`AUTH_REQUEST_FINISH`），双方 `OnSoftbusDeviceFound isOnline: 1` 并拿到 `networkId`。这只是 lab 放行，不是生产策略，也不等于 Gate A。详见 [KLink Linux lab](12-klink-linux-lab.md)。

### 2.9 Deepin 25 WSL（真实 Deepin 发行版）

```bash
wsl -d deepin25 -u root
# 只复制源码，不带 build/、third_party/
cp -a /mnt/c/Deepin-HarmonyOS-Bridge/{include,src,apps,tests,CMakeLists.txt} /root/dhb/
cd /root/dhb
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dhb selftest
```

结果：

- `CMAKE_RC=0`，`BUILD_RC=0`；
- `ctest` **1/1 passed**；
- `dhb selftest` 通过；
- 环境：Deepin 25 / g++ 12.3.0 / CMake 3.31.4；
- WSL 的 Windows PATH 含已失效的 `G:\DevEco...`，会打印 `Failed to translate` 警告并让 shell 返回 1，但构建/测试本身成功；这不是 Deepin 构建失败；
- 已从 Deepin 官方源安装开发工具：
  - `libdtk6widget-dev` / `libdtk6core-dev` **6.7.47.1**
  - Qt6 **6.8.0**
  - `dde-tray-loader-dev`（`PluginsItemInterfaceV2` 头文件）
  - `linglong-builder` **1.14.3** / `linglong-bin`
- `pkg-config --modversion dtk6widget` → `6.7.47`；`ll-builder --version` → `1.14.3`；
- 安装时 `linglong` 的 PackageManager D-Bus 激活失败（WSL 没有完整 systemd/D-Bus），但 `ll-builder` 二进制可用；
- `ll-builder` 后续实际构建结果见 2.10。

### 2.10 Deepin 桌面 MVP：D-Bus + DTK6 tray + Linyaps CLI

构建：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDHB_BUILD_DESKTOP=ON
cmake --build build -j4
```

结果：

- `dhb-daemon`、`dhb-tray.so` 编译通过；
- `ctest` **1/1 passed**；
- D-Bus 接口实测（`dbus-run-session`）：
  - `GetStatus` / `ListDevices` / `GetReceiveDir` / `SetReceiveDir` 可见；
  - `SetReceiveDir("/tmp/dhb-recv2")` 返回 `true`，`GetReceiveDir` 返回新路径；
  - `Pair("127.0.0.1", 47831, PSK)` 返回 `true`；
  - `GetStatus.connected=true`，`ListDevices` 返回一个 Deepin peer；
  - `SendFile("/tmp/dhb-send.txt")` 返回 `true`，服务端保存 22 字节，内容逐字节一致。
- DTK6 tray：
  - `QPluginLoader` ABI check：`loaded "dhb-tray" "DHB"`；
  - `trayplugin-loader -p .../dhb-tray.so -g dhb` 日志：
    `Loaded plugin: "dhb-tray"`、`itemAdded: "dhb-tray"`；
  - 这是 **dde-tray-loader 的插件加载验证**，不是完整 DDE 会话/面板视觉验证（WSL 无 DDE 桌面）。
- Linyaps：
  - `ll-builder build -f deepin/linglong.yaml --skip-run-container` → `Build completed successfully`；
  - `ll-builder export -f deepin/linglong.yaml --layer` 产出
    `org.dhb.cli_0.1.0.1_x86_64_binary.layer`（157K，
    SHA-256 `43646eb9...e2feb`）和 develop layer（2.2M，
    SHA-256 `0cb88ce9...4732f`）；
  - 不带 `--skip-run-container` 时，`Runtime Check` 在 WSL 嵌套容器里报
    `remount failed after all fallbacks`，所以 **运行验证未通过**；这只是 WSL 容器限制，不能写成 Linyaps 已运行。

### 2.11 HAP 可移植性收口（2026-11-05 已重编）

- `hello.caps` 只声明 `FILE | CLIPBOARD`，不再虚报 `SCREEN_SINK | INPUT`；
- 接收续传的 part 文件改为 `.dhb-<sha256>.part`，与 Linux 核心一致，
  不再用 transfer id；
- UI 副标题改为 `HarmonyOS / OpenHarmony`；
- 增加 `ohos.permission.READ_PASTEBOARD` 声明（编译器仍会打印静态提示，
  但 manifest 已声明）。
- 在 G 盘 DevEco 命令行工具上重编：
  - hvigor **6.26.9**；
  - SDK **26.0.0.38**（`G:\DevEcoStudioData\AppData\Local\OpenHarmony\Sdk\26.0.0`）；
  - `assembleHap` → **BUILD SUCCESSFUL**（unsigned）；
  - 产物 `entry-default-unsigned.hap`，**209487 bytes**，
    SHA-256 `AE18B69A...FCBB87`。
- HAP 已用 OpenHarmony 测试证书签名，并在 KaihongOS 6.1.1.11 上安装、启动成功；
  详见 2.13。这里不再把“编译成功”和“已安装”混为一谈。

### 2.12 G 盘 / VM 操作记录

- 只在 G 盘新建了一个工作目录：`G:\DHB-Lab`（含 `logs`、`artifacts`）；
  没有改动 `G:\DevEco Studios` 等现有工具目录。
- 找到的工具：
  - `G:\DevEco Studios\command-line-tools\bin\hvigorw.bat` — hvigor **6.26.9**；
  - `G:\DevEco Studios\command-line-tools\sdk\default\openharmony` — API **26**；
  - `G:\DevEcoStudioData\AppData\Local\OpenHarmony\Sdk\26.0.0` — SDK **26.0.0.38**；
  - `G:\DevEco Studios\arm64-packages\kaihongos-devkit` — Kaihong devkit deb；
  - `G:\DevEco Studios\command-line-tools\...\toolchains\hdc.exe`。
- VM：
  - Deepin-25.2.0：`192.168.128.11`，已抓快照 `DHB-before-work-20261105`；
  - KaihongOS-6.1：`192.168.128.10`，已有 `KaihongOS快照`，并补抓
    `DHB-before-work-20261105`；
  - `vmrun list` 确认两台都在运行。
- hdc 实际端口是 **10178**（不是 8710）：宿主
  `hdc tconn 192.168.128.10:10178` → `Connect OK`，
  `hdc list targets` 显示该设备。
- 设备信息：`KaihongOS 6.1.1.11`，API **23**，`x86_64_system`。
- HAP 签名/安装：
  - 使用 SDK 自带 `OpenHarmony.p12` + `OpenHarmonyProfileRelease.pem`；
  - 生成 `G:\DHB-Lab\signing\dhb-profile.p7b`、`app-release-chain.pem`；
  - `hap-sign-tool.jar sign-app` → `entry-default-signed.hap`（246634 bytes）；
  - `hdc install` → **install bundle successfully**；
  - `aa start -a EntryAbility -b org.dhb.bridge` → **start ability successfully**；
  - DHB UI 已在 Kaihong 桌面显示。
- 注意：`READ_PASTEBOARD` 在 Kaihong 上被拒绝导致安装失败，已从 manifest 移除；
  因此 **发送剪贴板读取能力未验证**，文件传输不受影响。
- HAP 副本：`G:\DHB-Lab\artifacts\entry-default-unsigned.hap`，
  签名副本 `G:\DHB-Lab\signing\entry-default-signed.hap`。

### 2.13 Linux ↔ Kaihong HAP：TCP/PSK 认证

- Linux 侧：Deepin 25 WSL 的 `dhb serve --port 47821 --psk dhb-test`；
- 网络：Windows 主机 `192.168.128.1:47821` 做 TCP proxy 到 WSL
  `172.20.47.26:47821`（VMware NAT 无法直接暴露 WSL 端口）；
- Kaihong HAP 设置 host `192.168.128.1`、port `47821`、PSK `dhb-test`；
- HAP 日志：

```text
TCP connected to 192.168.128.1:47821
peer: LAPTOP-AACOP06S (Deepin/UOS Linux)
authenticated with LAPTOP-AACOP06S
```

- 结论：**Linux↔Kaihong 的 TCP + PSK 认证已经真实打通**。
- 尚未完成：文件实际传输/接收（server 的 send-on-connect 测试文件路径曾失败，
  未重跑到成功），以及剪贴板读取、软总线、Gate A/B。

### 2.14 GitHub prerelease `v0.1.0-alpha.2`

- Release: <https://github.com/THUDou/Deepin-HarmonyOS-Bridge/releases/tag/v0.1.0-alpha.2>
- Source snapshot: `9a7444c`（完整 tracked source 已同步）
- Assets:
  - `dhb-0.1.0-kaihong-x86_64-signed.hap` — 246634 bytes，
    SHA-256 `e4e8b6a4...a6f8b9`
  - `dhb-0.1.0-kaihong-x86_64-unsigned.hap` — 209329 bytes，
    SHA-256 `6acceab3...b377b8b`
  - `SHA256SUMS`
- 签名使用 SDK 中的 **OpenHarmony test certificate**，不是生产证书。
- Release 说明明确标注：文件实传、华为真机、KLink/dsoftbus 未验证。

### 2.15 HAP UI v2（设备卡 / 附近设备 / 协作）

- 新增 HAP 产品化界面：可信设备卡、附近设备列表、手动连接、发送文件、
  剪贴板状态、运行日志。
- `hvigor assembleHap` **BUILD SUCCESSFUL**；签名后
  `install bundle successfully`。
- 首次 `aa start` 因 Kaihong 锁屏失败；手动解锁后再次 `aa start` →
  **start ability successfully**。
- UI v2 已在 Kaihong 桌面显示并截图：
  `G:\DHB-Lab\artifacts\hap-ui-v2.png`。
- 已看到：我的设备/尚未连接、连接 Deepin/UOS、附近设备 0 台、协作入口。
- UI v2 **未进入** `v0.1.0-alpha.2` release（release 里是旧 UI）。
- 下一步：文件实传；成功后打 `v0.1.0-alpha.3`。

### 2.16 DevEco 2in1（华为 ARM PC）模拟器验证

- DevEco 本地已有实例：`MateBook Pro`，**2in1**，
  HarmonyOS **7.0.0(26.0.0)**，序列号 `127.0.0.1:5555`，API **26**。
- 用 `hdc` 安装 UI v2 签名 HAP：
  - `install bundle successfully`
  - `aa start -a EntryAbility -b org.dhb.bridge` →
    **start ability successfully**
- UI v2 在 2in1 模拟器上真实显示：
  `G:\DHB-Lab\artifacts\emulator-2in1.jpeg`（3120x2080）。
- 截图里应用上方有“是否将应用自动添加至桌面？”系统弹窗，但 DHB 窗口本身可见：
  我的设备/尚未连接、连接 Deepin/UOS、附近设备、协作、运行日志。
- 这是 **Huawei 2in1 模拟器（ARM）**，不是华为实体机；不能写成“华为真机已验证”。
- 仍不等于功能交付：文件实传、双向剪贴板、屏幕/输入都还没完成。

### 2.17 Linux → Kaihong HAP 文件实传（单向，已通过）

- 时间：2026-11-07。
- Linux 侧：Deepin 25 WSL
  `dhb serve --port 47821 --psk dhb-test --send-on-connect /root/dhb/send.txt`；
- Kaihong HAP 通过 Windows TCP proxy `192.168.128.1:47821` 连接；
- server 日志：

```text
[session] connected: x86_64_system (HarmonyOS)
[file] sending on connect: /root/dhb/send.txt
[file] sent: /root/dhb/send.txt
[session] connection closed: connection closed
[session] disconnected: x86_64_system
```

- Kaihong 实际落盘：
  `/data/app/el2/100/base/org.dhb.bridge/haps/entry/files/received/send.txt.1794054132075`
- 源文件与接收文件 **18 bytes，SHA-256 完全一致**：

```text
635a6bfc1ce1855e37478ad87b7e58ea8d884e42d12de5d0fe381ed378be700e
```

- 证据：
  - `G:\DHB-Lab\logs\transfer-linux-to-kaihong-20261107.log`
  - `G:\DHB-Lab\artifacts\kaihong-received-send.txt`
  - `G:\DHB-Lab\artifacts\kaihong-linux-to-hap.png`
- 结论：**Linux → Kaihong HAP 的文件内容传输已真实通过。**
- 仍未完成：HAP → Linux 反向文件、双向剪贴板、屏幕/输入。

### 2.18 Kaihong HAP → Linux 反向文件（已通过，双向闭环）

- 修复：`runServe` 原来只是空转等待；HAP 空闲会话会被对端关闭，UI 却还显示“已连接”。
  新增长连接 keepalive（每 3 秒 `session.ping()`，失败即退出 reader），并重建
  `build-linux-r3/dhb`。
- HAP 通过文件选择器发送：
  `/storage/media/100/local/files/Docs/Download/dhb-from-kaihong.txt`
- server 日志：

```text
[session] connected: x86_64_system (HarmonyOS)
[session] incoming file: dhb-from-kaihong.txt (19 bytes, resume at 0)
[file] saved: /root/dhb/recv/dhb-from-kaihong.txt
```

- Linux 接收文件 **19 bytes**，源/收 SHA-256 完全一致：

```text
60bb2065ae4517f983d249cf631655f906b83fb375a793417ba0573188058bad
```

- 证据：
  - `G:\DHB-Lab\logs\transfer-kaihong-to-linux-20261107.log`
  - `G:\DHB-Lab\artifacts\kaihong-hap-to-linux-received.txt`
  - `G:\DHB-Lab\artifacts\kaihong-hap-to-linux.png`
- `build-linux-r3/dhb_tests` → `all DHB tests passed`。
- 结论：**Linux ↔ Kaihong HAP 的文件双向传输已闭环。**
- 仍未完成：双向剪贴板、屏幕/输入、SoftBus6 host build、openEuler RPM、华为实体机。

## 3. 还没有证明

| 项 | 状态 | 下一步 |
|---|---|---|
| Deepin 25 WSL：CMake + ctest + selftest | ✅ | `wsl -d deepin25` |
| Deepin 25 WSL：DTK6/Qt6/tray 头文件 | ✅ 6.7.47.1 / Qt6 6.8.0 | 可开始编译 tray |
| Deepin 25 WSL：`ll-builder` | ✅ 1.14.3（CLI） | 已 build/export，见 2.10 |
| Deepin 25 WSL：D-Bus daemon + Pair/SendFile | ✅ | `scripts/test-dbus-*.sh` |
| Deepin 25 WSL：DTK6 tray 插件加载 | ✅（trayplugin-loader） | 无完整 DDE 面板 |
| Linyaps `dhb` CLI build/export | ✅（`--skip-run-container`） | runtime check 未过 |
| Deepin/UOS **实机** CMake + `dev-loopback.sh` | ❌ | 在真实 Deepin/UOS 硬件上跑 |
| Ubuntu 22.04 容器：CMake + ctest + TCP/UDP | ✅ | `build-linux` + `dev-loopback.sh` |
| KLink 4.x Linux host build（Ubuntu 容器） | ✅ | `scripts/build-klink-lab.sh` |
| KLink 4.x Linux ↔ Linux COAP 发现 | ✅ | 两容器，`unAuth` 设备可见 |
| KLink 4.x Linux↔Linux PIN 认证 | ❌ | `status 5 / -20006`，见 docs/12 |
| KLink 4.x Linux↔Linux CLOUD_SN lab 认证 | ✅ | `status 7 / reason 0`，双方有 `networkId` |
| KLink 4.x ↔ OH 4.x/HarmonyOS 4.x 发现/PIN/可信列表 | ❌ | Gate A（docs/10） |
| OpenHarmony 6.0 ↔ KaihongOS 6.x 认证/session | ❌ | Gate B（docs/10） |
| KaihongOS 6.1.1.11：标准 HAP 安装/启动 | ✅ | `hdc install` + `aa start`；见 2.13 |
| Linux（WSL）↔ Kaihong HAP：TCP/PSK 认证 | ✅ | HAP 日志 `authenticated with LAPTOP-AACOP06S` |
| Linux ↔ Kaihong HAP：文件传输 | ✅ 双向 | 18/19 bytes SHA-256 对账通过；见 2.17/2.18 |
| Linux 内核/PipeWire/uinput | ❌ | M0 的 Deepin 侧 demo |
| HarmonyOS HAP 编译（DevEco 26 / SDK 26） | ✅ 2026-11-05 signed | 209487 unsigned；签名后 246634；见 2.13 |
| Kaihong x86 VM：hdc 连接 | ✅ | `192.168.128.10:10178` |
| DTK6 tray 完整 DDE 面板 | ❌ | 只验证了插件加载，无 DDE 会话 |
| `dhb-gui` | ❌ | 未写/未编译 |
| Linyaps `dhb` 运行 | ❌ | WSL 容器 remount 失败 |
| HAP 真机签名/安装/运行 | ✅ Kaihong x86 | 华为实体 2in1 仍未验 |
| DevEco 2in1 模拟器（MateBook Pro / ARM） | ✅ 安装/启动 | HarmonyOS 7.0.0(26.0.0)，API 26；见 2.16 |
| 屏幕协同（采集/编码/解码/回控） | ❌（仅设计） | M2 |
| 性能（1080p30、CPU、文件吞吐） | ❌ | M2 基准 |
| 传输加密 | ❌（TCP 明文，仅开发） | M1 TLS 1.3 |

## 4. 结论

**核心协议、认证、文件引擎、发现、TCP 双向传输已经在 Windows/MSYS2、Ubuntu 22.04 容器与 Deepin 25 WSL 跑通；Deepin 25 上 `dhb-daemon` 的 D-Bus + Pair/SendFile 闭环、DTK6 tray 插件加载、`dhb` CLI 的 Linyaps build/export 已通过。HarmonyOS HAP 已在 G 盘 DevEco 工具链上重编、签名，并安装/启动于 KaihongOS 6.1.1.11（API 23，x86_64）和 DevEco **MateBook Pro 2in1 模拟器**（HarmonyOS 7.0.0，API 26，ARM）；HAP 通过 TCP proxy 连到 Deepin WSL `dhb serve`，完成 PSK 认证；Linux→Kaihong HAP（18-byte）和 Kaihong HAP→Linux（19-byte）文件均已落盘并通过 SHA-256 对账，双向文件闭环。仍缺：双向剪贴板、生产 PIN、华为实体 2in1、KLink 真机、Linyaps runtime、完整 DDE 面板。**

因此当前状态是：

> **可运行的 Linux/ArkTS 双端骨架 + 可复现的 POSIX 端到端证据**，不是“已在 Deepin 25 + KaihongOS 上验证过的 MVP”。

M0 的下一步不变：KLink 4.x 在 Deepin 25 上编出来并对上 OH 4.x；OpenHarmony 6.0 的现代 profile 对上 KaihongOS 6.x；先只要求“认证/可信关系”，不要求文件。

## 5. 链路状态速查

> 一句话：**没有“4.x、6.x 都通了”。4.x 只到 Linux↔Linux 容器发现 +
> CLOUD_SN lab 认证；6.x 还没跑；Linux↔HarmonyOS 没有真机验证。**

| 链路 | 发现 | 认证 | session/字节流 | 文件 | 当前结论 |
|---|---|---|---|---|---|
| KLink 4.x Linux↔Linux（两容器） | ✅ COAP | ⚠️ CLOUD_SN lab 通过；PIN ❌ | ❌ | ❌ | 只到 lab 可信关系 |
| KLink 4.x Linux↔OH 4.x/华为 4.x | ❌ | ❌ | ❌ | ❌ | Gate A 未过 |
| OpenHarmony 6.x host build | ❌ | ❌ | ❌ | ❌ | 只有源码 diff/rebase 报告 |
| OH 6.x ↔ KaihongOS 6.x | ❌ | ❌ | ❌ | ❌ | Gate B 未过 |
| KLink 4.x ↔ 6.x | ❌ | ❌ | ❌ | ❌ | 不假设原生互通 |
| DHB TCP Linux 同机/回环 | n/a | ✅ PSK | ✅ TCP | ✅ 逐字节校验 | 可运行 |
| DHB TCP 两台独立 Linux | n/a | 未单独测 | 未单独测 | 未单独测 | 不能宣称已跨机验证 |
| DHB TCP Linux ↔ HAP | n/a | ❌ HAP 未安装 | ❌ | ❌ | 代码路径有，未验证 |
| HAP ↔ HAP | n/a | ❌ | ❌ | ❌ | 未实现/未测 |

补充：

- KLink 4.x 的 `CLOUD_SN` 是 **lab 放行策略**，不是生产认证，也不等于 Gate A。
- 6.x 不能拿 openEuler 3.2 代替；OpenHarmony 6.0 源还没有 Linux host build。
- DHB 的 TCP 链路不依赖软总线；它是当前唯一可运行的文件/剪贴板路径。
- HAP 上次编译通过（unsigned），但 **没有安装到任何 HarmonyOS/KaihongOS 设备**，
  所以“Linux↔鸿蒙”目前只能写“未验证”，不能写“通了”。
