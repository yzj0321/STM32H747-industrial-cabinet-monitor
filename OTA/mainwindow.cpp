#include "mainwindow.h"

#include "DashboardPage.h"
#include "LogPage.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRadioButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

namespace {
constexpr quint16 kDefaultTcpPort = 5000;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_engine(&m_network, nullptr)
{
    setWindowTitle(tr("CabinetGuard 上位机 v2.9.0 — OTA / 监控 / 日志"));
    resize(1360, 800);
    buildUi();
    applyStyleSheet();
    loadNetworkSettings();
    wireSignals();

    m_serial.moveToThread(&m_workerThread);
    m_serial.moveInternalObjectsToThread(&m_workerThread);
    m_network.moveToThread(&m_workerThread);
    m_network.moveInternalObjectsToThread(&m_workerThread);
    m_engine.moveToThread(&m_workerThread);
    m_workerThread.start();

    m_portRefreshTimer.setInterval(2000);
    connect(&m_portRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshSerialPorts);
    m_portRefreshTimer.start();
    refreshSerialPorts();
    refreshLocalIpHint();
    onProtocolChanged(m_protocolCombo->currentIndex());
    updateStatusBar();
    updateUpgradeControls();

    const int timeoutMs = m_timeoutSpin ? m_timeoutSpin->value() : 30000;
    QMetaObject::invokeMethod(
        &m_network,
        [this, timeoutMs]() { m_network.setResponseTimeoutMs(timeoutMs); },
        Qt::QueuedConnection);
    QMetaObject::invokeMethod(
        &m_serial,
        [this, timeoutMs]() { m_serial.setResponseTimeoutMs(timeoutMs); },
        Qt::QueuedConnection);
    // Production path uses sendWithRetry; disable single-shot debug mode.
    QMetaObject::invokeMethod(
        &m_engine,
        [this]() { m_engine.setDebugSingleShot(false); },
        Qt::QueuedConnection);
    QMetaObject::invokeMethod(
        &m_engine,
        [this]() { m_engine.setOtaCore(0); },
        Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    saveNetworkSettings();
    m_workerThread.quit();
    m_workerThread.wait(3000);
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *nav = new QWidget(central);
    nav->setObjectName(QStringLiteral("navRail"));
    nav->setFixedWidth(128);
    auto *navLay = new QVBoxLayout(nav);
    navLay->setContentsMargins(10, 16, 10, 16);
    navLay->setSpacing(8);

    auto *brand = new QLabel(tr("Cabinet\nGuard"), nav);
    brand->setObjectName(QStringLiteral("navBrand"));
    brand->setAlignment(Qt::AlignCenter);
    navLay->addWidget(brand);
    navLay->addSpacing(12);

    m_navOta = new QPushButton(tr("OTA 升级"), nav);
    m_navDash = new QPushButton(tr("监控仪表"), nav);
    m_navLog = new QPushButton(tr("查看日志"), nav);
    for (QPushButton *btn : {m_navOta, m_navDash, m_navLog}) {
        btn->setCheckable(true);
        btn->setObjectName(QStringLiteral("navButton"));
        btn->setMinimumHeight(44);
        navLay->addWidget(btn);
    }
    m_navOta->setChecked(true);

    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(m_navOta, 0);
    navGroup->addButton(m_navDash, 1);
    navGroup->addButton(m_navLog, 2);
    connect(navGroup, &QButtonGroup::idClicked, this, &MainWindow::switchPage);

    navLay->addStretch();
    auto *ver = new QLabel(tr("v2.9.0"), nav);
    ver->setObjectName(QStringLiteral("metaLabel"));
    ver->setAlignment(Qt::AlignCenter);
    navLay->addWidget(ver);

    m_stack = new QStackedWidget(central);
    m_stack->addWidget(buildOtaPage());
    m_dashboard = new DashboardPage(m_stack);
    m_stack->addWidget(m_dashboard);
    m_logPage = new LogPage(m_stack);
    m_stack->addWidget(m_logPage);

    root->addWidget(nav);
    root->addWidget(m_stack, 1);

    m_statusBarDetail = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusBarDetail, 1);
    statusBar()->showMessage(tr("就绪"));
}

QWidget *MainWindow::buildOtaPage()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *leftBox = new QGroupBox(tr("设备列表"), page);
    leftBox->setMinimumWidth(240);
    auto *leftLay = new QVBoxLayout(leftBox);

    leftLay->addWidget(new QLabel(tr("串口设备 COM"), leftBox));
    m_serialList = new QListWidget(leftBox);
    m_serialList->setAlternatingRowColors(true);
    leftLay->addWidget(m_serialList, 2);

