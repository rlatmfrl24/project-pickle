#pragma once

#include "application/ScanCommitResult.h"
#include "ports/IFileScanner.h"

class IMediaRepository;

class ScanLibraryUseCase
{
public:
    ScanLibraryUseCase(IFileScanner *scanner, IMediaRepository *repository);

    ScanCommitResult execute(
        const QString &rootPath,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr,
        const IFileScanner::ProgressCallback &progressCallback = {}) const;

private:
    IFileScanner *m_scanner = nullptr;
    IMediaRepository *m_repository = nullptr;
};
