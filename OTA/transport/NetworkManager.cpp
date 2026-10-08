#include "NetworkManager.h"
#include "OtaProtocol.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QNetworkInterface>

namespace {

QString frameToHexPreview(const QByteArray &frame, int maxBytes = 32)
{
    const int n = qMin(frame.size(), maxBytes);
    QString hex;
    hex.reserve(n * 3);
    for (int i = 0; i < n; ++i) {
        if (i > 0)
            hex.append(QLatin1Char(' '));
        hex.append(QStringLiteral("%1").arg(static_cast<quint8>(frame.at(i)), 2, 16, QChar('0')));
    }
    if (frame.size() > n)
        hex.append(QStringLiteral(" ..."));
    return hex;
}

} // namespace

NetworkManager::NetworkManager(QObject *parent)
    : OtaTransport(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
}

NetworkManager::~NetworkManager()
{
    close();
}

void NetworkManager::moveInternalObjectsToThread(QThread *thread)
{
    if (!thread)
        return;

    if (m_server.thread() != thread)
        m_server.moveToThread(thread);
}

namespace {

bool isLan192168(const QHostAddress &addr)
{
    if (addr.protocol() != QAbstractSocket::IPv4Protocol || addr.isLoopback())
        return false;
    return addr.toString().startsWith(QStringLiteral("192.168."));
}

} // namespace

QStringList NetworkManager::localIPv4Addresses()
{
    QStringList list;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (isLan192168(addr))
                list << addr.toString();
        }
    }
    list.removeDuplicates();
    return list;
}

void NetworkManager::startListen(quint16 port)
{
    close();

    m_listenPort = port;
    if (m_listenPort == 0) {
        emit connectFinished(false, tr("监听端口无效"));
        return;
    }

    if (!m_server.listen(QHostAddress::Any, m_listenPort)) {
        const QString msg =
            tr("无法在端口 %1 启动 TCP Server：%2").arg(m_listenPort).arg(m_server.errorString());
        emit communicationError(msg);
        emit connectFinished(false, msg);
        return;
    }

    m_listenPending = true;

    const QStringList addrs = localIPv4Addresses();
    emit listenStarted(m_listenPort, addrs);
    emit logMessage(tr("TCP Server 已启动，监听端口 %1，持续等待设备连接（无超时）...").arg(m_listenPort),
                    QStringLiteral("INFO"));
    if (!addrs.isEmpty()) {
        emit logMessage(tr("请让单片机连接: %1:%2")
                            .arg(addrs.first())
                            .arg(m_listenPort),
                        QStringLiteral("INFO"));
    }
}

void NetworkManager::finishListenAttempt(bool success, const QString &message)
{
    if (!m_listenPending)
        return;

    m_listenPending = false;

    if (!success) {
        m_server.close();
        emit communicationError(message);
        emit connectFinished(false, message);
        return;
    }

    m_rxBuffer.clear();
    m_seqCounter = 1;
    emit opened(connectionLabel());
    emit logMessage(tr("设备已接入: %1").arg(m_peerLabel), QStringLiteral("INFO"));
    emit connectFinished(true, QString());
}

void NetworkManager::onNewConnection()
{
    QTcpSocket *socket = m_server.nextPendingConnection();
    if (!socket)
        return;

    teardownClient();
    m_client = socket;
    m_peerLabel = QStringLiteral("%1:%2")
                      .arg(m_client->peerAddress().toString())
                      .arg(m_client->peerPort());

    connect(m_client, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_client, &QTcpSocket::disconnected, this, &NetworkManager::onClientDisconnected);
    m_client->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_client->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    if (m_listenPending)
        finishListenAttempt(true, QString());
}

void NetworkManager::onClientDisconnected()
{
    emit logMessage(tr("设备已断开: %1").arg(m_peerLabel), QStringLiteral("INFO"));
    teardownClient();

    /* 保持 TCP Server 监听，便于 MCU 自动重连 */
    if (m_listenPort != 0U && !m_server.isListening()) {
        if (m_server.listen(QHostAddress::Any, m_listenPort)) {
            emit logMessage(tr("TCP Server 继续监听端口 %1，等待设备重连...").arg(m_listenPort),
                            QStringLiteral("INFO"));
        } else {
            emit communicationError(
                tr("重连监听失败（端口 %1）：%2").arg(m_listenPort).arg(m_server.errorString()));
        }
    }

    emit closed();
}

void NetworkManager::teardownClient()
{
    if (!m_client)
        return;

    m_client->disconnect(this);
    m_client->deleteLater();
    m_client = nullptr;
    m_peerLabel.clear();
    m_rxBuffer.clear();
    m_deferred.clear();
}

void NetworkManager::clearDeferredResponses()
{
    m_deferred.clear();
}

void NetworkManager::storeDeferredFrame(const OtaProtocol::Frame &frame)
{
    m_deferred.insert(frame.seq, frame);
}

void NetworkManager::ingestParsedFrames()
{
    while (true) {
        auto frame = OtaProtocol::tryParseFrame(m_rxBuffer);
        if (!frame)
            break;
        storeDeferredFrame(*frame);
    }
}

void NetworkManager::appendSocketRx(bool logPayload)
{
    if (!m_client)
        return;

    const QByteArray data = m_client->readAll();
    if (data.isEmpty())
        return;

    if (logPayload) {
        emit logMessage(tr("TCP RX %1 bytes: %2")
                            .arg(data.size())
                            .arg(frameToHexPreview(data)),
                        QStringLiteral("INFO"));
    }
    emit rawDataReceived(data);
    m_rxBuffer.append(data);
}

