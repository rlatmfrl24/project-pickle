#include "core/PathSecurity.h"
#include "db/DatabaseService.h"
#include "db/MediaRepository.h"
#include "db/MigrationService.h"
#include "media/ScanService.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QTextStream>

namespace {
QString sortKeyName(MediaLibrarySortKey sortKey)
{
    switch (sortKey) {
    case MediaLibrarySortKey::Size:
        return QStringLiteral("size");
    case MediaLibrarySortKey::Modified:
        return QStringLiteral("modified");
    case MediaLibrarySortKey::Name:
    default:
        return QStringLiteral("name");
    }
}

bool readJsonFile(const QString &path, QJsonObject *object, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (object) {
        *object = {};
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Baseline JSON does not exist.");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Baseline JSON is not an object.")
                : parseError.errorString();
        }
        return false;
    }

    if (object) {
        *object = document.object();
    }
    return true;
}

double percentChange(double baselineValue, double currentValue)
{
    if (baselineValue == 0.0) {
        return currentValue == 0.0 ? 0.0 : 100.0;
    }
    return ((currentValue - baselineValue) / baselineValue) * 100.0;
}

QJsonObject metricDelta(const QJsonObject &baseline, const QJsonObject &current, const QString &key)
{
    const double baselineValue = baseline.value(key).toDouble();
    const double currentValue = current.value(key).toDouble();

    QJsonObject result;
    result.insert(QStringLiteral("baseline"), baselineValue);
    result.insert(QStringLiteral("current"), currentValue);
    result.insert(QStringLiteral("delta"), currentValue - baselineValue);
    result.insert(QStringLiteral("percentChange"), percentChange(baselineValue, currentValue));
    return result;
}

QString timedMetricKey(const QJsonObject &object, bool includeSearchText)
{
    QString key;
    if (includeSearchText) {
        key += object.value(QStringLiteral("searchText")).toString();
        key += QLatin1Char('|');
    }
    key += object.value(QStringLiteral("sortKey")).toString();
    key += QLatin1Char('|');
    key += object.value(QStringLiteral("ascending")).toBool() ? QStringLiteral("asc") : QStringLiteral("desc");
    return key;
}

QHash<QString, QJsonObject> indexTimedMetrics(const QJsonArray &metrics, bool includeSearchText)
{
    QHash<QString, QJsonObject> indexed;
    for (const QJsonValue &value : metrics) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        indexed.insert(timedMetricKey(object, includeSearchText), object);
    }
    return indexed;
}

QJsonArray compareTimedMetrics(
    const QJsonArray &baselineMetrics,
    const QJsonArray &currentMetrics,
    bool includeSearchText)
{
    const QHash<QString, QJsonObject> baselineByKey = indexTimedMetrics(baselineMetrics, includeSearchText);
    QJsonArray comparisons;

    for (const QJsonValue &value : currentMetrics) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject current = value.toObject();
        const QString key = timedMetricKey(current, includeSearchText);
        const QJsonObject baseline = baselineByKey.value(key);

        QJsonObject comparison;
        if (includeSearchText) {
            comparison.insert(QStringLiteral("searchText"), current.value(QStringLiteral("searchText")));
        }
        comparison.insert(QStringLiteral("sortKey"), current.value(QStringLiteral("sortKey")));
        comparison.insert(QStringLiteral("ascending"), current.value(QStringLiteral("ascending")));
        comparison.insert(QStringLiteral("baselineFound"), !baseline.isEmpty());
        if (baseline.isEmpty()) {
            comparison.insert(QStringLiteral("currentElapsedMs"), current.value(QStringLiteral("elapsedMs")));
            comparison.insert(QStringLiteral("currentRowCount"), current.value(QStringLiteral("rowCount")));
        } else {
            comparison.insert(QStringLiteral("elapsedMs"), metricDelta(baseline, current, QStringLiteral("elapsedMs")));
            comparison.insert(QStringLiteral("rowCount"), metricDelta(baseline, current, QStringLiteral("rowCount")));
        }
        comparisons.append(comparison);
    }

    return comparisons;
}

