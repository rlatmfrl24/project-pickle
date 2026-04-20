#pragma once

#include "domain/CancellationToken.h"
#include "domain/MediaEntities.h"

#include <memory>

class ISnapshotGenerator
{
public:
    virtual ~ISnapshotGenerator() = default;
    virtual SnapshotCaptureResult capture(
        const SnapshotCaptureRequest &request,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const = 0;
};
