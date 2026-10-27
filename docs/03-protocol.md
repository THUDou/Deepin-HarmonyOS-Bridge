# DHB 线协议 v1

> 状态：v0.1 已实现（C++ 与 ArkTS 双端）。本文是唯一权威格式说明；改协议必须先改本文并升版本。
> 所有多字节整数均为 **小端序（little-endian）**。字符串为 **u16 长度 + UTF-8 字节**，长度不含长度字段本身。

## 1. 帧格式

每一帧 = 20 字节固定头 + 可变长 payload。

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 4 | `magic` | `0x31424844`，即 ASCII `DHB1` 的小端表示 |
| 4 | 1 | `version` | 当前 `1` |
| 5 | 1 | `type` | 见第 2 节 |
| 6 | 2 | `flags` | bit0 `ACK_REQ`，bit1 `MORE`，其余保留 |
| 8 | 4 | `stream` | 逻辑流 ID；0 = 控制流 |
| 12 | 4 | `length` | payload 字节数，最大 16 MiB (`0x01000000`) |
| 16 | 4 | `crc32c` | payload 的 CRC-32C（Castagnoli），不覆盖头部 |
| 20 | N | `payload` | 类型相关 |

规则：

- 接收方必须校验 `magic`、`version`、`length ≤ 16 MiB`、CRC；任一失败即断开会话，不做"尽力解析"。
- `length = 0` 合法（例如无参数的 `AUTH_OK`）。
- 未知 `type` 必须忽略（前向兼容），但不能忽略头部/CRC 错误。
- 帧之间无 padding、无分隔符；TCP 是字节流，按 `length` 精确读取。

CRC-32C 参数：poly `0x1EDC6F41`（反射实现用 `0x82F63B78`），init `0xFFFFFFFF`，xorout `0xFFFFFFFF`。测试向量：`"123456789"` → `0xE3069283`。

## 2. 帧类型

| 值 | 名称 | 方向 | payload |
|---:|---|---|---|
| `0x01` | `HELLO` | 双向 | `Hello` |
| `0x02` | `HELLO_ACK` | 保留 | — |
| `0x03` | `AUTH_CHALLENGE` | S→C | 16 字节 nonce |
| `0x04` | `AUTH_RESPONSE` | C→S | 32 字节 HMAC |
| `0x05` | `AUTH_OK` | S→C | 空 |
| `0x06` | `AUTH_FAIL` | S→C | `u8 code` + `string reason` |
| `0x10` | `PING` | 双向 | `u64 timestamp_ms` |
| `0x11` | `PONG` | 双向 | `u64 timestamp_ms` |
| `0x20` | `FILE_OFFER` | 发送→接收 | `FileOffer` |
| `0x21` | `FILE_ACCEPT` | 接收→发送 | `u32 id` + `u64 offset` |
| `0x22` | `FILE_REJECT` | 接收→发送 | `u32 id` + `u8 code` + `string reason` |
| `0x23` | `FILE_CHUNK` | 发送→接收 | `u32 id` + `u64 offset` + 原始字节 |
| `0x24` | `FILE_DONE` | 发送→接收 | `u32 id` + 32 字节 SHA-256 |
| `0x25` | `FILE_RESULT` | 接收→发送 | `u32 id` + `u8 status` + 32 字节 SHA-256 + `string message` |
| `0x26` | `FILE_CANCEL` | 双向 | 保留（v1 未使用） |
| `0x27` | `FILE_PROGRESS` | 接收→发送 | `u32 id` + `u64 transferred` + `u64 total`（保留） |
| `0x30` | `CLIPBOARD_SET` | 双向 | `string text`（≤ 60000 字节） |
| `0x31` | `CLIPBOARD_DATA` | 保留 | 二进制剪贴板（图片等），v1 未使用 |
| `0x40` | `INPUT_EVENT` | 保留 | 鼠标/键盘/触摸，v1 未使用 |
| `0x50` | `SCREEN_CONFIG` | 保留 | 分辨率/编码/帧率，v1 未使用 |
| `0x51` | `SCREEN_FRAME` | 保留 | 编码帧，v1 未使用 |
| `0x52` | `SCREEN_KEYFRAME_REQUEST` | 保留 | v1 未使用 |
| `0x60` | `ERROR` | 双向 | `u16 code` + `string message` |
| `0x7F` | `BYE` | 双向 | 空 |

`stream` 约定：

| stream | 用途 |
|---:|---|
| 0 | 控制（握手、文件元数据、剪贴板、PING） |
| 1 | 文件数据块（v1 所有传输共用；用 `id` 区分） |
| 2 | 屏幕视频（M2） |
| 3 | 输入事件（M2） |
| 4 | 音频（M3） |
| ≥16 | 预留 |

