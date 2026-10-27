# KLink 4.x Linux lab：从“能编”到“能发现”

> 这是 **lab 验证**，不是 KLink 上游正式支持。目标是用两个 Linux 进程把 4.x legacy
> 软总线的发现链路跑通，为 Gate A（KLink ↔ OH 4.x/HarmonyOS 4.x）做前置验证。

## 1. 环境

| 项 | 值 |
|---|---|
| 内核 | Linux 6.6（Docker Desktop / WSL2） |
| 发行版 | Ubuntu 22.04 容器 |
| 编译器 | g++ 11.4.0 |
| CMake | 3.22.1 |
| KLink | `5f0d585`（2025-08-01） |

**必须把源码放在 Linux 文件系统上**（`/tmp`/`/root`），不能直接在 Windows 挂载盘上编：
KLink 的 `lninclude.sh` 用 `ln -s` 建头文件树，DrvFs 不支持。

## 2. 一键构建

```bash
scripts/build-klink-lab.sh
```

脚本做四件事：

1. `git archive HEAD` 导出干净 KLink；
2. 应用 [`patches/klink-4.x-linux-lab.patch`](../patches/klink-4.x-linux-lab.patch)；
3. `cmake -Dhost_system=ON -DAPPSERVER=OFF`，跳过 Qt 客户端，只编 dsoftbus/server/cmd；
4. 安装到 `/opt/dsoftbus`，`ifname.conf` 固定为 `eth0`。

依赖：`build-essential cmake pkg-config libssl-dev uuid-dev git patch`。

## 3. 启动

```bash
# 每台机器/容器需要不同的 UDID，否则 discovery 里会出现同一个 deviceId
mkdir -p /opt/dsoftbus/service/el1/public
printf '11111111-1111-4111-8111-111111111111' \
  > /opt/dsoftbus/service/el1/public/uuid

scripts/klink-lab-server.sh          # 后台起 connectivity_softbus_server
scripts/klink-lab-client.sh          # 跑一次 discovery/list 后退出
```

`klink-lab-client.sh` 会：

- `PublishDeviceDiscovery` + `StartDeviceDiscovery`（COAP）；
- 打印 `unAuth` 设备；
- 若 `/tmp/klink_auth_initiator` 存在，自动调用 `AuthenticateDevice(PIN)`；
- 从 `/tmp/klink_pin` 读取 PIN（用于自动化，不是产品行为）。

## 4. 已通过

### 4.1 构建

`BUILD_RC:0`，产物：

```text
/opt/dsoftbus/bin/connectivity_softbus_server
/opt/dsoftbus/bin/connectivity_devicemgr_cmd
/opt/dsoftbus/lib/{base,dsoftbus,devicemgr,third_party}/
```

启动日志确认接口：

```text
[I][COMM][SoftbusConfigAdapterInit:60]network interface name is 1:eth0
```

### 4.2 两容器 COAP 发现

两个容器 `klink-a`（172.19.0.2）/`klink-b`（172.19.0.3），各自 server + client：

```text
A: [E][BD][PrintDeviceInfo:70][PC]deviceName:klink-b(22222222-2222-4222-8222-222222222222)[][172.19.0.3]
B: [E][BD][PrintDeviceInfo:70][PC]deviceName:klink-a(11111111-1111-4111-8111-111111111111)[][172.19.0.2]
```

这说明 **KLink 4.x 的 COAP 发现、设备名/deviceId/peerAddr 交换在 Linux 上可用**。

## 5. 认证：PIN 未过，CLOUD_SN lab 路径已通过

### 5.1 PIN 路径（未通过）

A 自动发起 PIN 认证后：

```text
A: AuthenticateDevice ret: 0
B: **********************Auth PinCode:[398754]*********************
A: OnAuthResult deviceId: 2222... status: 5 reason: -20006
```

`-20006` = `ERR_DEV_CENTER_INPUT_PARA_INVALID`。当前判断：

- PIN 已经在 sink 侧显示，说明 auth 消息链路通了；
- source 侧没有走到 `OnInputAuthInfo`（日志无 `Read PinCode`），随后以 status 5 结束；
- B 的 `OnMemberJoin` 还报了 `authResponseContext_ or authUiStateMgr is nullptr`；
- 这是 Linux CLI 的 PIN/UI 状态机问题，不是 COAP 发现问题。

### 5.2 CLOUD_SN lab 路径（已通过）

把 lab 的认证类型切到 `DEVICE_AUTH_TYPE_CLOUD_SN`，并让
`OnCheckDeviceSNVaild` 接受对端设备 ID（**仅 lab**）：

```text
A: AuthenticateDevice start auth type 0
B: OnCheckDeviceSNVaild: 11111111-1111-4111-8111-111111111111
A: OnAuthResult ... status: 7 reason: 0
A: OnSoftbusDeviceFound ... isOnline: 1
A: networkId = e2c1a46696f11d5b12f8d7dbc2aac6f72da810d4ac2e50a87f2fa88e7c1ddb32
B: networkId = 6fe49bc024b47522426c962ebb3c03d5a2c2ee14a74154dc82cdd13785b986a7
```

`status 7` 是 `AUTH_REQUEST_FINISH`，`reason 0` 是成功；双方都拿到
`networkId` 并进入 `isOnline: 1`。这说明 **KLink 4.x 的 auth/session 链路在
Linux 上可以建立可信关系**。

边界必须说清楚：

- `OnCheckDeviceSNVaild` 返回 `true` 是 **lab 放行**，不是生产安全策略；
- `GetAllNodeDeviceInfo` 的 trusted-list 计数还没有作为独立结果重复采集
  （最后一次查询被 Docker/WSL runner 中断），所以不把它写成“已验收”；
- 仍然没有 OHOS/KaihongOS 对端，**Gate A 未通过**。

## 6. 下一步

1. 在 `OnMemberJoin`/`ShowConfigDialog` 加日志，修 PIN 状态机；
2. 用 **OH 4.1/HarmonyOS 4.x 真机**做 sink、Linux 只做 source，绕开 Linux↔Linux UI；
3. 用 lab SN 认证跑一次 `GetAllNodeDeviceInfo` + session 打开，作为 Gate A 的 Linux 侧前置证据；
4. 保留 PIN 路径作为生产候选，不把 `OnCheckDeviceSNVaild=true` 带进产品。

## 7. 边界

- 只证明 **Linux ↔ Linux** 的 KLink 4.x 发现/认证；没有 OHOS/KaihongOS 对端。
- 两个容器共享 Docker bridge，COAP 广播可用；跨网段/AP 隔离未测。
- 没有 session/文件传输；`GetAllNodeDeviceInfo` 在认证前为 0，符合预期。
- Lab 使用固定 UDID、默认配置、放行 SN 回调，**不可用于生产**。
- 这套 patch 是 **host bring-up patch**，不是要合入 KLink 上游。
