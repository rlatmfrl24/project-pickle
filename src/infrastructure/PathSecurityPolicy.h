#pragma once

#include "ports/IPathPolicy.h"

class PathSecurityPolicy : public IPathPolicy
{
public:
    QString appDataPath() const override;
    QString pathForComparison(const QString &path) const override;
    bool samePath(const QString &left, const QString &right) const override;
    bool isInsideOrEqual(const QString &path, const QString &root) const override;
    QString redactedLocalPath(const QString &path) const override;
};
