#include "RenameMediaUseCase.h"

#include "ports/IFileRenamer.h"
#include "ports/IMediaRepository.h"

RenameMediaUseCase::RenameMediaUseCase(IFileRenamer *renamer, IMediaRepository *repository)
    : m_renamer(renamer)
    , m_repository(repository)
{
}

OperationResult<ScannedMediaFile> RenameMediaUseCase::execute(
    int mediaId,
    const QString &filePath,
    const QString &newBaseName,
    const QString &rollbackBaseName) const
{
    OperationResult<ScannedMediaFile> result;
    if (!m_renamer) {
        result.errorMessage = QStringLiteral("File renamer is unavailable.");
        return result;
    }
    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }

    const FileRenameResult renameResult = m_renamer->renameFile(filePath, newBaseName);
    if (!renameResult.succeeded) {
        result.errorMessage = renameResult.errorMessage;
        return result;
    }

    if (!m_repository->renameMediaFile(mediaId, renameResult.file)) {
        if (!rollbackBaseName.isEmpty()) {
            m_renamer->renameFile(renameResult.file.filePath, rollbackBaseName);
        }
        result.errorMessage = m_repository->lastError();
        return result;
    }

    result.value = renameResult.file;
    result.succeeded = true;
    return result;
}
