#pragma once

#include "domain/MediaEntities.h"

struct ScanCommitResult {
    DirectoryScanResult scan;
    MediaUpsertResult upsert;
    QString errorMessage;
    bool succeeded = false;
};
