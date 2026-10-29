# AliOS 的“分布式软总线”：公开资料研究

> 问题：AliOS（斑马智行）有没有“分布式软总线”的说法？它和 HarmonyOS 的 dsoftbus 是不是一回事？
> 结论先行：**有说法，但只是架构层命名；公开资料没有协议/API 规范，也没有证据证明它兼容 OpenHarmony dsoftbus。**
> 这篇文章只基于公开资料，不把推测写成事实。

## 0. 一句话结论

AliOS 官方把 **Hypervisor、分布式软总线** 并列为“多域融合”组件；公开技术材料显示 AliOS 的跨域通信主干更可能是 **Fusion (SOA) + SOME/IP / DDS**。  
这跟 HarmonyOS 的 `communication_dsoftbus`（CoAP 发现、HiChain 认证、Session/字节通道）不是同一个东西，也没有公开的互通证据。

## 1. 官方口径：只有一句，没有规范

斑马智能官网“核心技术 → AliOS”页面写的是：

> 分层解耦 多域融合  
> 南北生态解耦，从基础系统到应用软件解耦，按需定制  
> **Hypervisor、分布式软总线等组件，支持多域融合体验**

英文站对应：

> Layered Decoupling, Multi-Domain Integration  
> **Hypervisor, Distributed Software Bus — support cross-domain experiences**

