#ifndef ALARMCARD_H
#define ALARMCARD_H

#include <QLabel>
#include <QWidget>

/**
 * @brief 告警状态卡片（急停 / 水浸）
 */
class AlarmCard : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmCard(const QString &title, QWidget *parent = nullptr);

    void setAlarm(bool alarming, const QString &detail = QString());

private:
    QLabel *m_icon = nullptr;
    QLabel *m_state = nullptr;
    QLabel *m_detail = nullptr;
    QString m_title;
};

#endif // ALARMCARD_H
