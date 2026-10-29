# DHB source snapshot — runtime still incomplete

> **This is the full tracked source snapshot, not a release.**
> HAP is unsigned; Gate A/B are not passed; no device runtime is claimed.

- Source commit: `d2ac0be` (`docs: separate Huawei HarmonyOS, KaihongOS and OpenHarmony`)
- Repository: `https://github.com/THUDou/Deepin-HarmonyOS-Bridge`
- Visibility: public
- Snapshot date: 2026-10-30
- Default branch: `main`

## Included

- C++17 core: `include/`, `src/`, `apps/dhb-cli/`, `tests/`, `CMakeLists.txt`\n- Deepin 25 client plan + `deepin/org.dhb.Bridge.xml` (design, not built)
- HarmonyOS ArkTS/HAP source: `ohos/`
- Docs: protocol, architecture, verification, softbus profiles, auth-first MVP,
  KLink→OH6 rebase, KLink Linux lab, AliOS research, repo plan, ADRs
- Scripts: build/asset/OHOS validation, KLink lab
- `patches/klink-4.x-linux-lab.patch` (lab-only, not upstream)
- `packaging/` deb/systemd skeleton
- `.github/` CI, issue forms, PR template

## Not included

- `third_party/` clones (KLink/OpenHarmony)
- Build outputs, `.hap`/`.app`, `oh_modules/`, `local.properties`
- Signing certificates, PSKs, real-device logs
- Any signed or installable HAP

## Verified at this snapshot

- Windows/MinGW CMake + `ctest 1/1` + `dhb selftest`
- Windows/MSVC build fix: parenthesized `std::min` to avoid the `min` macro;
  GitHub Actions run `40924781610`: Linux + Windows both **success**
- MSYS2 POSIX all tests
- Ubuntu 22.04 container CMake + `ctest 1/1` + TCP loopback + UDP discovery
- HAP compile with DevEco 26 / SDK 26 (`BUILD SUCCESSFUL`, unsigned)
- KLink 4.x Linux host build + two-container COAP discovery
- KLink CLOUD_SN **lab** auth: `AUTH_REQUEST_FINISH`, both peers have `networkId`
- Hash-based `.part` resume: 700 KiB file, offset 204807, final bytes and cleanup

## Still unverified

- Gate A: KLink 4.x ↔ OH 4.1 / HarmonyOS 4.x
- Gate B: OpenHarmony 6.0 ↔ KaihongOS 6.x
- KLink PIN authentication (lab CLOUD_SN uses a permissive SN callback)
- `GetAllNodeDeviceInfo` / `OpenSession` after lab auth
- HAP signing, installation, runtime
- TLS 1.3, screen collaboration, input control, notifications
- Deepin 25/UOS real-machine build

## Notes

GitHub `git push` over `github.com:443` is blocked from the current build host;
updates use the Git Data API with a valid Git Credential Manager token.
The earlier `UPLOAD-STATUS.md` “partial” phase is superseded by this snapshot.