    leftLay->addWidget(new QLabel(tr("网络 OTA 说明"), leftBox));
    auto *netHint = new QListWidget(leftBox);
    netHint->addItem(tr("上位机 = TCP Server"));
    netHint->addItem(tr("单片机 = TCP Client"));
    netHint->addItem(tr("设备主动连 PC IP:端口"));
    netHint->setEnabled(false);
    leftLay->addWidget(netHint, 1);

    auto *btnRefreshPorts = new QPushButton(tr("刷新串口"), leftBox);
    leftLay->addWidget(btnRefreshPorts);
    connect(btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);

    auto *centerBox = new QGroupBox(tr("OTA / IAP 升级配置"), page);
    centerBox->setMinimumWidth(520);
    auto *centerLay = new QVBoxLayout(centerBox);

    auto *fwRow = new QHBoxLayout();
    m_firmwarePathEdit = new QLineEdit(centerBox);
    m_firmwarePathEdit->setPlaceholderText(tr("选择 .bin / .hex 固件文件"));
    auto *btnBrowse = new QPushButton(tr("浏览..."), centerBox);
    fwRow->addWidget(new QLabel(tr("固件文件:"), centerBox));
    fwRow->addWidget(m_firmwarePathEdit, 1);
    fwRow->addWidget(btnBrowse);
    centerLay->addLayout(fwRow);
    connect(btnBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseFirmware);

