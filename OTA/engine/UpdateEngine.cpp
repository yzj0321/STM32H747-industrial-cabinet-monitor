#include "UpdateEngine.h"
#include "CrcUtils.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QList>
#include <QRandomGenerator>
#include <QSet>
#include <QThread>

UpdateEngine::UpdateEngine(OtaTransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
}

void UpdateEngine::setTransport(OtaTransport *transport)
{
    m_transport = transport;
}

void UpdateEngine::setFirmwarePath(const QString &path)
{
    m_firmwarePath = path;
}

void UpdateEngine::setFlashAddress(quint32 addr)
{
    /* 仅作缓存；正式擦写前必须以 GET_INFO.inactive 覆盖 */
    m_flashAddress = addr;
}

bool UpdateEngine::applyInactiveTargetFromDevice(QString *errOut)
{
    if (!m_deviceInfo.valid) {
        if (errOut)
            *errOut = tr("设备信息无效，请先 GET_INFO");
        return false;
    }

    const bool cm4 = (m_otaCore == 1U);
    const quint32 slotABase =
        cm4 ? OtaProtocol::kCm4SlotABase : OtaProtocol::kSlotABase;
    const quint32 slotBBase =
        cm4 ? OtaProtocol::kCm4SlotBBase : OtaProtocol::kSlotBBase;

    if (cm4) {
        if (!OtaProtocol::DeviceInfo::isValidCm4SlotBase(m_deviceInfo.appAddress)) {
            if (errOut) {
                *errOut = tr("GET_INFO.app_address=0x%1 非法（CM4 仅允许 A=0x%2 / B=0x%3)")
                              .arg(m_deviceInfo.appAddress, 8, 16, QChar('0'))
                              .arg(OtaProtocol::kCm4SlotABase, 8, 16, QChar('0'))
                              .arg(OtaProtocol::kCm4SlotBBase, 8, 16, QChar('0'));
            }
            return false;
        }
    } else if (!OtaProtocol::DeviceInfo::isValidSlotBase(m_deviceInfo.appAddress)) {
        if (errOut) {
            *errOut = tr("GET_INFO.app_address=0x%1 非法（仅允许 A=0x%2 / B=0x%3)")
                          .arg(m_deviceInfo.appAddress, 8, 16, QChar('0'))
                          .arg(OtaProtocol::kSlotABase, 8, 16, QChar('0'))
                          .arg(OtaProtocol::kSlotBBase, 8, 16, QChar('0'));
        }
        return false;
    }

    /* 与 MCU 一致：优先用 inactive 索引推导基址，避免只信裸地址字段 */
    quint32 expected = (m_deviceInfo.inactiveSlot == 0) ? slotABase : slotBBase;
    if (m_deviceInfo.runningSlot <= 1U && m_deviceInfo.inactiveSlot <= 1U) {
        if (m_deviceInfo.appAddress != expected) {
            emit logMessage(
                tr("GET_INFO 地址与 inactive 槽不一致: addr=0x%1 inactive=%2，改用 inactive 基址 0x%3")
                    .arg(m_deviceInfo.appAddress, 8, 16, QChar('0'))
                    .arg(m_deviceInfo.inactiveSlotName())
                    .arg(expected, 8, 16, QChar('0')),
                QStringLiteral("WARN"));
        }
        m_flashAddress = expected;
        m_deviceInfo.appAddress = expected;
    } else {
        m_flashAddress = m_deviceInfo.appAddress;
    }
    return true;
}

void UpdateEngine::warnFirmwareSlotMismatch()
{
    if (m_deviceInfo.inactiveSlot > 1U || m_firmwarePath.isEmpty())
        return;

    const QString name = QFileInfo(m_firmwarePath).fileName().toLower();
    const bool cm4 = (m_otaCore == 1U);
    const bool looksA = name.contains(QStringLiteral("slot_a"))
                        || name.contains(QStringLiteral("_a.bin"))
                        || name.contains(QStringLiteral("cm7_a"))
                        || name.contains(QStringLiteral("cm4_a"))
                        || name.contains(QStringLiteral("gpio_cm4_a"));
    const bool looksB = name.contains(QStringLiteral("slot_b"))
                        || name.contains(QStringLiteral("_b.bin"))
                        || name.contains(QStringLiteral("cm7_b"))
                        || name.contains(QStringLiteral("cm4_b"))
                        || name.contains(QStringLiteral("gpio_cm4_b"));
    const bool needA = (m_deviceInfo.inactiveSlot == 0);

    if ((needA && looksB) || (!needA && looksA)) {
        emit logMessage(
            tr("固件文件可能与目标槽不匹配: 目标=%1 Slot %2，文件=%3（请使用 %4）")
                .arg(cm4 ? QStringLiteral("CM4") : QStringLiteral("CM7"))
                .arg(m_deviceInfo.inactiveSlotName())
                .arg(QFileInfo(m_firmwarePath).fileName())
                .arg(m_deviceInfo.suggestedFirmware(m_otaCore)),
            QStringLiteral("WARN"));
    }
}

void UpdateEngine::setChunkSize(int size)
{
    m_chunkSize = qBound(OtaProtocol::kDefaultChunkSize, size, OtaProtocol::kMaxPayload - 12);
}

void UpdateEngine::setMaxRetries(int retries)
{
    m_maxRetries = qBound(1, retries, 20);
}

void UpdateEngine::setDebugEraseSize(quint32 bytes)
{
    m_debugEraseSize = qMax<quint32>(4096U, bytes);
}

void UpdateEngine::setDebugPreSendDelayMs(int ms)
{
    m_debugPreSendDelayMs = qMax(0, ms);
}

void UpdateEngine::setDebugSingleShot(bool singleShot)
{
    m_debugSingleShot = singleShot;
}

void UpdateEngine::setOtaCore(quint8 core)
{
    m_otaCore = (core == 1U) ? 1U : 0U;
}

