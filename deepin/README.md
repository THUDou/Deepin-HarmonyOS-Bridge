# DHB Deepin 25 client plan

> This directory is the Deepin/DDE integration layer. The **D-Bus daemon and
> minimal DTK6 tray plugin are implemented and built/tested in Deepin 25**;
> `dhb-gui` and Linyaps runtime are not shipped. Do not describe the tray as a
> complete DDE panel or the Linyaps layer as runtime-verified.

## 1. Target

- Deepin/UOS v25, DTK6 / Qt6.
- The existing `dhb` CLI/daemon owns the protocol, files, clipboard and KLink
  helpers.
- Skill selection and Huawei-vs-OH/KaihongOS boundaries:
  [docs/16-skill-study-openharmony-kaihong.md](../docs/16-skill-study-openharmony-kaihong.md).
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

## 2.1 Current implementation (2026-10-30)

- `apps/dhb-daemon`: Qt6DBus service `org.dhb.Bridge`; `GetStatus`/`ListDevices`/
  `Pair`/`SendFile` tested end-to-end over a real TCP loopback session.
- `deepin/dhb-tray`: minimal `PluginsItemInterfaceV2` status item; built and
  loaded by `trayplugin-loader` (`Loaded plugin: "dhb-tray"`). No full DDE panel.
- `deepin/linglong.yaml`: source-build manifest for `dhb`; build/export succeeds
  with `--skip-run-container`. Runtime check fails in WSL nested containers
  (`remount failed`), so the layer is not runtime-verified.
- Build: `cmake -S . -B build -DDHB_BUILD_DESKTOP=ON`.
- Tests: `scripts/test-dbus-mvp.sh`, `scripts/test-dbus-pair.sh`,
  `scripts/test-tray-load.sh`.

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
