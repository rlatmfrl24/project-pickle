#include "ProcessRunner.h"

#include <QElapsedTimer>
#include <QProcess>

namespace {
constexpr int ProcessPollIntervalMs = 100;
constexpr int ProcessStartTimeoutMs = 5000;
constexpr qsizetype MaxCapturedOutputBytes = 1024 * 1024;

QString trimmedProcessMessage(const QByteArray &message)
{
    return QString::fromUtf8(message).trimmed();
}

bool appendProcessOutput(QByteArray *target, const QByteArray &chunk)
{
    if (!target || chunk.isEmpty()) {
        return true;
    }

    if (target->size() + chunk.size() > MaxCapturedOutputBytes) {
        const qsizetype remainingBytes = MaxCapturedOutputBytes - target->size();
        if (remainingBytes > 0) {
            target->append(chunk.constData(), remainingBytes);
        }
        return false;
    }

    target->append(chunk);
    return true;
}
}

ProcessResult ProcessRunner::run(
    const QString &program,
    const QStringList &arguments,
    int timeoutMs,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    ProcessResult result;

    if (program.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Process program is empty.");
        return result;
    }

    if (cancellationToken && cancellationToken->isCancellationRequested()) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Operation canceled.");
        return result;
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();

    if (!process.waitForStarted(ProcessStartTimeoutMs)) {
        result.errorMessage = QStringLiteral("%1 could not be started.").arg(program);
        result.standardError = process.readAllStandardError();
        return result;
    }

    QElapsedTimer timer;
    timer.start();
    while (!process.waitForFinished(ProcessPollIntervalMs)) {
        if (!appendProcessOutput(&result.standardOutput, process.readAllStandardOutput())
            || !appendProcessOutput(&result.standardError, process.readAllStandardError())) {
            process.kill();
            process.waitForFinished();
            result.errorMessage = QStringLiteral("%1 produced too much output.").arg(program);
            return result;
        }

        if (cancellationToken && cancellationToken->isCancellationRequested()) {
            process.kill();
            process.waitForFinished();
            result.canceled = true;
            result.errorMessage = QStringLiteral("Operation canceled.");
            appendProcessOutput(&result.standardOutput, process.readAllStandardOutput());
            appendProcessOutput(&result.standardError, process.readAllStandardError());
            return result;
        }

        if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
            process.kill();
            process.waitForFinished();
            result.timedOut = true;
            result.errorMessage = QStringLiteral("%1 timed out.").arg(program);
            appendProcessOutput(&result.standardOutput, process.readAllStandardOutput());
            appendProcessOutput(&result.standardError, process.readAllStandardError());
            return result;
        }
    }

    if (!appendProcessOutput(&result.standardOutput, process.readAllStandardOutput())
        || !appendProcessOutput(&result.standardError, process.readAllStandardError())) {
        result.errorMessage = QStringLiteral("%1 produced too much output.").arg(program);
        return result;
    }
    result.exitCode = process.exitCode();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString errorText = trimmedProcessMessage(result.standardError);
        result.errorMessage = errorText.isEmpty()
            ? QStringLiteral("%1 failed with exit code %2.").arg(program).arg(process.exitCode())
            : errorText;
        return result;
    }

    result.succeeded = true;
    return result;
}
