#pragma once

#include <QVariantList>

class QString;

class WorkEventLog
{
public:
    QVariantList events() const;
    bool isEmpty() const;
    bool append(const QString &kind, const QString &message, bool warning = false);
    bool clear();

private:
    QVariantList m_events;
};
