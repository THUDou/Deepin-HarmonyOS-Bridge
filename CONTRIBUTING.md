# 贡献指南

DHB 还在早期，欢迎两类贡献：**真机验证日志**和**小而完整的纵向切片**。

## 开发环境

### Linux（主目标）

```bash
sudo apt install build-essential cmake git
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Deepin 25 / UOS 25 / openKylin 2.0 均可。Klink 适配另见 `docs/06-klink-dsoftbus-notes.md`。

### Windows（仅核心/测试）

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Windows 上 `TcpTransport`/UDP 发现是 stub；核心协议、文件引擎、内存传输测试可用。

### HarmonyOS

DevEco Studio（API 12），打开 `ohos/`，先运行 `python3 scripts/gen-assets.py`。真机签名自行配置。

## 代码规范

- C++17；`clang-format`（仓库将提供 `.clang-format`）；行宽 100；`namespace dhb`。
- 不用异常做控制流；错误用 `bool + std::string* err`。
- 所有网络输入必须边界检查；新增解析函数必须有对应测试向量。
- ArkTS 严格模式：不用 `any`；所有类字段初始化；协议代码与 C++ 注释互链。
- 提交信息：`area: short summary`（如 `protocol: reject trailing bytes`）。
- 每个 commit 加 `Signed-off-by`（DCO）：`git commit -s`。

## 协议变更流程

1. 先改 `docs/03-protocol.md`，说明兼容性影响。
2. 改 C++（`include/dhb/protocol.h`、`src/protocol.cpp`）。
3. 改 ArkTS（`ohos/entry/src/main/ets/core/Protocol.ets`）。
4. 加测试向量（CRC/SHA/消息 roundtrip/损坏帧）。
5. 跑 `ctest`；真机对传 1 GiB 文件。
6. 不兼容变更必须升 `version`，并在文档写迁移说明。

## 真机验证

最有价值的 PR 是"设备矩阵日志"：

- 设备型号、OS 版本、内核、桌面会话（Wayland/X11）；
- `dhb serve` / HAP 日志；
- 发现/配对/传输/断线重连结果；
- 失败时的错误码与抓包（脱敏）。

请放在 `docs/field-reports/`（新建目录）或 issue 中。

## 不接受的贡献

- 逆向/复制华为闭源"多屏协同"代码或二进制；
- 引入 GPL/AGPL 代码到核心（Apache-2.0 不兼容）；
- 未经说明的遥测/云上传；
- 破坏 `--insecure` 显式警告的"便利性"改动；
- 大规模格式化/重命名混在功能 PR 中。

## 报告问题

- Bug：复现步骤、版本、日志、设备信息。
- 功能：先开 issue 说明场景与能力档（Core/Pro），避免直接大 PR。
- 安全：见 `SECURITY.md`，不要公开 issue。

## 许可证

贡献即同意以 Apache-2.0 授权，并确认你有权提交该代码。第三方代码需在 `NOTICE` 中注明。
