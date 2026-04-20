#include "ScanLibraryUseCase.h"

#include "ports/IMediaRepository.h"

ScanLibraryUseCase::ScanLibraryUseCase(IFileScanner *scanner, IMediaRepository *repository)
    : m_scanner(scanner)
    , m_repository(repository)
{
}

ScanCommitResult ScanLibraryUseCase::execute(
    const QString &rootPath,
    const std::shared_ptr<CancellationToken> &cancellationToken,
    const IFileScanner::ProgressCallback &progressCallback) const
{
    ScanCommitResult result;

    if (!m_scanner) {
        result.errorMessage = QStringLiteral("Scanner is unavailable.");
        return result;
    }
    if (!m_repository) {
        result.errorMessage = QStringLiteral("Repository is unavailable.");
        return result;
    }

    result.scan = m_scanner->scanDirectory(rootPath, cancellationToken, progressCallback);
    if (result.scan.canceled || !result.scan.succeeded) {
        result.errorMessage = result.scan.errorMessage;
        return result;
    }

    if (!m_repository->upsertScanResult(result.scan.rootPath, result.scan.files, &result.upsert)) {
        result.errorMessage = m_repository->lastError();
        return result;
    }

    result.scan.files.clear();
    result.succeeded = true;
    return result;
}