    m_firmwareMetaLabel = new QLabel(tr("未选择文件"), centerBox);
    m_firmwareMetaLabel->setObjectName(QStringLiteral("metaLabel"));
    centerLay->addWidget(m_firmwareMetaLabel);

    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(tr("升级模式:"), centerBox));
    m_modeIap = new QRadioButton(tr("IAP (In-Application Programming)"), centerBox);
    m_modeOta = new QRadioButton(tr("OTA (Over-The-Air)"), centerBox);
    m_modeOta->setChecked(true);
    modeRow->addWidget(m_modeIap);
    modeRow->addWidget(m_modeOta);
    modeRow->addStretch();
    centerLay->addLayout(modeRow);

    auto *coreRow = new QHBoxLayout();
    coreRow->addWidget(new QLabel(tr("OTA 目标核:"), centerBox));
    m_otaCoreCombo = new QComboBox(centerBox);
    m_otaCoreCombo->addItem(tr("CM7 (0x0802/0x080A)"), 0);
    m_otaCoreCombo->addItem(tr("CM4 (0x0812/0x081A)"), 1);
    coreRow->addWidget(m_otaCoreCombo, 1);
    centerLay->addLayout(coreRow);

    auto *addrForm = new QFormLayout();
    m_flashStartEdit = new QLineEdit(QStringLiteral("0x08000000"), centerBox);
    m_appAddrEdit = new QLineEdit(QStringLiteral("(自动: GET_INFO inactive)"), centerBox);
    m_appAddrEdit->setReadOnly(true);
    m_appAddrEdit->setToolTip(
        tr("由 GET_INFO 自动填入当前未运行槽基址。\n"
           "CM7: A=0x08020000 / B=0x080A0000\n"
           "CM4: A=0x08120000 / B=0x081A0000\n"
           "禁止手填，避免误擦分区。"));
    addrForm->addRow(tr("Flash 起始地址:"), m_flashStartEdit);
    addrForm->addRow(tr("升级目标地址:"), m_appAddrEdit);
    centerLay->addLayout(addrForm);

    m_protocolCombo = new QComboBox(centerBox);
    m_protocolCombo->addItems({tr("TCP / 网络"), tr("UART / 串口")});

    m_pcIpLabel = new QLabel(tr("检测中..."), centerBox);
    m_pcIpLabel->setObjectName(QStringLiteral("metaLabel"));
    m_pcIpLabel->setWordWrap(true);

    m_portSpin = new QSpinBox(centerBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(kDefaultTcpPort);

    m_tcpRowWidget = new QWidget(centerBox);
    auto *tcpLay = new QHBoxLayout(m_tcpRowWidget);
    tcpLay->setContentsMargins(0, 0, 0, 0);
    tcpLay->addWidget(new QLabel(tr("监听端口"), m_tcpRowWidget));
    tcpLay->addWidget(m_portSpin);
    tcpLay->addStretch();

    m_baudCombo = new QComboBox(centerBox);
    m_baudCombo->addItems({QStringLiteral("115200"), QStringLiteral("230400"),
                           QStringLiteral("460800"), QStringLiteral("921600")});
    m_uartRowWidget = m_baudCombo;

    auto *connForm = new QFormLayout();
    connForm->addRow(tr("传输协议:"), m_protocolCombo);
    connForm->addRow(tr("本机 IP (设备连接目标):"), m_pcIpLabel);
    connForm->addRow(tr("TCP Server:"), m_tcpRowWidget);
    connForm->addRow(tr("波特率:"), m_uartRowWidget);
    centerLay->addLayout(connForm);

    auto *connBtnRow = new QHBoxLayout();
    m_btnConnect = new QPushButton(tr("开始监听"), centerBox);
    m_btnDisconnect = new QPushButton(tr("断开连接"), centerBox);
    m_btnReadInfo = new QPushButton(tr("读取设备信息"), centerBox);
    m_btnDisconnect->setEnabled(false);
    m_btnReadInfo->setEnabled(false);
    connBtnRow->addWidget(m_btnConnect);
    connBtnRow->addWidget(m_btnDisconnect);
    connBtnRow->addWidget(m_btnReadInfo);
    centerLay->addLayout(connBtnRow);

    auto *upgradeBox = new QGroupBox(tr("一键升级"), centerBox);
    auto *upgradeLay = new QVBoxLayout(upgradeBox);
    auto *upgradeHint = new QLabel(
        tr("流程: 连接设备 → 选择固件 → 点击「升级」\n"
           "自动执行: GET_INFO → BEGIN → ERASE → START_TRANSFER → 传固件 → 结束校验"),
        upgradeBox);
    upgradeHint->setObjectName(QStringLiteral("metaLabel"));
    upgradeHint->setWordWrap(true);
    upgradeLay->addWidget(upgradeHint);

    auto *paramForm = new QFormLayout();
    m_timeoutSpin = new QSpinBox(upgradeBox);
    m_timeoutSpin->setRange(1000, 180000);
    m_timeoutSpin->setSingleStep(1000);
    m_timeoutSpin->setValue(30000);
    m_sessionTokenLabel = new QLabel(tr("无"), upgradeBox);
    paramForm->addRow(tr("应答超时 (ms):"), m_timeoutSpin);
    paramForm->addRow(tr("Session token:"), m_sessionTokenLabel);
    upgradeLay->addLayout(paramForm);

    auto *upgradeBtnRow = new QHBoxLayout();
    m_btnUpgrade = new QPushButton(tr("升级"), upgradeBox);
    m_btnUpgrade->setObjectName(QStringLiteral("primaryButton"));
    m_btnUpgrade->setMinimumHeight(40);
    m_btnStop = new QPushButton(tr("停止"), upgradeBox);
    m_btnStop->setObjectName(QStringLiteral("dangerButton"));
    m_btnStop->setEnabled(false);
    m_btnReset = new QPushButton(tr("复位设备"), upgradeBox);
    m_btnReset->setEnabled(false);
    upgradeBtnRow->addWidget(m_btnUpgrade, 2);
    upgradeBtnRow->addWidget(m_btnStop, 1);
    upgradeBtnRow->addWidget(m_btnReset, 1);
    upgradeLay->addLayout(upgradeBtnRow);
    centerLay->addWidget(upgradeBox);

    m_progressBar = new QProgressBar(centerBox);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("%p%"));
    centerLay->addWidget(m_progressBar);

    m_progressDetailLabel = new QLabel(tr("等待操作..."), centerBox);
    centerLay->addWidget(m_progressDetailLabel);

    centerLay->addStretch();

    auto *rightSplitter = new QSplitter(Qt::Vertical, page);
    rightSplitter->setMinimumWidth(360);

    auto *statusBox = new QGroupBox(tr("设备状态"), rightSplitter);
    auto *statusForm = new QFormLayout(statusBox);
    m_runningSlotLabel = new QLabel(tr("—"), statusBox);
    m_upgradeTargetLabel = new QLabel(tr("—"), statusBox);
    m_firmwareHintLabel = new QLabel(tr("—"), statusBox);
    m_devTypeLabel = new QLabel(tr("—"), statusBox);
    m_chipIdLabel = new QLabel(tr("—"), statusBox);
    m_bootVerLabel = new QLabel(tr("—"), statusBox);
    m_protoVerLabel = new QLabel(tr("SOIP v2.0"), statusBox);
    m_connStatusLabel = new QLabel(tr("未连接"), statusBox);
    m_runStatusLabel = new QLabel(tr("空闲"), statusBox);
    statusForm->addRow(tr("设备型号:"), m_devTypeLabel);
    statusForm->addRow(tr("当前分区:"), m_runningSlotLabel);
    statusForm->addRow(tr("升级目标:"), m_upgradeTargetLabel);
    statusForm->addRow(tr("建议固件:"), m_firmwareHintLabel);
    statusForm->addRow(tr("芯片 ID:"), m_chipIdLabel);
    statusForm->addRow(tr("Bootloader:"), m_bootVerLabel);
    statusForm->addRow(tr("协议版本:"), m_protoVerLabel);
    statusForm->addRow(tr("连接状态:"), m_connStatusLabel);
    statusForm->addRow(tr("运行状态:"), m_runStatusLabel);

    auto *logBox = new QGroupBox(tr("日志输出"), rightSplitter);
    auto *logLay = new QVBoxLayout(logBox);
    auto *btnClearLog = new QPushButton(tr("Clear Log"), logBox);
    connect(btnClearLog, &QPushButton::clicked, this, [this]() {
        if (m_logView != nullptr)
            m_logView->clear();
    });
    logLay->addWidget(btnClearLog, 0, Qt::AlignRight);
    m_logView = new QPlainTextEdit(logBox);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000);
    logLay->addWidget(m_logView);

    rightSplitter->addWidget(statusBox);
    rightSplitter->addWidget(logBox);
    rightSplitter->setStretchFactor(0, 0);
    rightSplitter->setStretchFactor(1, 1);

    root->addWidget(leftBox, 1);
    root->addWidget(centerBox, 2);
    root->addWidget(rightSplitter, 1);
    return page;
}

