/**
 * @file LogClient.h
 * @brief 上位机日志 TCP Server（默认监听 :5001），板子主动连上来后拉取 SD 日志
 *
 * 与 OTA 同模型：PC 监听，MCU 连接。协议文本行：PING / STAT / SPACE / LIST / GET <name>
 */
#ifndef LOGCLIENT_H
#define LOGCLIENT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVector>

struct LogFileInfo {
    QString name;
    quint32 size = 0;
};

struct LogSpaceInfo {
    quint64 cardBytes = 0;   // SDNAND 芯片容量
    quint64 totalBytes = 0;  // 文件系统总容量
    quint64 freeBytes = 0;   // 剩余
};

class LogClient : public QObject
{
    Q_OBJECT
public:
    explicit LogClient(QObject *parent = nullptr);

    bool isListening() const;
    bool isConnected() const;
    /** 启动 TCP Server，等待板子连接 */
    void startListen(quint16 port);
    void stop();

    void ping();
    void queryStat();
    void querySpace();
    void listFiles();
    void downloadFile(const QString &name);

    static QStringList localIPv4Addresses();

signals:
    void listenStarted(quint16 port, const QStringList &localAddresses);
    void connected();
    void disconnected();
    void logMessage(const QString &text, const QString &level);
    void errorOccurred(const QString &message);
    void pongReceived();
    void statReceived(bool mounted);
    void spaceReceived(const LogSpaceInfo &space);
    void listReceived(const QVector<LogFileInfo> &files);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &name, const QByteArray &data);
    void downloadFailed(const QString &message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    enum class Mode {
        Idle,
        WaitLine,
        RecvFile
    };

    void bindClient(QTcpSocket *sock);
    void teardownClient();
    void sendLine(const QString &line);
    void processLine(const QString &line);
    void resetRx();

    QTcpServer m_server;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_rx;
    Mode m_mode = Mode::Idle;
    QVector<LogFileInfo> m_pendingList;
    int m_listExpect = -1;
    QString m_fileName;
    quint32 m_fileSize = 0;
    QByteArray m_fileBuf;
};

#endif // LOGCLIENT_H
