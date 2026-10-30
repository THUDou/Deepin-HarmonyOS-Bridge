/*
 * DHB - Deepin-HarmonyOS-Bridge
 * SPDX-License-Identifier: Apache-2.0
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

  private client: BridgeClient | null = null;
  private context: common.UIAbilityContext | null = null;

  aboutToAppear(): void {
    this.context = getContext(this) as common.UIAbilityContext;
    this.client = new BridgeClient(this.context);
    this.client.onLog = (message: string): void => {
      this.appendLog(message);
    };
    this.client.onState = (state: string): void => {
      this.status = state;
      if (state === 'authenticated') {
        this.connected = true;
        this.status = '已连接';
      } else if (state === 'disconnected') {
        this.connected = false;
        this.status = '未连接';
      }
    };
    this.client.onProgress = (sent: number, total: number): void => {
      this.progress = total === 0 ? 0 : Math.floor((sent * 100) / total);
    };
    this.client.onFileReceived = (path: string): void => {
      this.appendLog('文件已接收: ' + path);
    };
    this.client.onClipboard = (text: string): void => {
      const data = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text);
      pasteboard.getSystemPasteboard().setData(data).catch((error: Error): void => {
        this.appendLog('写入剪贴板失败: ' + error.message);
      });
      this.appendLog('已同步剪贴板 (' + text.length + ' 字符)');
    };
    this.appendLog('DHB v0.1.0 · 鸿蒙 ↔ Deepin/UOS');
  }

  private appendLog(message: string): void {
    const time = new Date().toLocaleTimeString();
    this.logText = '[' + time + '] ' + message + '\n' + this.logText;
  }

  private async doConnect(): Promise<void> {
    const client = this.client;
    if (client === null) return;
    if (this.psk.length === 0) {
      this.appendLog('请先填写 pairing key（在 Deepin 端 dhb serve 输出）');
      return;
    }
    this.status = '连接中...';
    try {
      await client.connect(this.host, Number(this.port), this.psk);
      this.connected = true;
      this.status = '已连接';
    } catch (error) {
      this.connected = false;
      this.status = '连接失败';
      this.appendLog('连接失败: ' + (error as Error).message);
      await client.close();
    }
  }

  private async doDisconnect(): Promise<void> {
    const client = this.client;
    if (client === null) return;
    await client.close();
    this.connected = false;
    this.status = '未连接';
  }

  private async doDiscover(): Promise<void> {
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
    }
  }

  private async doSendFile(): Promise<void> {
    const client = this.client;
    const context = this.context;
    if (client === null || context === null) return;
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
    if (client === null) return;
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
      this.appendLog('发送剪贴板失败: ' + (error as Error).message);
    }
  }

  build() {
    Column({ space: 12 }) {
      Row() {
        Column({ space: 2 }) {
          Text('DHB')
            .fontSize(28)
            .fontWeight(FontWeight.Bold)
            .fontColor('#E6F1FF')
          Text('深鸿桥 · Deepin ↔ HarmonyOS / OpenHarmony')
            .fontSize(12)
            .fontColor('#7A8CA3')
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Text(this.status)
          .fontSize(12)
          .fontColor(this.connected ? '#38D39F' : '#F2A65A')
          .padding({ left: 10, right: 10, top: 4, bottom: 4 })
          .backgroundColor('#162131')
          .borderRadius(10)
      }
      .width('100%')

      TextInput({ text: this.host, placeholder: 'Deepin 主机 IP' })
        .type(InputType.Normal)
        .backgroundColor('#111B29')
        .fontColor('#E6F1FF')
        .onChange((value: string): void => {
          this.host = value;
        })

      Row({ space: 8 }) {
        TextInput({ text: this.port, placeholder: '端口 47821' })
          .type(InputType.Number)
          .backgroundColor('#111B29')
          .fontColor('#E6F1FF')
          .layoutWeight(1)
          .onChange((value: string): void => {
            this.port = value;
          })
        Button('发现')
          .fontSize(12)
          .backgroundColor('#1B2A3D')
          .fontColor('#9FD3FF')
          .onClick((): void => {
            this.doDiscover();
          })
      }
      .width('100%')

      TextInput({ text: this.psk, placeholder: 'pairing key（64 位 hex）' })
        .type(InputType.Password)
        .backgroundColor('#111B29')
        .fontColor('#E6F1FF')
        .onChange((value: string): void => {
          this.psk = value;
        })

      Row({ space: 8 }) {
        Button(this.connected ? '重新连接' : '连接')
          .layoutWeight(1)
          .backgroundColor('#2F7CF6')
          .fontColor('#FFFFFF')
          .onClick((): void => {
            this.doConnect();
          })
        Button('断开')
          .layoutWeight(1)
          .backgroundColor('#1B2A3D')
          .fontColor('#C7D6E8')
          .enabled(this.connected)
          .onClick((): void => {
            this.doDisconnect();
          })
      }
      .width('100%')

      if (this.peers.length > 0) {
        Column({ space: 4 }) {
          Text('附近设备')
            .fontSize(12)
            .fontColor('#7A8CA3')
            .width('100%')
          ForEach(this.peers, (peer: DiscoveredPeer) => {
            Row({ space: 8 }) {
              Column({ space: 1 }) {
                Text(peer.name)
                  .fontSize(14)
                  .fontColor('#E6F1FF')
                Text(peer.host + ':' + peer.port + ' · ' + peer.os)
                  .fontSize(10)
                  .fontColor('#7A8CA3')
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)
              Button('使用')
                .fontSize(11)
                .height(28)
                .backgroundColor('#1B2A3D')
                .fontColor('#9FD3FF')
                .onClick((): void => {
                  if (peer.host.length > 0) this.host = peer.host;
                  this.port = String(peer.port);
                })
            }
            .width('100%')
            .padding(8)
            .backgroundColor('#0F1925')
            .borderRadius(8)
          }, (peer: DiscoveredPeer): string => peer.id)
        }
        .width('100%')
      }

      Divider().color('#1E2A3A')

      Row({ space: 8 }) {
        Button('发送文件')
          .layoutWeight(1)
          .backgroundColor('#16324A')
          .fontColor('#BFE3FF')
          .enabled(this.connected)
          .onClick((): void => {
            this.doSendFile();
          })
        Button('发送剪贴板')
          .layoutWeight(1)
          .backgroundColor('#16324A')
          .fontColor('#BFE3FF')
          .enabled(this.connected)
          .onClick((): void => {
            this.doSendClipboard();
          })
      }
      .width('100%')

      Progress({ value: this.progress, total: 100, type: ProgressType.Linear })
        .width('100%')
        .color('#2F7CF6')

      Scroll() {
        Text(this.logText)
          .fontSize(11)
          .fontColor('#8FA6BF')
          .fontFamily('monospace')
          .width('100%')
          .textAlign(TextAlign.Start)
      }
      .width('100%')
      .layoutWeight(1)
      .padding(10)
      .backgroundColor('#0A111B')
      .borderRadius(10)
    }
    .width('100%')
    .height('100%')
    .padding(16)
    .backgroundColor('#0B1220')
  }
}
