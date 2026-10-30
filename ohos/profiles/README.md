# KaihongOS build profile (candidate)

`kaihong-5.0.1.build-profile.json5` is a **candidate** profile for
KaihongOS Desktop 5.0.1. It is not wired into `ohos/build-profile.json5` and
has not been built against the Kaihong SDK.

Why it exists:

- KaihongOS Desktop 5.0.1 ships OpenHarmony SDK **API 11 / API 14**;
- the current default product is `runtimeOS: HarmonyOS`, target API 20;
- those are different product profiles, not one profile with a renamed string.

Before using it:

1. install the KaihongOS 5.0.1 devkit in its supported **ARM64** Linux container
   (or use a Kaihong PC);
2. create an API 14 project with the official ArkTS VS Code plugin and compare
   its generated `build-profile.json5` with this file;
3. replace the numeric SDK values if the Kaihong SDK uses different version
   strings;
4. build/sign/install an empty HAP first, then DHB with only `INTERNET`;
5. only then test `DISTRIBUTED_DATASYNC` and dsoftbus.

Do not copy this file over the existing profile and claim a Kaihong build until
steps 1–4 have evidence. See
[../../docs/18-kaihongos-desktop-5.0.1-notes.md](../../docs/18-kaihongos-desktop-5.0.1-notes.md).