bool UpdateEngine::loadFirmware()
{
    m_firmware.clear();
    m_firmwareSize = 0;
    m_imageCrc32 = 0;

    if (m_firmwarePath.isEmpty()) {
        emit logMessage(tr("No firmware file selected"), QStringLiteral("ERROR"));
        return false;
    }

    QFile file(m_firmwarePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("Cannot open firmware: %1").arg(file.errorString()),
                        QStringLiteral("ERROR"));
        return false;
    }

    m_firmware = file.readAll();
    file.close();
    m_firmwareSize = m_firmware.size();
    if (m_firmwareSize <= 0) {
        emit logMessage(tr("Firmware file is empty"), QStringLiteral("ERROR"));
        return false;
    }

    m_imageCrc32 = CrcUtils::crc32(m_firmware);
    emit logMessage(tr("Loaded firmware %1 (%2 bytes, CRC32=0x%3)")
                        .arg(m_firmwarePath)
                        .arg(m_firmwareSize)
                        .arg(m_imageCrc32, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));
    return true;
}

void UpdateEngine::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void UpdateEngine::refreshDeviceInfo()
{
    if (!m_transport) {
        emit logMessage(tr("内部错误：传输层未设置"), QStringLiteral("ERROR"));
        return;
    }

    if (!m_transport->isOpen()) {
        emit logMessage(tr("设备未连接，无法发送 GET_INFO"), QStringLiteral("WARN"));
        return;
    }

    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中，请稍后再试"), QStringLiteral("WARN"));
        return;
    }

    emit logMessage(tr("正在执行 GET_INFO (core=%1)...")
                        .arg(m_otaCore == 1U ? QStringLiteral("CM4") : QStringLiteral("CM7")),
                    QStringLiteral("INFO"));

    const QByteArray getInfoPl = OtaProtocol::payloadGetInfo(m_otaCore);
    OtaProtocol::Frame resp;
    const bool ok = m_debugSingleShot
                        ? sendOnce(OtaProtocol::Cmd::GetInfo, getInfoPl, resp, 8000)
                        : sendWithRetry(OtaProtocol::Cmd::GetInfo, getInfoPl, resp, 8000);
    if (!ok) {
        emit logMessage(tr("GET_INFO 失败"), QStringLiteral("ERROR"));
    } else if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        emit logMessage(tr("GET_INFO: %1")
                            .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))),
                        QStringLiteral("ERROR"));
    } else {
        m_deviceInfo = OtaProtocol::parseDeviceInfo(resp);
        if (m_deviceInfo.valid) {
            QString err;
            if (!applyInactiveTargetFromDevice(&err)) {
                emit logMessage(err, QStringLiteral("ERROR"));
            } else {
                setState(State::Connected);
                emit deviceInfoUpdated(m_deviceInfo);
                emit logMessage(tr("设备: %1, Boot %2, 协议 %3, Flash %4 KB")
                                    .arg(m_deviceInfo.deviceType)
                                    .arg(m_deviceInfo.bootVersion)
                                    .arg(m_deviceInfo.protocolVersion)
                                    .arg(m_deviceInfo.flashSize / 1024),
                                QStringLiteral("INFO"));
                if (m_deviceInfo.runningSlot <= 1U) {
                    emit logMessage(
                        tr("当前分区: Slot %1 | 升级目标: Slot %2 @ 0x%3 | 建议固件: %4")
                            .arg(m_deviceInfo.runningSlotName())
                            .arg(m_deviceInfo.inactiveSlotName())
                            .arg(m_flashAddress, 8, 16, QChar('0'))
                            .arg(m_deviceInfo.suggestedFirmware(m_otaCore)),
                        QStringLiteral("INFO"));
                } else {
                    emit logMessage(tr("OTA 目标地址(GET_INFO inactive): 0x%1")
                                        .arg(m_flashAddress, 8, 16, QChar('0')),
                                    QStringLiteral("INFO"));
                }
            }
        }
    }

    m_busy.storeRelease(0);
    emit operationFinished();
}