QJsonObject compareProbeResults(const QJsonObject &baseline, const QJsonObject &current)
{
    QJsonObject comparison;
    comparison.insert(QStringLiteral("baselineFound"), true);
    comparison.insert(QStringLiteral("visitedFileCount"), metricDelta(baseline, current, QStringLiteral("visitedFileCount")));
    comparison.insert(QStringLiteral("matchedFileCount"), metricDelta(baseline, current, QStringLiteral("matchedFileCount")));
    comparison.insert(QStringLiteral("scanElapsedMs"), metricDelta(baseline, current, QStringLiteral("scanElapsedMs")));
    comparison.insert(QStringLiteral("upsertElapsedMs"), metricDelta(baseline, current, QStringLiteral("upsertElapsedMs")));
    comparison.insert(
        QStringLiteral("fetches"),
        compareTimedMetrics(
            baseline.value(QStringLiteral("fetches")).toArray(),
            current.value(QStringLiteral("fetches")).toArray(),
            false));
    comparison.insert(
        QStringLiteral("searches"),
        compareTimedMetrics(
            baseline.value(QStringLiteral("searches")).toArray(),
            current.value(QStringLiteral("searches")).toArray(),
            true));
    return comparison;
}

QJsonObject measuredFetch(MediaRepository *repository, const MediaLibraryQuery &query)
{
    QJsonObject result;
    result.insert(QStringLiteral("sortKey"), sortKeyName(query.sortKey));
    result.insert(QStringLiteral("ascending"), query.ascending);
    result.insert(QStringLiteral("searchText"), query.searchText);

    QElapsedTimer timer;
    timer.start();
    const QVector<MediaLibraryItem> items = repository->fetchLibraryItems(query);
    result.insert(QStringLiteral("elapsedMs"), timer.elapsed());
    result.insert(QStringLiteral("rowCount"), items.size());
    result.insert(QStringLiteral("succeeded"), repository->lastError().isEmpty());
    if (!repository->lastError().isEmpty()) {
        result.insert(QStringLiteral("error"), repository->lastError());
    }

    return result;
}

QJsonObject extensionSummary(const QVector<ScannedMediaFile> &files)
{
    QHash<QString, int> counts;
    for (const ScannedMediaFile &file : files) {
        const QString extension = file.fileExtension.trimmed().isEmpty()
            ? QStringLiteral("<none>")
            : file.fileExtension.toLower();
        ++counts[extension];
    }

    QJsonObject summary;
    const auto keys = counts.keys();
    for (const QString &key : keys) {
        summary.insert(key, counts.value(key));
    }
    return summary;
}

