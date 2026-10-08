#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include "ModbusRtuClient.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVector>
#include <QWidget>

class GaugeWidget;
class AlarmCard;

/**
 * @brief 监控仪表页：温湿度 + 水浸告警 + 线圈输出（USB Modbus RTU）
 */
class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);

public slots:
    void refreshPorts();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onPoll();
    void onRelayToggled(bool on);
    void onBuzzerToggled(bool on);

private:
    void buildUi();
    void setConnectedUi(bool connected);
    void updateImmersionAlarm(quint16 adcMv, bool wet);
    void updateDoLeds(const QVector<bool> &coils);
    QWidget *makeLed(const QString &name);

    ModbusRtuClient m_modbus;
    QTimer m_pollTimer;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnDisconnect = nullptr;
    QPushButton *m_btnRefresh = nullptr;
    QLabel *m_connLabel = nullptr;
    QLabel *m_adcLabel = nullptr;

    GaugeWidget *m_tempGauge = nullptr;
    GaugeWidget *m_humiGauge = nullptr;
    AlarmCard *m_estopCard = nullptr;
    AlarmCard *m_leakCard = nullptr;

    QPushButton *m_relaySwitch = nullptr;
    QPushButton *m_buzzerSwitch = nullptr;
    QVector<QLabel *> m_doLeds;

    bool m_suppressCoilWrite = false;
    bool m_wetLatched = false;
    static constexpr quint8 kSlaveId = 1;
    static constexpr quint16 kWetEnterMv = 1200;
    static constexpr quint16 kWetExitMv = 800;
};

#endif // DASHBOARDPAGE_H
