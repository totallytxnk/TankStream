#include "LanDiscovery.hpp"

#include <QNetworkInterface>
#include <QDateTime>
#include <QRandomGenerator>
#include <QHostAddress>

#include <iostream>

LanDiscovery::LanDiscovery(QObject* parent) : QObject(parent) {
    m_instanceId = makeInstanceId();
}

LanDiscovery::~LanDiscovery() {
    stopAll();
}

QString LanDiscovery::makeInstanceId() {
    const quint32 a = QRandomGenerator::global()->generate();
    const quint32 b = QRandomGenerator::global()->generate();
    return QString("%1-%2").arg(a, 8, 16, QChar('0')).arg(b, 8, 16, QChar('0'));
}

void LanDiscovery::stopAll() {
    stopSender();
    stopReceiver();
}

// ---------------------------------------------------------------------------
// Sender
// ---------------------------------------------------------------------------

bool LanDiscovery::startSender(const QString& displayName) {
    stopAll();
    m_isSender = true;
    m_displayName = displayName.isEmpty() ? QHostInfo::localHostName() : displayName;
    m_offerSdp.clear();

    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::Any, 0)) {
        emit statusMessage("Failed to start signalling server: " + m_tcpServer->errorString());
        delete m_tcpServer;
        m_tcpServer = nullptr;
        m_isSender = false;
        return false;
    }

    connect(m_tcpServer, &QTcpServer::newConnection, this, &LanDiscovery::onNewTcpConnection);

    m_udp = new QUdpSocket(this);
    // Allow broadcast
    m_udp->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    m_beaconTimer = new QTimer(this);
    connect(m_beaconTimer, &QTimer::timeout, this, &LanDiscovery::onBeaconTimer);
    m_beaconTimer->start(1500);
    onBeaconTimer();  // immediate

    emit statusMessage(
        QString("Broadcasting as \"%1\" (signalling port %2)")
            .arg(m_displayName)
            .arg(m_tcpServer->serverPort()));
    return true;
}

void LanDiscovery::setSenderOffer(const QString& offerSdp) {
    m_offerSdp = offerSdp;
    // If a client is already waiting, send now
    if (m_senderClient && m_senderClient->state() == QAbstractSocket::ConnectedState
        && !m_offerSdp.isEmpty()) {
        sendOfferToSocket(m_senderClient);
    }
}

void LanDiscovery::stopSender() {
    if (m_beaconTimer) {
        m_beaconTimer->stop();
        m_beaconTimer->deleteLater();
        m_beaconTimer = nullptr;
    }
    if (m_senderClient) {
        m_senderClient->disconnectFromHost();
        m_senderClient->deleteLater();
        m_senderClient = nullptr;
    }
    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }
    if (m_udp && m_isSender) {
        m_udp->close();
        m_udp->deleteLater();
        m_udp = nullptr;
    }
    m_senderBuf.clear();
    m_offerSdp.clear();
    m_isSender = false;
}

QByteArray LanDiscovery::buildBeacon() const {
    if (!m_tcpServer) return {};
    QByteArray b;
    b += "TANKSTREAM/1\n";
    b += "role=sender\n";
    b += "id=" + m_instanceId.toUtf8() + "\n";
    b += "name=" + m_displayName.toUtf8() + "\n";
    b += "port=" + QByteArray::number(m_tcpServer->serverPort()) + "\n";
    return b;
}

void LanDiscovery::onBeaconTimer() {
    if (!m_udp || !m_tcpServer) return;
    const QByteArray packet = buildBeacon();
    // Broadcast on all interfaces that support it
    m_udp->writeDatagram(packet, QHostAddress::Broadcast, DISCOVERY_PORT);

    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || !(iface.flags() & QNetworkInterface::CanBroadcast)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QHostAddress bcast = entry.broadcast();
            if (!bcast.isNull()) {
                m_udp->writeDatagram(packet, bcast, DISCOVERY_PORT);
            }
        }
    }
}

