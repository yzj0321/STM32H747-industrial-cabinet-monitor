#ifndef GAUGEWIDGET_H
#define GAUGEWIDGET_H

#include <QWidget>

/**
 * @brief 工业风半圆弧仪表（温度/湿度）
 */
class GaugeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GaugeWidget(QWidget *parent = nullptr);

    void setRange(double minValue, double maxValue);
    void setValue(double value);
    void setUnit(const QString &unit);
    void setTitle(const QString &title);
    void setStatusText(const QString &text, bool normal);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_min = 0.0;
    double m_max = 100.0;
    double m_value = 0.0;
    QString m_unit;
    QString m_title;
    QString m_status = QStringLiteral("—");
    bool m_statusNormal = true;
};

#endif // GAUGEWIDGET_H
