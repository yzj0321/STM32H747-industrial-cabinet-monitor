#include "DashboardPage.h"

#include "AlarmCard.h"
#include "GaugeWidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QStyle>
#include <QVBoxLayout>

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    refreshPorts();

    m_pollTimer.setInterval(500);
    connect(&m_pollTimer, &QTimer::timeout, this, &DashboardPage::onPoll);
}

void DashboardPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    // Connection bar
    auto *connBox = new QGroupBox(tr("Modbus USB 连接"), this);
    auto *connLay = new QHBoxLayout(connBox);

    m_portCombo = new QComboBox(connBox);
    m_portCombo->setMinimumWidth(180);
    m_baudCombo = new QComboBox(connBox);
    m_baudCombo->addItems({QStringLiteral("115200"), QStringLiteral("57600"),
                           QStringLiteral("9600")});
    m_btnRefresh = new QPushButton(tr("刷新"), connBox);
    m_btnConnect = new QPushButton(tr("连接"), connBox);
    m_btnConnect->setObjectName(QStringLiteral("primaryButton"));
    m_btnDisconnect = new QPushButton(tr("断开"), connBox);
    m_btnDisconnect->setEnabled(false);
    m_connLabel = new QLabel(tr("未连接"), connBox);
    m_connLabel->setObjectName(QStringLiteral("metaLabel"));

    connLay->addWidget(new QLabel(tr("串口:"), connBox));
    connLay->addWidget(m_portCombo, 1);
    connLay->addWidget(new QLabel(tr("波特率:"), connBox));
    connLay->addWidget(m_baudCombo);
    connLay->addWidget(m_btnRefresh);
    connLay->addWidget(m_btnConnect);
    connLay->addWidget(m_btnDisconnect);
    connLay->addWidget(m_connLabel, 1);

    connect(m_btnRefresh, &QPushButton::clicked, this, &DashboardPage::refreshPorts);
    connect(m_btnConnect, &QPushButton::clicked, this, &DashboardPage::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &DashboardPage::onDisconnectClicked);

    // Main 3 columns
    auto *cols = new QHBoxLayout();
    cols->setSpacing(12);

    // Column 1: environment gauges
    auto *envBox = new QGroupBox(tr("环境监测 Environment"), this);
    auto *envLay = new QVBoxLayout(envBox);
    m_tempGauge = new GaugeWidget(envBox);
    m_tempGauge->setTitle(tr("温度 Temperature"));
    m_tempGauge->setRange(-20, 80);
    m_tempGauge->setUnit(QStringLiteral("°C"));
    m_tempGauge->setValue(0);
    m_tempGauge->setStatusText(tr("等待数据"), true);

    m_humiGauge = new GaugeWidget(envBox);
    m_humiGauge->setTitle(tr("湿度 Humidity"));
    m_humiGauge->setRange(0, 100);
    m_humiGauge->setUnit(QStringLiteral("%"));
    m_humiGauge->setValue(0);
    m_humiGauge->setStatusText(tr("等待数据"), true);

    envLay->addWidget(m_tempGauge, 1);
    envLay->addWidget(m_humiGauge, 1);

    // Column 2: alarms
    auto *alarmBox = new QGroupBox(tr("告警状态 Alarm"), this);
    auto *alarmLay = new QVBoxLayout(alarmBox);
    m_estopCard = new AlarmCard(tr("急停 E-STOP"), alarmBox);
    m_leakCard = new AlarmCard(tr("水浸 Water Leak"), alarmBox);
    m_adcLabel = new QLabel(tr("ADC: — mV"), alarmBox);
    m_adcLabel->setAlignment(Qt::AlignCenter);
    m_adcLabel->setObjectName(QStringLiteral("metaLabel"));
    alarmLay->addWidget(m_estopCard, 1);
    alarmLay->addWidget(m_leakCard, 1);
    alarmLay->addWidget(m_adcLabel);
    alarmLay->addStretch();

    // Column 3: outputs + DO
    auto *ioBox = new QGroupBox(tr("输出控制 / DI·DO"), this);
    auto *ioLay = new QVBoxLayout(ioBox);

    auto *outBox = new QGroupBox(tr("输出控制 Outputs"), ioBox);
    auto *outForm = new QFormLayout(outBox);

    m_relaySwitch = new QPushButton(tr("OFF"), outBox);
    m_relaySwitch->setCheckable(true);
    m_relaySwitch->setObjectName(QStringLiteral("toggleOff"));
    m_relaySwitch->setEnabled(false);

    m_buzzerSwitch = new QPushButton(tr("OFF"), outBox);
    m_buzzerSwitch->setCheckable(true);
    m_buzzerSwitch->setObjectName(QStringLiteral("toggleOff"));
    m_buzzerSwitch->setEnabled(false);

    outForm->addRow(tr("继电器 Relay (Coil0):"), m_relaySwitch);
    outForm->addRow(tr("蜂鸣器 Buzzer (Coil1):"), m_buzzerSwitch);

    connect(m_relaySwitch, &QPushButton::toggled, this, &DashboardPage::onRelayToggled);
    connect(m_buzzerSwitch, &QPushButton::toggled, this, &DashboardPage::onBuzzerToggled);

    auto *doBox = new QGroupBox(tr("远端 DO 状态 (Modbus Coils)"), ioBox);
    auto *doLay = new QHBoxLayout(doBox);
    m_doLeds.clear();
    for (int i = 0; i < 8; ++i) {
        auto *led = makeLed(QStringLiteral("DO%1").arg(i + 1));
        m_doLeds.append(led->findChild<QLabel *>(QStringLiteral("ledDot")));
        doLay->addWidget(led);
    }

    auto *legend = new QLabel(
        tr("绿=ON  灰=OFF  |  水浸阈值: ≥%1mV 告警 / ≤%2mV 恢复")
            .arg(kWetEnterMv)
            .arg(kWetExitMv),
        ioBox);
    legend->setObjectName(QStringLiteral("metaLabel"));
    legend->setWordWrap(true);

    ioLay->addWidget(outBox);
    ioLay->addWidget(doBox);
    ioLay->addWidget(legend);
    ioLay->addStretch();

    cols->addWidget(envBox, 2);
    cols->addWidget(alarmBox, 2);
    cols->addWidget(ioBox, 2);

    root->addWidget(connBox);
    root->addLayout(cols, 1);

    m_estopCard->setAlarm(false, tr("固件未映射 · 预留显示"));
    m_leakCard->setAlarm(false, tr("干燥 Dry"));
}

