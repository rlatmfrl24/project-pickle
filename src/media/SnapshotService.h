#pragma once

#include "domain/CancellationToken.h"
#include "media/MediaTypes.h"
#include "ports/ISnapshotGenerator.h"

#include <QString>

#include <memory>

class SnapshotService : public ISnapshotGenerator
{
public:
    SnapshotCaptureResult capture(
        const SnapshotCaptureRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const override;

    static QString defaultSnapshotRoot();
    static bool clearSnapshotRoot(const QString &rootPath, QString *errorMessage = nullptr);
};
