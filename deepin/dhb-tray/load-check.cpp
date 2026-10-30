// DHB - standalone tray plugin ABI check (does not start a DDE session).
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>

#include "pluginsiteminterface_v2.h"

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (argc != 2) {
    qCritical() << "usage: dhb-tray-load-check <dhb-tray.so>";
    return 2;
  }
  QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
  QObject* instance = loader.instance();
  if (instance == nullptr) {
    qCritical() << "load failed:" << loader.errorString();
    return 1;
  }
  auto* plugin = qobject_cast<PluginsItemInterfaceV2*>(instance);
  if (plugin == nullptr) {
    qCritical() << "loaded object does not implement PluginsItemInterfaceV2";
    return 1;
  }
  qInfo() << "loaded" << plugin->pluginName() << plugin->pluginDisplayName();
  return 0;
}
