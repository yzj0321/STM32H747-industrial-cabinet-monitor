#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "OtaTransport.h"

#include <QHash>
#include <QSerialPort>
#include <QThread>
#include <optional>

class SerialManager : public OtaTransport
{
    Q_OBJECT
public:
    explicit SerialManager(QObject *parent = nullptr);

    QStringList availablePorts() const;
    bool openPort(const QString &portName, int baudRate);
    void closePort() { close(); }
    void moveInternalObjectsToThread(QThread *thread);

    bool isOpen() const override;
    QString connectionLabel() const override;
    QString portName() const;

    void setResponseTimeoutMs(int ms) override;
    int responseTimeoutMs() const override { return m_responseTimeoutMs; }

    bool sendCommand(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 &outSeq) override;
    bool sendCommandAtSeq(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 seq) override;
    std::optional<OtaProtocol::Frame> waitResponse(OtaProtocol::Cmd expectedCmd, quint16 seq,
                                                     int timeoutMs = -1) override;
    bool transact(OtaProtocol::Cmd cmd, const QByteArray &payload, OtaProtocol::Frame &response,
                   int timeoutMs = -1) override;
    std::optional<OtaProtocol::Frame> takeDeferredResponse(OtaProtocol::Cmd expectedCmd,
                                                           quint16 seq) override;
    void pumpReceive(int maxWaitMs = 0) override;
    void clearDeferredResponses() override;
    void close() override;

private slots:
    void onReadyRead();

private:
    void ingestParsedFrames();
    void storeDeferredFrame(const OtaProtocol::Frame &frame);
    void appendPortRx();
    QSerialPort m_port;
    QByteArray m_rxBuffer;
    quint16 m_seqCounter = 1;
    int m_responseTimeoutMs = 30000;
    QHash<quint16, OtaProtocol::Frame> m_deferred;
};

#endif // SERIALMANAGER_H