void UpdateEngine::startGetInfoStressTest(int count, int intervalMs)
{
    if (!m_transport) {
        emit logMessage(tr("内部错误：传输层未设置"), QStringLiteral("ERROR"));
        return;
    }

    if (!m_transport->isOpen()) {
        emit logMessage(tr("设备未连接，无法执行 GET_INFO 压力测试"), QStringLiteral("WARN"));
        return;
    }

    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中，请稍后再试"), QStringLiteral("WARN"));
        return;
    }

    m_stopRequested.storeRelease(0);
    count = qBound(1, count, 100);
    intervalMs = qMax(0, intervalMs);

    emit logMessage(tr("=== GET_INFO 压力测试: 连发 %1 次, 间隔 %2 ms (不等 ACK 连续发送) ===")
                        .arg(count)
                        .arg(intervalMs),
                    QStringLiteral("INFO"));
    emit logMessage(tr("提示: 间隔=0 可复现 CIPSEND 竞态; 观察 MCU 是否每条都有 SOIP cmd=0x02"),
                    QStringLiteral("INFO"));

    m_transport->clearDeferredResponses();
    m_transport->pumpReceive(100);

    QList<quint16> sentSeqs;
    sentSeqs.reserve(count);

    QElapsedTimer burstTimer;
    burstTimer.start();

    int sendFail = 0;
    for (int i = 0; i < count; ++i) {
        if (m_stopRequested.loadAcquire()) {
            emit logMessage(tr("压力测试已取消 (已发 %1/%2)").arg(sentSeqs.size()).arg(count),
                            QStringLiteral("WARN"));
            break;
        }

        quint16 seq = 0;
        if (!m_transport->sendCommand(OtaProtocol::Cmd::GetInfo,
                                    OtaProtocol::payloadGetInfo(m_otaCore),
                                    seq)) {
            sendFail++;
            emit logMessage(tr("GET_INFO 发送失败 #%1").arg(i + 1), QStringLiteral("ERROR"));
            break;
        }
        sentSeqs.append(seq);

        if (intervalMs > 0 && i + 1 < count)
            QThread::msleep(static_cast<unsigned long>(intervalMs));
    }

    const qint64 burstMs = burstTimer.elapsed();
    emit logMessage(tr("连发完成: 已发送 %1 包, 耗时 %2 ms, 失败 %3 次")
                        .arg(sentSeqs.size())
                        .arg(burstMs)
                        .arg(sendFail),
                    QStringLiteral("INFO"));

    if (sentSeqs.isEmpty()) {
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    const int collectTimeoutMs = qMax(15000, count * qMax(500, intervalMs + 800));
    emit logMessage(tr("开始收集应答, 超时 %1 ms ...").arg(collectTimeoutMs),
                    QStringLiteral("INFO"));

    QSet<quint16> ackedSeqs;
    int okCount = 0;
    int errCount = 0;

    QElapsedTimer collectTimer;
    collectTimer.start();
    while (collectTimer.elapsed() < collectTimeoutMs) {
        if (m_stopRequested.loadAcquire())
            break;

        m_transport->pumpReceive(20);

        for (quint16 seq : sentSeqs) {
            if (ackedSeqs.contains(seq))
                continue;

            if (auto frame = m_transport->takeDeferredResponse(OtaProtocol::Cmd::GetInfo, seq)) {
                ackedSeqs.insert(seq);
                const auto st = OtaProtocol::parseResponseStatus(*frame);
                if (st == OtaProtocol::Status::Ok) {
                    okCount++;
                } else {
                    errCount++;
                    emit logMessage(tr("GET_INFO seq %1 应答异常: %2")
                                        .arg(seq)
                                        .arg(OtaProtocol::statusToString(st)),
                                    QStringLiteral("WARN"));
                }
            }
        }

        if (ackedSeqs.size() >= sentSeqs.size())
            break;
    }

    QStringList missing;
    for (quint16 seq : sentSeqs) {
        if (!ackedSeqs.contains(seq))
            missing << QString::number(seq);
    }

    const int lost = missing.size();
    emit logMessage(tr("压力测试结果: 发送 %1, 收到应答 %2 (OK=%3, ERR=%4, 丢失=%5), 收集耗时 %6 ms")
                        .arg(sentSeqs.size())
                        .arg(ackedSeqs.size())
                        .arg(okCount)
                        .arg(errCount)
                        .arg(lost)
                        .arg(collectTimer.elapsed()),
                    lost > 0 ? QStringLiteral("WARN") : QStringLiteral("INFO"));

    if (!missing.isEmpty()) {
        const QString preview =
            missing.size() <= 12 ? missing.join(QStringLiteral(", "))
                                 : missing.mid(0, 12).join(QStringLiteral(", "))
                                       + tr(" ... 等 %1 个").arg(missing.size());
        emit logMessage(tr("未收到应答的 seq: %1").arg(preview), QStringLiteral("WARN"));
        emit logMessage(tr("若 MCU 日志有 +IPD 但缺少对应 SOIP cmd=0x02, 说明存在发太快丢包"),
                        QStringLiteral("WARN"));
    } else {
        emit logMessage(tr("全部应答已收到 — 当前间隔下链路正常"), QStringLiteral("INFO"));
    }

    m_busy.storeRelease(0);
    emit operationFinished();
}

bool UpdateEngine::ensureSession()
{
    if (m_sessionToken != 0)
        return true;

    m_hostNonce = QRandomGenerator::global()->generate();
    OtaProtocol::Frame resp;
    if (!sendWithRetry(OtaProtocol::Cmd::BeginSession,
                       OtaProtocol::payloadBeginSession(m_hostNonce), resp)) {
        return false;
    }

    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        emit logMessage(tr("BEGIN_SESSION: %1")
                            .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))),
                        QStringLiteral("ERROR"));
        return false;
    }

    m_sessionToken = OtaProtocol::parseSessionToken(resp);
    if (m_sessionToken == 0) {
        emit logMessage(tr("Invalid session token"), QStringLiteral("ERROR"));
        return false;
    }

    emit sessionTokenChanged(m_sessionToken);
    emit logMessage(tr("Session token 0x%1").arg(m_sessionToken, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));
    return true;
}

void UpdateEngine::resetTransferState()
{
    m_transferReady = false;
    m_dataTransferDone = false;
    emit transferReadyChanged(false);
    emit dataTransferDoneChanged(false);
}

void UpdateEngine::clearSession()
{
    if (m_sessionToken != 0) {
        emit logMessage(tr("已清除 PC 端 Session token 0x%1（MCU 状态不变，需重连或重新 ③④）")
                            .arg(m_sessionToken, 8, 16, QChar('0')),
                        QStringLiteral("INFO"));
    }
    m_sessionToken = 0;
    m_hostNonce = 0;
    resetTransferState();
    emit sessionTokenChanged(0);
}

