#pragma once

#include <QString>

namespace PathSecurity {

QString appDataPath();
QString pathForComparison(const QString &path);
bool samePath(const QString &left, const QString &right);
bool isInsideOrEqual(const QString &path, const QString &root);
QString redactedLocalPath(const QString &path);

}
