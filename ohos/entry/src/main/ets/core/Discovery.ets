/*
 * DHB - Deepin-HarmonyOS-Bridge
 * SPDX-License-Identifier: Apache-2.0
 *
 * UDP broadcast discovery, matching docs/03-protocol.md section 4.
 */

import { socket } from '@kit.NetworkKit';
import * as P from './Protocol';

export class DiscoveredPeer {
  id: string = '';
  name: string = '';
  host: string = '';
  port: number = 0;
  os: string = '';
  appVersion: string = '';
}

const DISCOVERY_PORT: number = 47822;

export async function discoverPeers(timeoutMs: number = 3000): Promise<DiscoveredPeer[]> {
  const udp = socket.constructUDPSocketInstance();
  const peers: Map<string, DiscoveredPeer> = new Map<string, DiscoveredPeer>();

  udp.on('message', (info: socket.SocketMessageInfo): void => {
    const text = P.utf8Decode(new Uint8Array(info.message));
    const line = text.replace(/\r/g, '').replace(/\n/g, '');
    if (!line.startsWith('DHB1!')) return;
    const fields = line.substring(5).split('\t');
    if (fields.length < 5) return;
    const peer = new DiscoveredPeer();
    peer.port = Number(fields[0]);
    peer.id = fields[1];
    peer.name = fields[2];
    peer.os = fields[3];
    peer.appVersion = fields[4];
    // The daemon replies with the address it sees for this client (field 5).
    // Fall back to SocketRemoteInfo for older daemons.
    if (fields.length >= 6 && fields[5].length > 0 && fields[5] !== '0.0.0.0') {
      peer.host = fields[5];
    } else {
      peer.host = info.remoteInfo.address;
    }
    if (peer.id.length === 0 || peer.port === 0 || peer.host.length === 0) return;
    peers.set(peer.id, peer);
  });

  await udp.bind({ address: '0.0.0.0', port: 0, family: 1 });
  try {
    await udp.setExtraOptions({ broadcast: true });
  } catch (error) {
    // Some devices disallow broadcast; discovery is optional.
  }
  const query = P.utf8Encode('DHB1?\n');
  const data = query.buffer.slice(query.byteOffset, query.byteOffset + query.byteLength);
  try {
    await udp.send({
      data: data,
      address: { address: '255.255.255.255', port: DISCOVERY_PORT, family: 1 }
    });
  } catch (error) {
    // Ignore; the UI falls back to manual IP.
  }

  await new Promise<void>((resolve): void => {
    setTimeout((): void => resolve(), timeoutMs);
  });
  try {
    await udp.close();
  } catch (error) {
    // Already closed.
  }

  const result: DiscoveredPeer[] = [];
  peers.forEach((peer: DiscoveredPeer): void => {
    result.push(peer);
  });
  return result;
}
