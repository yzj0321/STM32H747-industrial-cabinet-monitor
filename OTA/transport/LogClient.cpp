#include "LogClient.h"

#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QRegularExpression>

static bool isLan192168(const QHostAddress &addr)
{
    if (addr.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    const quint32 v = addr.toIPv4Address();
    return ((v >> 24) & 0xFFu) == 192u && ((v >> 16) & 0xFFu) == 168u;
}

LogClient::LogClient(QObject *parent)
    : QObject(parent)
{
    m_server.setProxy(QNetworkProxy::NoProxy);
    connect(&m_server, &QTcpServer::newConnection, this, &LogClient::onNewConnection);
}

QStringList LogClient::localIPv4Addresses()
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

bool LogClient::isListening() const
{
    return m_server.isListening();
}

bool LogClient::isConnected() const
{
    return m_socket != nullptr && m_socket->state() == QAbstractSocket::ConnectedState;
}

void LogClient::startListen(quint16 port)
{
    stop();
    if (port == 0) {
        emit errorOccurred(tr("监听端口无效"));
        return;
    }
    if (!m_server.listen(QHostAddress::Any, port)) {
        emit errorOccurred(tr("无法监听端口 %1：%2").arg(port).arg(m_server.errorString()));
        return;
    }
    const QStringList addrs = localIPv4Addresses();
    emit listenStarted(port, addrs);
    emit logMessage(tr("日志 TCP Server 已启动，端口 %1，等待板子连接...").arg(port),
                    QStringLiteral("INFO"));
    if (!addrs.isEmpty()) {
        emit logMessage(tr("请确认板子配置主机为 %1:%2（与 OTA 同网段）")
                            .arg(addrs.first())
                            .arg(port),
                        QStringLiteral("INFO"));
    }
}

void LogClient::stop()
{
    teardownClient();
    if (m_server.isListening())
        m_server.close();
    resetRx();
}

void LogClient::bindClient(QTcpSocket *sock)
{
    teardownClient();
    m_socket = sock;
    m_socket->setProxy(QNetworkProxy::NoProxy);
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &LogClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &LogClient::onSocketDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &LogClient::onSocketError);
    resetRx();
    emit logMessage(tr("板子已连接：%1").arg(m_socket->peerAddress().toString()),
                    QStringLiteral("INFO"));
    emit connected();
    queryStat();
}

void LogClient::teardownClient()
{
    if (m_socket == nullptr)
        return;
    QTcpSocket *sock = m_socket;
    m_socket = nullptr;
    sock->disconnect(this);
    sock->abort();
    sock->deleteLater();
}

void LogClient::ping()
{
    sendLine(QStringLiteral("PING"));
}

void LogClient::queryStat()
{
    sendLine(QStringLiteral("STAT"));
}

void LogClient::querySpace()
{
    sendLine(QStringLiteral("SPACE"));
}

void LogClient::listFiles()
{
    m_pendingList.clear();
    m_listExpect = -1;
    sendLine(QStringLiteral("LIST"));
}

void LogClient::downloadFile(const QString &name)
{
    if (name.trimmed().isEmpty()) {
        emit downloadFailed(tr("文件名为空"));
        return;
    }
    m_fileName.clear();
    m_fileSize = 0;
    m_fileBuf.clear();
    m_mode = Mode::WaitLine;
    sendLine(QStringLiteral("GET %1").arg(name.trimmed()));
}

void LogClient::sendLine(const QString &line)
{
    if (!isConnected()) {
        emit errorOccurred(tr("板子尚未连上日志服务"));
        return;
    }
    const QByteArray data = (line + QLatin1Char('\n')).toUtf8();
    m_socket->write(data);
    m_socket->flush();
    m_mode = Mode::WaitLine;
}

void LogClient::resetRx()
{
    m_rx.clear();
    m_mode = Mode::Idle;
    m_pendingList.clear();
    m_listExpect = -1;
    m_fileName.clear();
    m_fileSize = 0;
    m_fileBuf.clear();
}

void LogClient::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *sock = m_server.nextPendingConnection();
        if (sock == nullptr)
            continue;
        if (isConnected()) {
            emit logMessage(tr("已有连接，拒绝新连接 %1")
                                .arg(sock->peerAddress().toString()),
                            QStringLiteral("WARN"));
            sock->abort();
            sock->deleteLater();
            continue;
        }
        bindClient(sock);
    }
}

