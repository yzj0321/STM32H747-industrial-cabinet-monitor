#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "UpdateEngine.h"
#include "NetworkManager.h"
#include "SerialManager.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>

class DashboardPage;
class LogPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshSerialPorts();
    void onProtocolChanged(int index);
    void onConnectClicked();
    void onTcpConnectFinished(bool success, const QString &message);
    void onTcpListenStarted(quint16 port, const QStringList &localAddresses);
    void onTcpClientDisconnected();
    void onDisconnectClicked();
    void onReadInfoClicked();
    void onBrowseFirmware();
    void onStartUpgrade();
    void appendLog(const QString &text, const QString &level);
    void onProgress(double percent, qint64 sent, qint64 total, double speedBps, int etaSec);
    void onUpgradeFinished(bool success, const QString &message);
    void onDeviceInfo(const OtaProtocol::DeviceInfo &info);
    void onEngineStateChanged(UpdateEngine::State state);
    void onSessionTokenChanged(quint32 token);
    void updateStatusBar();

private:
    void buildUi();
    QWidget *buildOtaPage();
    void applyStyleSheet();
    void wireSignals();
    void updateConnectionFields();
    bool isTcpMode() const;
    OtaTransport *activeTransport() const;
    QString formatSize(qint64 bytes) const;
    QString formatDuration(int seconds) const;
    void saveNetworkSettings() const;
    void loadNetworkSettings();
    void setConnectionBusy(bool busy, bool waitingDevice = true);
    void updateUpgradeControls();
    void onConnectSucceeded(const QString &connLabel);
    void refreshLocalIpHint();
    void switchPage(int index);

    SerialManager m_serial;
    NetworkManager m_network;
    UpdateEngine m_engine;

    QThread m_workerThread;
    QTimer m_portRefreshTimer;

    QStackedWidget *m_stack = nullptr;
    QPushButton *m_navOta = nullptr;
    QPushButton *m_navDash = nullptr;
    QPushButton *m_navLog = nullptr;
    DashboardPage *m_dashboard = nullptr;
    LogPage *m_logPage = nullptr;

    QListWidget *m_serialList = nullptr;

    QLineEdit *m_firmwarePathEdit = nullptr;
    QLabel *m_firmwareMetaLabel = nullptr;
    QRadioButton *m_modeIap = nullptr;
    QRadioButton *m_modeOta = nullptr;
    QLineEdit *m_flashStartEdit = nullptr;
    QLineEdit *m_appAddrEdit = nullptr;
    QComboBox *m_protocolCombo = nullptr;
    QComboBox *m_otaCoreCombo = nullptr;
    QLabel *m_pcIpLabel = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QWidget *m_tcpRowWidget = nullptr;
    QWidget *m_uartRowWidget = nullptr;
    QComboBox *m_baudCombo = nullptr;

    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnDisconnect = nullptr;
    QPushButton *m_btnReadInfo = nullptr;
    QPushButton *m_btnUpgrade = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnReset = nullptr;

    QProgressBar *m_progressBar = nullptr;
    QLabel *m_progressDetailLabel = nullptr;

    QLabel *m_runningSlotLabel = nullptr;
    QLabel *m_upgradeTargetLabel = nullptr;
    QLabel *m_firmwareHintLabel = nullptr;
    QLabel *m_devTypeLabel = nullptr;
    QLabel *m_chipIdLabel = nullptr;
    QLabel *m_bootVerLabel = nullptr;
    QLabel *m_protoVerLabel = nullptr;
    QLabel *m_connStatusLabel = nullptr;
    QLabel *m_runStatusLabel = nullptr;
    QLabel *m_sessionTokenLabel = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;

    QPlainTextEdit *m_logView = nullptr;
    QLabel *m_statusBarDetail = nullptr;

    bool m_waitingDevice = false;
    bool m_deviceConnected = false;
    bool m_readInfoPending = false;
    bool m_upgrading = false;
    QString m_activeConnLabel;
};

#endif // MAINWINDOW_H