void UpdateEngine::startBeginSessionTest(bool forceNewSession)
{
    if (!m_transport || !m_transport->isOpen()) {
        emit logMessage(tr("设备未连接，无法测试 BEGIN_SESSION"), QStringLiteral("ERROR"));
        emit operationFinished();
        return;
    }
    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中，请稍后再试"), QStringLiteral("WARN"));
        return;
    }

    if (forceNewSession)
        clearSession();

    m_hostNonce = QRandomGenerator::global()->generate();
    emit logMessage(tr("[单步] 发送 BEGIN_SESSION, host_nonce=0x%1, 超时=%2 ms ...")
                        .arg(m_hostNonce, 8, 16, QChar('0'))
                        .arg(m_transport->responseTimeoutMs()),
                    QStringLiteral("INFO"));

    QElapsedTimer timer;
    timer.start();

    OtaProtocol::Frame resp;
    const bool ok = m_debugSingleShot
                        ? sendOnce(OtaProtocol::Cmd::BeginSession,
                                   OtaProtocol::payloadBeginSession(m_hostNonce), resp)
                        : sendWithRetry(OtaProtocol::Cmd::BeginSession,
                                        OtaProtocol::payloadBeginSession(m_hostNonce), resp);
    const qint64 elapsed = timer.elapsed();

    if (!ok) {
        emit logMessage(tr("[单步] BEGIN_SESSION 失败，耗时 %1 ms").arg(elapsed),
                        QStringLiteral("ERROR"));
        setState(State::Failed);
    } else {
        const auto st = OtaProtocol::parseResponseStatus(resp);
        if (st != OtaProtocol::Status::Ok) {
            emit logMessage(tr("[单步] BEGIN_SESSION 应答: %1, 耗时 %2 ms")
                                .arg(OtaProtocol::statusToString(st))
                                .arg(elapsed),
                            QStringLiteral("ERROR"));
            setState(State::Failed);
        } else {
            m_sessionToken = OtaProtocol::parseSessionToken(resp);
            emit sessionTokenChanged(m_sessionToken);
            setState(State::Connected);
            emit logMessage(tr("[单步] BEGIN_SESSION 成功, token=0x%1, 耗时 %2 ms")
                                .arg(m_sessionToken, 8, 16, QChar('0'))
                                .arg(elapsed),
                            QStringLiteral("INFO"));
        }
    }

    m_busy.storeRelease(0);
    emit operationFinished();
}

void UpdateEngine::startEraseOnlyTest()
{
    if (!m_transport || !m_transport->isOpen()) {
        emit logMessage(tr("设备未连接，无法测试 ERASE"), QStringLiteral("ERROR"));
        emit operationFinished();
        return;
    }
    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中，请稍后再试"), QStringLiteral("WARN"));
        return;
    }

    resetTransferState();

    if (m_sessionToken == 0) {
        emit logMessage(tr("[单步] 请先执行 BEGIN_SESSION 获取 Session token"), QStringLiteral("ERROR"));
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    /* 擦除前强制 GET_INFO，禁止手填/旧缓存地址导致误擦 B */
    {
        emit logMessage(tr("[单步] ERASE 前先 GET_INFO 确认 inactive 目标槽 ..."),
                        QStringLiteral("INFO"));
        OtaProtocol::Frame infoResp;
        const QByteArray getInfoPl = OtaProtocol::payloadGetInfo(m_otaCore);
        const bool infoOk = m_debugSingleShot
                                ? sendOnce(OtaProtocol::Cmd::GetInfo, getInfoPl, infoResp, 8000)
                                : sendWithRetry(OtaProtocol::Cmd::GetInfo, getInfoPl, infoResp, 8000);
        if (!infoOk || OtaProtocol::parseResponseStatus(infoResp) != OtaProtocol::Status::Ok) {
            emit logMessage(tr("[单步] GET_INFO 失败，取消 ERASE"), QStringLiteral("ERROR"));
            m_busy.storeRelease(0);
            emit operationFinished();
            return;
        }
        m_deviceInfo = OtaProtocol::parseDeviceInfo(infoResp);
        QString err;
        if (!applyInactiveTargetFromDevice(&err)) {
            emit logMessage(err, QStringLiteral("ERROR"));
            m_busy.storeRelease(0);
            emit operationFinished();
            return;
        }
        emit deviceInfoUpdated(m_deviceInfo);
        emit logMessage(tr("[单步] 确认擦除目标 Slot %1 @ 0x%2")
                            .arg(m_deviceInfo.inactiveSlotName())
                            .arg(m_flashAddress, 8, 16, QChar('0')),
                        QStringLiteral("INFO"));
    }

    quint32 eraseSize = 0;
    if (!m_firmwarePath.isEmpty() && loadFirmware()) {
        eraseSize = static_cast<quint32>(m_firmwareSize);
    } else {
        eraseSize = m_debugEraseSize;
        emit logMessage(tr("[单步] 未选固件，使用调试擦除大小 %1 bytes").arg(eraseSize),
                        QStringLiteral("WARN"));
    }

    emit logMessage(tr("[单步] 发送 ERASE @ 0x%1, size=%2, token=0x%3, 超时=120s ...")
                        .arg(m_flashAddress, 8, 16, QChar('0'))
                        .arg(eraseSize)
                        .arg(m_sessionToken, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));

    QElapsedTimer timer;
    timer.start();

    OtaProtocol::Frame resp;
    const QByteArray pl =
        OtaProtocol::payloadErase(m_sessionToken, m_flashAddress, eraseSize);
    const bool ok = m_debugSingleShot
                        ? sendOnce(OtaProtocol::Cmd::Erase, pl, resp, 120000)
                        : sendWithRetry(OtaProtocol::Cmd::Erase, pl, resp, 120000);
    const qint64 elapsed = timer.elapsed();

    if (!ok) {
        emit logMessage(tr("[单步] ERASE 失败，耗时 %1 ms").arg(elapsed), QStringLiteral("ERROR"));
        setState(State::Failed);
    } else {
        const auto st = OtaProtocol::parseResponseStatus(resp);
        if (st != OtaProtocol::Status::Ok) {
            emit logMessage(tr("[单步] ERASE 应答: %1, 耗时 %2 ms")
                                .arg(OtaProtocol::statusToString(st))
                                .arg(elapsed),
                            QStringLiteral("ERROR"));
            setState(State::Failed);
        } else {
            emit logMessage(tr("[单步] ERASE 成功，耗时 %1 ms (%2 s)")
                                .arg(elapsed)
                                .arg(elapsed / 1000.0, 0, 'f', 2),
                            QStringLiteral("INFO"));
            emit logMessage(tr("请尽快点击 ⑤ START_TRANSFER（建议 5 秒内）"),
                            QStringLiteral("WARN"));
            setState(State::Connected);
        }
    }

    m_busy.storeRelease(0);
    emit operationFinished();
}