void MainWindow::switchPage(int index)
{
    if (m_stack != nullptr)
        m_stack->setCurrentIndex(index);
    if (index == 1 && m_dashboard != nullptr)
        m_dashboard->refreshPorts();
}

void MainWindow::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background-color: #1e1e2e; color: #cdd6f4; }
        QWidget#navRail { background-color: #181825; border-right: 1px solid #313244; }
        QLabel#navBrand { color: #89b4fa; font-weight: bold; font-size: 15px; }
        QPushButton#navButton {
            background-color: transparent; border: 1px solid #313244; border-radius: 8px;
            text-align: center; padding: 10px 6px; font-weight: bold;
        }
        QPushButton#navButton:hover { background-color: #313244; }
        QPushButton#navButton:checked { background-color: #1e66f5; border-color: #1e66f5; color: white; }
        QGroupBox { border: 1px solid #45475a; border-radius: 6px; margin-top: 10px; padding-top: 8px; font-weight: bold; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #89b4fa; }
        QListWidget, QLineEdit, QComboBox, QSpinBox, QPlainTextEdit {
            background-color: #313244; border: 1px solid #45475a; border-radius: 4px; padding: 4px;
        }
        QListWidget::item:selected { background-color: #45475a; }
        QPushButton { background-color: #45475a; border: none; border-radius: 4px; padding: 8px 12px; }
        QPushButton:hover { background-color: #585b70; }
        QPushButton:disabled { color: #6c7086; }
        QPushButton#primaryButton { background-color: #1e66f5; color: white; font-weight: bold; }
        QPushButton#primaryButton:hover { background-color: #3584e4; }
        QPushButton#dangerButton { background-color: #e01b24; color: white; }
        QPushButton#toggleOn { background-color: #40a02b; color: white; font-weight: bold; min-width: 72px; }
        QPushButton#toggleOff { background-color: #45475a; color: #cdd6f4; min-width: 72px; }
        QProgressBar { border: 1px solid #45475a; border-radius: 4px; text-align: center; height: 22px; }
        QProgressBar::chunk { background-color: #1e66f5; border-radius: 3px; }
        QLabel#metaLabel { color: #a6adc8; font-size: 11px; }
    )"));
}

void MainWindow::wireSignals()
{
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProtocolChanged);
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { refreshLocalIpHint(); });
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_btnUpgrade, &QPushButton::clicked, this, &MainWindow::onStartUpgrade);
    connect(m_btnStop, &QPushButton::clicked, &m_engine, &UpdateEngine::stopUpgrade);
    connect(m_btnReset, &QPushButton::clicked, &m_engine, &UpdateEngine::resetDevice);
    connect(m_btnReadInfo, &QPushButton::clicked, this, &MainWindow::onReadInfoClicked);
    connect(m_otaCoreCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const quint8 core = static_cast<quint8>(m_otaCoreCombo->itemData(index).toUInt());
                QMetaObject::invokeMethod(
                    &m_engine,
                    [this, core]() { m_engine.setOtaCore(core); },
                    Qt::QueuedConnection);
            });
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int ms) {
        QMetaObject::invokeMethod(
            &m_network,
            [this, ms]() { m_network.setResponseTimeoutMs(ms); },
            Qt::QueuedConnection);
        QMetaObject::invokeMethod(
            &m_serial,
            [this, ms]() { m_serial.setResponseTimeoutMs(ms); },
            Qt::QueuedConnection);
    });

    connect(&m_serial, &SerialManager::logMessage, this, &MainWindow::appendLog);
    connect(&m_serial, &SerialManager::communicationError, this, [this](const QString &msg) {
        appendLog(msg, QStringLiteral("ERROR"));
    });
    connect(&m_network, &NetworkManager::logMessage, this, &MainWindow::appendLog);
    connect(&m_network, &NetworkManager::communicationError, this, [this](const QString &msg) {
        appendLog(msg, QStringLiteral("ERROR"));
    });
    connect(&m_network, &NetworkManager::connectFinished, this, &MainWindow::onTcpConnectFinished);
    connect(&m_network, &NetworkManager::listenStarted, this, &MainWindow::onTcpListenStarted);
    connect(&m_network, &NetworkManager::closed, this, &MainWindow::onTcpClientDisconnected);
    connect(&m_engine, &UpdateEngine::logMessage, this, &MainWindow::appendLog);
    connect(&m_engine, &UpdateEngine::warningRaised, this, [this](const QString &msg) {
        appendLog(msg, QStringLiteral("WARN"));
    });
    connect(&m_engine, &UpdateEngine::progressChanged, this, &MainWindow::onProgress);
    connect(&m_engine, &UpdateEngine::upgradeFinished, this, &MainWindow::onUpgradeFinished);
    connect(&m_engine, &UpdateEngine::deviceInfoUpdated, this, &MainWindow::onDeviceInfo);
    connect(&m_engine, &UpdateEngine::stateChanged, this, &MainWindow::onEngineStateChanged);
    connect(&m_engine, &UpdateEngine::sessionTokenChanged, this, &MainWindow::onSessionTokenChanged);
    connect(&m_engine, &UpdateEngine::operationFinished, this, [this]() {
        m_readInfoPending = false;
        if (!m_upgrading && m_deviceConnected)
            m_btnReadInfo->setEnabled(true);
        updateUpgradeControls();
    });
}

