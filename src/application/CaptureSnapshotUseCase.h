#pragma once

#include "domain/MediaEntities.h"

#include <memory>

class CancellationToken;
class ISnapshotGenerator;

class CaptureSnapshotUseCase
{
public:
    explicit CaptureSnapshotUseCase(ISnapshotGenerator *snapshotGenerator);

    SnapshotCaptureResult execute(
        const SnapshotCaptureRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;

private:
    ISnapshotGenerator *m_snapshotGenerator = nullptr;
};