void UpdateEngine::startStartTransferTest()
{
    if (!m_transport || !m_transport->isOpen()) {
        emit logMessage(tr("设备未连接，无法测试 START_TRANSFER"), QStringLiteral("ERROR"));
        emit operationFinished();
        return;
    }
    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中，请稍后再试"), QStringLiteral("WARN"));
        return;
    }

    resetTransferState();

    if (m_sessionToken == 0) {
        emit logMessage(tr("[单步] 请先完成 ③ BEGIN 与 ④ ERASE"), QStringLiteral("ERROR"));
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    if (!loadFirmware()) {
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    m_transport->clearDeferredResponses();

    emit logMessage(tr("=== [⑤ START_TRANSFER] size=%1, CRC32=0x%2 @ 0x%3 ===")
                        .arg(m_firmwareSize)
                        .arg(m_imageCrc32, 8, 16, QChar('0'))
                        .arg(m_flashAddress, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));

    OtaProtocol::Frame resp;
    QElapsedTimer timer;
    timer.start();

    const bool ok = sendOnce(OtaProtocol::Cmd::StartTransfer,
                             OtaProtocol::payloadStartTransfer(m_sessionToken, m_flashAddress,
                                                               static_cast<quint32>(m_firmwareSize),
                                                               m_imageCrc32),
                             resp, OtaProtocol::kStartTransferTimeoutMs, false);

    if (!ok || OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        const auto st = OtaProtocol::parseResponseStatus(resp);
        emit logMessage(tr("[单步] START_TRANSFER 失败 (%1), 耗时 %2 ms")
                            .arg(ok ? OtaProtocol::statusToString(st) : tr("无应答"))
                            .arg(timer.elapsed()),
                        QStringLiteral("ERROR"));
        setState(State::Failed);
    } else {
        m_transferReady = true;
        emit transferReadyChanged(true);
        setState(State::Connected);
        emit logMessage(tr("[单步] START_TRANSFER 成功, 耗时 %1 ms，可点击 ⑥ 传固件")
                            .arg(timer.elapsed()),
                        QStringLiteral("INFO"));
    }

    m_busy.storeRelease(0);
    emit operationFinished();
}

bool UpdateEngine::sendOnce(OtaProtocol::Cmd cmd, const QByteArray &payload,
                            OtaProtocol::Frame &resp, int timeoutMs, bool applyPreDelay)
{
    if (!m_transport)
        return false;

    int perTryTimeout = timeoutMs;
    if (perTryTimeout < 0) {
        if (cmd == OtaProtocol::Cmd::Data)
            perTryTimeout = 90000;
        else if (cmd == OtaProtocol::Cmd::Erase)
            perTryTimeout = 120000;
        else
            perTryTimeout = m_transport->responseTimeoutMs();
    }

    if (applyPreDelay && m_debugPreSendDelayMs > 0) {
        emit logMessage(tr("发送前延时 %1 ms ...").arg(m_debugPreSendDelayMs),
                        QStringLiteral("INFO"));
        QThread::msleep(static_cast<unsigned long>(m_debugPreSendDelayMs));
    }

    QElapsedTimer timer;
    timer.start();
    const bool ok = m_transport->transact(cmd, payload, resp, perTryTimeout);
    const qint64 elapsed = timer.elapsed();

    if (!ok) {
        emit logMessage(tr("%1 无有效应答 (超时设定 %2 ms, 实际等待 %3 ms)")
                            .arg(OtaProtocol::cmdToString(cmd))
                            .arg(perTryTimeout)
                            .arg(elapsed),
                        QStringLiteral("ERROR"));
        return false;
    }

    const auto st = OtaProtocol::parseResponseStatus(resp);
    if (st != OtaProtocol::Status::Ok) {
        emit logMessage(tr("%1 应答状态: %2, RTT %3 ms")
                            .arg(OtaProtocol::cmdToString(cmd),
                                 OtaProtocol::statusToString(st))
                            .arg(elapsed),
                        QStringLiteral("WARN"));
        return false;
    }
    return true;
}

bool UpdateEngine::sendWithRetry(OtaProtocol::Cmd cmd, const QByteArray &payload,
                                 OtaProtocol::Frame &resp, int timeoutMs)
{
    const int retries = (cmd == OtaProtocol::Cmd::GetInfo) ? 3 : m_maxRetries;
    int perTryTimeout = timeoutMs;

    if (perTryTimeout < 0) {
        if (cmd == OtaProtocol::Cmd::Data) {
            perTryTimeout = 90000;
        } else if (cmd == OtaProtocol::Cmd::Erase) {
            perTryTimeout = 120000;
        } else {
            perTryTimeout = m_transport->responseTimeoutMs();
        }
    }

    for (int attempt = 1; attempt <= retries; ++attempt) {
        if (m_stopRequested.loadAcquire())
            return false;

        if (m_transport->transact(cmd, payload, resp, perTryTimeout)) {
            const auto st = OtaProtocol::parseResponseStatus(resp);
            if (st == OtaProtocol::Status::Ok)
                return true;
            if (st == OtaProtocol::Status::ErrBusy) {
                emit warningRaised(tr("Device busy, retry %1/%2").arg(attempt).arg(m_maxRetries));
                QThread::msleep(50 * attempt);
                continue;
            }
            emit logMessage(tr("%1 -> %2")
                                .arg(OtaProtocol::cmdToString(cmd),
                                     OtaProtocol::statusToString(st)),
                            QStringLiteral("WARN"));
            if (st == OtaProtocol::Status::ErrAborted)
                return false;
        } else {
            emit warningRaised(tr("Comm retry %1/%2 for %3")
                                   .arg(attempt)
                                   .arg(retries)
                                   .arg(OtaProtocol::cmdToString(cmd)));
        }
        QThread::msleep(30 * attempt);
    }
    return false;
}

bool UpdateEngine::stoppedOrDisconnected(const QString &stage)
{
    if (m_stopRequested.loadAcquire()) {
        abortUpgrade();
        return true;
    }
    if (!m_transport || !m_transport->isOpen()) {
        fail(tr("%1 失败：连接已断开").arg(stage));
        return true;
    }
    return false;
}

bool UpdateEngine::doGetInfo()
{
    emit logMessage(tr("① GET_INFO ..."), QStringLiteral("INFO"));
    OtaProtocol::Frame resp;
    const QByteArray getInfoPl = OtaProtocol::payloadGetInfo(m_otaCore);
    if (!sendWithRetry(OtaProtocol::Cmd::GetInfo, getInfoPl, resp, 8000)) {
        fail(tr("GET_INFO 失败"));
        return false;
    }
    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        fail(tr("GET_INFO: %1")
                 .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))));
        return false;
    }

    m_deviceInfo = OtaProtocol::parseDeviceInfo(resp);
    if (!m_deviceInfo.valid) {
        fail(tr("GET_INFO 应答无效"));
        return false;
    }

    QString err;
    if (!applyInactiveTargetFromDevice(&err)) {
        fail(err);
        return false;
    }

    warnFirmwareSlotMismatch();

    setState(State::Connected);
    emit deviceInfoUpdated(m_deviceInfo);
    emit logMessage(tr("设备: %1, Boot %2 | 运行 Slot %3 → 擦写 Slot %4 @ 0x%5")
                        .arg(m_deviceInfo.deviceType)
                        .arg(m_deviceInfo.bootVersion)
                        .arg(m_deviceInfo.runningSlotName())
                        .arg(m_deviceInfo.inactiveSlotName())
                        .arg(m_flashAddress, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));
    return true;
}