来源：[斑马智能中文站](https://www.ebanma.com/coreTechnology?#technology2) · [英文站](https://www.ebanma.com/en/coreTechnology#technology3)

这句话能确认的只有：

- “分布式软总线”是 AliOS 的一个**组件/能力名称**；
- 它服务于**多域融合**（座舱、驾驶、车云等域之间）；
- 与 Hypervisor 并列，属于系统层，而不是某个 App。

不能确认的：

- 它的 wire protocol、帧格式、发现机制；
- 是否支持第三方接入、有没有 SDK；
- 是否和 OpenHarmony `dsoftbus` 有任何协议级关系。

## 2. 公开技术材料：更像 Fusion (SOA)，不是 HarmonyOS dsoftbus

### 2.1 经济日报：AliOS Cyber 的“通信软总线”

2023 年世界人工智能大会报道里，斑马智行对 AliOS Cyber 的描述是：

> 基于自研分布式计算、存储及**通信软总线**等核心技术打造并量产异构融合式智能座舱系统——AliOS Cyber。  
> 该系统采用双核分布异构融合生态架构，支持 SOA 全域沉浸体验，具有跨域融合能力。

来源：[经济日报报道](http://www.jingjiribao.cn/static/detail.jsp?id=466067)

这里“通信软总线”是**异构融合座舱的内部通信基础设施**，和 SOA / 跨域服务化放在一起，仍然没有协议细节。

### 2.2 中国青年报/CRI：AliOS 原生 Fusion (SOA)，支持 SOME/IP、DDS

2022 年一篇车载 OS 对比报道写得更具体：

> AliOS 原生自带自主可控、稳定可靠的 **Fusion (SOA) 框架**，能够实现跨域、跨车云一体通信，**支持 SOME/IP、DDS 多种协议**。  
> Fusion 可实现整车硬件能力的原子化和服务化。

来源：[中国青年报/CRI 转载](https://auto.cri.cn/chinanews/20220530/5dfb5daf-9718-55ac-c5b9-b905c8d96b8a.html)

这条材料很关键：

- AliOS 对外可验证的跨域通信栈是 **Fusion (SOA)**；
- 协议层明确提到 **SOME/IP、DDS**，这是汽车 SOA 的常见组合；
- 没有提到 CoAP、HiChain、Session、dsoftbus 那套 HarmonyOS 术语。

需要说明：这是媒体技术报道，不是 AliOS 官方协议文档；但它和官方“分布式软总线 + 多域融合”的定位是吻合的。

### 2.3 AliOS Things 的“网络”不是车载软总线

AliOS Things 是另一个开源 IoT RTOS 线，它的网络组件是：

- `uMesh` 自组织网络（Wi-Fi/BLE/802.15.4，ID2 认证、AES-128）
- `SAL` 套接字适配层
- LwIP TCP/IP 栈
- Wi-Fi / BLE / LoRaWAN / GPRS / NB-IoT

来源：[AliOS Things Network 文档](https://aliosthings.gitbook.io/docs/documentation/components/network.md)

这说明：**AliOS Things 的 uMesh 不是车载 AliOS 的“分布式软总线”**，两者不要混为一谈。

### 2.4 专利：术语是通用的，不能拿来证明 AliOS

公开检索“分布式软总线”会命中不少专利，例如：

- `CN117061270A`《一种基于分布式软总线实现远程设备本地化虚拟与访问的方法》
- 申请人是 **江苏润和软件**，不是斑马智行
- 其背景描述是“分布式软总线借鉴传统总线思路，完成自主发现、连接、组网、多协议数据传输”

来源：[Google Patents CN117061270A](https://patents.google.com/patent/CN117061270A/zh)

这说明“分布式软总线”是一个**通用架构术语**，不是 HarmonyOS 专属，也不能因为专利里出现这个词就推断 AliOS 与 OpenHarmony 同源。

## 3. 它和 HarmonyOS dsoftbus 的对比

| 维度 | HarmonyOS / OpenHarmony dsoftbus | AliOS 的公开说法 |
|---|---|---|
| 公开规范 | 有开源代码、接口、协议实现（`communication_dsoftbus`） | 官方只有一句组件描述；无公开协议/API |
| 发现 | CoAP/BLE/BR 等，`PublishLNN`/`RefreshLNN` | 未公开；Fusion/SOA 更偏服务注册/发现 |
| 认证 | HiChain、设备认证、可信群组 | 未公开；车端可能依赖 PKI/车云证书/ID2 等 |
| 传输 | Session、字节/消息/流、TCP/P2P | 公开材料指向 SOME/IP、DDS 等汽车协议 |
| 跨域 | 多设备组网、分布式硬件 | 座舱/驾驶/车云多域融合 |
| 开放接入 | 开源 SDK/系统接口 | 车厂/OEM 合作、Fusion SDK 非公开 |
| 与对方互通 | 无公开证据 | 无公开证据 |

**判断**：AliOS 的“分布式软总线”是**产品架构层的通信抽象**；HarmonyOS dsoftbus 是**有公开实现的设备协同协议栈**。两者名字相似，但不能假设协议兼容。

## 4. 对 DHB 的意义

### 4.1 不要把 AliOS 加成“第四条软总线 profile”

DHB 现在的 profile 是：

- KLink 4.x legacy
- OpenHarmony 6.x modern
- DHB App/TCP 保底

AliOS 不在这个矩阵里。原因：

1. 没有公开 SDK/协议，无法做可复现的 Linux 适配；
2. 它是车端/座舱域方案，不解决 HarmonyOS 4.x/6.x 手机认证；
3. 即使做，也应该走 **Fusion/SOA + SOME/IP/DDS**，而不是硬套 dsoftbus。

### 4.2 如果未来做“车机”场景

AliOS 车机的现实入口不是 dsoftbus，而是：

- **Fusion (SOA)**：服务化、SOME/IP、DDS；
- **HiCar**：斑马智行 2025 年已展示 AliOS + HarmonyOS 的 HiCar 手车互联 2.0，通过 AliOS Linux SDK 适配 HiCar，而不是 dsoftbus。

来源：[斑马智行 ICCE 2025 报道](http://www.chinahaiheng.com/content/detail/694cd6b37fac9a61215d93ef.html)（转载自斑马智行）

所以如果 DHB 要做车机，正确问题是：

> “DHB 如何作为一个 SOA 服务/客户端接入 Fusion，或如何复用 HiCar 通道？”

而不是：

> “AliOS 的分布式软总线和鸿蒙 dsoftbus 怎么互通？”

### 4.3 可复用的只有设计思想

AliOS 的“多域融合”对 DHB 有两点启发：

- **能力服务化**：把文件、剪贴板、屏幕抽象成服务，而不是写死消息；
- **多域路由**：座舱/手机/车云之间按能力路由，而不是按设备类型写死。

DHB 现有 `Capability`/`Hello.caps`/`ITransport` 已经是这个方向；不需要为了 AliOS 改协议。

## 5. 建议

| 选项 | 建议 |
|---|---|
| 把 AliOS 加入 Gate A/B | ❌ 不做；与鸿蒙认证无关 |
| 做 AliOS 兼容层（现在） | ❌ 没有公开 SDK/协议，成本不可控 |
| 作为 S5 研究项 | ✅ 可以，关注 Fusion/SOME/IP/DDS 和 HiCar |
| 车机合作时 | 向斑马/车厂要 Fusion SDK、协议文档、HIL 环境 |
| 对 DHB 代码的影响 | 无；最多未来加 `SOME/IP`/`DDS` transport adapter |

## 6. 还没证实的事

- AliOS 的“分布式软总线”内部是否真的复用 SOME/IP/DDS，还是自研二进制协议；
- 它是否有公开的第三方 SDK（目前未找到）；
- 它和 HarmonyOS dsoftbus 是否有任何桥接/转译层（目前无公开证据）；
- 2025 年后的 AliOS Cyber/Drive 是否改了通信栈。

这些需要斑马智行/车厂提供文档或在量产车上抓包/看 SDK 才能确认；公开网页不够。

## 7. 参考

- [斑马智能官网：AliOS 核心技术](https://www.ebanma.com/coreTechnology?#technology2)
- [Banma Intelligence: AliOS (EN)](https://www.ebanma.com/en/coreTechnology#technology3)
- [经济日报：斑马智行构建第三代汽车 AI 能力体系](http://www.jingjiribao.cn/static/detail.jsp?id=466067)
- [中国青年报/CRI：国产汽车车载 OS 对比（Fusion/SOME-IP/DDS）](https://auto.cri.cn/chinanews/20220530/5dfb5daf-9718-55ac-c5b9-b905c8d96b8a.html)
- [AliOS Things: Network / uMesh](https://aliosthings.gitbook.io/docs/documentation/components/network.md)
- [Google Patents CN117061270A（申请人：江苏润和）](https://patents.google.com/patent/CN117061270A/zh)
- [斑马智行 ICCE 2025：AliOS + HarmonyOS HiCar 2.0](http://www.chinahaiheng.com/content/detail/694cd6b37fac9a61215d93ef.html)
