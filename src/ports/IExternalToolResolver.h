#pragma once

#include "domain/MediaEntities.h"

#include <QString>

class IExternalToolResolver
{
public:
    virtual ~IExternalToolResolver() = default;
    virtual ExternalToolStatus probeTool(const QString &toolName, const QString &configuredPath) const = 0;
};
