#include "GaugeWidget.h"

#include <QPainter>
#include <QtMath>

GaugeWidget::GaugeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(220, 240);
}

void GaugeWidget::setRange(double minValue, double maxValue)
{
    m_min = minValue;
    m_max = (maxValue > minValue) ? maxValue : (minValue + 1.0);
    update();
}

void GaugeWidget::setValue(double value)
{
    m_value = qBound(m_min, value, m_max);
    update();
}

void GaugeWidget::setUnit(const QString &unit)
{
    m_unit = unit;
    update();
}

void GaugeWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void GaugeWidget::setStatusText(const QString &text, bool normal)
{
    m_status = text;
    m_statusNormal = normal;
    update();
}

QSize GaugeWidget::sizeHint() const
{
    return {260, 280};
}

QSize GaugeWidget::minimumSizeHint() const
{
    return {200, 220};
}

void GaugeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(12, 28, -12, -48);
    const qreal side = qMin(area.width(), area.height() * 1.35);
    const QRectF arcRect(area.center().x() - side / 2.0,
                         area.top() + 8,
                         side,
                         side);

    // Title
    p.setPen(QColor(QStringLiteral("#89b4fa")));
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    p.setFont(titleFont);
    p.drawText(QRect(0, 6, width(), 22), Qt::AlignHCenter | Qt::AlignVCenter, m_title);

    // Background track
    QPen trackPen(QColor(QStringLiteral("#313244")), 14, Qt::SolidLine, Qt::RoundCap);
    p.setPen(trackPen);
    p.drawArc(arcRect, 210 * 16, -240 * 16);

    // Gradient value arc
    QConicalGradient grad(arcRect.center(), 210);
    grad.setColorAt(0.0, QColor(QStringLiteral("#89b4fa")));
    grad.setColorAt(0.45, QColor(QStringLiteral("#a6e3a1")));
    grad.setColorAt(0.8, QColor(QStringLiteral("#fab387")));
    grad.setColorAt(1.0, QColor(QStringLiteral("#f38ba8")));

    const double ratio = (m_value - m_min) / (m_max - m_min);
    QPen valuePen(QBrush(grad), 14, Qt::SolidLine, Qt::RoundCap);
    p.setPen(valuePen);
    p.drawArc(arcRect, 210 * 16, static_cast<int>(-240.0 * 16.0 * ratio));

    // Needle
    const double angleDeg = 210.0 - 240.0 * ratio;
    const double angleRad = qDegreesToRadians(angleDeg);
    const QPointF center = arcRect.center();
    const qreal radius = arcRect.width() / 2.0 - 8.0;
    const QPointF tip(center.x() + qCos(angleRad) * (radius - 18),
                      center.y() - qSin(angleRad) * (radius - 18));
    p.setPen(QPen(QColor(QStringLiteral("#a6e3a1")), 3));
    p.drawLine(center, tip);
    p.setBrush(QColor(QStringLiteral("#a6e3a1")));
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, 5, 5);

    // Value text
    p.setPen(QColor(QStringLiteral("#a6e3a1")));
    QFont valueFont = font();
    valueFont.setBold(true);
    valueFont.setPointSize(22);
    p.setFont(valueFont);
    const QString valueText = QStringLiteral("%1 %2")
                                  .arg(m_value, 0, 'f', m_unit == QStringLiteral("%") ? 0 : 1)
                                  .arg(m_unit);
    p.drawText(QRect(0, int(center.y() + 18), width(), 36),
               Qt::AlignHCenter | Qt::AlignVCenter,
               valueText);

    // Range labels
    p.setPen(QColor(QStringLiteral("#6c7086")));
    QFont small = font();
    small.setPointSize(9);
    p.setFont(small);
    p.drawText(QRect(int(arcRect.left()), int(arcRect.bottom() - 18), 48, 18),
               Qt::AlignLeft | Qt::AlignVCenter,
               QString::number(m_min, 'f', 0));
    p.drawText(QRect(int(arcRect.right() - 48), int(arcRect.bottom() - 18), 48, 18),
               Qt::AlignRight | Qt::AlignVCenter,
               QString::number(m_max, 'f', 0));

    // Status
    p.setPen(m_statusNormal ? QColor(QStringLiteral("#a6e3a1"))
                            : QColor(QStringLiteral("#f38ba8")));
    QFont statusFont = font();
    statusFont.setBold(true);
    statusFont.setPointSize(10);
    p.setFont(statusFont);
    p.drawText(QRect(0, height() - 28, width(), 22),
               Qt::AlignHCenter | Qt::AlignVCenter,
               m_status);
}
