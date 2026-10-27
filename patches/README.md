# patches

## `klink-4.x-linux-lab.patch`

Host bring-up patch for **KLink 4.x (`5f0d585`)**. It is applied automatically by
[`scripts/build-klink-lab.sh`](../scripts/build-klink-lab.sh); do not apply it to
OpenHarmony 6.x.

Contains:

- `CMakeLists.txt`: `ifname=eth0` for the lab; add `bounds_checking_function` and
  `third_party` include roots.
- `third_party/coap/CMakeLists.txt`: add `include` and `securec` include paths.
- `main/CMakeLists.txt`: skip the Qt `devicemanagerclient`/`business_demo`, keep
  `server` + `devicemanagercmd` so the host stack builds without Qt.
- `dsoftbus/.../session_service_impl.cpp`: add `<mutex>` (GCC 11 no longer gets it
  transitively).
- `base/begetutil/.../parameter.cpp`: add `0600` mode to `open(O_CREAT)`; GCC 11
  rejects the old two-argument call at `-O2`.
- `main/devicemanagercmd/devicemanagercmd.cpp`: lab-only discovery/publish,
  CLOUD_SN authentication, and permissive `OnCheckDeviceSNVaild` for the
  Linux↔Linux trust experiment.

The patch is **not** an upstream submission. It exists to make the Linux lab
reproducible and to document the host deltas; see
[`docs/12-klink-linux-lab.md`](../docs/12-klink-linux-lab.md).
