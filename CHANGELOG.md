# Changelog

All notable changes to DHB are documented here. The project follows
[Semantic Versioning](https://semver.org/) once it reaches a stable release.

## v0.1.0-alpha.4 — 2026-11-07

### Added

- HAP uses ArkUI `PasteButton` for temporary clipboard-read authorization.
- Linux ↔ HarmonyOS 2in1 text clipboard verified in both directions:
  Linux→HAP `DHB_CLIPBOARD clipboard-from-linux`; HAP→Linux 20-byte
  `clipboard-from-linux` received by server.
- SoftBus6 P0: `nstackx_util` and `nstackx_ctrl` Linux static libraries build.

### Not verified

- Kaihong x86 clipboard same path, image/rich-text clipboard.
- Screen sharing, keyboard/mouse control.
- Full SoftBus6 server/CoAP, openEuler RPM.
- Physical Huawei 2in1.

## v0.1.0-alpha.3 — 2026-11-07

### Added

- HAP UI v2 signed/unsigned release assets (Kaihong x86_64/2in1 form factor).
- Bidirectional Linux ↔ Kaihong HAP file transfer, verified by SHA-256:
  Linux→HAP 18 bytes, HAP→Linux 19 bytes.
- `dhb serve` 3s ping keepalive for idle HAP sessions.

### Not verified

- Bidirectional clipboard, screen sharing, keyboard/mouse control.
- SoftBus 6.x host build / openEuler RPM.
- Physical Huawei 2in1.

## v0.1.0-alpha.2 — 2026-11-06

### Added

- GitHub prerelease assets: signed and unsigned Kaihong x86_64 HAP + `SHA256SUMS`.
- HAP installed and launched on KaihongOS 6.1.1.11 (API 23, x86_64_system).
- Linux (Deepin 25 WSL) ↔ Kaihong HAP TCP + PSK authentication verified.
- HAP signed with the SDK OpenHarmony test profile; not a production certificate.

### Not verified

- File transfer on the Kaihong HAP session.
- Huawei HarmonyOS devices.
- KLink/dsoftbus 4.x/6.x real-device authentication.

## Unreleased

### Added

- `dhb-daemon`: Qt6DBus service `org.dhb.Bridge` with status/list/pair/send;
  real TCP loopback file transfer verified.
- `deepin/dhb-tray`: minimal `PluginsItemInterfaceV2` plugin; built and loaded
  by `trayplugin-loader`.
- `deepin/linglong.yaml`: source-build manifest for the `dhb` CLI; layer
  build/export succeeds with `--skip-run-container`.
- Deepin 25 WSL verification: DTK6 6.7.47.1, Qt6 6.8.0, `ll-builder` 1.14.3.
- Scripts: `test-dbus-mvp.sh`, `test-dbus-pair.sh`, `test-tray-load.sh`.
- `dhb serve`: 3s ping keepalive for idle HAP sessions; fixes stale
  "connected" UI with a dead socket before the first file offer.
- `dhb serve --clipboard-on-connect TEXT` for inbound clipboard testing;
  HAP logs `DHB_CLIPBOARD` on receive. End-to-end pasteboard verification is
  still blocked by the Kaihong lock screen / 2in1 IME dialog.
- HAP uses ArkUI `PasteButton` security component for temporary clipboard
  read authorization instead of `READ_PASTEBOARD`; installed on the 2in1
  emulator. Actual paste-and-send verification still pending.
- SoftBus6 P0: `scripts/build-softbus6-common-p0.sh` compiles OH6
  `core/common` into `libsoftbus6_common_p0.a` (9 objects, 159924 bytes),
  adds a minimal hilog stub and a working `log_smoke`; `adapter/common` Linux
  POSIX adapter also builds into `libsoftbus6_adapter_p0.a` (129760 bytes,
  192 undefined symbols); securec/cJSON/sqlite host deps and `deps_smoke`
  pass (`securec_rc=0 json_object=1 sqlite=3.37.2`); `link_smoke` links
  common/adapter/deps and executes `SoftBusGetSysTimeMs`; `nstackx_util`
  Linux library builds (109416 bytes, 185 undefined symbols); `nstackx_ctrl`
  core also builds into `libsoftbus6_nstackx_ctrl_p0.a` (198736 bytes,
  226 undefined symbols); full host server/CoAP still pending.

### Docs

- KaihongOS Desktop 5.0.1 notes: DevEco + Kaihong SDK / VS Code + ARM64 devkit;
  API 11/14; candidate API 14 HAP profile; Gate B0 added.
- Clarified that KaihongOS Meta 6.1 platform docs are not a desktop SDK.
- HAP portability note: standard HAP first, vendor profile only if install fails.
- HAP caps no longer claim screen/input before implementation; receive resume
  part name aligned to `.dhb-<sha256>.part`.
  Rebuilt 2026-11-05 with hvigor 6.26.9 / SDK 26.0.0.38 (unsigned, 209487 bytes).
- HAP signed with an OpenHarmony test profile and installed/launched on
  KaihongOS 6.1.1.11 (API 23, x86_64) via hdc `192.168.128.10:10178`.
  `READ_PASTEBOARD` was rejected by the device and removed from the manifest.

### Verified

- Deepin 25 WSL: CMake build, `ctest` 1/1, `dhb selftest`.
- D-Bus `Pair`/`SendFile`: 22-byte payload byte-identical on the server.
- Tray plugin loader log: `Loaded plugin: "dhb-tray"`.
- Kaihong HAP: `install bundle successfully`, `start ability successfully`.
- Linux ↔ Kaihong HAP file transfer, both directions:
  - Linux→HAP 18 bytes, SHA-256 `635a6bfc...`;
  - HAP→Linux 19 bytes, SHA-256 `60bb2065...`.
- `build-linux-r3/dhb_tests`: all DHB tests passed.
- Linux↔Kaihong: HAP authenticated to Deepin WSL `dhb serve` through a
  Windows TCP proxy (`peer: LAPTOP-AACOP06S (Deepin/UOS Linux)`).
- File transfer on that HAP session is **not** yet verified.

## v0.1.0-alpha — 2026-10-28

### Added

- DHB wire protocol v1: 20-byte header, CRC-32C, 16 MiB payload cap.
- PSK authentication: 64-hex or passphrase-derived key, dual nonces,
  challenge nonce, HMAC-SHA256, constant-time compare, downgrade refusal.
- File transfer: offer/accept/chunk/done/result, 256 KiB chunks, `.part`
  resume, SHA-256 verification, path sanitization.
- Clipboard text sync and UDP discovery (`47822`).
- `dhb` CLI: `serve`, `send`, `pair`, `recv`, `discover`, `doctor`, `selftest`.
- `dhb pair`: authentication-only path for the App-profile gate.
- HarmonyOS ArkTS client: protocol codec, TCP client, files, clipboard,
  UDP discovery, UI.
- HAP build with DevEco Studio 26 / SDK 26; `targetSdkVersion 6.0.0(20)`.
- KLink 4.x Linux host build recipe and lab patch.
- Linux↔Linux KLink COAP discovery and CLOUD_SN lab authentication.
- OpenHarmony 6.0 dsoftbus / device-auth source-diff report.
- Verification log, softbus profile ADR, auth-first MVP plan.
- Packaging skeleton for deb/systemd.
- CI for Linux and Windows core tests; repository plan and issue templates.

### Verified

- Linux (Ubuntu 22.04 container): CMake build, `ctest` 1/1, TCP file
  round-trip, UDP discovery.
- Windows/MinGW and MSYS2 POSIX: unit tests, TCP file transfer in both
  directions, clipboard, ping, wrong-PSK rejection.
- HAP compile: `BUILD SUCCESSFUL`, unsigned artifact 209247 bytes.
- KLink 4.x host build: `BUILD_RC=0`.
- KLink two-container COAP discovery: peer device ID/name/address visible.

### Known limitations

- **Gate A/B not passed**: no OH 4.1 / HarmonyOS 4.x / KaihongOS 6.x
  trusted-list or session verification.
- HAP is **unsigned** and has not been installed/run on a device.
- KLink PIN path fails with `status 5 / -20006`; only the lab CLOUD_SN
  path succeeds (permissive SN callback, not production).
- DHB TCP is plaintext; TLS 1.3 is not implemented.
- No screen collaboration, input control, or notification bridge yet.
- Deepin 25/UOS real-machine build and kernel/PipeWire/uinput are unverified.
- openEuler public dsoftbus is 3.2, not 6.x.
