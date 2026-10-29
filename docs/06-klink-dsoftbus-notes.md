# Klink / dsoftbus 接入笔记

> 目标：把 Linux 侧的 Klink（或 openEuler dsoftbus）接成 DHB 的第二个 `ITransport`，让 Pro 档复用同一套 Session/文件/屏幕逻辑。
> 状态：设计 + 待真机验证。**不是已完成的适配**。

## 1. 为什么是 Klink

- 麒麟软件在 openKylin 开源，Apache-2.0；官方定位"通用 Linux 端间互联组件"，兼容鸿蒙软总线 4.x。
- 已封装：设备发现、发布/订阅、HiChain PIN 认证、可信设备列表、会话、字节/消息/流/文件传输。
- 提供 Linux SDK 头文件与 `softbus_client`/`devicemgrservice` pkg-config 库。
- 相比直接使用 openEuler 的 `dsoftbus` 包，Klink 更贴近"跨发行版 + 桌面"的用法。

## 2. Klink 关键 API 映射

| Klink SDK | 用途 | DHB 对应 |
|---|---|---|
| `InitDeviceManagerCenter()` | 初始化设备管理 | `KlinkDiscovery::start()` |
| `RegisterDiscoverCallback()` | 发现回调 | 设备列表 |
| `PublishDeviceDiscovery()` | 发布本机能力 | `kCapFile` 等能力广播 |
| `StartDeviceDiscovery()` | 主动发现 | UI 扫描 |
| `AuthenticateDevice(deviceId, PINCODE)` | PIN 认证 | 配对流程 |
| `GetTrustedDeviceList()` | 可信设备 | 自动重连 |
| `RegisterUIStateCallback()` | PIN 输入/展示 | Linux 托盘/对话框 |
| `CreateSessionServer(pkg, name, listener, true)` | 会话服务端 | `KlinkTransport::listen` |
| `OpenSession(my, peer, networkId, "0", attr)` | 打开会话 | `KlinkTransport::connect` |
| `SendBytes()` | 字节传输 | `ITransport::sendAll` |
| `OnBytesReceived()` | 字节接收 | 推入阻塞队列 → `recvExact` |
| `SendStream()` | 视频流 | 屏幕帧（M3） |
| `SendFile()` | 文件传输 | 可选；DHB 仍用自己的文件引擎 |
| `GetPeerDeviceId()` | 对端 networkId | 日志/重连 |

> 具体常量/枚举名以 Klink 头文件为准；上表是 README 中的 SDK 语义映射，不是逐字签名。

## 3. 构建与运行 Klink（Linux）

依赖（Klink `build-depends`）：

```
dsoftbus: cmake g++ pkgconf libssl-dev uuid-dev
main(devicemanagerclient): qtbase5-dev qttools5-dev
  libavformat-dev libswscale-dev libavcodec-dev libavutil-dev libffmpegthumbnailer-dev
```

步骤（概述，详见 Klink README）：

```bash
git clone https://gitee.com/openkylin/klink.git
cd klink
sudo bash ./third_party/openssl/openssl_install.sh ./third_party/openssl amd   # 或 arm
mkdir build && cd build
cmake -Dhost_system=ON ..
make -j
sudo make install
```

运行：

```bash
# 选择实际网卡
sudo vi /opt/dsoftbus/config/ifname.conf
sudo /opt/dsoftbus/bin/connectivity_softbus_server
sudo /opt/dsoftbus/bin/connectivity_devicemgr_client
```

CMake 链接：

```cmake
pkg_check_modules(SOFTBUS_CLIENT REQUIRED softbus_client)
pkg_check_modules(DEVICEMGR_SDK REQUIRED devicemgrservice)
include_directories(${SOFTBUS_CLIENT_INCLUDE_DIRS} ${DEVICEMGR_SDK_INCLUDE_DIRS})
target_link_libraries(dhb_klink PUBLIC ${SOFTBUS_CLIENT_LIBRARIES} ${DEVICEMGR_SDK_LIBRARIES})
```

## 4. `KlinkTransport` 设计

Klink 是**回调推送**模型，DHB 的 `ITransport` 是**阻塞读**模型；用一个字节队列桥接：

```cpp
class KlinkTransport final : public ITransport {
 public:
  bool sendAll(ByteView data, std::string* err) override {
    // Klink SendBytes 可能有单次长度上限；按 max_payload 分片。
    for (chunk : split(data, max_payload)) {
      if (SendBytes(session_id_, chunk.data(), chunk.size()) != 0) return false;
    }
    return true;
  }

  bool recvExact(std::uint8_t* out, std::size_t n, std::string* err) override {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return queue_.size() >= n || closed_ || error_; });
    if (queue_.size() < n) return false;
    for (i...) { out[i] = queue_.front(); queue_.pop_front(); }
    return true;
  }

  // Klink 回调线程调用：
  void onBytes(const void* data, std::size_t n) {
    std::lock_guard lock(mutex_);
    queue_.insert(queue_.end(), bytes...);
    cv_.notify_all();
  }

 private:
  int session_id_ = -1;
  std::deque<std::uint8_t> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool closed_ = false;
};
```

