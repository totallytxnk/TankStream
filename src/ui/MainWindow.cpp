#include "MainWindow.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QHostInfo>

#include <chrono>
#include <cstring>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_lan = new LanDiscovery(this);
    connect(m_lan, &LanDiscovery::peerListChanged, this, &MainWindow::onPeerListChanged);
    connect(m_lan, &LanDiscovery::statusMessage, this, &MainWindow::onDiscoveryStatus);
    connect(m_lan, &LanDiscovery::offerReceived, this, &MainWindow::onOfferFromLan);
    connect(m_lan, &LanDiscovery::answerReceived, this, &MainWindow::onAnswerFromLan);
    connect(m_lan, &LanDiscovery::signalingFailed, this, &MainWindow::onSignalingFailed);

    buildUi();
    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, &QTimer::timeout, this, &MainWindow::onFrameTick);
    m_frameTimer->start(16);
    setStatus("Ready — choose Sender or Receiver");
}

MainWindow::~MainWindow() {
    stopSenderInternal();
    stopReceiverInternal();
}

void MainWindow::buildUi() {
    setWindowTitle("TankStream v0.5");
    resize(900, 640);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);

    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel("Mode:"));
    m_modeBox = new QComboBox;
    m_modeBox->addItem("Sender (stream out)");
    m_modeBox->addItem("Receiver (watch stream)");
    modeRow->addWidget(m_modeBox, 1);
    root->addLayout(modeRow);
    connect(m_modeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);

    // ---- Sender ----
    m_senderPage = new QWidget;
    auto* sLayout = new QVBoxLayout(m_senderPage);

    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Display name:"));
    m_nameEdit = new QLineEdit(QHostInfo::localHostName());
    nameRow->addWidget(m_nameEdit, 1);
    sLayout->addLayout(nameRow);

    auto* sBtns = new QHBoxLayout;
    m_startBtn = new QPushButton("Start Streaming");
    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setEnabled(false);
    sBtns->addWidget(m_startBtn);
    sBtns->addWidget(m_stopBtn);
    sLayout->addLayout(sBtns);

    m_senderHint = new QLabel(
        "This PC will appear on the LAN.\n"
        "On the other PC: choose Receiver → Start Listening → select this PC → Connect.\n"
        "No SDP copy-paste required.");
    m_senderHint->setWordWrap(true);
    m_senderHint->setStyleSheet("color:#555; padding:12px;");
    sLayout->addWidget(m_senderHint);
    sLayout->addStretch(1);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartSender);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopSender);

    // ---- Receiver ----
    m_receiverPage = new QWidget;
    auto* rLayout = new QVBoxLayout(m_receiverPage);

    m_videoLabel = new QLabel;
    m_videoLabel->setMinimumSize(640, 360);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setStyleSheet("background:#111; color:#888;");
    m_videoLabel->setText("Video will appear here");
    rLayout->addWidget(m_videoLabel, 2);

    auto* rBtns = new QHBoxLayout;
    m_recvStartBtn = new QPushButton("Start Listening");
    m_recvStopBtn = new QPushButton("Stop");
    m_recvStopBtn->setEnabled(false);
    m_connectBtn = new QPushButton("Connect");
    m_connectBtn->setEnabled(false);
    rBtns->addWidget(m_recvStartBtn);
    rBtns->addWidget(m_connectBtn);
    rBtns->addWidget(m_recvStopBtn);
    rLayout->addLayout(rBtns);

    rLayout->addWidget(new QLabel("Nearby senders:"));
    m_peerList = new QListWidget;
    rLayout->addWidget(m_peerList, 1);

    connect(m_recvStartBtn, &QPushButton::clicked, this, &MainWindow::onStartReceiver);
    connect(m_recvStopBtn, &QPushButton::clicked, this, &MainWindow::onStopReceiver);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectPeer);
    connect(m_peerList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onConnectPeer();
    });

    root->addWidget(m_senderPage, 1);
    root->addWidget(m_receiverPage, 1);
    m_receiverPage->hide();

    m_status = statusBar();
}

void MainWindow::onModeChanged(int index) {
    const bool sender = (index == 0);
    m_senderPage->setVisible(sender);
    m_receiverPage->setVisible(!sender);
    setStatus(sender ? "Sender mode" : "Receiver mode");
}

void MainWindow::setStatus(const QString& text) {
    if (m_status) m_status->showMessage(text);
}