void MainWindow::onProtocolChanged(int index)
{
    Q_UNUSED(index);
    updateConnectionFields();
    updateStatusBar();
}

void MainWindow::updateConnectionFields()
{
    const bool tcp = isTcpMode();
    m_tcpRowWidget->setVisible(tcp);
    m_pcIpLabel->setVisible(tcp);
    m_uartRowWidget->setVisible(!tcp);
    m_serialList->setEnabled(!tcp);
    m_btnConnect->setText(tcp ? tr("开始监听") : tr("连接设备"));
    if (tcp)
        refreshLocalIpHint();
}

bool MainWindow::isTcpMode() const
{
    return m_protocolCombo->currentIndex() == 0;
}

OtaTransport *MainWindow::activeTransport() const
{
    if (isTcpMode())
        return const_cast<OtaTransport *>(static_cast<const OtaTransport *>(&m_network));
    return const_cast<OtaTransport *>(static_cast<const OtaTransport *>(&m_serial));
}

void MainWindow::refreshLocalIpHint()
{
    const QStringList addrs = NetworkManager::localIPv4Addresses();
    if (addrs.isEmpty()) {
        m_pcIpLabel->setText(
            tr("未检测到 192.168.x.x 地址，请确认 PC 已连接 WiFi/路由器局域网"));
        return;
    }

    QStringList lines;
    for (const QString &ip : addrs)
        lines << QStringLiteral("%1:%2").arg(ip).arg(m_portSpin->value());
    m_pcIpLabel->setText(
        tr("请让单片机连接以下任一地址:\n%1").arg(lines.join(QStringLiteral("  /  "))));
}

void MainWindow::loadNetworkSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("network"));
    m_portSpin->setValue(settings.value(QStringLiteral("port"), kDefaultTcpPort).toInt());
    const int proto = settings.value(QStringLiteral("protocol"), 0).toInt();
    m_protocolCombo->setCurrentIndex(proto);
    settings.endGroup();
}

void MainWindow::saveNetworkSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("network"));
    settings.setValue(QStringLiteral("port"), m_portSpin->value());
    settings.setValue(QStringLiteral("protocol"), m_protocolCombo->currentIndex());
    settings.endGroup();
}

void MainWindow::refreshSerialPorts()
{
    const QString current = m_serialList->currentItem()
                                ? m_serialList->currentItem()->text().section(' ', 0, 0)
                                : QString();
    m_serialList->clear();

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString desc = info.description();
        if (desc.isEmpty())
            desc = info.manufacturer();
        const QString line = desc.isEmpty()
                                 ? QStringLiteral("%1  (Online)").arg(info.portName())
                                 : QStringLiteral("%1  %2").arg(info.portName(), desc);
        m_serialList->addItem(line);
    }

    if (m_serialList->count() == 0)
        m_serialList->addItem(tr("(无可用串口)"));

    for (int i = 0; i < m_serialList->count(); ++i) {
        if (m_serialList->item(i)->text().startsWith(current)) {
            m_serialList->setCurrentRow(i);
            break;
        }
    }
}

