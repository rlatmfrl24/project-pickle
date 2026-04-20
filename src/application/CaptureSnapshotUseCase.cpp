#include "CaptureSnapshotUseCase.h"

#include "ports/ISnapshotGenerator.h"

CaptureSnapshotUseCase::CaptureSnapshotUseCase(ISnapshotGenerator *snapshotGenerator)
    : m_snapshotGenerator(snapshotGenerator)
{
}

SnapshotCaptureResult CaptureSnapshotUseCase::execute(
    const SnapshotCaptureRequest &request,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    if (!m_snapshotGenerator) {
        SnapshotCaptureResult result;
        result.mediaId = request.mediaId;
        result.errorMessage = QStringLiteral("Snapshot generator is unavailable.");
        return result;
    }

    return m_snapshotGenerator->capture(request, cancellationToken);
}