bool writeJsonFile(const QString &path, const QJsonObject &object, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create output directory.");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pickle_perf_probe"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PICKLE_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Pickle local folder performance probe."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption scanRootOption(
        QStringLiteral("scan-root"),
        QStringLiteral("Root folder to scan."),
        QStringLiteral("path"));
    const QCommandLineOption databaseOption(
        QStringLiteral("database"),
        QStringLiteral("SQLite database path for the probe."),
        QStringLiteral("path"));
    const QCommandLineOption jsonOption(
        QStringLiteral("json"),
        QStringLiteral("JSON output path."),
        QStringLiteral("path"));
    const QCommandLineOption compareOption(
        QStringLiteral("compare"),
        QStringLiteral("Existing JSON baseline to compare against."),
        QStringLiteral("path"));
    parser.addOption(scanRootOption);
    parser.addOption(databaseOption);
    parser.addOption(jsonOption);
    parser.addOption(compareOption);
    parser.process(app);

    const QString scanRoot = parser.value(scanRootOption);
    const QString databasePath = parser.value(databaseOption);
    const QString jsonPath = parser.value(jsonOption);
    const QString comparePath = parser.value(compareOption);
    QTextStream stderrStream(stderr);

    if (scanRoot.trimmed().isEmpty() || databasePath.trimmed().isEmpty() || jsonPath.trimmed().isEmpty()) {
        stderrStream << "Missing required --scan-root, --database, or --json option.\n";
        return 2;
    }
    const QFileInfo scanRootInfo(scanRoot);
    if (!scanRootInfo.exists() || !scanRootInfo.isDir()) {
        stderrStream << "Scan root does not exist or is not a directory.\n";
        return 2;
    }

    QJsonObject baseline;
    QString baselineError;
    const bool compareRequested = !comparePath.trimmed().isEmpty();
    const bool baselineLoaded = compareRequested
        ? readJsonFile(comparePath, &baseline, &baselineError)
        : false;

    QString databaseError;
    QSqlDatabase database = DatabaseService::openNamedConnection(
        databasePath,
        QStringLiteral("pickle_perf_probe"),
        &databaseError);
    if (!database.isOpen()) {
        stderrStream << "Database open failed: " << databaseError << '\n';
        return 1;
    }

    {
        MigrationService migrationService(database);
        if (!migrationService.migrate()) {
            stderrStream << "Migration failed: " << migrationService.lastError() << '\n';
            database.close();
            database = {};
            QSqlDatabase::removeDatabase(QStringLiteral("pickle_perf_probe"));
            return 1;
        }
    }

    QElapsedTimer scanTimer;
    scanTimer.start();
    ScanService scanService;
    DirectoryScanResult scanResult = scanService.scanDirectory(scanRoot);
    const qint64 scanElapsedMs = scanTimer.elapsed();
    if (!scanResult.succeeded) {
        stderrStream << "Scan failed: " << scanResult.errorMessage << '\n';
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(QStringLiteral("pickle_perf_probe"));
        return 1;
    }

    MediaUpsertResult upsertResult;
    QJsonArray fetches;
    QJsonArray searches;
    qint64 upsertElapsedMs = 0;
    {
        MediaRepository repository(database);
        QElapsedTimer upsertTimer;
        upsertTimer.start();
        if (!repository.upsertScanResult(scanResult.rootPath, scanResult.files, &upsertResult)) {
            stderrStream << "Upsert failed: " << repository.lastError() << '\n';
            database.close();
            database = {};
            QSqlDatabase::removeDatabase(QStringLiteral("pickle_perf_probe"));
            return 1;
        }
        upsertElapsedMs = upsertTimer.elapsed();

        for (const MediaLibrarySortKey sortKey : {MediaLibrarySortKey::Name, MediaLibrarySortKey::Size, MediaLibrarySortKey::Modified}) {
            MediaLibraryQuery query;
            query.sortKey = sortKey;
            query.ascending = sortKey != MediaLibrarySortKey::Size;
            fetches.append(measuredFetch(&repository, query));
        }

        for (const QString &searchText : {QStringLiteral("mp4"), QStringLiteral("temp")}) {
            MediaLibraryQuery query;
            query.searchText = searchText;
            query.sortKey = MediaLibrarySortKey::Name;
            query.ascending = true;
            searches.append(measuredFetch(&repository, query));
        }
    }

    QJsonObject output;
    output.insert(QStringLiteral("probeVersion"), QCoreApplication::applicationVersion());
    output.insert(QStringLiteral("scanRoot"), PathSecurity::redactedLocalPath(scanRootInfo.absoluteFilePath()));
    output.insert(QStringLiteral("database"), PathSecurity::redactedLocalPath(databasePath));
    output.insert(QStringLiteral("visitedFileCount"), scanResult.visitedFileCount);
    output.insert(QStringLiteral("matchedFileCount"), scanResult.matchedFileCount);
    output.insert(QStringLiteral("scanElapsedMs"), scanElapsedMs);
    output.insert(QStringLiteral("upsertElapsedMs"), upsertElapsedMs);
    output.insert(QStringLiteral("upsertedFileCount"), upsertResult.upsertedFileCount);
    output.insert(QStringLiteral("extensionSummary"), extensionSummary(scanResult.files));
    output.insert(QStringLiteral("fetches"), fetches);
    output.insert(QStringLiteral("searches"), searches);
    if (compareRequested) {
        QJsonObject comparison;
        comparison.insert(QStringLiteral("baseline"), PathSecurity::redactedLocalPath(QFileInfo(comparePath).absoluteFilePath()));
        if (baselineLoaded) {
            const QJsonObject resultComparison = compareProbeResults(baseline, output);
            for (auto it = resultComparison.begin(); it != resultComparison.end(); ++it) {
                comparison.insert(it.key(), it.value());
            }
        } else {
            comparison.insert(QStringLiteral("baselineFound"), false);
            comparison.insert(QStringLiteral("error"), baselineError);
        }
        output.insert(QStringLiteral("comparison"), comparison);
    }

    QString writeError;
    if (!writeJsonFile(jsonPath, output, &writeError)) {
        stderrStream << "JSON write failed: " << writeError << '\n';
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(QStringLiteral("pickle_perf_probe"));
        return 1;
    }

    database.close();
    database = {};
    QSqlDatabase::removeDatabase(QStringLiteral("pickle_perf_probe"));

    QTextStream stdoutStream(stdout);
    stdoutStream << QJsonDocument(output).toJson(QJsonDocument::Compact) << '\n';
    return 0;
}