bool UpdateEngine::doBeginSession()
{
    clearSession();
    m_hostNonce = QRandomGenerator::global()->generate();
    emit logMessage(tr("② BEGIN_SESSION, nonce=0x%1 ...")
                        .arg(m_hostNonce, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));

    OtaProtocol::Frame resp;
    if (!sendWithRetry(OtaProtocol::Cmd::BeginSession,
                       OtaProtocol::payloadBeginSession(m_hostNonce), resp)) {
        fail(tr("BEGIN_SESSION 失败"));
        return false;
    }
    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        fail(tr("BEGIN_SESSION: %1")
                 .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))));
        return false;
    }

    m_sessionToken = OtaProtocol::parseSessionToken(resp);
    if (m_sessionToken == 0) {
        fail(tr("无效 Session token"));
        return false;
    }
    emit sessionTokenChanged(m_sessionToken);
    emit logMessage(tr("Session token 0x%1").arg(m_sessionToken, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));
    return true;
}

bool UpdateEngine::doErase()
{
    const quint32 eraseSize = static_cast<quint32>(m_firmwareSize);
    setState(State::Erasing);
    emit logMessage(tr("③ ERASE inactive Slot %1 @ 0x%2, size=%3 ...")
                        .arg(m_deviceInfo.inactiveSlotName())
                        .arg(m_flashAddress, 8, 16, QChar('0'))
                        .arg(eraseSize),
                    QStringLiteral("INFO"));

    OtaProtocol::Frame resp;
    QElapsedTimer timer;
    timer.start();
    if (!sendWithRetry(OtaProtocol::Cmd::Erase,
                       OtaProtocol::payloadErase(m_sessionToken, m_flashAddress, eraseSize),
                       resp, 120000)) {
        fail(tr("ERASE 失败"));
        return false;
    }
    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        fail(tr("ERASE: %1")
                 .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))));
        return false;
    }

    emit logMessage(tr("ERASE 成功，耗时 %1 ms").arg(timer.elapsed()), QStringLiteral("INFO"));
    return true;
}

bool UpdateEngine::doStartTransfer()
{
    resetTransferState();
    m_transport->clearDeferredResponses();
    emit logMessage(tr("④ START_TRANSFER size=%1, CRC32=0x%2 @ 0x%3 ...")
                        .arg(m_firmwareSize)
                        .arg(m_imageCrc32, 8, 16, QChar('0'))
                        .arg(m_flashAddress, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));

    OtaProtocol::Frame resp;
    if (!sendWithRetry(OtaProtocol::Cmd::StartTransfer,
                       OtaProtocol::payloadStartTransfer(m_sessionToken, m_flashAddress,
                                                         static_cast<quint32>(m_firmwareSize),
                                                         m_imageCrc32),
                       resp, OtaProtocol::kStartTransferTimeoutMs)) {
        fail(tr("START_TRANSFER 失败"));
        return false;
    }
    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        fail(tr("START_TRANSFER: %1")
                 .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))));
        return false;
    }

    m_transferReady = true;
    emit transferReadyChanged(true);
    emit logMessage(tr("START_TRANSFER 成功"), QStringLiteral("INFO"));
    return true;
}

bool UpdateEngine::doFinishVerify()
{
    OtaProtocol::Frame resp;
    emit logMessage(tr("⑥ END_TRANSFER + VERIFY ..."), QStringLiteral("INFO"));

    if (!sendWithRetry(OtaProtocol::Cmd::EndTransfer,
                       OtaProtocol::payloadEndTransfer(m_sessionToken), resp, 30000)) {
        fail(tr("END_TRANSFER 失败"));
        return false;
    }

    setState(State::Verifying);
    emit logMessage(tr("校验固件 CRC32=0x%1 ...").arg(m_imageCrc32, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));

    if (!sendWithRetry(OtaProtocol::Cmd::Verify,
                       OtaProtocol::payloadVerify(m_sessionToken, m_imageCrc32), resp)) {
        fail(tr("VERIFY 失败"));
        return false;
    }
    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        fail(tr("校验被拒绝: %1")
                 .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))));
        return false;
    }

    emit logMessage(tr("校验成功"), QStringLiteral("INFO"));
    return true;
}

