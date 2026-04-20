#pragma once

#include "domain/MediaEntities.h"

#include <QFutureWatcher>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <memory>

class CancellationToken;

struct MetadataControllerResult
{
    MetadataExtractionResult extraction;
    int mediaId = -1;
    bool manual = false;
};

Q_DECLARE_METATYPE(MetadataControllerResult)

class MetadataController : public QObject
{
    Q_OBJECT

public:
    explicit MetadataController(QObject *parent = nullptr);
    ~MetadataController() override;

    bool isRunning() const;
    bool cancelAvailable() const;
    void startExtraction(
        int mediaId,
        const QString &filePath,
        const QString &displayName,
        bool manual,
        const QString &ffprobeProgram);
    bool cancel();
    void waitForFinished();

signals:
    void extractionRejected(const QString &status);
    void extractionStarted(const QString &status);
    void extractionFinished(const MetadataControllerResult &result);

private:
    void handleFinished();

    QFutureWatcher<MetadataControllerResult> m_watcher;
    std::shared_ptr<CancellationToken> m_cancellation;
};
