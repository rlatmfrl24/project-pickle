#pragma once

#include "domain/MediaEntities.h"

#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

struct DiagnosticsContext
{
    bool databaseReady = false;
    QString databaseStatus;
    QString databasePath;
    QString currentScanRoot;
    int libraryItems = 0;
    bool scanInProgress = false;
    bool metadataInProgress = false;
    bool snapshotInProgress = false;
    bool thumbnailMaintenanceInProgress = false;
    AppSettings settings;
    QString toolsStatus;
    QString settingsStatus;
    QVariantList workEvents;
};

class DiagnosticsController
{
public:
    QVariantMap aboutInfo(const QString &databasePath) const;
    QVariantMap systemInfo(const DiagnosticsContext &context) const;
    QString diagnosticReport(const DiagnosticsContext &context) const;
    QString openLogFile() const;
    QString openLogFolder() const;
    QString openAppDataFolder() const;
    QString localPathFromUrl(const QUrl &url) const;
};
