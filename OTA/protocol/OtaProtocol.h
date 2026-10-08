#ifndef OTAPROTOCOL_H
#define OTAPROTOCOL_H

#include <QByteArray>
#include <QObject>
#include <optional>

/**
 * SOIP v2 — STM32 OTA/IAP Serial Protocol
 * Frame: SOF(AA55) VER CMD SEQ LEN PAYLOAD CRC16
 * CRC16-CCITT over [VER .. end of PAYLOAD]
 */
class OtaProtocol
{
public:
    static constexpr quint8 kVersion = 0x02;
    static constexpr quint8 kSof0 = 0xAA;
    static constexpr quint8 kSof1 = 0x55;
    static constexpr int kHeaderSize = 8;
    static constexpr int kCrcSize = 2;
    /** 与固件 BSP_OTA_CHUNK_SIZE 对齐 */
    static constexpr int kDefaultChunkSize = 512;
    static constexpr int kMaxPayload = 1024;
    static constexpr int kMaxRetries = 5;
    /** 停等窗口=1 */
    static constexpr int kDataPipelineWindow = 1;
    /** ACK 后短间隔（常见 TCP OTA CHUNK_DELAY） */
    static constexpr int kDataInterPacketDelayMs = 2;
    /** 单包 ACK 超时；板端 SENDOK 正常应 <100ms */
    static constexpr int kDataAckTimeoutMs = 3000;
    /** 超时后同序重发前间隔 */
    static constexpr int kDataRetryDelayMs = 20;
    static constexpr int kStartTransferTimeoutMs = 60000;

    /** CM7 A/B 槽基址（与 boot_slot.h / MCU GET_INFO.app_address 对齐） */
    static constexpr quint32 kSlotABase = 0x08020000U;
    static constexpr quint32 kSlotBBase = 0x080A0000U;
    static constexpr quint32 kSlotSize = 384U * 1024U;
    /** CM4 A/B 槽基址（Bank2） */
    static constexpr quint32 kCm4SlotABase = 0x08120000U;
    static constexpr quint32 kCm4SlotBBase = 0x081A0000U;
    static constexpr quint32 kCm4SlotSize = 384U * 1024U;

    enum class Cmd : quint8 {
        Ping = 0x01,
        GetInfo = 0x02,
        BeginSession = 0x03,
        Erase = 0x04,
        StartTransfer = 0x05,
        Data = 0x06,
        EndTransfer = 0x07,
        Verify = 0x08,
        Reset = 0x09,
        Abort = 0x0A,
        ResponseMask = 0x80
    };

    enum class Status : quint8 {
        Ok = 0x00,
        ErrCrc = 0x01,
        ErrCmd = 0x02,
        ErrState = 0x03,
        ErrFlash = 0x04,
        ErrVerify = 0x05,
        ErrTimeout = 0x06,
        ErrBusy = 0x07,
        ErrParam = 0x08,
        ErrSession = 0x09,
        ErrAborted = 0x0A,
        ErrUnknown = 0xFF
    };

    struct Frame {
        Cmd cmd = Cmd::Ping;
        quint16 seq = 0;
        QByteArray payload;
        bool valid = false;
    };

    struct DeviceInfo {
        QString deviceType;
        quint32 chipId = 0;
        QString bootVersion;
        QString protocolVersion;
        quint32 flashSize = 0;
        /**
         * GET_INFO.app_address = inactive 槽基址（擦写目标）
         * 运行 A → 0x080A0000；运行 B → 0x08020000。不是固定 Slot A/B。
         */
        quint32 appAddress = 0;
        quint8 runningSlot = 0xFF;
        quint8 inactiveSlot = 0xFF;
        quint8 activeSlot = 0xFF;
        quint8 pendingSlot = 0xFF;
        quint8 flags = 0;
        quint8 bootTrial = 0;
        bool valid = false;

        QString runningSlotName() const
        {
            return runningSlot == 0 ? QStringLiteral("A") : QStringLiteral("B");
        }
        QString inactiveSlotName() const
        {
            return inactiveSlot == 0 ? QStringLiteral("A") : QStringLiteral("B");
        }
        QString suggestedFirmware(quint8 otaCore = 0) const
        {
            if (otaCore == 1U) {
                return inactiveSlot == 0 ? QStringLiteral("GPIO_CM4_A.bin / cm4_a.bin")
                                       : QStringLiteral("GPIO_CM4_B.bin / cm4_b.bin");
            }
            return inactiveSlot == 0 ? QStringLiteral("GPIO_CM7_A.bin / slot_a.bin")
                                     : QStringLiteral("GPIO_CM7_B.bin / slot_b.bin");
        }

        static bool isValidSlotBase(quint32 addr)
        {
            return addr == kSlotABase || addr == kSlotBBase;
        }
        static bool isValidCm4SlotBase(quint32 addr)
        {
            return addr == kCm4SlotABase || addr == kCm4SlotBBase;
        }
    };

    static QByteArray buildFrame(Cmd cmd, quint16 seq, const QByteArray &payload = {});
    static std::optional<Frame> tryParseFrame(QByteArray &rxBuffer);
    static bool verifyFrameCrc(const QByteArray &frameRaw);
    static QString statusToString(Status s);
    static QString cmdToString(Cmd c);

    static QByteArray payloadPing();
    /** GET_INFO 载荷 byte0：0=CM7，1=CM4 */
    static QByteArray payloadGetInfo(quint8 core = 0);
    static QByteArray payloadBeginSession(quint32 hostNonce);
    static QByteArray payloadStartTransfer(quint32 sessionToken, quint32 flashAddr,
                                           quint32 imageSize, quint32 imageCrc32);
    static QByteArray payloadData(quint32 sessionToken, quint32 offset, const QByteArray &chunk);
    static QByteArray payloadErase(quint32 sessionToken, quint32 flashAddr, quint32 size);
    static QByteArray payloadEndTransfer(quint32 sessionToken);
    static QByteArray payloadVerify(quint32 sessionToken, quint32 imageCrc32);
    static QByteArray payloadAbort(quint32 sessionToken);

    static Status parseResponseStatus(const Frame &frame);
    static DeviceInfo parseDeviceInfo(const Frame &frame);
    static quint32 parseSessionToken(const Frame &frame);
};

#endif // OTAPROTOCOL_H
