#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QHostInfo>

#include <functional>
#include <string>
#include <unordered_map>

/**
 * LAN discovery + simple TCP signalling for TankStream.
 *
 * Discovery: UDP broadcast on DISCOVERY_PORT
 * Signalling: TCP, full SDP offer/answer (non-trickle)
 *
 * Protocol (UDP beacon, UTF-8 lines):
 *   TANKSTREAM/1
 *   role=sender
 *   id=<unique>
 *   name=<display name>
 *   port=<tcp signalling port>
 *
 * Protocol (TCP):
 *   OFFER\n
 *   <sdp>\n
 *   END\n
 *   ANSWER\n
 *   <sdp>\n
 *   END\n
 */
class LanDiscovery : public QObject {
    Q_OBJECT
public:
    static constexpr quint16 DISCOVERY_PORT = 47829;

    struct PeerInfo {
        QString id;
        QString name;
        QHostAddress address;
        quint16 signalPort = 0;
        qint64 lastSeenMs = 0;
    };

    explicit LanDiscovery(QObject* parent = nullptr);
    ~LanDiscovery() override;

    // ---- Sender side ----
    /** Start TCP signalling server + UDP beacons. offerSdp may be set later. */
    bool startSender(const QString& displayName);
    void setSenderOffer(const QString& offerSdp);
    void stopSender();

    // ---- Receiver side ----
    bool startReceiver();
    void stopReceiver();
    void connectToPeer(const PeerInfo& peer);
    /** Receiver: send answer SDP back to the sender over TCP. */
    void sendAnswer(const QString& answerSdp);

    void stopAll();

signals:
    void peerListChanged(const QList<LanDiscovery::PeerInfo>& peers);
    void statusMessage(const QString& msg);
    /** Receiver obtained remote offer */
    void offerReceived(const QString& sdp);
    /** Sender obtained remote answer */
    void answerReceived(const QString& sdp);
    void signalingFailed(const QString& reason);

private slots:
    void onBeaconTimer();
    void onUdpReady();
    void onNewTcpConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onReceiverSocketReadyRead();
    void onReceiverSocketConnected();
    void onReceiverSocketError(QAbstractSocket::SocketError err);
    void onStaleTimer();

private:
    QByteArray buildBeacon() const;
    void parseBeacon(const QByteArray& data, const QHostAddress& from);
    void sendOfferToSocket(QTcpSocket* sock);
    void handleSenderTcpBuffer(QTcpSocket* sock);
    void handleReceiverTcpBuffer();
    static QString makeInstanceId();

    // UDP
    QUdpSocket* m_udp = nullptr;
    QTimer* m_beaconTimer = nullptr;
    QTimer* m_staleTimer = nullptr;

    // Sender TCP
    QTcpServer* m_tcpServer = nullptr;
    QTcpSocket* m_senderClient = nullptr;  // one active receiver at a time
    QByteArray m_senderBuf;
    QString m_offerSdp;
    QString m_instanceId;
    QString m_displayName;
    bool m_isSender = false;

    // Receiver TCP
    QTcpSocket* m_recvSocket = nullptr;
    QByteArray m_recvBuf;
    bool m_isReceiver = false;

    std::unordered_map<std::string, PeerInfo> m_peers;
};
