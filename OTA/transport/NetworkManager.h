#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "OtaTransport.h"

#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

class NetworkManager : public OtaTransport
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager() override;

    /** Start TCP Server and wait for MCU client; result via connectFinished(). */
    void startListen(quint16 port);
    void disconnectHost() { close(); }
    void moveInternalObjectsToThread(QThread *thread);

    bool isListening() const;
    bool isOpen() const override;
    QString connectionLabel() const override;
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

    quint16 listenPort() const { return m_listenPort; }
    static QStringList localIPv4Addresses();

signals:
    void connectFinished(bool success, const QString &message);
    void listenStarted(quint16 port, const QStringList &localAddresses);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    void finishListenAttempt(bool success, const QString &message);
    void teardownClient();
    void ingestParsedFrames();
    void storeDeferredFrame(const OtaProtocol::Frame &frame);
    void appendSocketRx(bool logPayload);

    QTcpServer m_server;
    QTcpSocket *m_client = nullptr;
    QByteArray m_rxBuffer;
    quint16 m_seqCounter = 1;
    int m_responseTimeoutMs = 30000;
    quint16 m_listenPort = 0;
    QString m_peerLabel;
    bool m_listenPending = false;
    QHash<quint16, OtaProtocol::Frame> m_deferred;
};

#endif // NETWORKMANAGER_H
