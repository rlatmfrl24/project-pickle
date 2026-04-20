#pragma once

#include "domain/MediaEntities.h"
#include "domain/OperationResult.h"

class ISettingsRepository;

class SettingsUseCase
{
public:
    explicit SettingsUseCase(ISettingsRepository *settingsRepository);

    OperationResult<AppSettings> load() const;
    VoidResult save(const AppSettings &settings) const;

private:
    ISettingsRepository *m_settingsRepository = nullptr;
};
