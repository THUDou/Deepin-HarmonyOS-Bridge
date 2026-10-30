// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
//
// Minimal DDE tray plugin. It intentionally only renders a status item and
// leaves all protocol work to dhb-daemon over org.dhb.Bridge.
#include <QIcon>
#include <QLabel>
#include <QWidget>
#include <QtPlugin>

#include "pluginsiteminterface_v2.h"

namespace {

constexpr const char* kItemKey = "dhb-tray";

class DhbTrayPlugin final : public QObject, public PluginsItemInterfaceV2 {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "com.deepin.dock.PluginsItemInterface_V2" FILE
                        "dhb-tray.json")
  Q_INTERFACES(PluginsItemInterfaceV2)

 public:
  const QString pluginName() const override { return QStringLiteral("dhb-tray"); }
  const QString pluginDisplayName() const override { return QStringLiteral("DHB"); }

  void init(PluginProxyInterface* proxyInter) override {
    m_proxyInter = proxyInter;
    if (m_proxyInter != nullptr) {
      m_proxyInter->itemAdded(this, QString::fromLatin1(kItemKey));
    }
  }

  QWidget* itemWidget(const QString& itemKey) override {
    if (itemKey != QString::fromLatin1(kItemKey)) return nullptr;
    if (m_widget == nullptr) {
      auto* label = new QLabel(QStringLiteral("DHB"));
      label->setToolTip(QStringLiteral("Deepin-HarmonyOS-Bridge"));
      label->setFixedSize(Dock::TRAY_PLUGIN_ITEM_FIXED_SIZE);
      label->setAlignment(Qt::AlignCenter);
      m_widget = label;
    }
    return m_widget;
  }

  QWidget* itemTipsWidget(const QString& itemKey) override {
    if (itemKey != QString::fromLatin1(kItemKey)) return nullptr;
    auto* label = new QLabel(QStringLiteral("DHB：通过 dhb-daemon 传输"));
    label->setAlignment(Qt::AlignCenter);
    return label;
  }

  Dock::PluginFlags flags() const override {
    return Dock::Type_Tray | Dock::Attribute_Normal;
  }

  QIcon icon(Dock::IconType, Dock::ThemeType) const override {
    return QIcon::fromTheme(QStringLiteral("network-wired"));
  }

  bool pluginIsAllowDisable() override { return false; }
  bool pluginIsDisable() override { return false; }

 private:
  QWidget* m_widget = nullptr;
};

}  // namespace

#include "dhb-tray.moc"