void UpdateEngine::startUpgrade()
{
    if (!m_transport || !m_transport->isOpen()) {
        emit logMessage(tr("设备未连接"), QStringLiteral("ERROR"));
        emit upgradeFinished(false, tr("请先连接设备"));
        return;
    }
    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中"), QStringLiteral("WARN"));
        return;
    }

    m_stopRequested.storeRelease(0);
    emit logMessage(tr("=== 一键升级开始 ==="), QStringLiteral("INFO"));

    auto releaseBusy = [this]() {
        m_busy.storeRelease(0);
        emit operationFinished();
    };

    if (!loadFirmware()) {
        fail(tr("固件加载失败"));
        releaseBusy();
        return;
    }

    if (stoppedOrDisconnected(tr("升级")) || !doGetInfo()) {
        releaseBusy();
        return;
    }
    if (stoppedOrDisconnected(tr("GET_INFO")) || !doBeginSession()) {
        releaseBusy();
        return;
    }
    if (stoppedOrDisconnected(tr("BEGIN")) || !doErase()) {
        releaseBusy();
        return;
    }
    if (stoppedOrDisconnected(tr("ERASE")) || !doStartTransfer()) {
        releaseBusy();
        return;
    }
    if (stoppedOrDisconnected(tr("START_TRANSFER")) || !doTransferDataOnly()) {
        releaseBusy();
        return;
    }
    if (stoppedOrDisconnected(tr("传固件")) || !doFinishVerify()) {
        releaseBusy();
        return;
    }

    setState(State::Completed);
    resetTransferState();
    emit progressChanged(100.0, m_firmwareSize, m_firmwareSize, m_lastSpeed, 0);
    emit logMessage(tr("=== 一键升级完成 ==="), QStringLiteral("INFO"));
    emit upgradeFinished(true, tr("升级成功，设备将自动重启"));
    releaseBusy();
}