QWidget *DashboardPage::makeLed(const QString &name)
{
    auto *wrap = new QWidget(this);
    auto *lay = new QVBoxLayout(wrap);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    auto *dot = new QLabel(wrap);
    dot->setObjectName(QStringLiteral("ledDot"));
    dot->setFixedSize(28, 28);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(QStringLiteral(
        "background:#45475a; border-radius:14px; border:1px solid #585b70;"));

    auto *label = new QLabel(name, wrap);
    label->setAlignment(Qt::AlignCenter);
    label->setObjectName(QStringLiteral("metaLabel"));

    lay->addWidget(dot, 0, Qt::AlignHCenter);
    lay->addWidget(label);
    return wrap;
}

void DashboardPage::refreshPorts()
{
    const QString cur = m_portCombo->currentText().section(' ', 0, 0);
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString desc = info.description();
        if (desc.isEmpty())
            desc = info.manufacturer();
        const QString line = desc.isEmpty()
                                 ? info.portName()
                                 : QStringLiteral("%1  %2").arg(info.portName(), desc);
        m_portCombo->addItem(line, info.portName());
    }
    if (m_portCombo->count() == 0)
        m_portCombo->addItem(tr("(无可用串口)"), QString());

    for (int i = 0; i < m_portCombo->count(); ++i) {
        if (m_portCombo->itemData(i).toString() == cur ||
            m_portCombo->itemText(i).startsWith(cur)) {
            m_portCombo->setCurrentIndex(i);
            break;
        }
    }
}

void DashboardPage::setConnectedUi(bool connected)
{
    m_btnConnect->setEnabled(!connected);
    m_btnDisconnect->setEnabled(connected);
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_relaySwitch->setEnabled(connected);
    m_buzzerSwitch->setEnabled(connected);
    m_connLabel->setText(connected ? tr("已连接 · 轮询中") : tr("未连接"));
}

void DashboardPage::onConnectClicked()
{
    const QString port = m_portCombo->currentData().toString();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("连接"), tr("请选择 USB/串口设备（板端 CDC）。"));
        return;
    }

    const int baud = m_baudCombo->currentText().toInt();
    if (!m_modbus.open(port, baud)) {
        QMessageBox::warning(this, tr("连接失败"), m_modbus.lastError());
        return;
    }

    setConnectedUi(true);
    m_pollTimer.start();
    onPoll();
}

void DashboardPage::onDisconnectClicked()
{
    m_pollTimer.stop();
    m_modbus.close();
    setConnectedUi(false);
    m_tempGauge->setStatusText(tr("已断开"), true);
    m_humiGauge->setStatusText(tr("已断开"), true);
}