要点：

- `OnBytesReceived` 可能一次给半帧/多帧；队列 + `readFrame` 的 `recvExact` 天然处理。
- `SendBytes` 的返回值/长度限制需在真机确认；必要时分片到 4 KiB/64 KiB（以 Klink 头文件为准）。
- `CloseSession` 后要唤醒 `recvExact`，否则 reader 线程挂死。
- 会话建立后仍需 DHB `HELLO`，但 **跳过 PSK 认证**（dsoftbus 已做 HiChain）；在 `SessionConfig` 增加 `auth_mode = kTransportTrusted`。
- 传输层已加密；DHB 不再套 TLS（避免双重加密/性能损失）。

## 5. 会话参数

| 参数 | 建议值 | 说明 |
|---|---|---|
| `pkgName` | `org.dhb.bridge` | 与 HAP bundle 名一致便于排查 |
| `sessionName` | `org.dhb.bridge.session` | 两端必须一致 |
| `groupId` | `"0"` | 无群组 |
| `dataType` | `TYPE_BYTES` | 控制/文件 |
| `linkType` | Wi-Fi P2P / 5G / 2.4G / BR | 按可用性排序 |
| 视频 | `SendStream` + `COMMON_VIDEO_STREAM` | M3 屏幕 |
| 认证 | PINCODE / 远程 SN | 首次配对 |

## 6. openEuler dsoftbus 备选

openEuler 22.03 LTS SP4 文档：

```bash
sudo dnf install dsoftbus -y
echo "1" | sudo tee /etc/SN          # 设备 ID，不能重复
sudo softbus_server_main
# 客户端：gcc softbus_client_main.c -I/usr/include/dsoftbus/ \
#        -L/usr/lib64 -lsoftbus_client.z -lboundscheck -o softbus_client_main
```

注意：

- 需要与 OpenHarmony 设备在同一局域网、互相 ping 通。
- 首次认证用 HiChain PIN 弹窗；OpenHarmony 侧要允许。
- openEuler 包偏嵌入式；桌面集成/打包不如 Klink 完整。
- 设备 ID 来自 `/etc/SN`，不是 machine-id。

## 7. 真机 probe 清单（M0）

**Linux（Deepin 25 + Klink）**

1. `connectivity_softbus_server` 正常启动，`ifname.conf` 是实际网卡。
2. `connectivity_devicemgr_client` 输入 `l` 能看到 KaihongOS 设备。
3. PIN 认证后状态从 `discovery` → `online`。
4. `CreateSessionServer` + `OpenSession` 成功，`SendBytes("hello")` 对端收到。

**KaihongOS 5.x**

1. 开发者模式/调试签名；确认 dsoftbus 服务在跑。
2. 安装/运行一个系统权限的 native 测试程序（或系统自带的 dsoftbus demo）。
3. 允许 PIN；确认 session 打开。
4. 回传 `"world"`，Linux 收到。

**记录**

- 设备型号、OS 版本、内核、网卡、Wi-Fi 频段；
- `hilog`/Klink 日志；
- 失败步骤与错误码；
- 抓包（脱敏）。

## 8. 兼容性矩阵（待填）

| KaihongOS 版本 | Klink 版本 | 发现 | PIN | Session | Bytes | 备注 |
|---|---|---|---|---|---|---|
| 5.0.2.01 x86 | upstream (2024-06) | ? | ? | ? | ? | 待测 |
| 5.0.x RK3588 | upstream | ? | ? | ? | ? | 待测 |
| OpenHarmony 3.1 RK3568 | openEuler dsoftbus | ✅ 文档 | ✅ | ✅ demo | ✅ | 官方 demo |

## 9. 失败模式与排查

| 现象 | 可能原因 | 排查 |
|---|---|---|
| 发现不到 | 网卡选错/不在同一 LAN/组播被禁 | `ifname.conf`、`ip addr`、防火墙 |
| PIN 弹窗不出现 | 对端不是系统权限/认证服务未起 | OHOS dsoftbus 日志、`device_manager` |
| 认证失败 | 设备 ID 重复/时间不同步/信任环 | `/etc/SN`、NTP、删除旧信任 |
| Session 打不开 | sessionName 不一致/权限 JSON | 两端 session 名、`softbus_trans_permission.json` |
| 收到半包 | `SendBytes` 分片 | 队列 + 长度前缀（DHB 已有） |
| 断连不恢复 | 没有重连状态机 | daemon 指数退避；重新 `OpenSession` |

## 10. 结论

Klink 的 API 覆盖 DHB 所需的所有原语，接入工作量主要在**回调→阻塞队列**、**认证模式分支**、**会话生命周期/重连**，而不是协议本身。最大不确定性是 **4.x ↔ 5.x 兼容性**，必须在 M0 用真机回答。