void LogClient::onSocketDisconnected()
{
    emit logMessage(tr("板子日志连接已断开"), QStringLiteral("WARN"));
    teardownClient();
    resetRx();
    emit disconnected();
}

void LogClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (m_socket != nullptr)
        emit errorOccurred(m_socket->errorString());
}

void LogClient::onReadyRead()
{
    if (m_socket == nullptr)
        return;
    m_rx.append(m_socket->readAll());

    while (true) {
        if (m_mode == Mode::RecvFile) {
            const qint64 need = static_cast<qint64>(m_fileSize) - m_fileBuf.size();
            if (need <= 0) {
                const QString name = m_fileName;
                const QByteArray data = m_fileBuf;
                resetRx();
                emit downloadFinished(name, data);
                continue;
            }
            if (m_rx.isEmpty())
                return;
            const QByteArray chunk = m_rx.left(static_cast<int>(qMin(need, static_cast<qint64>(m_rx.size()))));
            m_rx.remove(0, chunk.size());
            m_fileBuf.append(chunk);
            emit downloadProgress(m_fileBuf.size(), m_fileSize);
            if (static_cast<quint32>(m_fileBuf.size()) >= m_fileSize) {
                const QString name = m_fileName;
                const QByteArray data = m_fileBuf;
                resetRx();
                emit downloadFinished(name, data);
            }
            continue;
        }

        const int nl = m_rx.indexOf('\n');
        if (nl < 0)
            return;

        QByteArray raw = m_rx.left(nl);
        m_rx.remove(0, nl + 1);
        if (raw.endsWith('\r'))
            raw.chop(1);
        processLine(QString::fromUtf8(raw));
    }
}

void LogClient::processLine(const QString &line)
{
    if (line == QLatin1String("PONG")) {
        emit pongReceived();
        return;
    }

    if (line.startsWith(QLatin1String("STAT "))) {
        const bool mounted = line.contains(QLatin1String("mounted=1"));
        emit statReceived(mounted);
        return;
    }

    if (line.startsWith(QLatin1String("SPACE "))) {
        LogSpaceInfo sp;
        const QRegularExpression re(
            QStringLiteral(R"(card=(\d+)\s+total=(\d+)\s+free=(\d+))"));
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch()) {
            sp.cardBytes = m.captured(1).toULongLong();
            sp.totalBytes = m.captured(2).toULongLong();
            sp.freeBytes = m.captured(3).toULongLong();
            emit spaceReceived(sp);
        } else {
            emit errorOccurred(tr("SPACE 响应格式错误: %1").arg(line));
        }
        return;
    }

    if (line.startsWith(QLatin1String("LIST "))) {
        m_pendingList.clear();
        m_listExpect = line.mid(5).trimmed().toInt();
        if (m_listExpect <= 0) {
            m_listExpect = -1;
            emit listReceived({});
        }
        return;
    }

    if (line == QLatin1String("END")) {
        emit listReceived(m_pendingList);
        m_listExpect = -1;
        return;
    }

    if (line.startsWith(QLatin1String("FILE "))) {
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            emit downloadFailed(tr("FILE 响应格式错误"));
            resetRx();
            return;
        }
        m_fileName = parts[1];
        m_fileSize = parts[2].toUInt();
        m_fileBuf.clear();
        m_mode = Mode::RecvFile;
        emit downloadProgress(0, m_fileSize);
        if (m_fileSize == 0) {
            const QString name = m_fileName;
            resetRx();
            emit downloadFinished(name, {});
        }
        return;
    }

    if (line.startsWith(QLatin1String("ERR "))) {
        emit errorOccurred(line);
        if (m_mode == Mode::RecvFile || !m_fileName.isEmpty())
            emit downloadFailed(line);
        m_mode = Mode::Idle;
        return;
    }

    if (m_listExpect >= 0) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        LogFileInfo info;
        info.name = parts.value(0).trimmed();
        info.size = parts.value(1).trimmed().toUInt();
        if (!info.name.isEmpty())
            m_pendingList.push_back(info);
        return;
    }

    emit logMessage(tr("未识别响应: %1").arg(line), QStringLiteral("WARN"));
}
