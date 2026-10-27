# Partial upload status — NOT the complete DHB repository

> **This repository is intentionally incomplete.**
> It is a partial code upload for review/bootstrap, not a release.

- Source snapshot: `87bdb3f` (`docs: sharpen README feature overview`)
- Intended repository: `https://github.com/THUDou/Deepin-HarmonyOS-Bridge`
- Visibility: public (proposed)
- Upload date: 2026-10-28
- Partial build verification: CMake/Ninja, `ctest 1/1 passed`, `dhb selftest OK`.
- Status at preparation time: **GitHub API/HTTPS was unreachable from the build host**
  (`api.github.com:443` timed out; `github.com` also timed out). The repository has
  not been created/pushed yet; the local partial git repo and bundle are ready.
- Local git credential: a token for `THUDou` was found via `git credential fill`;
  `gh auth status` reports the keyring token as invalid. Network is the blocker,
  not the absence of a credential.

## What is in this partial upload

- `include/`, `src/`, `apps/dhb-cli/`, `tests/`, `CMakeLists.txt`
  — a buildable Linux/Windows C++ core snapshot.
- `README.md`, `CHANGELOG.md`, `LICENSE`, `NOTICE`, `SECURITY.md`, `CONTRIBUTING.md`.
- `docs/03-protocol.md`, `docs/08-verification-log.md`,
  `docs/12-klink-linux-lab.md`, `docs/13-repo-plan.md`.
- `scripts/build-klink-lab.sh`, `scripts/klink-lab-*.sh`.
- `patches/klink-4.x-linux-lab.patch` and its README.

## What is NOT uploaded yet

- `ohos/` — HarmonyOS ArkTS/HAP client and DevEco project.
- `docs/01`, `02`, `04`, `05`, `06`, `07`, `09`, `10`, `11` and ADRs.
- `.github/` — CI, issue forms, PR template.
- `packaging/` — deb/systemd skeleton.
- Full `scripts/` (asset generation, OHOS config checker, bootstrap).
- `third_party/` — KLink/OpenHarmony clones.
- Build outputs, HAP/APP files, `oh_modules/`, `local.properties`, certificates,
  PSKs, and real-device logs.

## What this partial snapshot can do

- Build the Linux/Windows C++ core:

  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
  ctest --test-dir build --output-on-failure
  ./build/dhb selftest
  ```

- Run the local TCP file/pair/discovery demo (see `README.md`).
- Inspect the DHB protocol and verification boundaries.
- Reproduce the KLink 4.x host build only after fetching KLink separately:
  `scripts/build-klink-lab.sh` expects `third_party/klink` and a Linux filesystem.

## What is still unverified

- Gate A: KLink 4.x ↔ OH 4.1/HarmonyOS 4.x.
- Gate B: OpenHarmony 6.0 ↔ KaihongOS 6.x.
- HAP signing, installation, and runtime.
- KLink PIN authentication (the lab CLOUD_SN path is permissive and lab-only).
- TLS 1.3, screen collaboration, input control, notifications.
- Deepin 25/UOS real-machine build.

## Next upload phases

1. Add `ohos/` (HAP/ArkTS), `docs/01–11`, ADRs, `.github/`, `packaging/`.
2. Add the complete `scripts/` set and reproducibility notes.
3. Add signed HAP/release assets only after signing and device verification.
4. Update `docs/08` before claiming any Gate A/B result.

If you are reading this on GitHub and the tree looks incomplete: **that is expected**.
Use the full local repository or wait for the next upload phase.
