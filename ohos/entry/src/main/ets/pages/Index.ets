/*
 * DHB - Deepin-HarmonyOS-Bridge
 * SPDX-License-Identifier: Apache-2.0
 *
 * Product-facing HAP shell. Transport/auth is still DHB over TCP; this page
 * deliberately makes the user-facing distinction between:
 *   - my trusted peer
 *   - nearby/discovered devices
 *   - transfers and clipboard
 */

import { common } from '@kit.AbilityKit';
import { picker } from '@kit.CoreFileKit';
import { pasteboard } from '@kit.BasicServicesKit';
import { BridgeClient } from '../core/BridgeClient';
import { DiscoveredPeer, discoverPeers } from '../core/Discovery';

@Entry
@Component
struct Index {
  @State host: string = '192.168.1.100';
  @State port: string = '47821';
  @State psk: string = '';
  @State status: string = '未连接';
  @State connected: boolean = false;
  @State progress: number = 0;
  @State logText: string = '';
  @State peers: DiscoveredPeer[] = [];
  @State peerName: string = '';
  @State peerOs: string = '';
  @State busy: boolean = false;

  private client: BridgeClient | null = null;
  private context: common.UIAbilityContext | null = null;

  aboutToAppear(): void {
    this.context = getContext(this) as common.UIAbilityContext;
    this.client = new BridgeClient(this.context);
    this.client.onLog = (message: string): void => {
      this.appendLog(message);
    };
    this.client.onState = (state: string): void => {
      if (state === 'authenticated') {
        this.connected = true;
        this.status = '已连接';
        this.peerName = this.client?.getPeerName() ?? '';
        this.peerOs = this.client?.getPeerOs() ?? '';
      } else if (state === 'disconnected') {
        this.connected = false;
        this.status = '未连接';
        this.peerName = '';
        this.peerOs = '';
      } else if (state === 'connected') {
        this.status = '握手中...';
      }
    };
    this.client.onProgress = (sent: number, total: number): void => {
      this.progress = total === 0 ? 0 : Math.floor((sent * 100) / total);
    };
    this.client.onFileReceived = (path: string): void => {
      this.appendLog('文件已接收: ' + path);
    };
    this.client.onClipboard = (text: string): void => {
      console.info('DHB_CLIPBOARD ' + text);
      const data = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text);
      pasteboard.getSystemPasteboard().setData(data).catch((error: Error): void => {
        this.appendLog('写入剪贴板失败: ' + error.message);
      });
      this.appendLog('已同步剪贴板 (' + text.length + ' 字符)');
    };
    this.appendLog('DHB v0.1.0 · 深鸿桥');
    this.appendLog('核心：Deepin ↔ KaihongOS / OpenHarmony');
  }

  private appendLog(message: string): void {
    const time = new Date().toLocaleTimeString();
    this.logText = '[' + time + '] ' + message + '\n' + this.logText;
  }

  private async doConnect(): Promise<void> {
    const client = this.client;
    if (client === null || this.busy) return;
    if (this.psk.length === 0) {
      this.appendLog('请先填写 pairing key');
      return;
    }
    this.busy = true;
    this.status = '连接中...';
    try {
      await client.connect(this.host, Number(this.port), this.psk);
      this.connected = true;
      this.status = '已连接';
      this.peerName = client.getPeerName();
      this.peerOs = client.getPeerOs();
    } catch (error) {
      this.connected = false;
      this.status = '连接失败';
      this.appendLog('连接失败: ' + (error as Error).message);
      await client.close();
    } finally {
      this.busy = false;
    }
  }

  private async doDisconnect(): Promise<void> {
    const client = this.client;
    if (client === null) return;
    await client.close();
    this.connected = false;
    this.status = '未连接';
    this.peerName = '';
    this.peerOs = '';
  }

  private async doDiscover(): Promise<void> {
    if (this.busy) return;
    this.busy = true;
    this.appendLog('正在局域网发现...');
    try {
      this.peers = await discoverPeers(3000);
      if (this.peers.length === 0) {
        this.appendLog('未发现 DHB 设备，可手动填写 IP');
      } else {
        this.appendLog('发现 ' + this.peers.length + ' 台设备');
      }
    } catch (error) {
      this.appendLog('发现失败: ' + (error as Error).message);
    } finally {
      this.busy = false;
    }
  }

  private async doSendFile(): Promise<void> {
    const client = this.client;
    const context = this.context;
    if (client === null || context === null || !this.connected) return;
    try {
      const documentPicker = new picker.DocumentViewPicker(context);
      const options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;
      const uris = await documentPicker.select(options);
      if (uris.length === 0) return;
      this.progress = 0;
      await client.sendFile(uris[0]);
      this.progress = 100;
    } catch (error) {
      this.appendLog('发送失败: ' + (error as Error).message);
    }
  }

  private async doSendClipboard(): Promise<void> {
    const client = this.client;
    if (client === null || !this.connected) return;
    try {
      const data = await pasteboard.getSystemPasteboard().getData();
      const text = data.getPrimaryText();
      if (text.length === 0) {
        this.appendLog('剪贴板为空');
        return;
      }
      await client.sendClipboard(text);
      this.appendLog('已发送剪贴板 (' + text.length + ' 字符)');
    } catch (error) {
      this.appendLog('剪贴板权限/读取失败: ' + (error as Error).message);
    }
  }

  private async doPasteClipboard(result: PasteButtonOnClickResult): Promise<void> {
    if (result !== PasteButtonOnClickResult.SUCCESS) {
      this.appendLog('粘贴控件未授权，不能读取剪贴板');
      return;
    }
    await this.doSendClipboard();
  }

  private usePeer(peer: DiscoveredPeer): void {
    this.host = peer.host;
    this.port = String(peer.port);
    this.appendLog('已选择附近设备: ' + peer.name);
  }

  build() {
    Column() {
      // Header
      Row() {
        Column({ space: 2 }) {
          Text('DHB')
            .fontSize(30)
            .fontWeight(FontWeight.Bold)
            .fontColor('#EAF2FF')
          Text('深鸿桥 · 跨生态设备桥')
            .fontSize(12)
            .fontColor('#7F94AD')
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Text(this.status)
          .fontSize(12)
          .fontColor(this.connected ? '#38D39F' : '#F2A65A')
          .padding({ left: 12, right: 12, top: 6, bottom: 6 })
          .backgroundColor(this.connected ? '#0F2A24' : '#2A2116')
          .borderRadius(14)
      }
      .width('100%')
      .padding({ left: 4, right: 4, bottom: 10 })

      Scroll() {
        Column({ space: 12 }) {
          // Trusted peer card
          Column({ space: 8 }) {
            Row() {
              Text(this.connected ? '我的设备' : '尚未建立可信连接')
                .fontSize(12)
                .fontColor('#8FA6BF')
                .layoutWeight(1)
              Text(this.connected ? '● 可信' : '○ 未连接')
                .fontSize(11)
                .fontColor(this.connected ? '#38D39F' : '#7F94AD')
            }
            .width('100%')

            if (this.connected) {
              Row({ space: 10 }) {
                Text('DHB')
                  .fontSize(16)
                  .fontWeight(FontWeight.Bold)
                  .fontColor('#0B1220')
                  .width(44)
                  .height(44)
                  .textAlign(TextAlign.Center)
                  .backgroundColor('#38D39F')
                  .borderRadius(12)
                Column({ space: 2 }) {
                  Text(this.peerName.length > 0 ? this.peerName : 'Deepin/UOS 设备')
                    .fontSize(16)
                    .fontWeight(FontWeight.Medium)
                    .fontColor('#EAF2FF')
                  Text((this.peerOs.length > 0 ? this.peerOs : 'Linux') + ' · 已认证')
                    .fontSize(11)
                    .fontColor('#8FA6BF')
                }
                .alignItems(HorizontalAlign.Start)
                .layoutWeight(1)
              }
              .width('100%')
            } else {
              Text('连接后这里会显示已认证的 Deepin/UOS 设备。')
                .fontSize(12)
                .fontColor('#6F8198')
                .width('100%')
            }
          }
          .width('100%')
          .padding(14)
          .backgroundColor('#101B2B')
          .borderRadius(14)

          // Manual connection
          Column({ space: 8 }) {
            Text('连接 Deepin / UOS')
              .fontSize(13)
              .fontWeight(FontWeight.Medium)
              .fontColor('#C9D8EA')
              .width('100%')

            TextInput({ text: this.host, placeholder: '主机 IP' })
              .type(InputType.Normal)
              .backgroundColor('#0B1522')
              .fontColor('#EAF2FF')
              .placeholderColor('#5E7188')
              .onChange((value: string): void => {
                this.host = value;
              })

            Row({ space: 8 }) {
              TextInput({ text: this.port, placeholder: '端口 47821' })
                .type(InputType.Number)
                .backgroundColor('#0B1522')
                .fontColor('#EAF2FF')
                .placeholderColor('#5E7188')
                .layoutWeight(1)
                .onChange((value: string): void => {
                  this.port = value;
                })
              Button('发现')
                .fontSize(12)
                .backgroundColor('#14263A')
                .fontColor('#8FD3FF')
                .enabled(!this.busy)
                .onClick((): void => {
                  this.doDiscover();
                })
            }
            .width('100%')

            TextInput({ text: this.psk, placeholder: 'pairing key' })
              .type(InputType.Password)
              .backgroundColor('#0B1522')
              .fontColor('#EAF2FF')
              .placeholderColor('#5E7188')
              .onChange((value: string): void => {
                this.psk = value;
              })

            Row({ space: 8 }) {
              Button(this.connected ? '重新连接' : '连接')
                .layoutWeight(1)
                .backgroundColor('#2F7CF6')
                .fontColor('#FFFFFF')
                .enabled(!this.busy)
                .onClick((): void => {
                  this.doConnect();
                })
              Button('断开')
                .layoutWeight(1)
                .backgroundColor('#14263A')
                .fontColor('#C7D6E8')
                .enabled(this.connected)
                .onClick((): void => {
                  this.doDisconnect();
                })
            }
            .width('100%')
          }
          .width('100%')
          .padding(14)
          .backgroundColor('#101B2B')
          .borderRadius(14)

          // Nearby devices
          Column({ space: 8 }) {
            Row() {
              Text('附近设备')
                .fontSize(13)
                .fontWeight(FontWeight.Medium)
                .fontColor('#C9D8EA')
                .layoutWeight(1)
              Text(this.peers.length.toString() + ' 台')
                .fontSize(11)
                .fontColor('#6F8198')
            }
            .width('100%')

            if (this.peers.length === 0) {
              Text('点击“发现”搜索同一局域网内的 DHB 设备。')
                .fontSize(11)
                .fontColor('#6F8198')
                .width('100%')
            } else {
              ForEach(this.peers, (peer: DiscoveredPeer) => {
                Row({ space: 10 }) {
                  Text('PC')
                    .fontSize(11)
                    .fontColor('#0B1220')
                    .width(34)
                    .height(34)
                    .textAlign(TextAlign.Center)
                    .backgroundColor('#8FD3FF')
                    .borderRadius(9)
                  Column({ space: 1 }) {
                    Text(peer.name)
                      .fontSize(13)
                      .fontColor('#EAF2FF')
                    Text(peer.host + ':' + peer.port + ' · ' + peer.os)
                      .fontSize(10)
                      .fontColor('#7F94AD')
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  Button('使用')
                    .fontSize(11)
                    .height(28)
                    .backgroundColor('#14263A')
                    .fontColor('#8FD3FF')
                    .onClick((): void => {
                      this.usePeer(peer);
                    })
                }
                .width('100%')
                .padding(8)
                .backgroundColor('#0B1522')
                .borderRadius(9)
              }, (peer: DiscoveredPeer): string => peer.id)
            }
          }
          .width('100%')
          .padding(14)
          .backgroundColor('#101B2B')
          .borderRadius(14)

          // Actions
          Column({ space: 8 }) {
            Text('协作')
              .fontSize(13)
              .fontWeight(FontWeight.Medium)
              .fontColor('#C9D8EA')
              .width('100%')

            Row({ space: 8 }) {
              Button('发送文件')
                .layoutWeight(1)
                .backgroundColor('#16324A')
                .fontColor('#BFE3FF')
                .enabled(this.connected)
                .onClick((): void => {
                  this.doSendFile();
                })
              PasteButton({ icon: PasteIconStyle.LINES, text: PasteDescription.PASTE })
                .width('48%')
                .onClick((event: ClickEvent, result: PasteButtonOnClickResult): void => {
                  this.doPasteClipboard(result);
                })
            }
            .width('100%')

            Text('读取剪贴板需点击系统“粘贴”控件授权；不再申请 READ_PASTEBOARD。')
              .fontSize(10)
              .fontColor('#6F8198')
              .width('100%')

            Progress({ value: this.progress, total: 100, type: ProgressType.Linear })
              .width('100%')
              .color('#2F7CF6')
          }
          .width('100%')
          .padding(14)
          .backgroundColor('#101B2B')
          .borderRadius(14)

          // Log
          Column({ space: 4 }) {
            Text('运行日志')
              .fontSize(12)
              .fontColor('#8FA6BF')
              .width('100%')
            Scroll() {
              Text(this.logText)
                .fontSize(10)
                .fontColor('#8FA6BF')
                .fontFamily('monospace')
                .width('100%')
                .textAlign(TextAlign.Start)
            }
            .width('100%')
            .height(130)
            .padding(8)
            .backgroundColor('#0A111B')
            .borderRadius(10)
          }
          .width('100%')
          .padding(14)
          .backgroundColor('#101B2B')
          .borderRadius(14)

          Text('DHB · 只做桥，不替代 dsoftbus · TCP 先行，软总线后接')
            .fontSize(9)
            .fontColor('#53657A')
            .width('100%')
            .textAlign(TextAlign.Center)
            .padding({ top: 4, bottom: 12 })
        }
        .width('100%')
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
    .padding(16)
    .backgroundColor('#0B1220')
  }
}
