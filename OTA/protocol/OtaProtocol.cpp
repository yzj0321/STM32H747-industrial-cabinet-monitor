#include "OtaProtocol.h"
#include "CrcUtils.h"

#include <QDataStream>
#include <QIODevice>
QByteArray OtaProtocol::buildFrame(Cmd cmd, quint16 seq, const QByteArray &payload)
{
    QByteArray frame;
    frame.reserve(kHeaderSize + payload.size() + kCrcSize);
    frame.append(char(kSof0));
    frame.append(char(kSof1));
    frame.append(char(kVersion));
    frame.append(char(static_cast<quint8>(cmd)));
    frame.append(char(seq & 0xFF));
    frame.append(char((seq >> 8) & 0xFF));
    const quint16 len = static_cast<quint16>(payload.size());
    frame.append(char(len & 0xFF));
    frame.append(char((len >> 8) & 0xFF));
    frame.append(payload);

    const quint16 crc = CrcUtils::crc16Ccitt(
        reinterpret_cast<const uint8_t *>(frame.constData() + 2),
        frame.size() - 2);
    frame.append(char(crc & 0xFF));
    frame.append(char((crc >> 8) & 0xFF));
    return frame;
}

std::optional<OtaProtocol::Frame> OtaProtocol::tryParseFrame(QByteArray &rxBuffer)
{
    while (rxBuffer.size() >= kHeaderSize + kCrcSize) {
        int start = -1;
        for (int i = 0; i + 1 < rxBuffer.size(); ++i) {
            if (static_cast<quint8>(rxBuffer[i]) == kSof0
                && static_cast<quint8>(rxBuffer[i + 1]) == kSof1) {
                start = i;
                break;
            }
        }
        if (start < 0) {
            rxBuffer.clear();
            return std::nullopt;
        }
        if (start > 0)
            rxBuffer.remove(0, start);

        if (rxBuffer.size() < kHeaderSize)
            return std::nullopt;

        const quint16 payloadLen =
            static_cast<quint8>(rxBuffer[6]) | (static_cast<quint8>(rxBuffer[7]) << 8);
        if (payloadLen > kMaxPayload) {
            rxBuffer.remove(0, 2);
            continue;
        }

        const int totalLen = kHeaderSize + payloadLen + kCrcSize;
        if (rxBuffer.size() < totalLen)
            return std::nullopt;

        const QByteArray raw = rxBuffer.left(totalLen);
        if (!verifyFrameCrc(raw)) {
            /* 可能是脏字节/粘包错位，跳过当前 SOF 继续找下一帧 */
            rxBuffer.remove(0, 2);
            continue;
        }

        Frame f;
        f.cmd = static_cast<Cmd>(static_cast<quint8>(raw[3]));
        f.seq = static_cast<quint8>(raw[4]) | (static_cast<quint8>(raw[5]) << 8);
        f.payload = raw.mid(kHeaderSize, payloadLen);
        f.valid = true;
        rxBuffer.remove(0, totalLen);
        return f;
    }
    return std::nullopt;
}

bool OtaProtocol::verifyFrameCrc(const QByteArray &frameRaw)
{
    if (frameRaw.size() < kHeaderSize + kCrcSize)
        return false;
    const int bodyLen = frameRaw.size() - kCrcSize;
    const quint16 expected = CrcUtils::crc16Ccitt(
        reinterpret_cast<const uint8_t *>(frameRaw.constData() + 2),
        bodyLen - 2);
    const quint16 got = static_cast<quint8>(frameRaw[bodyLen])
        | (static_cast<quint8>(frameRaw[bodyLen + 1]) << 8);
    return expected == got;
}

QString OtaProtocol::statusToString(Status s)
{
    switch (s) {
    case Status::Ok: return QStringLiteral("OK");
    case Status::ErrCrc: return QStringLiteral("CRC error");
    case Status::ErrCmd: return QStringLiteral("Unknown command");
    case Status::ErrState: return QStringLiteral("Invalid state");
    case Status::ErrFlash: return QStringLiteral("Flash operation failed");
    case Status::ErrVerify: return QStringLiteral("Verification failed");
    case Status::ErrTimeout: return QStringLiteral("Device timeout");
    case Status::ErrBusy: return QStringLiteral("Device busy");
    case Status::ErrParam: return QStringLiteral("Invalid parameter");
    case Status::ErrSession: return QStringLiteral("Session invalid");
    case Status::ErrAborted: return QStringLiteral("Aborted");
    default: return QStringLiteral("Unknown error");
    }
}