void DashboardPage::updateImmersionAlarm(quint16 adcMv, bool wet)
{
    m_wetLatched = wet;

    m_adcLabel->setText(tr("ADC: %1 mV").arg(adcMv));
    if (m_wetLatched) {
        m_leakCard->setAlarm(true, tr("浸水 Wet · %1 mV").arg(adcMv));
    } else {
        m_leakCard->setAlarm(false, tr("干燥 Dry · %1 mV").arg(adcMv));
    }
}

void DashboardPage::updateDoLeds(const QVector<bool> &coils)
{
    for (int i = 0; i < m_doLeds.size(); ++i) {
        const bool on = (i < coils.size()) ? coils.at(i) : false;
        m_doLeds[i]->setStyleSheet(on
            ? QStringLiteral("background:#40a02b; border-radius:14px; border:1px solid #a6e3a1;")
            : QStringLiteral("background:#45475a; border-radius:14px; border:1px solid #585b70;"));
    }
}

void DashboardPage::onPoll()
{
    if (!m_modbus.isOpen())
        return;

    QVector<quint16> regs;
    if (!m_modbus.readHoldingRegisters(kSlaveId, 0, 4, &regs)) {
        m_connLabel->setText(tr("读寄存器失败: %1").arg(m_modbus.lastError()));
        return;
    }

    const double humidity = regs.at(0);
    const double temperature = regs.at(1);
    const quint16 adcMv = regs.at(2);
    const bool wet = regs.at(3) != 0;

    m_humiGauge->setValue(humidity);
    m_tempGauge->setValue(temperature);

    const bool tempOk = (temperature >= 0.0 && temperature <= 60.0);
    const bool humiOk = (humidity >= 10.0 && humidity <= 90.0);
    m_tempGauge->setStatusText(tempOk ? tr("正常 Normal") : tr("超限 Warning"), tempOk);
    m_humiGauge->setStatusText(humiOk ? tr("正常 Normal") : tr("超限 Warning"), humiOk);

    updateImmersionAlarm(adcMv, wet);

    QVector<bool> coils;
    if (m_modbus.readCoils(kSlaveId, 0, 8, &coils)) {
        updateDoLeds(coils);

        m_suppressCoilWrite = true;
        m_relaySwitch->setChecked(coils.value(0));
        m_relaySwitch->setText(coils.value(0) ? tr("ON") : tr("OFF"));
        m_relaySwitch->setObjectName(coils.value(0) ? QStringLiteral("toggleOn")
                                                    : QStringLiteral("toggleOff"));
        m_relaySwitch->style()->unpolish(m_relaySwitch);
        m_relaySwitch->style()->polish(m_relaySwitch);

        m_buzzerSwitch->setChecked(coils.value(1));
        m_buzzerSwitch->setText(coils.value(1) ? tr("ON") : tr("OFF"));
        m_buzzerSwitch->setObjectName(coils.value(1) ? QStringLiteral("toggleOn")
                                                     : QStringLiteral("toggleOff"));
        m_buzzerSwitch->style()->unpolish(m_buzzerSwitch);
        m_buzzerSwitch->style()->polish(m_buzzerSwitch);
        m_suppressCoilWrite = false;
    }

    m_connLabel->setText(tr("已连接 · T=%1°C H=%2% ADC=%3mV")
                             .arg(temperature, 0, 'f', 0)
                             .arg(humidity, 0, 'f', 0)
                             .arg(adcMv));
}

void DashboardPage::onRelayToggled(bool on)
{
    if (m_suppressCoilWrite || !m_modbus.isOpen())
        return;
    if (!m_modbus.writeSingleCoil(kSlaveId, 0, on)) {
        QMessageBox::warning(this, tr("写线圈"), m_modbus.lastError());
        return;
    }
    m_relaySwitch->setText(on ? tr("ON") : tr("OFF"));
    m_relaySwitch->setObjectName(on ? QStringLiteral("toggleOn") : QStringLiteral("toggleOff"));
    m_relaySwitch->style()->unpolish(m_relaySwitch);
    m_relaySwitch->style()->polish(m_relaySwitch);
}

void DashboardPage::onBuzzerToggled(bool on)
{
    if (m_suppressCoilWrite || !m_modbus.isOpen())
        return;
    if (!m_modbus.writeSingleCoil(kSlaveId, 1, on)) {
        QMessageBox::warning(this, tr("写线圈"), m_modbus.lastError());
        return;
    }
    m_buzzerSwitch->setText(on ? tr("ON") : tr("OFF"));
    m_buzzerSwitch->setObjectName(on ? QStringLiteral("toggleOn") : QStringLiteral("toggleOff"));
    m_buzzerSwitch->style()->unpolish(m_buzzerSwitch);
    m_buzzerSwitch->style()->polish(m_buzzerSwitch);
}