void MainWindow::updateUpgradeControls()
{
    const bool busy = m_waitingDevice || m_upgrading;
    const bool hasFirmware = !m_firmwarePathEdit->text().isEmpty();
    const bool canUpgrade = m_deviceConnected && hasFirmware && !busy;

    m_btnUpgrade->setEnabled(canUpgrade);
    m_btnStop->setEnabled(m_upgrading);
    m_btnReset->setEnabled(m_deviceConnected && !m_upgrading);
    m_btnReadInfo->setEnabled(m_deviceConnected && !busy);
    m_timeoutSpin->setEnabled(!busy && !m_deviceConnected);
}

void MainWindow::setConnectionBusy(bool busy, bool waitingDevice)
{
    m_waitingDevice = busy && waitingDevice;
    m_btnConnect->setEnabled(!busy && !m_deviceConnected);
    m_btnDisconnect->setEnabled(busy || m_deviceConnected);
    m_protocolCombo->setEnabled(!busy && !m_deviceConnected);
    m_portSpin->setEnabled(!busy && !m_deviceConnected);
    m_baudCombo->setEnabled(!busy && !m_deviceConnected);
    m_serialList->setEnabled(!busy && !m_deviceConnected && !isTcpMode());
    if (busy && waitingDevice) {
        m_runStatusLabel->setText(tr("等待设备..."));
        m_progressDetailLabel->setText(tr("TCP Server 已启动，持续等待单片机接入..."));
    }
    updateUpgradeControls();
    updateStatusBar();
}

void MainWindow::onTcpListenStarted(quint16 port, const QStringList &localAddresses)
{
    Q_UNUSED(port);
    if (!localAddresses.isEmpty()) {
        QStringList lines;
        for (const QString &ip : localAddresses)
            lines << QStringLiteral("%1:%2").arg(ip).arg(m_portSpin->value());
        m_pcIpLabel->setText(
            tr("请让单片机连接以下任一地址:\n%1").arg(lines.join(QStringLiteral("  /  "))));
    }
}

void MainWindow::onConnectClicked()
{
    if (m_waitingDevice || m_deviceConnected)
        return;

    if (isTcpMode()) {
        const quint16 port = static_cast<quint16>(m_portSpin->value());
        saveNetworkSettings();
        refreshLocalIpHint();
        setConnectionBusy(true, true);
        QMetaObject::invokeMethod(
            &m_network,
            [this, port]() { m_network.startListen(port); },
            Qt::QueuedConnection);
        return;
    }

    if (!m_serialList->currentItem() || m_serialList->currentItem()->text().startsWith('(')) {
        QMessageBox::warning(this, tr("连接"), tr("请先选择串口。"));
        return;
    }

    const QString port = m_serialList->currentItem()->text().section(' ', 0, 0);
    const int baud = m_baudCombo->currentText().toInt();
    bool okOpen = false;
    QMetaObject::invokeMethod(
        &m_serial,
        [this, port, baud, &okOpen]() { okOpen = m_serial.openPort(port, baud); },
        Qt::BlockingQueuedConnection);

    if (!okOpen) {
        QMessageBox::warning(this, tr("连接失败"), tr("无法打开串口 %1。").arg(port));
        return;
    }

    QMetaObject::invokeMethod(
        &m_engine,
        [this]() { m_engine.setTransport(&m_serial); },
        Qt::BlockingQueuedConnection);

    onConnectSucceeded(port);
}

