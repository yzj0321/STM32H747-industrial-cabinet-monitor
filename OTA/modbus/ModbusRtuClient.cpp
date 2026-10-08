#include "ModbusRtuClient.h"

#include <QElapsedTimer>
#include <QThread>

ModbusRtuClient::ModbusRtuClient(QObject *parent)
    : QObject(parent)
{
}

ModbusRtuClient::~ModbusRtuClient()
{
    close();
}

bool ModbusRtuClient::open(const QString &portName, int baudRate)
{
    close();
    m_port.setPortName(portName);
    m_port.setBaudRate(baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        m_lastError = m_port.errorString();
        return false;
    }

    m_port.clear();
    m_lastError.clear();
    return true;
}

void ModbusRtuClient::close()
{
    if (m_port.isOpen())
        m_port.close();
}

bool ModbusRtuClient::isOpen() const
{
    return m_port.isOpen();
}

quint16 ModbusRtuClient::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (unsigned char b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001)
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            else
                crc = static_cast<quint16>(crc >> 1);
        }
    }
    return crc;
}

bool ModbusRtuClient::transact(const QByteArray &request, int expectedDataBytes, QByteArray *response)
{
    if (!m_port.isOpen()) {
        m_lastError = QStringLiteral("port closed");
        return false;
    }

    m_port.clear();
    if (m_port.write(request) != request.size() || !m_port.waitForBytesWritten(m_timeoutMs)) {
        m_lastError = QStringLiteral("write failed: %1").arg(m_port.errorString());
        return false;
    }

    // Minimal RTU response: addr + func + ... + CRC(2)
    const int minLen = 5;
    const int wantLen = (expectedDataBytes >= 0) ? (3 + expectedDataBytes + 2) : -1;

    QByteArray buf;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < m_timeoutMs) {
        if (m_port.waitForReadyRead(20))
            buf.append(m_port.readAll());
        else if (!buf.isEmpty()) {
            // inter-frame silence roughly done
            QThread::msleep(5);
            if (m_port.bytesAvailable() > 0)
                buf.append(m_port.readAll());
            break;
        }

        if (wantLen > 0 && buf.size() >= wantLen)
            break;
        if (buf.size() >= 5 && static_cast<quint8>(buf.at(1)) & 0x80)
            break; // exception
    }

    if (buf.size() < minLen) {
        m_lastError = QStringLiteral("timeout / short frame (%1 bytes)").arg(buf.size());
        return false;
    }

    const quint16 gotCrc = static_cast<quint8>(buf.at(buf.size() - 2))
                         | (static_cast<quint16>(static_cast<quint8>(buf.at(buf.size() - 1))) << 8);
    const quint16 calcCrc = crc16(buf.left(buf.size() - 2));
    if (gotCrc != calcCrc) {
        m_lastError = QStringLiteral("CRC error");
        return false;
    }

    if (static_cast<quint8>(buf.at(0)) != static_cast<quint8>(request.at(0))) {
        m_lastError = QStringLiteral("slave mismatch");
        return false;
    }

    if (static_cast<quint8>(buf.at(1)) & 0x80) {
        m_lastError = QStringLiteral("exception 0x%1")
                          .arg(static_cast<quint8>(buf.at(2)), 2, 16, QLatin1Char('0'));
        return false;
    }

    *response = buf;
    m_lastError.clear();
    return true;
}

bool ModbusRtuClient::readHoldingRegisters(quint8 slave, quint16 address, quint16 count, QVector<quint16> *out)
{
    QByteArray req;
    req.append(char(slave));
    req.append(char(0x03));
    req.append(char((address >> 8) & 0xFF));
    req.append(char(address & 0xFF));
    req.append(char((count >> 8) & 0xFF));
    req.append(char(count & 0xFF));
    const quint16 crc = crc16(req);
    req.append(char(crc & 0xFF));
    req.append(char((crc >> 8) & 0xFF));

    QByteArray resp;
    if (!transact(req, 1 + count * 2, &resp))
        return false;

    if (resp.size() < 5 + count * 2)
        return false;

    const int byteCount = static_cast<quint8>(resp.at(2));
    if (byteCount != count * 2) {
        m_lastError = QStringLiteral("unexpected byte count");
        return false;
    }

    out->resize(count);
    for (int i = 0; i < count; ++i) {
        const int off = 3 + i * 2;
        (*out)[i] = static_cast<quint16>((static_cast<quint8>(resp.at(off)) << 8)
                                         | static_cast<quint8>(resp.at(off + 1)));
    }
    return true;
}

bool ModbusRtuClient::readCoils(quint8 slave, quint16 address, quint16 count, QVector<bool> *out)
{
    QByteArray req;
    req.append(char(slave));
    req.append(char(0x01));
    req.append(char((address >> 8) & 0xFF));
    req.append(char(address & 0xFF));
    req.append(char((count >> 8) & 0xFF));
    req.append(char(count & 0xFF));
    const quint16 crc = crc16(req);
    req.append(char(crc & 0xFF));
    req.append(char((crc >> 8) & 0xFF));

    const int byteCount = (count + 7) / 8;
    QByteArray resp;
    if (!transact(req, 1 + byteCount, &resp))
        return false;

    if (resp.size() < 5 + byteCount)
        return false;

    out->resize(count);
    for (int i = 0; i < count; ++i) {
        const int bi = i / 8;
        const int bit = i % 8;
        (*out)[i] = (static_cast<quint8>(resp.at(3 + bi)) >> bit) & 0x01;
    }
    return true;
}

bool ModbusRtuClient::writeSingleCoil(quint8 slave, quint16 address, bool on)
{
    QByteArray req;
    req.append(char(slave));
    req.append(char(0x05));
    req.append(char((address >> 8) & 0xFF));
    req.append(char(address & 0xFF));
    req.append(char(on ? 0xFF : 0x00));
    req.append(char(0x00));
    const quint16 crc = crc16(req);
    req.append(char(crc & 0xFF));
    req.append(char((crc >> 8) & 0xFF));

    QByteArray resp;
    return transact(req, 4, &resp);
}