void UpdateEngine::startFinishVerifyTest()
{
    if (!m_transport || !m_transport->isOpen()) {
        emit logMessage(tr("设备未连接"), QStringLiteral("ERROR"));
        emit operationFinished();
        return;
    }
    if (!m_busy.testAndSetOrdered(0, 1)) {
        emit logMessage(tr("其他操作进行中，请稍后再试"), QStringLiteral("WARN"));
        return;
    }

    if (!m_dataTransferDone) {
        emit logMessage(tr("[单步] 请先完成 ⑥ 传固件 (DATA)"), QStringLiteral("ERROR"));
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    OtaProtocol::Frame resp;

    emit logMessage(tr("=== [⑦ 结束校验] END_TRANSFER + VERIFY ==="), QStringLiteral("INFO"));

    if (!sendOnce(OtaProtocol::Cmd::EndTransfer,
                  OtaProtocol::payloadEndTransfer(m_sessionToken), resp, 30000, false)) {
        fail(tr("END_TRANSFER failed"));
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    setState(State::Verifying);
    emit logMessage(tr("Verifying firmware (CRC32=0x%1)...")
                        .arg(m_imageCrc32, 8, 16, QChar('0')),
                    QStringLiteral("INFO"));

    if (!sendWithRetry(OtaProtocol::Cmd::Verify,
                       OtaProtocol::payloadVerify(m_sessionToken, m_imageCrc32), resp)) {
        fail(tr("VERIFY failed"));
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    if (OtaProtocol::parseResponseStatus(resp) != OtaProtocol::Status::Ok) {
        fail(tr("Verification rejected: %1")
                 .arg(OtaProtocol::statusToString(OtaProtocol::parseResponseStatus(resp))));
        m_busy.storeRelease(0);
        emit operationFinished();
        return;
    }

    emit logMessage(tr("Verification successful"), QStringLiteral("INFO"));
    setState(State::Completed);
    resetTransferState();
    emit progressChanged(100.0, m_firmwareSize, m_firmwareSize, m_lastSpeed, 0);
    emit upgradeFinished(true, tr("OTA 完成，设备将自动重启"));
    emit logMessage(tr("可点击「复位设备」或等待 MCU 自动重启"), QStringLiteral("INFO"));

    m_busy.storeRelease(0);
    emit operationFinished();
}

bool UpdateEngine::doTransferDataOnly()
{
    if (!m_transferReady) {
        fail(tr("内部错误：START_TRANSFER 未就绪"));
        return false;
    }

    if (!loadFirmware()) {
        fail(tr("固件加载失败"));
        return false;
    }

    m_chunkSize = OtaProtocol::kDefaultChunkSize;
    m_transport->clearDeferredResponses();
    m_bytesSent = 0;
    m_speedWindowBytes = 0;
    m_lastSpeed = 0.0;
    m_speedTimer.invalidate();
    m_dataTransferDone = false;
    emit dataTransferDoneChanged(false);

    emit logMessage(tr("⑤ 传固件 DATA %1B stop-and-wait ...").arg(m_chunkSize),
                    QStringLiteral("INFO"));

    setState(State::Transferring);
    m_transport->pumpReceive(100);
    QThread::msleep(20);

    if (!transferDataPipelined())
        return false;

    m_dataTransferDone = true;
    emit dataTransferDoneChanged(true);
    setState(State::Connected);
    emit logMessage(tr("DATA 传输完成"), QStringLiteral("INFO"));
    return true;
}

void UpdateEngine::tickProgress(qint64 deltaBytes)
{
    m_bytesSent += deltaBytes;
    m_speedWindowBytes += deltaBytes;

    if (!m_speedTimer.isValid())
        m_speedTimer.start();

    const qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed >= 500) {
        m_lastSpeed = (m_speedWindowBytes * 1000.0) / qMax<qint64>(1, elapsed);
        m_speedWindowBytes = 0;
        m_speedTimer.restart();
    }

    const double pct = m_firmwareSize > 0
                           ? (100.0 * m_bytesSent) / m_firmwareSize
                           : 0.0;
    int eta = 0;
    if (m_lastSpeed > 1.0 && m_firmwareSize > m_bytesSent)
        eta = static_cast<int>((m_firmwareSize - m_bytesSent) / m_lastSpeed);

    emit progressChanged(pct, m_bytesSent, m_firmwareSize, m_lastSpeed, eta);
}

bool UpdateEngine::transferDataPipelined()
{
    /* Classic stop-and-wait ARQ: DATA -> wait ACK(seq) -> next; timeout resend same seq */
    const int ackTimeoutMs = OtaProtocol::kDataAckTimeoutMs;
    const int retryDelayMs = OtaProtocol::kDataRetryDelayMs;
    quint32 offset = 0;
    int lastLoggedStep = -1;
    int ackedChunks = 0;

    while (offset < static_cast<quint32>(m_firmwareSize)) {
        if (m_stopRequested.loadAcquire()) {
            abortUpgrade();
            return false;
        }

        if (!m_transport || !m_transport->isOpen()) {
            fail(tr("TCP disconnected, cannot continue DATA transfer"));
            return false;
        }

        const int chunkLen =
            qMin(m_chunkSize, static_cast<int>(m_firmwareSize - static_cast<qint64>(offset)));
        const QByteArray chunk = m_firmware.mid(static_cast<int>(offset), chunkLen);
        const QByteArray pl = OtaProtocol::payloadData(m_sessionToken, offset, chunk);

        quint16 seq = 0;
        if (!m_transport->sendCommand(OtaProtocol::Cmd::Data, pl, seq)) {
            fail(tr("DATA send failed at offset 0x%1").arg(offset, 8, 16, QChar('0')));
            return false;
        }

        bool acked = false;
        int attempt = 0;
        while (!acked) {
            if (m_stopRequested.loadAcquire()) {
                abortUpgrade();
                return false;
            }

            auto frame = m_transport->waitResponse(OtaProtocol::Cmd::Data, seq, ackTimeoutMs);
            if (frame) {
                const auto st = OtaProtocol::parseResponseStatus(*frame);
                if (st != OtaProtocol::Status::Ok) {
                    fail(tr("DATA rejected at seq %1: %2")
                             .arg(seq)
                             .arg(OtaProtocol::statusToString(st)));
                    return false;
                }
                acked = true;
                break;
            }

            ++attempt;
            if (attempt == 1 || (attempt % 5) == 0) {
                emit logMessage(
                    tr("DATA seq %1 offset 0x%2 no ACK, resend same seq (try %3)")
                        .arg(seq)
                        .arg(offset, 8, 16, QChar('0'))
                        .arg(attempt),
                    QStringLiteral("WARN"));
            }

            if (!m_transport->isOpen()) {
                fail(tr("TCP disconnected (offset 0x%1)").arg(offset, 8, 16, QChar('0')));
                return false;
            }

            if (retryDelayMs > 0)
                QThread::msleep(static_cast<unsigned long>(retryDelayMs));

            if (!m_transport->sendCommandAtSeq(OtaProtocol::Cmd::Data, pl, seq)) {
                emit logMessage(tr("DATA resend failed offset 0x%1")
                                    .arg(offset, 8, 16, QChar('0')),
                                QStringLiteral("WARN"));
                QThread::msleep(50);
            }
        }

        tickProgress(chunkLen);
        ++ackedChunks;
        offset += static_cast<quint32>(chunkLen);

        const int pctStep =
            static_cast<int>((100.0 * m_bytesSent) / qMax<qint64>(1, m_firmwareSize)) / 10;
        if (pctStep != lastLoggedStep && pctStep > 0 && pctStep <= 10) {
            lastLoggedStep = pctStep;
            emit logMessage(tr("Progress %1%").arg(pctStep * 10), QStringLiteral("INFO"));
        }

        if (OtaProtocol::kDataInterPacketDelayMs > 0)
            QThread::msleep(OtaProtocol::kDataInterPacketDelayMs);
    }

    emit logMessage(tr("DATA transfer done, %1 chunks acked").arg(ackedChunks),
                    QStringLiteral("INFO"));
    return true;
}

void UpdateEngine::stopUpgrade()
{
    m_stopRequested.storeRelease(1);
}

void UpdateEngine::abortUpgrade()
{
    if (m_sessionToken != 0 && m_transport && m_transport->isOpen()) {
        OtaProtocol::Frame resp;
        m_transport->transact(OtaProtocol::Cmd::Abort,
                           OtaProtocol::payloadAbort(m_sessionToken), resp, 1000);
        emit logMessage(tr("Sent ABORT to device"), QStringLiteral("WARN"));
    }
    m_sessionToken = 0;
    setState(State::Aborted);
    emit upgradeFinished(false, tr("Upgrade aborted"));
    emit logMessage(tr("Upgrade stopped by user"), QStringLiteral("WARN"));
}

void UpdateEngine::resetDevice()
{
    if (!m_transport || !m_transport->isOpen())
        return;

    OtaProtocol::Frame resp;
    if (sendWithRetry(OtaProtocol::Cmd::Reset, QByteArray(), resp)) {
        emit logMessage(tr("Reset command sent"), QStringLiteral("INFO"));
        m_sessionToken = 0;
        setState(State::Idle);
    }
}

void UpdateEngine::fail(const QString &reason)
{
    setState(State::Failed);
    emit logMessage(reason, QStringLiteral("ERROR"));
    emit upgradeFinished(false, reason);
}
