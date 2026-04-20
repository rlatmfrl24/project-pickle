#include "app/WorkEventLog.h"

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QVariantMap>

namespace {
constexpr int MaxWorkEvents = 200;
}

QVariantList WorkEventLog::events() const
{
    return m_events;
}

bool WorkEventLog::isEmpty() const
{
    return m_events.isEmpty();
}

bool WorkEventLog::append(const QString &kind, const QString &message, bool warning)
{
    if (message.trimmed().isEmpty()) {
        return false;
    }

    QVariantMap event;
    event.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    event.insert(QStringLiteral("kind"), kind);
    event.insert(QStringLiteral("message"), message);
    event.insert(QStringLiteral("warning"), warning);
    m_events.prepend(event);
    while (m_events.size() > MaxWorkEvents) {
        m_events.removeLast();
    }

    if (warning) {
        qWarning().noquote() << QStringLiteral("[%1] %2").arg(kind, message);
    } else {
        qInfo().noquote() << QStringLiteral("[%1] %2").arg(kind, message);
    }
    return true;
}

bool WorkEventLog::clear()
{
    if (m_events.isEmpty()) {
        return false;
    }

    m_events.clear();
    return true;
}
