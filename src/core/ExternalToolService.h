#pragma once

#include "media/MediaTypes.h"

#include <QString>

class ExternalToolService
{
public:
    ExternalToolStatus validateFfprobe(const QString &configuredPath) const;
    ExternalToolStatus validateFfmpeg(const QString &configuredPath) const;

    static QString programForTool(const QString &toolName, const QString &configuredPath);

private:
    ExternalToolStatus validateTool(const QString &toolName, const QString &configuredPath) const;
};