v1 的控制消息和文件块共用一个 TCP 连接；`stream` 目前只做语义标记，不做独立流控。

## 3. 控制消息

### 3.1 Hello

| 字段 | 类型 | 说明 |
|---|---|---|
| `proto` | u16 | 协议版本，当前 1 |
| `flags` | u16 | 软总线 profile 位；bit0=4.x，bit1=5.x，bit2=6.x；其余保留 |
| `caps` | u32 | 能力位图 |
| `nonce` | 16 bytes | 每个连接随机 |
| `device_id` | string(≤256) | 稳定设备 ID（Linux 用 machine-id 前 16 字符） |
| `device_name` | string(≤256) | 可读名称 |
| `os` | string(≤128) | `Deepin/UOS Linux` / `HarmonyOS` / `KaihongOS` |
| `app_version` | string(≤128) | DHB 版本 |

能力位：

| 位 | 名称 | 含义 |
|---:|---|---|
| 0 | `FILE` | 支持文件传输 |
| 1 | `CLIPBOARD` | 支持文本剪贴板 |
| 2 | `SCREEN_SINK` | 能显示远端屏幕 |
| 3 | `SCREEN_SOURCE` | 能采集并发送屏幕 |
| 4 | `INPUT` | 能注入输入 |
| 5 | `NOTIFY` | 能发布/订阅通知 |
| 6 | `DISCOVERY` | 支持 DHB 发现 |

双方在 TCP 连接建立后**立即**各发一个 HELLO，然后读取对端 HELLO。任一方 `proto != 1` 即失败。

`flags` 的 profile 位（bit0/1/2）只做诊断和对端展示，不替代传输选择：KLink legacy 适配器置 4.x，OpenHarmony 5.x/6.x 适配器置对应位，App/TCP 置 0。详见 [软总线版本 profile](09-softbus-profiles.md)。

### 3.2 认证

仅当 `SessionConfig.psk` 非空时执行。记：

- `C` = 客户端（发起 TCP 连接的一方，通常是鸿蒙 App）
- `S` = 服务端（`dhb serve`）
- `nc` = C 的 HELLO nonce；`ns` = S 的 HELLO nonce
- `na` = S 在 `AUTH_CHALLENGE` 中生成的 16 字节 nonce
- `idc` / `ids` = 双方 `device_id`
- `K` = PSK 字节（64 hex 直接解码为 32 字节；否则 `SHA-256(passphrase)`）

证明：

```
transcript = "DHB1-AUTH" || nc || ns || na || idc || ids
proof      = HMAC-SHA256(K, transcript)
```

流程：

```
C                                   S
│ ── HELLO(nc,idc) ───────────────▶ │
│ ◀────────────── HELLO(ns,ids) ─── │
│ ◀────────────── AUTH_CHALLENGE(na)│
│ ── AUTH_RESPONSE(proof) ────────▶ │
│                                   │ 常量时间比较
│ ◀────────────── AUTH_OK / FAIL ── │
```

会话密钥（v1 只派生，不用于加密；预留给 v2 AEAD）：

```
session_key = HKDF-SHA256(
    ikm  = K,
    salt = nc || ns,
    info = "DHB1-SESSION",
    L    = 32)
```

规则：

- 有 PSK 的客户端收到 `AUTH_OK` 而不是 `AUTH_CHALLENGE` → **拒绝**，防降级。
- 有 PSK 的服务端收到非 `AUTH_RESPONSE` → 失败并断开。
- `--insecure` 双方都不发 challenge；服务端直接 `AUTH_OK`，客户端必须显式 `insecure=true` 才接受。
- 当前 TCP 明文只提供"身份认证"，不提供"机密性"；生产 TCP 必须 TLS 1.3。

### 3.3 文件

`FILE_OFFER`：

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | u32 | 本次传输 ID，发送端自增/随机 |
| `size` | u64 | 文件字节数 |
| `mtime` | u64 | Unix 秒（当前仅展示） |
| `mode` | u32 | Unix 权限位（当前固定 0644） |
| `sha256` | 32 bytes | 整文件 SHA-256 |
| `name` | string(≤1024) | 原始文件名（接收端必须净化） |

`FILE_ACCEPT`：`u32 id` + `u64 offset`。`offset` 是接收端希望发送端开始的位置（断点续传）。

`FILE_CHUNK`：`u32 id` + `u64 offset` + `0..(16MiB-12)` 字节。v1 建议 256 KiB；`offset` 必须等于接收端当前偏移，否则丢弃并记日志。

