#include "AlarmCard.h"

#include <QVBoxLayout>

AlarmCard::AlarmCard(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
    setObjectName(QStringLiteral("alarmCard"));
    setMinimumSize(180, 160);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(8);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral("color:#cdd6f4; font-weight:bold; font-size:14px;"));

    m_icon = new QLabel(this);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setFixedHeight(64);
    m_icon->setStyleSheet(QStringLiteral("font-size:36px;"));

    m_state = new QLabel(this);
    m_state->setAlignment(Qt::AlignCenter);
    m_state->setStyleSheet(QStringLiteral("font-weight:bold; font-size:18px;"));

    m_detail = new QLabel(this);
    m_detail->setAlignment(Qt::AlignCenter);
    m_detail->setObjectName(QStringLiteral("metaLabel"));

    lay->addWidget(titleLabel);
    lay->addStretch();
    lay->addWidget(m_icon);
    lay->addWidget(m_state);
    lay->addWidget(m_detail);
    lay->addStretch();

    setAlarm(false);
}

void AlarmCard::setAlarm(bool alarming, const QString &detail)
{
    if (alarming) {
        setStyleSheet(QStringLiteral(
            "QWidget#alarmCard { background:#2a1a1f; border:2px solid #e01b24; border-radius:12px; }"));
        m_icon->setText(QStringLiteral("●"));
        m_icon->setStyleSheet(QStringLiteral("color:#e01b24; font-size:42px;"));
        m_state->setText(tr("告警 ALARM"));
        m_state->setStyleSheet(QStringLiteral("color:#e01b24; font-weight:bold; font-size:18px;"));
    } else {
        setStyleSheet(QStringLiteral(
            "QWidget#alarmCard { background:#1a2a22; border:2px solid #40a02b; border-radius:12px; }"));
        m_icon->setText(QStringLiteral("✓"));
        m_icon->setStyleSheet(QStringLiteral("color:#40a02b; font-size:42px;"));
        m_state->setText(tr("正常 Normal"));
        m_state->setStyleSheet(QStringLiteral("color:#40a02b; font-weight:bold; font-size:18px;"));
    }
    m_detail->setText(detail.isEmpty() ? m_title : detail);
}
