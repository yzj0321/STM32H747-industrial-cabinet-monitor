/**
 * @file    OtaTransport.h
 * @brief   SOIP 传输层抽象接口（UART / TCP 共用）
 */

#ifndef OTATRANSPORT_H
#define OTATRANSPORT_H

#include "OtaProtocol.h"

#include <QObject>
#include <optional>

/**
 * @brief  SOIP 协议传输层抽象基类，统一串口与 TCP 的收发接口
 */
class OtaTransport : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief  构造传输层对象
     * @param  parent 父对象
     */
    explicit OtaTransport(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief  析构传输层对象
     */
    ~OtaTransport() override = default;

    /**
     * @brief  判断传输链路是否已打开
     * @retval true 已连接/已打开
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief  获取当前连接标识（如 IP:端口 或 COM 口名）
     * @retval 连接描述字符串
     */
    virtual QString connectionLabel() const = 0;

    /**
     * @brief  设置等待应答的超时时间
     * @param  ms 超时毫秒数
     */
    virtual void setResponseTimeoutMs(int ms) = 0;

    /**
     * @brief  获取当前应答超时时间
     * @retval 超时毫秒数
     */
    virtual int responseTimeoutMs() const = 0;

    /**
     * @brief  发送 SOIP 命令帧（自动分配新序列号）
     * @param  cmd 命令字
     * @param  payload 载荷数据
     * @param  outSeq 输出序列号
     * @retval true 发送成功
     */
    virtual bool sendCommand(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 &outSeq) = 0;

    /**
     * @brief  按指定序列号重发（DATA 超时重传必须复用原 seq，才能匹配迟到 ACK）
     */
    virtual bool sendCommandAtSeq(OtaProtocol::Cmd cmd, const QByteArray &payload, quint16 seq) = 0;

    /**
     * @brief  阻塞等待指定命令与序列号的应答帧
     * @param  expectedCmd 期望的命令字
     * @param  seq 序列号
     * @param  timeoutMs 超时毫秒数，-1 表示使用默认值
     * @retval 解析成功的应答帧，超时或失败返回空
     */
    virtual std::optional<OtaProtocol::Frame> waitResponse(OtaProtocol::Cmd expectedCmd,
                                                           quint16 seq,
                                                           int timeoutMs = -1) = 0;

    /**
     * @brief  发送命令并同步等待应答（请求-应答事务）
     * @param  cmd 命令字
     * @param  payload 载荷数据
     * @param  response 输出应答帧
     * @param  timeoutMs 超时毫秒数，-1 表示使用默认值
     * @retval true 收到有效应答
     */
    virtual bool transact(OtaProtocol::Cmd cmd, const QByteArray &payload,
                          OtaProtocol::Frame &response, int timeoutMs = -1) = 0;

    /**
     * @brief  非阻塞取已缓存的应答帧（流水线传输用）
     * @param  expectedCmd 期望的命令字
     * @param  seq 序列号
     * @retval 匹配的应答帧，未找到返回空
     */
    virtual std::optional<OtaProtocol::Frame> takeDeferredResponse(OtaProtocol::Cmd expectedCmd,
                                                                   quint16 seq) = 0;

    /**
     * @brief  拉取链路数据并解析进缓存
     * @param  maxWaitMs 最大等待毫秒数，0 表示仅处理已有数据
     */
    virtual void pumpReceive(int maxWaitMs = 0) = 0;

    /**
     * @brief  清空已缓存的延迟应答帧
     */
    virtual void clearDeferredResponses() = 0;

    /**
     * @brief  关闭传输链路
     */
    virtual void close() = 0;

signals:
    void opened(const QString &label);
    void closed();
    void rawDataReceived(const QByteArray &data);
    void logMessage(const QString &line, const QString &level);
    void communicationError(const QString &message);
};

#endif // OTATRANSPORT_H
