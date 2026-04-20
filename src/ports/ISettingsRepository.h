#pragma once

#include "domain/MediaEntities.h"

#include <QString>

class ISettingsRepository
{
public:
    virtual ~ISettingsRepository() = default;
    virtual AppSettings load() = 0;
    virtual bool save(const AppSettings &settings) = 0;
    virtual QString lastError() const = 0;
};
