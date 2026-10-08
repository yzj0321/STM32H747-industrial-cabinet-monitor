#ifndef UPDATEENGINE_H
#define UPDATEENGINE_H

#include "OtaProtocol.h"
#include "OtaTransport.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>

class UpdateEngine : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Connected,
        Erasing,
        Transferring,
        Verifying,
        Completed,
        Failed,
        Aborted
    };
    Q_ENUM(State)

    explicit UpdateEngine(OtaTransport *transport, QObject *parent = nullptr);

    void setTransport(OtaTransport *transport);
    OtaTransport *transport() const { return m_transport; }

    void setFirmwarePath(const QString &path);
    void setFlashAddress(quint32 addr);
    void setChunkSize(int size);
    void setMaxRetries(int retries);
    void setDebugEraseSize(quint32 bytes);
    void setDebugPreSendDelayMs(int ms);
    void setDebugSingleShot(bool singleShot);
    /** 0=CM7 OTA，1=CM4 OTA（GET_INFO 载荷 byte0） */
    void setOtaCore(quint8 core);
    quint8 otaCore() const { return m_otaCore; }

    QString firmwarePath() const { return m_firmwarePath; }
    quint32 debugEraseSize() const { return m_debugEraseSize; }
    quint32 flashAddress() const { return m_flashAddress; }
    State state() const { return m_state; }
    quint32 sessionToken() const { return m_sessionToken; }
    OtaProtocol::DeviceInfo deviceInfo() const { return m_deviceInfo; }

    bool loadFirmware();
    quint32 firmwareCrc32() const { return m_imageCrc32; }
    qint64 firmwareSize() const { return m_firmwareSize; }
    bool transferReady() const { return m_transferReady; }
    bool dataTransferDone() const { return m_dataTransferDone; }

public slots:
    void refreshDeviceInfo();
    void startGetInfoStressTest(int count, int intervalMs);
    void startBeginSessionTest(bool forceNewSession = true);
    void startEraseOnlyTest();
    void startStartTransferTest();
    void clearSession();
    /** One-click OTA: GET_INFO → BEGIN → ERASE → START_TRANSFER → DATA → END → VERIFY */
    void startUpgrade();
    void startFinishVerifyTest();
    void stopUpgrade();
    void resetDevice();

signals:
    void operationFinished();
    void sessionTokenChanged(quint32 token);
    void transferReadyChanged(bool ready);
    void dataTransferDoneChanged(bool done);
    void stateChanged(UpdateEngine::State state);
    void progressChanged(double percent, qint64 sent, qint64 total, double speedBps, int etaSec);
    void logMessage(const QString &line, const QString &level);
    void deviceInfoUpdated(const OtaProtocol::DeviceInfo &info);
    void upgradeFinished(bool success, const QString &message);
    void warningRaised(const QString &message);

private:
    bool ensureSession();
    bool sendWithRetry(OtaProtocol::Cmd cmd, const QByteArray &payload, OtaProtocol::Frame &resp,
                       int timeoutMs = -1);
    bool sendOnce(OtaProtocol::Cmd cmd, const QByteArray &payload, OtaProtocol::Frame &resp,
                  int timeoutMs = -1, bool applyPreDelay = true);
    void setState(State s);
    void fail(const QString &reason);
    void abortUpgrade();
    void tickProgress(qint64 deltaBytes);
    void resetTransferState();
    bool doGetInfo();
    bool doBeginSession();
    bool doErase();
    bool doStartTransfer();
    bool doTransferDataOnly();
    bool doFinishVerify();
    bool transferDataPipelined();
    bool stoppedOrDisconnected(const QString &stage);
    /** 用 GET_INFO 的 inactive 基址覆盖 m_flashAddress；失败返回 false */
    bool applyInactiveTargetFromDevice(QString *errOut = nullptr);
    void warnFirmwareSlotMismatch();

    OtaTransport *m_transport = nullptr;
    QString m_firmwarePath;
    /** 仅允许由 GET_INFO（inactive 槽）更新，禁止手填固定 B */
    quint32 m_flashAddress = OtaProtocol::kSlotABase;
    int m_chunkSize = OtaProtocol::kDefaultChunkSize;
    int m_maxRetries = OtaProtocol::kMaxRetries;

    State m_state = State::Idle;
    QByteArray m_firmware;
    qint64 m_firmwareSize = 0;
    quint32 m_imageCrc32 = 0;
    quint32 m_sessionToken = 0;
    quint32 m_hostNonce = 0;
    bool m_transferReady = false;
    bool m_dataTransferDone = false;
    quint32 m_debugEraseSize = 65536;
    int m_debugPreSendDelayMs = 0;
    bool m_debugSingleShot = true;
    quint8 m_otaCore = 0;

    OtaProtocol::DeviceInfo m_deviceInfo;
    QAtomicInt m_stopRequested = 0;
    QAtomicInt m_busy = 0;

    qint64 m_bytesSent = 0;
    QElapsedTimer m_speedTimer;
    qint64 m_speedWindowBytes = 0;
    double m_lastSpeed = 0.0;
};

#endif // UPDATEENGINE_H