`FILE_DONE`：`u32 id` + 32 字节 SHA-256。接收端比较实际哈希与 `size`。

`FILE_RESULT`：`u32 id` + `u8 status`（0=成功，非 0=失败）+ 32 字节实际 SHA-256 + `string message`（≤512）。

发送端流程：

1. 计算整文件 SHA-256（流式）。
2. 发 `FILE_OFFER`。
3. 等 `FILE_ACCEPT`（30s 超时）或 `FILE_REJECT`。
4. 从 `offset` 开始，按 256 KiB 发 `FILE_CHUNK`。
5. 发 `FILE_DONE`。
6. 等 `FILE_RESULT`（5min 超时），比较 status 与哈希。

接收端流程：

1. 净化文件名；创建接收目录。
2. 若 `.dhb-<id>.part` 存在且 ≤ `size`，从该大小续传；否则截断。
3. 发 `FILE_ACCEPT(id, offset)`。
4. 每个 chunk 必须 `offset == 当前偏移`，写入 + 增量 SHA-256。
5. 收到 `FILE_DONE`：`flush/close`，比较大小与哈希。
6. 成功：`rename(.part, final)`；失败：删除 `.part`。
7. 发 `FILE_RESULT`；成功时回调 `onFileReceived(final)`。

边界：

- 单文件上限 v1 不设硬限制（u64），但接收端应可配置（M2）。
- 文件名为空、`.`、`..`、包含 `/`/`\`/控制字符 → 拒绝。
- 目标重名 → `name (1).ext`，最多 9999 次。
- 并发：v1 同一会话只允许一个活跃接收；`FILE_OFFER` 在忙时回 `FILE_REJECT(3)`。

### 3.4 剪贴板

`CLIPBOARD_SET`：`string text`，UTF-8，≤ 60000 字节（受 u16 长度限制）。接收端只做展示/写入系统剪贴板，不自动回传，避免环路。

### 3.5 PING / ERROR / BYE

- `PING`/`PONG`：`u64` 本地单调毫秒；对端原样回显。RTT 由发起方计算。
- `ERROR`：`u16 code` + `string message`；仅用于诊断，不改变状态机。
- `BYE`：优雅关闭提示；收到后停止 reader，等待对端关闭。

## 4. 发现协议（UDP）

端口默认 `47822`，广播地址 `255.255.255.255`，同时向 `127.0.0.1` 发一份便于本机调试。

查询（6 字节）：`DHB1?\n`

应答（UTF-8 单行，`\t` 分隔，`\n` 结尾）：

```
DHB1!<tcp_port>\t<device_id>\t<device_name>\t<os>\t<app_version>\t<host>\n
```

- `host` 是服务端认为"到达查询方"的本机 IPv4 地址（通过临时 UDP `connect` + `getsockname` 得到）；旧客户端可忽略。
- 字段中的 `\t`、`\n`、`\r`、`|` 替换为空格；单字段 ≤120 字节。
- 客户端按 `device_id` 去重；优先用 `host`，缺失时用 `SocketRemoteInfo.address` 兜底。
- 兼容：5 字段的旧应答仍可解析（无 `host`）。

## 5. 版本与兼容

- `version` 是主版本；不兼容变更必须递增，且旧端应明确报错而不是猜测。
- 新增帧类型/能力位是兼容变更：旧端忽略未知类型，能力协商决定是否启用。
- 新增控制消息字段必须放在末尾，并用长度前缀/`done()` 校验；v1 解析器要求"无尾随字节"，所以新增字段需要升 `version` 或改为可选 TLV。
- 未来 v2 计划：帧序号 + AEAD（ChaCha20-Poly1305）、流控 credit、屏幕帧分片。

## 6. 安全注意事项

- 不要在有 PSK 时接受 `AUTH_OK`；不要静默降级到 `--insecure`。
- 文件路径必须先净化再拼接；禁止 `..`、绝对路径、符号链接跟随（M2 加 `O_NOFOLLOW`）。
- 接收目录不应是 `$HOME` 根；默认 `~/DHB`。
- 日志不记录剪贴板内容、不记录文件内容；只记录大小/哈希/路径。
- TCP 明文仅限本机回环；跨机测试请用 SSH 隧道或等待 TLS 支持。

## 7. 参考实现

- C++：`include/dhb/protocol.h`、`src/protocol.cpp`、`src/session.cpp`
- ArkTS：`ohos/entry/src/main/ets/core/Protocol.ets`、`BridgeClient.ets`
- 测试：`tests/dhb_tests.cpp`（CRC/SHA/HMAC/HKDF 向量、帧损坏、端到端文件）
