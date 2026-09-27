#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QStatusBar>
#include <QTimer>
#include <QImage>
#include <QMutex>
#include <QLineEdit>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "tankstream/H264Encoder.hpp"
#include "tankstream/H264Decoder.hpp"
#include "tankstream/WebRTCBridge.hpp"
#include "tankstream/WebRTCReceiver.hpp"
#include "LanDiscovery.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onModeChanged(int index);
    void onStartSender();
    void onStopSender();
    void onStartReceiver();
    void onStopReceiver();
    void onConnectPeer();
    void onPeerListChanged(const QList<LanDiscovery::PeerInfo>& peers);
    void onDiscoveryStatus(const QString& msg);
    void onOfferFromLan(const QString& sdp);
    void onAnswerFromLan(const QString& sdp);
    void onSignalingFailed(const QString& reason);
    void onFrameTick();

private:
    void buildUi();
    void setStatus(const QString& text);
    void stopSenderInternal();
    void stopReceiverInternal();
    void generateSyntheticNV12(std::vector<uint8_t>& buf, int w, int h, int frameIdx);
    void showVideoFrame(const QImage& img);

    QComboBox*    m_modeBox = nullptr;
    QWidget*      m_senderPage = nullptr;
    QWidget*      m_receiverPage = nullptr;
    QLineEdit*    m_nameEdit = nullptr;
    QPushButton*  m_startBtn = nullptr;
    QPushButton*  m_stopBtn = nullptr;
    QPushButton*  m_recvStartBtn = nullptr;
    QPushButton*  m_recvStopBtn = nullptr;
    QPushButton*  m_connectBtn = nullptr;
    QListWidget*  m_peerList = nullptr;
    QLabel*       m_videoLabel = nullptr;
    QLabel*       m_senderHint = nullptr;
    QStatusBar*   m_status = nullptr;
    QTimer*       m_frameTimer = nullptr;

    LanDiscovery* m_lan = nullptr;
    QList<LanDiscovery::PeerInfo> m_currentPeers;

    std::unique_ptr<H264Encoder>  m_encoder;
    std::unique_ptr<WebRTCBridge> m_bridge;
    std::thread m_sendThread;
    std::atomic<bool> m_sending{false};

    std::unique_ptr<H264Decoder>    m_decoder;
    std::unique_ptr<WebRTCReceiver> m_receiver;
    QMutex m_frameMutex;
    QImage m_pendingFrame;
    std::atomic<bool> m_hasFrame{false};
};
