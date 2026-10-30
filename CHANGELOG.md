# Changelog

All notable changes to DHB are documented here. The project follows
[Semantic Versioning](https://semver.org/) once it reaches a stable release.

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

### Verified

- Deepin 25 WSL: CMake build, `ctest` 1/1, `dhb selftest`.
- D-Bus `Pair`/`SendFile`: 22-byte payload byte-identical on the server.
- Tray plugin loader log: `Loaded plugin: "dhb-tray"`.

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