void MainWindow::onTcpConnectFinished(bool success, const QString &message)
{
    if (!m_waitingDevice && !success)
        return;

    setConnectionBusy(false, false);

    if (!success) {
        m_progressDetailLabel->setText(tr("等待操作..."));
        QMessageBox::warning(this, tr("等待设备失败"), message);
        return;
    }

    QString connLabel;
    QMetaObject::invokeMethod(
        &m_network,
        [&connLabel, this]() { connLabel = m_network.connectionLabel(); },
        Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(
        &m_engine,
        [this]() { m_engine.setTransport(&m_network); },
        Qt::BlockingQueuedConnection);
    onConnectSucceeded(connLabel);
}

void MainWindow::onSessionTokenChanged(quint32 token)
{
    if (token == 0)
        m_sessionTokenLabel->setText(tr("无"));
    else
        m_sessionTokenLabel->setText(QStringLiteral("0x%1").arg(token, 8, 16, QChar('0')));
}

void MainWindow::onReadInfoClicked()
{
    if (m_readInfoPending) {
        appendLog(tr("GET_INFO already in progress"), QStringLiteral("WARN"));
        return;
    }

    if (m_waitingDevice) {
        QMessageBox::information(this, tr("读取设备信息"),
                                 tr("设备尚未连接，请先等待单片机接入。"));
        return;
    }

    if (!m_deviceConnected) {
        QMessageBox::information(this, tr("读取设备信息"), tr("请先连接或等待设备接入。"));
        return;
    }

    appendLog(tr("发送 GET_INFO 请求..."), QStringLiteral("INFO"));
    m_readInfoPending = true;
    m_btnReadInfo->setEnabled(false);
    QMetaObject::invokeMethod(&m_engine, &UpdateEngine::refreshDeviceInfo, Qt::QueuedConnection);
}

void MainWindow::onTcpClientDisconnected()
{
    if (!m_deviceConnected && !m_waitingDevice)
        return;

    m_waitingDevice = false;
    m_deviceConnected = false;
    m_readInfoPending = false;
    m_upgrading = false;
    m_activeConnLabel.clear();
    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    m_sessionTokenLabel->setText(tr("无"));
    m_connStatusLabel->setText(tr("未连接"));
    m_runStatusLabel->setText(tr("空闲"));
    m_progressDetailLabel->setText(tr("等待操作..."));
    appendLog(tr("设备 TCP 连接已断开"), QStringLiteral("WARN"));
    updateUpgradeControls();
    updateStatusBar();
}

void MainWindow::onConnectSucceeded(const QString &connLabel)
{
    /* 不在此处写死 Flash 地址；等 GET_INFO / 一键升级内的 GET_INFO 决定 inactive 槽 */
    m_deviceConnected = true;
    m_activeConnLabel = connLabel;
    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    m_connStatusLabel->setText(tr("已连接 (%1)").arg(connLabel));
    m_runStatusLabel->setText(tr("就绪"));
    m_progressDetailLabel->setText(tr("等待操作..."));
    appendLog(tr("设备已连接：请先「读取设备信息」或直接「升级」(将自动 GET_INFO 选槽)"),
              QStringLiteral("INFO"));
    updateUpgradeControls();
    updateStatusBar();
}

void MainWindow::onDisconnectClicked()
{
    if (m_engine.state() == UpdateEngine::State::Transferring
        || m_engine.state() == UpdateEngine::State::Erasing
        || m_engine.state() == UpdateEngine::State::Verifying) {
        QMetaObject::invokeMethod(&m_engine, &UpdateEngine::stopUpgrade, Qt::QueuedConnection);
    }

    if (isTcpMode()) {
        QMetaObject::invokeMethod(&m_network, &NetworkManager::close, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(&m_serial, &SerialManager::close, Qt::QueuedConnection);
    }

    m_waitingDevice = false;
    m_deviceConnected = false;
    m_readInfoPending = false;
    m_upgrading = false;
    m_activeConnLabel.clear();
    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    m_sessionTokenLabel->setText(tr("无"));
    m_connStatusLabel->setText(tr("未连接"));
    m_runStatusLabel->setText(tr("空闲"));
    m_progressDetailLabel->setText(tr("等待操作..."));
    updateUpgradeControls();
    updateStatusBar();
}

void MainWindow::onBrowseFirmware()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("选择固件"), QString(),
        tr("固件 (*.bin *.hex);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    m_firmwarePathEdit->setText(path);
    m_engine.setFirmwarePath(path);

    const QFileInfo fi(path);
    m_firmwareMetaLabel->setText(
        tr("大小: %1 | 修改: %2")
            .arg(formatSize(fi.size()))
            .arg(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
    updateUpgradeControls();
}

void MainWindow::onStartUpgrade()
{
    if (!m_deviceConnected) {
        QMessageBox::information(this, tr("升级"), tr("请先连接设备。"));
        return;
    }
    if (m_firmwarePathEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("升级"), tr("请先选择固件文件。"));
        return;
    }

    appendLog(tr("=== 开始一键升级 ==="), QStringLiteral("INFO"));
    appendLog(tr("擦写地址将由 GET_INFO 的 inactive 槽决定，不会使用界面手填地址"),
              QStringLiteral("INFO"));

    const QString path = m_firmwarePathEdit->text();
    QMetaObject::invokeMethod(
        &m_engine,
        [this, path]() {
            m_engine.setFirmwarePath(path);
            /* 禁止 setFlashAddress(UI/0x080A0000)：地址只允许来自 GET_INFO */
            m_engine.startUpgrade();
        },
        Qt::QueuedConnection);

    m_upgrading = true;
    m_btnUpgrade->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_btnReset->setEnabled(false);
    m_btnReadInfo->setEnabled(false);
    m_runStatusLabel->setText(tr("升级中..."));
    m_progressBar->setValue(0);
    m_progressDetailLabel->setText(tr("升级进行中..."));
}

void MainWindow::appendLog(const QString &text, const QString &level)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    QString prefix = QStringLiteral("[INFO]");
    if (level == QLatin1String("WARN"))
        prefix = QStringLiteral("[WARN]");
    else if (level == QLatin1String("ERROR"))
        prefix = QStringLiteral("[ERROR]");

    m_logView->appendPlainText(QStringLiteral("[%1] %2 %3").arg(ts, prefix, text));
}

void MainWindow::onProgress(double percent, qint64 sent, qint64 total, double speedBps, int etaSec)
{
    m_progressBar->setValue(static_cast<int>(percent * 10.0));
    m_progressDetailLabel->setText(
        tr("已传输: %1 / %2 | 速度: %3/s | 剩余: %4")
            .arg(formatSize(sent))
            .arg(formatSize(total))
            .arg(formatSize(static_cast<qint64>(speedBps)))
            .arg(formatDuration(etaSec)));
    updateStatusBar();
}

void MainWindow::onUpgradeFinished(bool success, const QString &message)
{
    m_upgrading = false;
    m_btnStop->setEnabled(false);
    updateUpgradeControls();
    m_runStatusLabel->setText(success ? tr("升级完成") : tr("失败 / 已中止"));
    appendLog(message, success ? QStringLiteral("INFO") : QStringLiteral("ERROR"));
    if (!success)
        QMessageBox::warning(this, tr("升级结果"), message);
    else
        QMessageBox::information(this, tr("升级结果"), message);
    updateStatusBar();
}

void MainWindow::onDeviceInfo(const OtaProtocol::DeviceInfo &info)
{
    m_devTypeLabel->setText(info.deviceType);
    m_chipIdLabel->setText(QStringLiteral("0x%1").arg(info.chipId, 8, 16, QChar('0')));
    m_bootVerLabel->setText(info.bootVersion);
    m_protoVerLabel->setText(info.protocolVersion);
    if (!m_upgrading)
        m_runStatusLabel->setText(tr("可升级"));

    if (info.runningSlot <= 1U) {
        m_runningSlotLabel->setText(
            tr("Slot %1 (index %2, EEPROM active=%3 pending=%4 trial=%5 flags=0x%6)")
                .arg(info.runningSlotName())
                .arg(info.runningSlot)
                .arg(info.activeSlot)
                .arg(info.pendingSlot)
                .arg(info.bootTrial)
                .arg(info.flags, 2, 16, QChar('0')));
        m_upgradeTargetLabel->setText(
            tr("Slot %1 @ 0x%2")
                .arg(info.inactiveSlotName())
                .arg(info.appAddress, 8, 16, QChar('0')));
        const quint8 core = static_cast<quint8>(m_otaCoreCombo->currentData().toUInt());
        m_firmwareHintLabel->setText(info.suggestedFirmware(core));
    } else {
        m_runningSlotLabel->setText(tr("—"));
        m_upgradeTargetLabel->setText(tr("—"));
        m_firmwareHintLabel->setText(tr("—"));
    }

    if (info.appAddress != 0U) {
        m_appAddrEdit->setText(
            tr("Slot %1 @ 0x%2")
                .arg(info.inactiveSlotName())
                .arg(info.appAddress, 8, 16, QChar('0')));
        /* engine 侧已在 GET_INFO 路径 applyInactiveTarget；此处仅同步 UI */
    } else {
        m_appAddrEdit->setText(tr("(自动: GET_INFO inactive)"));
    }
}

void MainWindow::onEngineStateChanged(UpdateEngine::State state)
{
    switch (state) {
    case UpdateEngine::State::Erasing:
        m_runStatusLabel->setText(tr("擦除中..."));
        break;
    case UpdateEngine::State::Transferring:
        m_runStatusLabel->setText(tr("传固件中..."));
        break;
    case UpdateEngine::State::Verifying:
        m_runStatusLabel->setText(tr("校验固件..."));
        break;
    case UpdateEngine::State::Failed:
    case UpdateEngine::State::Aborted:
        m_upgrading = false;
        m_btnStop->setEnabled(false);
        updateUpgradeControls();
        break;
    case UpdateEngine::State::Completed:
        m_upgrading = false;
        break;
    default:
        break;
    }
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    QString conn;
    if (m_waitingDevice)
        conn = tr("监听中 :%1").arg(m_portSpin->value());
    else if (m_deviceConnected)
        conn = tr("已连接: %1").arg(m_activeConnLabel);
    else
        conn = tr("未连接");

    const QString proto = isTcpMode() ? tr("TCP Server/SOIP") : tr("UART/SOIP");
    const QString detail = isTcpMode()
                               ? tr("监听端口: %1").arg(m_portSpin->value())
                               : tr("波特率: %1")
                                     .arg(m_deviceConnected ? m_baudCombo->currentText()
                                                            : QStringLiteral("-"));

    m_statusBarDetail->setText(
        tr("%1 | %2 | 协议: %3 | 状态: %4")
            .arg(conn, detail, proto, m_runStatusLabel->text()));
}

QString MainWindow::formatSize(qint64 bytes) const
{
    if (bytes < 1024)
        return tr("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    return tr("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

QString MainWindow::formatDuration(int seconds) const
{
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg((seconds % 3600) / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}
