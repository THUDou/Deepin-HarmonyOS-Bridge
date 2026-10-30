// DHB - Deepin-HarmonyOS-Bridge
// SPDX-License-Identifier: Apache-2.0
//
// Minimal org.dhb.Bridge D-Bus daemon. It owns one DHB Session and exposes
// status/list/pair/send over the session bus. This is an MVP, not a full
// transfer manager: SendFile is synchronous and CancelTransfer is a stub.
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QString>
#include <QVariantMap>

#include <iostream>
#include <memory>
#include <string>

#include "dhb/session.h"

namespace {

QString toQString(const std::string& value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

class BridgeDaemon final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.dhb.Bridge")

 public:
  explicit BridgeDaemon(QString recvDir) : recvDir_(std::move(recvDir)) {}

  ~BridgeDaemon() override { dropSession(); }

 public slots:
  QVariantMap GetStatus() const {
    QVariantMap status;
    status.insert(QStringLiteral("version"), QStringLiteral("0.1.0"));
    status.insert(QStringLiteral("connected"), session_ != nullptr);
    status.insert(QStringLiteral("recvDir"), recvDir_);
    status.insert(QStringLiteral("interface"), QStringLiteral("org.dhb.Bridge"));
    if (session_ != nullptr) {
      const dhb::DeviceInfo& peer = session_->peer();
      status.insert(QStringLiteral("peerId"), toQString(peer.id));
      status.insert(QStringLiteral("peerName"), toQString(peer.name));
      status.insert(QStringLiteral("peerOs"), toQString(peer.os));
      status.insert(QStringLiteral("peerVersion"), toQString(peer.app_version));
      // MVP has no separate networkId in the app profile; the peer device id is
      // the only stable identifier available here.
      status.insert(QStringLiteral("peerAddress"), toQString(peer.id));
    }
    return status;
  }

  QList<QVariantMap> ListDevices() const {
    QList<QVariantMap> devices;
    if (session_ == nullptr) return devices;
    const dhb::DeviceInfo& peer = session_->peer();
    QVariantMap device;
    device.insert(QStringLiteral("id"), toQString(peer.id));
    device.insert(QStringLiteral("name"), toQString(peer.name));
    device.insert(QStringLiteral("os"), toQString(peer.os));
    device.insert(QStringLiteral("version"), toQString(peer.app_version));
    device.insert(QStringLiteral("networkId"), toQString(peer.id));
    devices.append(device);
    return devices;
  }

  QString GetReceiveDir() const { return recvDir_; }

  bool SetReceiveDir(const QString& path) {
    if (path.isEmpty()) return false;
    recvDir_ = path;
    return true;
  }

  bool Pair(const QString& host, ushort port, const QString& psk) {
    if (host.isEmpty() || port == 0 || psk.isEmpty()) return false;
    dropSession();

    std::string err;
    auto transport = dhb::TcpTransport::connect(
        host.toStdString(), static_cast<std::uint16_t>(port), 10000, &err);
    if (!transport) {
      std::cerr << "dhb-daemon: connect failed: " << err << "\n";
      emit StatusChanged(QStringLiteral("disconnected"));
      return false;
    }

    dhb::SessionConfig config;
    config.local.id = "dhb-daemon";
    config.local.name = "Deepin DHB";
    config.local.os = "Deepin/UOS Linux";
    config.local.app_version = "0.1.0";
    config.psk = psk.toStdString();
    config.recv_dir = recvDir_.toStdString();
    config.is_client = true;

    auto session = std::make_unique<dhb::Session>(std::move(transport), config);
    session->setLogHandler([](std::string message) {
      std::cerr << "dhb-daemon: " << message << "\n";
    });
    if (!session->handshake(&err) || !session->start(&err)) {
      std::cerr << "dhb-daemon: handshake/start failed: " << err << "\n";
      session->close();
      emit StatusChanged(QStringLiteral("disconnected"));
      return false;
    }
    session_ = std::move(session);
    emit StatusChanged(QStringLiteral("connected"));
    emit DeviceFound(ListDevices().constFirst());
    return true;
  }

  bool SendFile(const QString& path) {
    if (session_ == nullptr || path.isEmpty()) return false;
    std::string err;
    const bool ok = session_->sendFile(path.toStdString(), nullptr, &err);
    emit TransferFinished(path, ok, toQString(err));
    return ok;
  }

  bool CancelTransfer(const QString&) {
    // The current Session API has no cancellation token. Keep the D-Bus contract
    // honest: report false instead of pretending the transfer was stopped.
    return false;
  }

 signals:
  void DeviceFound(const QVariantMap& device);
  void TransferProgress(const QString& id, qulonglong sent, qulonglong total);
  void TransferFinished(const QString& id, bool ok, const QString& message);
  void StatusChanged(const QString& status);

 private:
  void dropSession() {
    if (session_ != nullptr) {
      session_->close();
      session_.reset();
    }
  }

  QString recvDir_;
  std::unique_ptr<dhb::Session> session_;
};

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("dhb-daemon"));

  qDBusRegisterMetaType<QList<QVariantMap>>();

  QString recvDir = QStringLiteral("/tmp/dhb-recv");
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--recv-dir") recvDir = QString::fromLocal8Bit(argv[++i]);
  }

  BridgeDaemon daemon(recvDir);
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    std::cerr << "dhb-daemon: session bus is not available\n";
    return 1;
  }
  if (!bus.registerService(QStringLiteral("org.dhb.Bridge"))) {
    std::cerr << "dhb-daemon: cannot register org.dhb.Bridge: "
              << bus.lastError().message().toStdString() << "\n";
    return 1;
  }
  if (!bus.registerObject(QStringLiteral("/org/dhb/Bridge"), &daemon,
                          QDBusConnection::ExportAllSlots |
                              QDBusConnection::ExportAllSignals)) {
    std::cerr << "dhb-daemon: cannot register /org/dhb/Bridge: "
              << bus.lastError().message().toStdString() << "\n";
    return 1;
  }
  std::cout << "dhb-daemon: org.dhb.Bridge ready, recv="
            << recvDir.toStdString() << "\n";
  return app.exec();
}

#include "main.moc"
