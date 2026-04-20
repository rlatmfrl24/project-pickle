#include "PathSecurityPolicy.h"

#include "core/PathSecurity.h"

QString PathSecurityPolicy::appDataPath() const
{
    return PathSecurity::appDataPath();
}

QString PathSecurityPolicy::pathForComparison(const QString &path) const
{
    return PathSecurity::pathForComparison(path);
}

bool PathSecurityPolicy::samePath(const QString &left, const QString &right) const
{
    return PathSecurity::samePath(left, right);
}

bool PathSecurityPolicy::isInsideOrEqual(const QString &path, const QString &root) const
{
    return PathSecurity::isInsideOrEqual(path, root);
}

QString PathSecurityPolicy::redactedLocalPath(const QString &path) const
{
    return PathSecurity::redactedLocalPath(path);
}
