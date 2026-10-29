# DHB Deepin 25 client plan

> This directory is the Deepin/DDE integration layer. It is a **plan + D-Bus
> contract** today; the DTK6 tray/GUI and Linyaps package are not implemented
> or built yet. Do not describe them as shipped.

## 1. Target

- Deepin/UOS v25, DTK6 / Qt6.
- The existing `dhb` CLI/daemon owns the protocol, files, clipboard and KLink
  helpers.
- The desktop side only talks to it over D-Bus; it must not reimplement transfer
  logic.

## 2. Components

| Component | Skill | Role |
|---|---|---|
| `dhb-daemon` | C++ core | Owns `Session`, files, discovery, KLink helper; exposes `org.dhb.Bridge` |
| `dhb-tray` | `dde-tray-development` + `dtk-development` | DDE tray plugin: status, pause, open, quit |
| `dhb-gui` | `dtk-development` | DTK6 main window: devices, pairing, transfers, settings |
| DDE control-center module | `dde-control-center-development` | Optional settings page; phase 2 |
| `dde-shell` applet | `dde-shell-development` | **Not planned for MVP**; tray is enough |
| Linyaps package | `linyaps-development` | Package `dhb` + `dhb-tray`/`dhb-gui` |

The tray plugin must implement `PluginsItemInterfaceV2` and load through
`dde-tray-loader`; a plain `QSystemTrayIcon` is not the DDE v25 path.

## 3. D-Bus contract

Service: `org.dhb.Bridge`  
Object: `/org/dhb/Bridge`  
Interface: `org.dhb.Bridge`  
Spec: [`org.dhb.Bridge.xml`](org.dhb.Bridge.xml)

MVP methods:

- `GetStatus() -> a{sv}`
- `ListDevices() -> aa{sv}`
- `GetReceiveDir() -> s`
- `SetReceiveDir(s) -> b`
- `Pair(s host, q port, s psk) -> b`
- `SendFile(s path) -> b`
- `CancelTransfer(s id) -> b`

Signals:

- `DeviceFound(a{sv})`
- `TransferProgress(s id, t sent, t total)`
- `TransferFinished(s id, b ok, s message)`
- `StatusChanged(s status)`

The PSK is passed only over the local session bus; the daemon must never log or
persist it. D-Bus policy should allow only the same user by default.

## 4. Phases

1. **P0** — add `org.dhb.Bridge` adapter to the daemon (QtDBus or sd-bus);
   keep CLI behavior unchanged.
2. **P1** — minimal `dhb-tray` plugin: connected/disconnected icon, open window,
   quit; no custom quick-panel yet.
3. **P2** — `dhb-gui` DTK6 window: device list, pair dialog, drag/drop send,
   transfer progress.
4. **P3** — optional DDE control-center module and `dde-shell` applet.
5. **P4** — Linyaps `linglong.yaml`: build `dhb`, install to `${PREFIX}`, set
   desktop file/icon/D-Bus service; then `ll-builder run` and export UAB.

## 5. Why not start with DDE shell/control center

- Tray is the smallest user-visible integration.
- `dde-shell` applets and control-center modules have their own plugin
  frameworks and installation contracts; they multiply the failure surface.
- The daemon D-Bus contract must be stable before any UI binds to it.

## 6. Verification gates

- `busctl introspect org.dhb.Bridge /org/dhb/Bridge` shows the interface.
- `dhb-tray` is discovered by `dde-tray-loader` and survives theme/restart.
- `dhb-gui` pairs and sends a file while `dhb serve` runs on another host.
- `ll-builder build` + `ll-builder run` works on Deepin 25; UAB export optional.
- Gate A/B remain unrelated to this UI work; do not conflate them.

## 7. Current environment blocker

`deepin25` is registered as a WSL2 distro at
`C:\Users\ASUS\AppData\Local\wsl\{ca4411ab-b186-4917-ba94-8041e99f94c3}`,
but `LxssManager` is stuck in `START_PENDING` and `wsl.exe` does not respond.
DTK6/`ll-builder` verification is therefore **not executed yet**.
