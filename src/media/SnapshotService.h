#pragma once

#include "core/CancellationToken.h"
#include "media/MediaTypes.h"

#include <QString>

#include <memory>

class SnapshotService
{
public:
    SnapshotCaptureResult capture(
        const SnapshotCaptureRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;

    static QString defaultSnapshotRoot();
    static bool clearSnapshotRoot(const QString &rootPath, QString *errorMessage = nullptr);
};