void LanDiscovery::onNewTcpConnection() {
    while (m_tcpServer && m_tcpServer->hasPendingConnections()) {
        QTcpSocket* sock = m_tcpServer->nextPendingConnection();
        // Single receiver for v1
        if (m_senderClient) {
            sock->disconnectFromHost();
            sock->deleteLater();
            continue;
        }
        m_senderClient = sock;
        m_senderBuf.clear();
        connect(sock, &QTcpSocket::readyRead, this, &LanDiscovery::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &LanDiscovery::onClientDisconnected);
        emit statusMessage("Receiver connected — exchanging SDP…");
        if (!m_offerSdp.isEmpty()) {
            sendOfferToSocket(sock);
        }
    }
}

void LanDiscovery::sendOfferToSocket(QTcpSocket* sock) {
    if (!sock || m_offerSdp.isEmpty()) return;
    QByteArray msg;
    msg += "OFFER\n";
    msg += m_offerSdp.toUtf8();
    if (!m_offerSdp.endsWith('\n')) msg += '\n';
    msg += "END\n";
    sock->write(msg);
    sock->flush();
}

void LanDiscovery::onClientReadyRead() {
    if (!m_senderClient) return;
    m_senderBuf += m_senderClient->readAll();
    handleSenderTcpBuffer(m_senderClient);
}

void LanDiscovery::handleSenderTcpBuffer(QTcpSocket* sock) {
    const int endPos = m_senderBuf.indexOf("\nEND\n");
    if (endPos < 0) {
        // also accept END at end without trailing newline variants
        if (!m_senderBuf.contains("END")) return;
    }

    // Find ANSWER block
    const int ans = m_senderBuf.indexOf("ANSWER\n");
    if (ans < 0) return;

    int start = ans + 7;
    int end = m_senderBuf.indexOf("\nEND", start);
    if (end < 0) end = m_senderBuf.indexOf("END", start);
    if (end < 0) return;

    QByteArray sdp = m_senderBuf.mid(start, end - start);
    // trim trailing newlines
    while (sdp.endsWith('\n') || sdp.endsWith('\r')) sdp.chop(1);

    m_senderBuf.clear();
    emit answerReceived(QString::fromUtf8(sdp));
    emit statusMessage("Answer received — connecting media…");
    Q_UNUSED(sock);
}

void LanDiscovery::onClientDisconnected() {
    if (sender() == m_senderClient) {
        m_senderClient->deleteLater();
        m_senderClient = nullptr;
        m_senderBuf.clear();
        emit statusMessage("Receiver disconnected");
    }
}

// ---------------------------------------------------------------------------
// Receiver
// ---------------------------------------------------------------------------

bool LanDiscovery::startReceiver() {
    stopAll();
    m_isReceiver = true;
    m_peers.clear();

    m_udp = new QUdpSocket(this);
    if (!m_udp->bind(QHostAddress::AnyIPv4, DISCOVERY_PORT,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit statusMessage("Failed to bind discovery port: " + m_udp->errorString());
        delete m_udp;
        m_udp = nullptr;
        m_isReceiver = false;
        return false;
    }
    connect(m_udp, &QUdpSocket::readyRead, this, &LanDiscovery::onUdpReady);

    m_staleTimer = new QTimer(this);
    connect(m_staleTimer, &QTimer::timeout, this, &LanDiscovery::onStaleTimer);
    m_staleTimer->start(2000);

    emit statusMessage("Listening for senders on the LAN…");
    return true;
}

void LanDiscovery::stopReceiver() {
    if (m_staleTimer) {
        m_staleTimer->stop();
        m_staleTimer->deleteLater();
        m_staleTimer = nullptr;
    }
    if (m_recvSocket) {
        m_recvSocket->disconnectFromHost();
        m_recvSocket->deleteLater();
        m_recvSocket = nullptr;
    }
    if (m_udp && m_isReceiver) {
        m_udp->close();
        m_udp->deleteLater();
        m_udp = nullptr;
    }
    m_recvBuf.clear();
    m_peers.clear();
    m_isReceiver = false;
}

void LanDiscovery::onUdpReady() {
    while (m_udp && m_udp->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(int(m_udp->pendingDatagramSize()));
        QHostAddress from;
        quint16 port = 0;
        m_udp->readDatagram(data.data(), data.size(), &from, &port);
        parseBeacon(data, from);
    }
}

void LanDiscovery::parseBeacon(const QByteArray& data, const QHostAddress& from) {
    if (!data.startsWith("TANKSTREAM/1")) return;

    QString id, name;
    quint16 signalPort = 0;
    bool isSender = false;

    const QList<QByteArray> lines = data.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.startsWith("role=")) {
            isSender = (line.mid(5) == "sender");
        } else if (line.startsWith("id=")) {
            id = QString::fromUtf8(line.mid(3));
        } else if (line.startsWith("name=")) {
            name = QString::fromUtf8(line.mid(5));
        } else if (line.startsWith("port=")) {
            signalPort = line.mid(5).toUShort();
        }
    }

    if (!isSender || id.isEmpty() || signalPort == 0) return;
    // Ignore our own beacons if any
    if (id == m_instanceId) return;

    PeerInfo info;
    info.id = id;
    info.name = name.isEmpty() ? from.toString() : name;
    info.address = from;
    info.signalPort = signalPort;
    info.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

    m_peers[id.toStdString()] = info;

    QList<PeerInfo> list;
    for (const auto& kv : m_peers) {
        list.push_back(kv.second);
    }
    emit peerListChanged(list);
}

