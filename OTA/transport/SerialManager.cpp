#include "SerialManager.h"

#include <QElapsedTimer>
#include <QIODevice>
#include <QSerialPortInfo>

SerialManager::SerialManager(QObject *parent)
    : OtaTransport(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
}

void SerialManager::moveInternalObjectsToThread(QThread *thread)
{
    if (!thread)
        return;

    if (m_port.thread() != thread)
        m_port.moveToThread(thread);
}

QStringList SerialManager::availablePorts() const
{
    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        names.append(info.portName());
    names.sort();
    return names;
}

bool SerialManager::openPort(const QString &portName, int baudRate)
{
    if (m_port.isOpen())
        close();

    m_port.setPortName(portName);
    m_port.setBaudRate(baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        emit communicationError(tr("Cannot open %1: %2").arg(portName, m_port.errorString()));
        return false;
    }

    m_rxBuffer.clear();
    m_seqCounter = 1;
    emit opened(connectionLabel());
    emit logMessage(tr("Opened %1 @ %2 baud").arg(portName).arg(baudRate), QStringLiteral("INFO"));
    return true;
}

void SerialManager::close()
{
    if (!m_port.isOpen())
        return;
    const QString name = m_port.portName();
    m_port.close();
    m_rxBuffer.clear();
    m_deferred.clear();
    emit closed();
    emit logMessage(tr("Closed %1").arg(name), QStringLiteral("INFO"));
}

bool SerialManager::isOpen() const
{
    return m_port.isOpen();
}

QString SerialManager::connectionLabel() const
{
    return m_port.portName();
}

QString SerialManager::portName() const
{
    return m_port.portName();
}

void SerialManager::setResponseTimeoutMs(int ms)
{
    m_responseTimeoutMs = qMax(500, ms);
}

bool SerialManager::sendCommandAtSeq(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 seq)
{
    if (!m_port.isOpen()) {
        emit communicationError(tr("Serial port not open"));
        return false;
    }

    const QByteArray frame = OtaProtocol::buildFrame(cmd, seq, payload);
    const qint64 written = m_port.write(frame);
    if (written != frame.size()) {
        emit communicationError(tr("Write failed: %1").arg(m_port.errorString()));
        return false;
    }
    if (!m_port.waitForBytesWritten(m_responseTimeoutMs)) {
        emit communicationError(tr("Write timeout"));
        return false;
    }
    return true;
}

bool SerialManager::sendCommand(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 &outSeq)
{
    outSeq = m_seqCounter++;
    return sendCommandAtSeq(cmd, payload, outSeq);
}

void SerialManager::clearDeferredResponses()
{
    m_deferred.clear();
}

void SerialManager::storeDeferredFrame(const OtaProtocol::Frame &frame)
{
    m_deferred.insert(frame.seq, frame);
}

void SerialManager::ingestParsedFrames()
{
    while (true) {
        auto frame = OtaProtocol::tryParseFrame(m_rxBuffer);
        if (!frame)
            break;
        storeDeferredFrame(*frame);
    }
}

void SerialManager::appendPortRx()
{
    const QByteArray data = m_port.readAll();
    if (data.isEmpty())
        return;
    emit rawDataReceived(data);
    m_rxBuffer.append(data);
}

void SerialManager::pumpReceive(int maxWaitMs)
{
    if (!m_port.isOpen())
        return;

    if (maxWaitMs > 0) {
        if (m_port.waitForReadyRead(maxWaitMs))
            appendPortRx();
    }
    if (m_port.bytesAvailable() > 0)
        appendPortRx();
    ingestParsedFrames();
}

std::optional<OtaProtocol::Frame> SerialManager::takeDeferredResponse(
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

std::optional<OtaProtocol::Frame> SerialManager::waitResponse(OtaProtocol::Cmd expectedCmd,
                                                              quint16 seq, int timeoutMs)
{
    if (auto cached = takeDeferredResponse(expectedCmd, seq))
        return cached;

    if (timeoutMs < 0)
        timeoutMs = m_responseTimeoutMs;

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        const int remain = timeoutMs - static_cast<int>(timer.elapsed());
        pumpReceive(qMin(50, qMax(1, remain)));

        if (auto cached = takeDeferredResponse(expectedCmd, seq))
            return cached;
    }
    return std::nullopt;
}

bool SerialManager::transact(OtaProtocol::Cmd cmd, const QByteArray &payload,
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

void SerialManager::onReadyRead()
{
    appendPortRx();
    ingestParsedFrames();
}
