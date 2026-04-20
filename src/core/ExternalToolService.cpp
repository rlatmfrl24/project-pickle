#include "ExternalToolService.h"

#include "core/ProcessRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace {
constexpr int ToolValidationTimeoutMs = 5000;

QString firstLine(const QByteArray &output)
{
    const QString text = QString::fromUtf8(output).trimmed();
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}
}

ExternalToolStatus ExternalToolService::validateFfprobe(const QString &configuredPath) const
{
    return validateTool(QStringLiteral("ffprobe"), configuredPath);
}

ExternalToolStatus ExternalToolService::validateFfmpeg(const QString &configuredPath) const
{
    return validateTool(QStringLiteral("ffmpeg"), configuredPath);
}

QString ExternalToolService::programForTool(const QString &toolName, const QString &configuredPath)
{
    const QString trimmedPath = configuredPath.trimmed();
    return trimmedPath.isEmpty() ? toolName : QDir::toNativeSeparators(QFileInfo(trimmedPath).absoluteFilePath());
}

ExternalToolStatus ExternalToolService::validateTool(const QString &toolName, const QString &configuredPath) const
{
    ExternalToolStatus status;
    status.toolName = toolName;
    status.configuredPath = configuredPath.trimmed();
    status.configuredPathUsed = !status.configuredPath.isEmpty();
    status.resolvedProgram = programForTool(toolName, status.configuredPath);

    if (status.configuredPathUsed) {
        const QFileInfo toolInfo(status.configuredPath);
        if (!toolInfo.exists() || !toolInfo.isFile()) {
            status.errorMessage = QStringLiteral("%1 path does not exist.").arg(toolName);
            return status;
        }
    }

    ProcessRunner runner;
    const ProcessResult result = runner.run(status.resolvedProgram, {QStringLiteral("-version")}, ToolValidationTimeoutMs);
    if (!result.succeeded) {
        status.errorMessage = result.errorMessage.isEmpty()
            ? QStringLiteral("%1 is not available.").arg(toolName)
            : result.errorMessage;
        return status;
    }

    status.available = true;
    status.succeeded = true;
    status.versionText = firstLine(result.standardOutput);
    if (status.versionText.isEmpty()) {
        status.versionText = QStringLiteral("%1 is available.").arg(toolName);
    }

    return status;
}