QString OtaProtocol::cmdToString(Cmd c)
{
    const quint8 v = static_cast<quint8>(c) & ~static_cast<quint8>(Cmd::ResponseMask);
    switch (static_cast<Cmd>(v)) {
    case Cmd::Ping: return QStringLiteral("PING");
    case Cmd::GetInfo: return QStringLiteral("GET_INFO");
    case Cmd::BeginSession: return QStringLiteral("BEGIN_SESSION");
    case Cmd::Erase: return QStringLiteral("ERASE");
    case Cmd::StartTransfer: return QStringLiteral("START_TRANSFER");
    case Cmd::Data: return QStringLiteral("DATA");
    case Cmd::EndTransfer: return QStringLiteral("END_TRANSFER");
    case Cmd::Verify: return QStringLiteral("VERIFY");
    case Cmd::Reset: return QStringLiteral("RESET");
    case Cmd::Abort: return QStringLiteral("ABORT");
    default: return QStringLiteral("CMD_%1").arg(v, 2, 16, QChar('0'));
    }
}

QByteArray OtaProtocol::payloadPing()
{
    return QByteArray(4, '\0');
}

QByteArray OtaProtocol::payloadGetInfo(quint8 core)
{
    QByteArray p;
    p.append(char(core & 0xFF));
    return p;
}

QByteArray OtaProtocol::payloadBeginSession(quint32 hostNonce)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << hostNonce;
    return p;
}

QByteArray OtaProtocol::payloadStartTransfer(quint32 sessionToken, quint32 flashAddr,
                                               quint32 imageSize, quint32 imageCrc32)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << sessionToken << flashAddr << imageSize << imageCrc32;
    return p;
}

QByteArray OtaProtocol::payloadData(quint32 sessionToken, quint32 offset, const QByteArray &chunk)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << sessionToken << offset;
    p.append(chunk);
    return p;
}

QByteArray OtaProtocol::payloadErase(quint32 sessionToken, quint32 flashAddr, quint32 size)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << sessionToken << flashAddr << size;
    return p;
}

QByteArray OtaProtocol::payloadEndTransfer(quint32 sessionToken)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << sessionToken;
    return p;
}

QByteArray OtaProtocol::payloadVerify(quint32 sessionToken, quint32 imageCrc32)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << sessionToken << imageCrc32;
    return p;
}

QByteArray OtaProtocol::payloadAbort(quint32 sessionToken)
{
    QByteArray p;
    QDataStream ds(&p, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << sessionToken;
    return p;
}

OtaProtocol::Status OtaProtocol::parseResponseStatus(const Frame &frame)
{
    if (!frame.valid || frame.payload.isEmpty())
        return Status::ErrUnknown;
    return static_cast<Status>(static_cast<quint8>(frame.payload[0]));
}

OtaProtocol::DeviceInfo OtaProtocol::parseDeviceInfo(const Frame &frame)
{
    DeviceInfo info;
    if (frame.payload.size() < 25)
        return info;

    QDataStream ds(frame.payload);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint8 status = 0;
    ds >> status;
    if (static_cast<Status>(status) != Status::Ok)
        return info;

    quint8 nameLen = 0;
    ds >> nameLen;
    if (nameLen > 32)
        return info;
    QByteArray name = frame.payload.mid(ds.device()->pos(), nameLen);
    ds.skipRawData(nameLen);
    info.deviceType = QString::fromLatin1(name);

    quint8 bootLen = 0;
    ds >> info.chipId >> bootLen;
    if (bootLen > 16)
        return info;
    QByteArray boot = frame.payload.mid(ds.device()->pos(), bootLen);
    ds.skipRawData(bootLen);
    info.bootVersion = QString::fromLatin1(boot);

    quint8 protoLen = 0;
    ds >> info.flashSize >> info.appAddress >> protoLen;
    if (protoLen > 16)
        return info;
    info.protocolVersion = QString::fromLatin1(
        frame.payload.mid(ds.device()->pos(), protoLen));
    ds.skipRawData(protoLen);

    if (frame.payload.size() >= static_cast<int>(ds.device()->pos() + 6)) {
        info.runningSlot = static_cast<quint8>(frame.payload.at(ds.device()->pos()));
        info.inactiveSlot = static_cast<quint8>(frame.payload.at(ds.device()->pos() + 1));
        info.activeSlot = static_cast<quint8>(frame.payload.at(ds.device()->pos() + 2));
        info.pendingSlot = static_cast<quint8>(frame.payload.at(ds.device()->pos() + 3));
        info.flags = static_cast<quint8>(frame.payload.at(ds.device()->pos() + 4));
        info.bootTrial = static_cast<quint8>(frame.payload.at(ds.device()->pos() + 5));
    }

    info.valid = true;
    return info;
}

quint32 OtaProtocol::parseSessionToken(const Frame &frame)
{
    if (frame.payload.size() < 5)
        return 0;
    quint32 token = 0;
    QDataStream ds(frame.payload);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint8 status = 0;
    ds >> status >> token;
    Q_UNUSED(status);
    return token;
}
