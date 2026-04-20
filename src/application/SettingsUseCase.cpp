#include "SettingsUseCase.h"

#include "ports/ISettingsRepository.h"

SettingsUseCase::SettingsUseCase(ISettingsRepository *settingsRepository)
    : m_settingsRepository(settingsRepository)
{
}

OperationResult<AppSettings> SettingsUseCase::load() const
{
    OperationResult<AppSettings> result;
    if (!m_settingsRepository) {
        result.errorMessage = QStringLiteral("Settings repository is unavailable.");
        return result;
    }

    result.value = m_settingsRepository->load();
    if (!m_settingsRepository->lastError().isEmpty()) {
        result.errorMessage = m_settingsRepository->lastError();
        return result;
    }

    result.succeeded = true;
    return result;
}

VoidResult SettingsUseCase::save(const AppSettings &settings) const
{
    VoidResult result;
    if (!m_settingsRepository) {
        result.errorMessage = QStringLiteral("Settings repository is unavailable.");
        return result;
    }

    if (!m_settingsRepository->save(settings)) {
        result.errorMessage = m_settingsRepository->lastError();
        return result;
    }

    result.succeeded = true;
    return result;
}