void NetworkManager::pumpReceive(int maxWaitMs)
{
    if (!m_client)
        return;

    if (maxWaitMs > 0) {
        if (m_client->waitForReadyRead(maxWaitMs))
            appendSocketRx(false);
    }
    /* readyRead 可能已把数据读进 m_rxBuffer，这里始终再抽一次并解析 */
    if (m_client->bytesAvailable() > 0)
        appendSocketRx(false);
    ingestParsedFrames();
}

std::optional<OtaProtocol::Frame> NetworkManager::takeDeferredResponse(
    OtaProtocol::Cmd expectedCmd, quint16 seq)
{
    auto it = m_deferred.find(seq);
    if (it == m_deferred.end())
        return std::nullopt;

    const quint8 expected = static_cast<quint8>(expectedCmd)
                            | static_cast<quint8>(OtaProtocol::Cmd::ResponseMask);
    if (static_cast<quint8>(it->cmd) != expected)
        return std::nullopt;

    OtaProtocol::Frame frame = it.value();
    m_deferred.erase(it);
    return frame;
}

void NetworkManager::close()
{
    m_listenPending = false;
    teardownClient();
    if (m_server.isListening())
        m_server.close();
}

bool NetworkManager::isListening() const
{
    return m_server.isListening();
}

bool NetworkManager::isOpen() const
{
    return m_client && m_client->state() == QAbstractSocket::ConnectedState;
}

QString NetworkManager::connectionLabel() const
{
    if (!isOpen())
        return QString();
    return m_peerLabel;
}

void NetworkManager::setResponseTimeoutMs(int ms)
{
    m_responseTimeoutMs = qMax(500, ms);
}

bool NetworkManager::sendCommandAtSeq(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 seq)
{
    if (!isOpen() || !m_client) {
        emit communicationError(tr("设备未连接"));
        return false;
    }

    const QByteArray frame = OtaProtocol::buildFrame(cmd, seq, payload);
    if (cmd != OtaProtocol::Cmd::Data) {
        emit logMessage(tr("TCP TX %1 seq %2 (%3 bytes): %4")
                            .arg(OtaProtocol::cmdToString(cmd))
                            .arg(seq)
                            .arg(frame.size())
                            .arg(frameToHexPreview(frame)),
                        QStringLiteral("INFO"));
    }
    const qint64 written = m_client->write(frame);
    if (written != frame.size()) {
        emit communicationError(tr("TCP write failed: %1").arg(m_client->errorString()));
        return false;
    }
    const int writeWaitMs =
        (cmd == OtaProtocol::Cmd::Data) ? 3000 : m_responseTimeoutMs;
    if (!m_client->waitForBytesWritten(writeWaitMs)) {
        emit communicationError(tr("TCP write timeout"));
        return false;
    }
    return true;
}

bool NetworkManager::sendCommand(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 &outSeq)
{
    outSeq = m_seqCounter++;
    return sendCommandAtSeq(cmd, payload, outSeq);
}

std::optional<OtaProtocol::Frame> NetworkManager::waitResponse(OtaProtocol::Cmd expectedCmd,
                                                                 quint16 seq, int timeoutMs)
{
    if (!m_client)
        return std::nullopt;

    if (auto cached = takeDeferredResponse(expectedCmd, seq))
        return cached;

    if (timeoutMs < 0)
        timeoutMs = m_responseTimeoutMs;

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        /* 短切片轮询：readyRead 与 wait 交错，避免 ACK 已进 deferred 仍空等 */
        const int slice = (expectedCmd == OtaProtocol::Cmd::Data) ? 10 : 50;
        const int remain = timeoutMs - static_cast<int>(timer.elapsed());
        pumpReceive(qMin(slice, qMax(1, remain)));

        if (m_client->bytesAvailable() > 0)
            appendSocketRx(false);
        ingestParsedFrames();

        if (auto cached = takeDeferredResponse(expectedCmd, seq)) {
            if (expectedCmd != OtaProtocol::Cmd::Data) {
                emit logMessage(tr("TCP RX %1 seq %2 OK (%3 ms)")
                                    .arg(OtaProtocol::cmdToString(expectedCmd))
                                    .arg(seq)
                                    .arg(timer.elapsed()),
                                QStringLiteral("INFO"));
            }
            return cached;
        }
    }
    if (expectedCmd != OtaProtocol::Cmd::Data) {
        emit communicationError(tr("等待 %1 seq %2 超时 (已等 %3 ms, 上限 %4 ms)")
                                    .arg(OtaProtocol::cmdToString(expectedCmd))
                                    .arg(seq)
                                    .arg(timer.elapsed())
                                    .arg(timeoutMs));
    }
    return std::nullopt;
}

bool NetworkManager::transact(OtaProtocol::Cmd cmd, const QByteArray &payload,
                              OtaProtocol::Frame &response, int timeoutMs)
{
    quint16 seq = 0;
    if (!sendCommand(cmd, payload, seq))
        return false;

    auto frame = waitResponse(cmd, seq, timeoutMs);
    if (!frame) {
        emit communicationError(tr("No response for %1 (seq %2)")
                                    .arg(OtaProtocol::cmdToString(cmd))
                                    .arg(seq));
        return false;
    }
    response = *frame;
    return true;
}

void NetworkManager::onReadyRead()
{
    if (!m_client)
        return;
    const QByteArray data = m_client->readAll();
    if (data.isEmpty())
        return;
    m_rxBuffer.append(data);
    ingestParsedFrames();
}
