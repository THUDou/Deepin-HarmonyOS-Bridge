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
| Linux 内核/PipeWire/uinput | ❌ | M0 的 Deepin 侧 demo |
| HarmonyOS HAP 编译（DevEco 26 / SDK 26） | ✅ unsigned | 产物已生成 |
| DTK6 tray 完整 DDE 面板 | ❌ | 只验证了插件加载，无 DDE 会话 |
| `dhb-gui` | ❌ | 未写/未编译 |
| Linyaps `dhb` 运行 | ❌ | WSL 容器 remount 失败 |
| HAP 真机签名/安装/运行 | ❌ | 配置签名 + 连接真机/模拟器 |
| 屏幕协同（采集/编码/解码/回控） | ❌（仅设计） | M2 |
| 性能（1080p30、CPU、文件吞吐） | ❌ | M2 基准 |
| 传输加密 | ❌（TCP 明文，仅开发） | M1 TLS 1.3 |

## 4. 结论

**核心协议、认证、文件引擎、发现、TCP 双向传输已经在 Windows/MSYS2、Ubuntu 22.04 容器与 Deepin 25 WSL 跑通；Deepin 25 上 `dhb-daemon` 的 D-Bus + Pair/SendFile 闭环、DTK6 tray 插件加载、`dhb` CLI 的 Linyaps build/export 已通过。KLink 4.x host 栈已编译并完成两容器 COAP 发现 + CLOUD_SN lab 认证；HarmonyOS HAP 已用 DevEco 26 编译通过（unsigned）。仍缺：生产 PIN 认证、OHOS/KaihongOS 真机、HAP 安装运行、Deepin/UOS 实机、完整 DDE 面板、Linyaps 运行。**

因此当前状态是：

> **可运行的 Linux/ArkTS 双端骨架 + 可复现的 POSIX 端到端证据**，不是“已在 Deepin 25 + KaihongOS 上验证过的 MVP”。

M0 的下一步不变：KLink 4.x 在 Deepin 25 上编出来并对上 OH 4.x；OpenHarmony 6.0 的现代 profile 对上 KaihongOS 6.x；先只要求“认证/可信关系”，不要求文件。
