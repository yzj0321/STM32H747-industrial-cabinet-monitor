#include "LogPage.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QVBoxLayout>

LogPage::LogPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    connect(m_btnListen, &QPushButton::clicked, this, &LogPage::onListenClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &LogPage::onStopClicked);
    connect(m_btnRefresh, &QPushButton::clicked, this, &LogPage::onRefreshClicked);
    connect(m_btnSpace, &QPushButton::clicked, this, &LogPage::onSpaceClicked);
    connect(m_btnDownload, &QPushButton::clicked, this, &LogPage::onDownloadClicked);
    connect(m_btnSave, &QPushButton::clicked, this, &LogPage::onSaveClicked);

    connect(&m_client, &LogClient::listenStarted, this, &LogPage::onListenStarted);
    connect(&m_client, &LogClient::connected, this, &LogPage::onClientConnected);
    connect(&m_client, &LogClient::disconnected, this, &LogPage::onClientDisconnected);
    connect(&m_client, &LogClient::statReceived, this, &LogPage::onStat);
    connect(&m_client, &LogClient::spaceReceived, this, &LogPage::onSpace);
    connect(&m_client, &LogClient::listReceived, this, &LogPage::onList);
    connect(&m_client, &LogClient::downloadProgress, this, &LogPage::onDownloadProgress);
    connect(&m_client, &LogClient::downloadFinished, this, &LogPage::onDownloadFinished);
    connect(&m_client, &LogClient::downloadFailed, this, &LogPage::onDownloadFailed);
    connect(&m_client, &LogClient::logMessage, this, &LogPage::appendStatus);
    connect(&m_client, &LogClient::errorOccurred, this, [this](const QString &msg) {
        appendStatus(msg, QStringLiteral("ERROR"));
    });

    setListeningUi(false);
    setDeviceLinkedUi(false);
}

void LogPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    auto *connBox = new QGroupBox(tr("日志服务（上位机监听，同 OTA 模型）"), this);
    auto *connLay = new QHBoxLayout(connBox);

    m_portSpin = new QSpinBox(connBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(5001);

    m_btnListen = new QPushButton(tr("开始监听"), connBox);
    m_btnListen->setObjectName(QStringLiteral("primaryButton"));
    m_btnStop = new QPushButton(tr("停止"), connBox);
    m_btnRefresh = new QPushButton(tr("刷新列表"), connBox);
    m_btnSpace = new QPushButton(tr("查询 SD 容量"), connBox);
    m_btnDownload = new QPushButton(tr("获取选中文件"), connBox);
    m_btnDownload->setObjectName(QStringLiteral("primaryButton"));
    m_btnSave = new QPushButton(tr("另存为..."), connBox);

    m_connLabel = new QLabel(tr("未监听"), connBox);
    m_connLabel->setObjectName(QStringLiteral("metaLabel"));
    m_mountLabel = new QLabel(tr("SD: —"), connBox);
    m_mountLabel->setObjectName(QStringLiteral("metaLabel"));
    m_spaceLabel = new QLabel(tr("容量: —"), connBox);
    m_spaceLabel->setObjectName(QStringLiteral("metaLabel"));

    connLay->addWidget(new QLabel(tr("监听端口:"), connBox));
    connLay->addWidget(m_portSpin);
    connLay->addWidget(m_btnListen);
    connLay->addWidget(m_btnStop);
    connLay->addWidget(m_btnRefresh);
    connLay->addWidget(m_btnSpace);
    connLay->addWidget(m_btnDownload);
    connLay->addWidget(m_btnSave);
    connLay->addWidget(m_connLabel, 1);
    connLay->addWidget(m_mountLabel);
    connLay->addWidget(m_spaceLabel);

    m_ipHint = new QLabel(
        tr("说明：与 OTA 一样由上位机监听。板子会连到配置的主机 IP:5001（默认 192.168.1.10）。"
           "先点「开始监听」，再等板子连上 →「刷新列表」→「获取选中文件」。"),
        this);
    m_ipHint->setObjectName(QStringLiteral("metaLabel"));
    m_ipHint->setWordWrap(true);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftBox = new QGroupBox(tr("SD 日志文件 /log"), splitter);
    auto *leftLay = new QVBoxLayout(leftBox);
    m_fileList = new QListWidget(leftBox);
    m_fileList->setAlternatingRowColors(true);
    leftLay->addWidget(m_fileList);
    m_progress = new QProgressBar(leftBox);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    leftLay->addWidget(m_progress);

    auto *rightSplit = new QSplitter(Qt::Vertical, splitter);

    auto *previewBox = new QGroupBox(tr("日志预览"), rightSplit);
    auto *previewLay = new QVBoxLayout(previewBox);
    m_table = new QTableWidget(previewBox);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels(
        {tr("tick_ms"), tr("level"), tr("code"), tr("temp"), tr("humi"), tr("adc_mv"), tr("message")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_textView = new QPlainTextEdit(previewBox);
    m_textView->setReadOnly(true);
    m_textView->setVisible(false);
    previewLay->addWidget(m_table);
    previewLay->addWidget(m_textView);

    auto *statusBox = new QGroupBox(tr("通信状态"), rightSplit);
    auto *statusLay = new QVBoxLayout(statusBox);
    m_statusLog = new QPlainTextEdit(statusBox);
    m_statusLog->setReadOnly(true);
    m_statusLog->setMaximumBlockCount(800);
    statusLay->addWidget(m_statusLog);

    rightSplit->addWidget(previewBox);
    rightSplit->addWidget(statusBox);
    rightSplit->setStretchFactor(0, 3);
    rightSplit->setStretchFactor(1, 1);

    splitter->addWidget(leftBox);
    splitter->addWidget(rightSplit);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    root->addWidget(connBox);
    root->addWidget(m_ipHint);
    root->addWidget(splitter, 1);
}

void LogPage::setListeningUi(bool listening)
{
    m_btnListen->setEnabled(!listening);
    m_btnStop->setEnabled(listening);
    m_portSpin->setEnabled(!listening);
    if (!listening) {
        m_connLabel->setText(tr("未监听"));
        setDeviceLinkedUi(false);
    }
}

void LogPage::setDeviceLinkedUi(bool linked)
{
    m_btnRefresh->setEnabled(linked);
    m_btnSpace->setEnabled(linked);
    m_btnDownload->setEnabled(linked);
    if (linked)
        m_connLabel->setText(tr("板子已连接"));
    else if (m_client.isListening())
        m_connLabel->setText(tr("监听中，等待板子..."));
}

void LogPage::onListenClicked()
{
    m_client.startListen(static_cast<quint16>(m_portSpin->value()));
}

void LogPage::onStopClicked()
{
    m_client.stop();
    setListeningUi(false);
    m_mountLabel->setText(tr("SD: —"));
    m_spaceLabel->setText(tr("容量: —"));
    appendStatus(tr("已停止监听"));
}

void LogPage::onListenStarted(quint16 port, const QStringList &addrs)
{
    setListeningUi(true);
    setDeviceLinkedUi(false);
    if (!addrs.isEmpty()) {
        m_ipHint->setText(
            tr("正在监听 %1 — 板子应连接: %2:%1（固件里 BSP_W5500_OTA_HOST）")
                .arg(port)
                .arg(addrs.join(QStringLiteral(" / "))));
    }
}

void LogPage::onRefreshClicked()
{
    if (!m_client.isConnected()) {
        QMessageBox::information(this, tr("刷新"), tr("请先开始监听，并等待板子连上。"));
        return;
    }
    appendStatus(tr("请求文件列表..."));
    m_client.listFiles();
}

void LogPage::onSpaceClicked()
{
    if (!m_client.isConnected()) {
        QMessageBox::information(this, tr("容量"), tr("请先开始监听，并等待板子连上。"));
        return;
    }
    appendStatus(tr("查询 SDNAND 容量..."));
    m_client.querySpace();
}

void LogPage::onDownloadClicked()
{
    const QString name = selectedFileName();
    if (name.isEmpty()) {
        QMessageBox::information(this, tr("获取"), tr("请先在左侧选择一个日志文件。"));
        return;
    }
    appendStatus(tr("开始下载 %1").arg(name));
    m_progress->setValue(0);
    m_client.downloadFile(name);
}

void LogPage::onSaveClicked()
{
    if (m_lastData.isEmpty() || m_lastName.isEmpty()) {
        QMessageBox::information(this, tr("另存为"), tr("请先获取一个文件。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存日志"), m_lastName,
        tr("CSV/文本 (*.csv *.txt);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("保存失败"), f.errorString());
        return;
    }
    f.write(m_lastData);
    f.close();
    appendStatus(tr("已保存到 %1").arg(path));
}

void LogPage::onClientConnected()
{
    setDeviceLinkedUi(true);
    m_client.listFiles();
    m_client.querySpace();
}

void LogPage::onClientDisconnected()
{
    setDeviceLinkedUi(false);
    m_mountLabel->setText(tr("SD: —"));
    m_spaceLabel->setText(tr("容量: —"));
}

void LogPage::onStat(bool mounted)
{
    m_mountLabel->setText(mounted ? tr("SD: 已挂载") : tr("SD: 未就绪"));
    appendStatus(mounted ? tr("设备 SD 已挂载") : tr("设备 SD 未就绪"),
                 mounted ? QStringLiteral("INFO") : QStringLiteral("WARN"));
}

void LogPage::onSpace(const LogSpaceInfo &space)
{
    const quint64 used = (space.totalBytes > space.freeBytes)
                             ? (space.totalBytes - space.freeBytes)
                             : 0;
    m_spaceLabel->setText(
        tr("容量: 芯片 %1 / 文件系统 %2 / 已用 %3 / 剩余 %4")
            .arg(formatBytes(space.cardBytes),
                 formatBytes(space.totalBytes),
                 formatBytes(used),
                 formatBytes(space.freeBytes)));
    appendStatus(
        tr("SDNAND 芯片=%1, 文件系统=%2, 已用=%3, 剩余=%4")
            .arg(formatBytes(space.cardBytes),
                 formatBytes(space.totalBytes),
                 formatBytes(used),
                 formatBytes(space.freeBytes)));
}

void LogPage::onList(const QVector<LogFileInfo> &files)
{
    m_fileList->clear();
    for (const LogFileInfo &info : files) {
        auto *item = new QListWidgetItem(
            tr("%1    (%2 字节)").arg(info.name).arg(info.size), m_fileList);
        item->setData(Qt::UserRole, info.name);
        item->setData(Qt::UserRole + 1, info.size);
    }
    appendStatus(tr("共 %1 个日志文件").arg(files.size()));
    if (!files.isEmpty())
        m_fileList->setCurrentRow(0);
}

void LogPage::onDownloadProgress(qint64 received, qint64 total)
{
    if (total <= 0) {
        m_progress->setValue(0);
        return;
    }
    m_progress->setValue(static_cast<int>(received * 1000 / total));
}

void LogPage::onDownloadFinished(const QString &name, const QByteArray &data)
{
    m_lastName = name;
    m_lastData = data;
    m_progress->setValue(1000);
    appendStatus(tr("下载完成: %1 (%2 字节)").arg(name).arg(data.size()));
    showCsvOrText(data);
}

void LogPage::onDownloadFailed(const QString &message)
{
    m_progress->setValue(0);
    appendStatus(message, QStringLiteral("ERROR"));
    QMessageBox::warning(this, tr("下载失败"), message);
}

void LogPage::showCsvOrText(const QByteArray &data)
{
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                         Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        m_table->setVisible(false);
        m_textView->setVisible(true);
        m_textView->setPlainText(tr("(空文件)"));
        return;
    }

    const bool looksCsv = lines.first().contains(QLatin1Char(','))
                          || (lines.size() > 1 && lines.at(1).contains(QLatin1Char(',')));
    if (!looksCsv) {
        m_table->setVisible(false);
        m_textView->setVisible(true);
        m_textView->setPlainText(text);
        return;
    }

    m_textView->setVisible(false);
    m_table->setVisible(true);
    m_table->setRowCount(0);

    int start = 0;
    if (lines.first().contains(QLatin1String("tick_ms")))
        start = 1;

    for (int i = start; i < lines.size(); ++i) {
        const QStringList cols = lines.at(i).split(QLatin1Char(','));
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        for (int c = 0; c < 7; ++c)
            m_table->setItem(row, c, new QTableWidgetItem(cols.value(c).trimmed()));
    }
}

QString LogPage::selectedFileName() const
{
    const QListWidgetItem *item = m_fileList->currentItem();
    if (item == nullptr)
        return {};
    return item->data(Qt::UserRole).toString();
}

QString LogPage::formatBytes(quint64 bytes)
{
    const double kb = static_cast<double>(bytes) / 1024.0;
    const double mb = kb / 1024.0;
    const double gb = mb / 1024.0;
    if (gb >= 1.0)
        return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
    if (mb >= 1.0)
        return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
    if (kb >= 1.0)
        return QStringLiteral("%1 KB").arg(kb, 0, 'f', 0);
    return QStringLiteral("%1 B").arg(bytes);
}

void LogPage::appendStatus(const QString &text, const QString &level)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    m_statusLog->appendPlainText(QStringLiteral("[%1][%2] %3").arg(ts, level, text));
}
