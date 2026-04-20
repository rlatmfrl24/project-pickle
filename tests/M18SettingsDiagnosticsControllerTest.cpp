#include "app/DiagnosticsController.h"
#include "app/SettingsController.h"
#include "app/WorkEventLog.h"
#include "ports/IExternalToolResolver.h"
#include "ports/ISettingsRepository.h"

#include <QDir>
#include <QVariantMap>
#include <QtTest/QtTest>

namespace {
class FakeSettingsRepository final : public ISettingsRepository
{
public:
    AppSettings load() override { return storedSettings; }

    bool save(const AppSettings &settings) override
    {
        if (!saveSucceeds) {
            m_lastError = QStringLiteral("save failed");
            return false;
        }
        storedSettings = settings;
        ++saveCount;
        m_lastError.clear();
        return true;
    }

    QString lastError() const override { return m_lastError; }

    AppSettings storedSettings;
    bool saveSucceeds = true;
    int saveCount = 0;

private:
    QString m_lastError;
};

class FakeExternalToolResolver final : public IExternalToolResolver
{
public:
    ExternalToolStatus probeTool(const QString &toolName, const QString &configuredPath) const override
    {
        ExternalToolStatus status;
        status.toolName = toolName;
        status.configuredPath = configuredPath;
        status.resolvedProgram = programForTool(toolName, configuredPath);
        status.available = true;
        status.succeeded = true;
        status.versionText = QStringLiteral("%1 1.0").arg(toolName);
        status.configuredPathUsed = !configuredPath.isEmpty();
        return status;
    }

    QString programForTool(const QString &toolName, const QString &configuredPath) const override
    {
        return configuredPath.isEmpty()
            ? QStringLiteral("resolved-%1.exe").arg(toolName)
            : configuredPath;
    }
};
}

class M18SettingsDiagnosticsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void settingsControllerPersistsNormalizesAndResolvesTools();
    void diagnosticsReportRedactsLocalPathsAndKeepsCoreStrings();
    void workEventLogKeepsRecentTwoHundredEvents();
};

void M18SettingsDiagnosticsControllerTest::settingsControllerPersistsNormalizesAndResolvesTools()
{
    FakeSettingsRepository repository;
    repository.storedSettings.sortKey = QStringLiteral("MODIFIED");
    repository.storedSettings.playerVolume = 2.0;
    repository.storedSettings.ffprobePath = QStringLiteral(" C:/Tools/ffprobe.exe ");

    FakeExternalToolResolver resolver;
    SettingsController controller(&repository, &resolver);
    QCOMPARE(controller.settings().sortKey, QStringLiteral("modified"));
    QCOMPARE(controller.settings().playerVolume, 1.0);
    QCOMPARE(controller.ffprobeProgram(), QStringLiteral("C:/Tools/ffprobe.exe"));

    QVariantMap update;
    update.insert(QStringLiteral("sortKey"), QStringLiteral("size"));
    update.insert(QStringLiteral("showThumbnails"), false);
    update.insert(QStringLiteral("ffmpegPath"), QStringLiteral("C:/Tools/ffmpeg.exe"));
    const SettingsUpdateResult saveResult = controller.saveSettings(update);
    QVERIFY(saveResult.settingsChanged);
    QVERIFY(saveResult.libraryQueryChanged);
    QVERIFY(saveResult.thumbnailVisibilityChanged);
    QCOMPARE(saveResult.status, QStringLiteral("Settings saved"));
    QCOMPARE(repository.saveCount, 1);
    QCOMPARE(repository.storedSettings.sortKey, QStringLiteral("size"));
    QCOMPARE(repository.storedSettings.ffmpegPath, QStringLiteral("C:/Tools/ffmpeg.exe"));

    const ToolValidationResult validation = controller.validateExternalTools();
    QVERIFY(validation.allOk);
    QCOMPARE(validation.status, QStringLiteral("ffprobe OK, ffmpeg OK"));
    QCOMPARE(validation.ffmpeg.resolvedProgram, QStringLiteral("C:/Tools/ffmpeg.exe"));

    const SettingsUpdateResult resetResult = controller.resetToolPathsToPath();
    QVERIFY(resetResult.settingsChanged);
    QCOMPARE(resetResult.status, QStringLiteral("Tool paths reset to PATH"));
    QCOMPARE(controller.ffmpegProgram(), QStringLiteral("resolved-ffmpeg.exe"));
}

void M18SettingsDiagnosticsControllerTest::diagnosticsReportRedactsLocalPathsAndKeepsCoreStrings()
{
    DiagnosticsContext context;
    context.databaseReady = true;
    context.databaseStatus = QStringLiteral("Ready");
    context.databasePath = QDir::toNativeSeparators(QDir::home().filePath(QStringLiteral("Pickle/private.sqlite3")));
    context.currentScanRoot = QDir::toNativeSeparators(QDir::home().filePath(QStringLiteral("Videos")));
    context.libraryItems = 7;
    context.settings.ffprobePath = QDir::toNativeSeparators(QDir::home().filePath(QStringLiteral("Tools/ffprobe.exe")));
    context.settings.ffmpegPath.clear();
    context.toolsStatus = QStringLiteral("ffprobe OK, ffmpeg OK");
    context.settingsStatus = QStringLiteral("Settings saved");

    WorkEventLog log;
    QVERIFY(log.append(QStringLiteral("settings"), QStringLiteral("Settings saved")));
    context.workEvents = log.events();

    DiagnosticsController controller;
    const QString report = controller.diagnosticReport(context);
    QVERIFY(report.contains(QStringLiteral("Pickle Diagnostic Report")));
    QVERIFY(report.contains(QStringLiteral("Database status: Ready")));
    QVERIFY(report.contains(QStringLiteral("ffmpeg path: PATH")));
    QVERIFY(report.contains(QStringLiteral("<home>/private.sqlite3")));
    QVERIFY(!report.contains(context.databasePath));
    QVERIFY(!report.contains(context.settings.ffprobePath));
}

void M18SettingsDiagnosticsControllerTest::workEventLogKeepsRecentTwoHundredEvents()
{
    WorkEventLog log;
    for (int index = 0; index < 205; ++index) {
        QVERIFY(log.append(QStringLiteral("test"), QStringLiteral("event-%1").arg(index)));
    }

    const QVariantList events = log.events();
    QCOMPARE(events.size(), 200);
    QCOMPARE(events.first().toMap().value(QStringLiteral("message")).toString(), QStringLiteral("event-204"));
    QCOMPARE(events.last().toMap().value(QStringLiteral("message")).toString(), QStringLiteral("event-5"));

    QVERIFY(log.clear());
    QVERIFY(log.isEmpty());
}

QTEST_MAIN(M18SettingsDiagnosticsControllerTest)

#include "M18SettingsDiagnosticsControllerTest.moc"