void MainWindow::onDiscoveryStatus(const QString& msg) {
    setStatus(msg);
}

void MainWindow::onSignalingFailed(const QString& reason) {
    setStatus("Signalling failed: " + reason);
    QMessageBox::warning(this, "Signalling", reason);
}

void MainWindow::generateSyntheticNV12(std::vector<uint8_t>& buf, int w, int h, int frameIdx) {
    buf.resize(static_cast<size_t>(w * h * 3 / 2));
    uint8_t* y = buf.data();
    uint8_t* uv = y + w * h;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            y[row * w + col] = static_cast<uint8_t>((col + frameIdx * 3) & 0xFF);
        }
    }
    for (int row = 0; row < h / 2; ++row) {
        for (int col = 0; col < w; col += 2) {
            uv[row * w + col]     = static_cast<uint8_t>(128 + (frameIdx * 2 + col) % 64);
            uv[row * w + col + 1] = static_cast<uint8_t>(128 + (frameIdx + row) % 64);
        }
    }
}

void MainWindow::onStartSender() {
    stopSenderInternal();
    stopReceiverInternal();

    H264Encoder::Config ecfg;
    ecfg.width = 1280;
    ecfg.height = 720;
    ecfg.fps = 30;
    ecfg.bitrateKbps = 2500;

    m_encoder = std::make_unique<H264Encoder>();
    if (!m_encoder->init(ecfg)) {
        QMessageBox::critical(this, "Encoder", "Failed to open H.264 encoder");
        m_encoder.reset();
        return;
    }

    if (!m_lan->startSender(m_nameEdit->text().trimmed())) {
        QMessageBox::critical(this, "LAN", "Failed to start LAN discovery");
        m_encoder.reset();
        return;
    }

    m_bridge = std::make_unique<WebRTCBridge>();
    m_bridge->onLocalDescription([this](const std::string& sdp, const std::string& type) {
        Q_UNUSED(type);
        QMetaObject::invokeMethod(this, [this, sdp]() {
            m_lan->setSenderOffer(QString::fromStdString(sdp));
            setStatus("Offer ready — waiting for a Receiver to Connect…");
        }, Qt::QueuedConnection);
    });
    m_bridge->onStateChange([this](rtc::PeerConnection::State state) {
        QMetaObject::invokeMethod(this, [this, state]() {
            if (state == rtc::PeerConnection::State::Connected) {
                setStatus("CONNECTED — streaming");
            } else if (state == rtc::PeerConnection::State::Failed) {
                setStatus("Connection failed");
            }
        }, Qt::QueuedConnection);
    });

    if (!m_bridge->start()) {
        QMessageBox::critical(this, "WebRTC", "Failed to start PeerConnection");
        m_lan->stopSender();
        return;
    }

    m_sending = true;
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_sendThread = std::thread([this, ecfg]() {
        std::vector<uint8_t> nv12;
        int frameIdx = 0;
        using clock = std::chrono::steady_clock;
        auto next = clock::now();
        const auto interval = std::chrono::milliseconds(1000 / ecfg.fps);

        while (m_sending) {
            generateSyntheticNV12(nv12, ecfg.width, ecfg.height, frameIdx);
            const int64_t ptsMs = frameIdx * (1000 / ecfg.fps);
            auto encoded = m_encoder->encodeNV12(nv12.data(), ptsMs);
            if (!encoded.empty() && m_bridge) {
                m_bridge->sendVideoFrame(encoded.data(), encoded.size(), ptsMs);
            }
            ++frameIdx;
            next += interval;
            std::this_thread::sleep_until(next);
        }
    });
}

void MainWindow::onStopSender() {
    stopSenderInternal();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    setStatus("Sender stopped");
}

void MainWindow::stopSenderInternal() {
    m_sending = false;
    if (m_sendThread.joinable()) m_sendThread.join();
    if (m_lan) m_lan->stopSender();
    if (m_bridge) {
        m_bridge->stop();
        m_bridge.reset();
    }
    m_encoder.reset();
}

void MainWindow::onAnswerFromLan(const QString& sdp) {
    if (!m_bridge) return;
    m_bridge->setRemoteDescription(sdp.toStdString(), "answer");
    setStatus("Answer applied — connecting media…");
}

