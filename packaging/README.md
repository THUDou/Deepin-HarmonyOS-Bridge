# 打包

## Debian / Deepin / UOS

```bash
sudo apt install debhelper cmake g++ ninja-build
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../dhb_0.1.0_amd64.deb
```

产物包含：

- `/usr/bin/dhb`
- `/usr/include/dhb/*`
- systemd user unit 需要手动安装（见 `packaging/systemd/`）：

```bash
mkdir -p ~/.config/systemd/user
cp packaging/systemd/dhb.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now dhb.service
```

## HarmonyOS

HAP 由 DevEco Studio 构建；仓库只保留源码与配置。签名、上架、KaihongOS 系统签名流程见 `ohos/README.md` 与 `docs/02-architecture.md`。

## 后续

- M1：`dhbd` 独立包、D-Bus 激活、DDE 控制中心插件。
- M2：Klink 运行依赖/`/opt/dsoftbus` 的打包与权限配置。
- M4：UOS 商业签名、应用商店、企业分发。
