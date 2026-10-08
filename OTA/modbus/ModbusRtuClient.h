#ifndef MODBUSRTUCLIENT_H
#define MODBUSRTUCLIENT_H

#include <QObject>
#include <QSerialPort>
#include <QVector>

/**
 * @brief 精简 Modbus RTU 主机（USB CDC / 串口），对接板端从站
 *
 * 寄存器约定（与 bsp_modbus_app.h 对齐）：
 * - Holding 0: 湿度 %
 * - Holding 1: 温度 °C
 * - Holding 2: 水浸 ADC mV
 * - Holding 3: 水浸状态（0=干燥，1=浸水）
 * - Coil 0: PA3 LED / 继电器
 */
class ModbusRtuClient : public QObject
{
    Q_OBJECT
public:
    explicit ModbusRtuClient(QObject *parent = nullptr);
    ~ModbusRtuClient() override;

    bool open(const QString &portName, int baudRate = 115200);
    void close();
    bool isOpen() const;

    bool readHoldingRegisters(quint8 slave, quint16 address, quint16 count, QVector<quint16> *out);
    bool readCoils(quint8 slave, quint16 address, quint16 count, QVector<bool> *out);
    bool writeSingleCoil(quint8 slave, quint16 address, bool on);

    QString lastError() const { return m_lastError; }

private:
    bool transact(const QByteArray &request, int expectedDataBytes, QByteArray *response);
    static quint16 crc16(const QByteArray &data);

    QSerialPort m_port;
    QString m_lastError;
    int m_timeoutMs = 500;
};

#endif // MODBUSRTUCLIENT_H