void MainWindow::onStartReceiver() {
    stopSenderInternal();
    stopReceiverInternal();

    m_decoder = std::make_unique<H264Decoder>();
    if (!m_decoder->init()) {
        QMessageBox::critical(this, "Decoder", "Failed to open H.264 decoder");
        m_decoder.reset();
        return;
    }

    if (!m_lan->startReceiver()) {
        QMessageBox::critical(this, "LAN", "Failed to listen for senders");
        m_decoder.reset();
        return;
    }

    m_recvStartBtn->setEnabled(false);
    m_recvStopBtn->setEnabled(true);
    m_connectBtn->setEnabled(true);
    m_videoLabel->setText("Select a sender and click Connect");
    setStatus("Listening for senders…");
}

void MainWindow::onStopReceiver() {
    stopReceiverInternal();
    m_recvStartBtn->setEnabled(true);
    m_recvStopBtn->setEnabled(false);
    m_connectBtn->setEnabled(false);
    m_peerList->clear();
    m_currentPeers.clear();
    m_videoLabel->setText("Video will appear here");
    m_videoLabel->setPixmap(QPixmap());
    setStatus("Receiver stopped");
}

void MainWindow::stopReceiverInternal() {
    if (m_lan) m_lan->stopReceiver();
    if (m_receiver) {
        m_receiver->stop();
        m_receiver.reset();
    }
    m_decoder.reset();
    m_hasFrame = false;
}

void MainWindow::onPeerListChanged(const QList<LanDiscovery::PeerInfo>& peers) {
    m_currentPeers = peers;
    const int row = m_peerList->currentRow();
    m_peerList->clear();
    for (const auto& p : peers) {
        m_peerList->addItem(QString("%1  —  %2:%3")
                                .arg(p.name, p.address.toString())
                                .arg(p.signalPort));
    }
    if (row >= 0 && row < m_peerList->count()) {
        m_peerList->setCurrentRow(row);
    }
}

void MainWindow::onConnectPeer() {
    const int row = m_peerList->currentRow();
    if (row < 0 || row >= m_currentPeers.size()) {
        QMessageBox::information(this, "Connect", "Select a sender from the list first");
        return;
    }

    if (m_receiver) {
        m_receiver->stop();
        m_receiver.reset();
    }
    m_receiver = std::make_unique<WebRTCReceiver>();

    m_receiver->onLocalDescription([this](const std::string& sdp, const std::string& type) {
        Q_UNUSED(type);
        QMetaObject::invokeMethod(this, [this, sdp]() {
            m_lan->sendAnswer(QString::fromStdString(sdp));
        }, Qt::QueuedConnection);
    });

    m_receiver->onStateChange([this](rtc::PeerConnection::State state) {
        QMetaObject::invokeMethod(this, [this, state]() {
            if (state == rtc::PeerConnection::State::Connected) {
                setStatus("CONNECTED — receiving");
            } else if (state == rtc::PeerConnection::State::Failed) {
                setStatus("Connection failed");
            }
        }, Qt::QueuedConnection);
    });

    m_receiver->onVideoFrame([this](const uint8_t* data, size_t size, int64_t ptsMs) {
        if (!m_decoder) return;
        auto frame = m_decoder->decode(data, size, ptsMs);
        if (frame.rgb.empty() || frame.width <= 0) return;
        QImage img(frame.rgb.data(), frame.width, frame.height,
                   frame.width * 3, QImage::Format_RGB888);
        QImage copy = img.copy();
        QMutexLocker lock(&m_frameMutex);
        m_pendingFrame = std::move(copy);
        m_hasFrame = true;
    });

    m_lan->connectToPeer(m_currentPeers[row]);
}

void MainWindow::onOfferFromLan(const QString& sdp) {
    if (!m_receiver) {
        setStatus("Offer received but receiver not ready");
        return;
    }
    if (!m_receiver->acceptOffer(sdp.toStdString())) {
        QMessageBox::critical(this, "WebRTC", "Failed to apply offer");
        return;
    }
    setStatus("Offer applied — creating answer…");
}

void MainWindow::onFrameTick() {
    if (!m_hasFrame) return;
    QImage img;
    {
        QMutexLocker lock(&m_frameMutex);
        if (!m_hasFrame) return;
        img = m_pendingFrame;
        m_hasFrame = false;
    }
    showVideoFrame(img);
}

void MainWindow::showVideoFrame(const QImage& img) {
    if (img.isNull() || !m_videoLabel) return;
    const QPixmap px = QPixmap::fromImage(img).scaled(
        m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_videoLabel->setPixmap(px);
}
