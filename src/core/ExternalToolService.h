#pragma once

#include "media/MediaTypes.h"
#include "ports/IExternalToolResolver.h"

#include <QString>

class ExternalToolService : public IExternalToolResolver
{
public:
    ExternalToolStatus validateFfprobe(const QString &configuredPath) const;
    ExternalToolStatus validateFfmpeg(const QString &configuredPath) const;

    ExternalToolStatus probeTool(const QString &toolName, const QString &configuredPath) const override;
    QString programForTool(const QString &toolName, const QString &configuredPath) const override;

private:
    ExternalToolStatus validateTool(const QString &toolName, const QString &configuredPath) const;
};