void LanDiscovery::onStaleTimer() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        if (now - it->second.lastSeenMs > 5000) {
            it = m_peers.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        QList<PeerInfo> list;
        for (const auto& kv : m_peers) {
            list.push_back(kv.second);
        }
        emit peerListChanged(list);
    }
}

void LanDiscovery::connectToPeer(const PeerInfo& peer) {
    if (m_recvSocket) {
        m_recvSocket->disconnectFromHost();
        m_recvSocket->deleteLater();
        m_recvSocket = nullptr;
    }
    m_recvBuf.clear();

    m_recvSocket = new QTcpSocket(this);
    connect(m_recvSocket, &QTcpSocket::connected, this, &LanDiscovery::onReceiverSocketConnected);
    connect(m_recvSocket, &QTcpSocket::readyRead, this, &LanDiscovery::onReceiverSocketReadyRead);
    connect(m_recvSocket, &QTcpSocket::errorOccurred, this, &LanDiscovery::onReceiverSocketError);

    emit statusMessage(QString("Connecting to %1 (%2:%3)…")
                           .arg(peer.name, peer.address.toString())
                           .arg(peer.signalPort));
    m_recvSocket->connectToHost(peer.address, peer.signalPort);
}

void LanDiscovery::onReceiverSocketConnected() {
    emit statusMessage("Connected to sender — waiting for offer…");
}

void LanDiscovery::onReceiverSocketReadyRead() {
    if (!m_recvSocket) return;
    m_recvBuf += m_recvSocket->readAll();
    handleReceiverTcpBuffer();
}

void LanDiscovery::handleReceiverTcpBuffer() {
    const int off = m_recvBuf.indexOf("OFFER\n");
    if (off < 0) return;

    const int start = off + 6;
    int end = m_recvBuf.indexOf("\nEND", start);
    if (end < 0) end = m_recvBuf.indexOf("END", start);
    if (end < 0) return;

    QByteArray sdp = m_recvBuf.mid(start, end - start);
    while (sdp.endsWith('\n') || sdp.endsWith('\r')) sdp.chop(1);
    m_recvBuf.clear();

    emit offerReceived(QString::fromUtf8(sdp));
    emit statusMessage("Offer received — creating answer…");
}

void LanDiscovery::onReceiverSocketError(QAbstractSocket::SocketError) {
    if (m_recvSocket) {
        emit signalingFailed(m_recvSocket->errorString());
    }
}

void LanDiscovery::sendAnswer(const QString& answerSdp) {
    if (!m_recvSocket || m_recvSocket->state() != QAbstractSocket::ConnectedState) {
        emit signalingFailed("Not connected to sender");
        return;
    }
    QByteArray msg;
    msg += "ANSWER\n";
    msg += answerSdp.toUtf8();
    if (!answerSdp.endsWith('\n')) msg += '\n';
    msg += "END\n";
    m_recvSocket->write(msg);
    m_recvSocket->flush();
    emit statusMessage("Answer sent — waiting for media…");
}
