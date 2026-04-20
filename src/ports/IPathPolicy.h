#pragma once

#include <QString>

class IPathPolicy
{
public:
    virtual ~IPathPolicy() = default;
    virtual QString appDataPath() const = 0;
    virtual QString pathForComparison(const QString &path) const = 0;
    virtual bool samePath(const QString &left, const QString &right) const = 0;
    virtual bool isInsideOrEqual(const QString &path, const QString &root) const = 0;
    virtual QString redactedLocalPath(const QString &path) const = 0;
};
