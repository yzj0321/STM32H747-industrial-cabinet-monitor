/**
 * @file LogPage.h
 * @brief 查看日志页：上位机监听，板子连上后拉取 SDNAND 异常日志
 */
#ifndef LOGPAGE_H
#define LOGPAGE_H

#include "LogClient.h"

#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

class LogPage : public QWidget
{
    Q_OBJECT
public:
    explicit LogPage(QWidget *parent = nullptr);

private slots:
    void onListenClicked();
    void onStopClicked();
    void onRefreshClicked();
    void onSpaceClicked();
    void onDownloadClicked();
    void onSaveClicked();
    void onListenStarted(quint16 port, const QStringList &addrs);
    void onClientConnected();
    void onClientDisconnected();
    void onStat(bool mounted);
    void onSpace(const LogSpaceInfo &space);
    void onList(const QVector<LogFileInfo> &files);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString &name, const QByteArray &data);
    void onDownloadFailed(const QString &message);
    void appendStatus(const QString &text, const QString &level = QStringLiteral("INFO"));

private:
    void buildUi();
    void setListeningUi(bool listening);
    void setDeviceLinkedUi(bool linked);
    void showCsvOrText(const QByteArray &data);
    QString selectedFileName() const;
    static QString formatBytes(quint64 bytes);

    LogClient m_client;
    QSpinBox *m_portSpin = nullptr;
    QPushButton *m_btnListen = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnRefresh = nullptr;
    QPushButton *m_btnSpace = nullptr;
    QPushButton *m_btnDownload = nullptr;
    QPushButton *m_btnSave = nullptr;
    QLabel *m_connLabel = nullptr;
    QLabel *m_mountLabel = nullptr;
    QLabel *m_spaceLabel = nullptr;
    QLabel *m_ipHint = nullptr;
    QListWidget *m_fileList = nullptr;
    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QProgressBar *m_progress = nullptr;
    QPlainTextEdit *m_statusLog = nullptr;

    QByteArray m_lastData;
    QString m_lastName;
};

#endif // LOGPAGE_H
