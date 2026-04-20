#pragma once

#include "domain/CancellationToken.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

struct ProcessResult {
    QByteArray standardOutput;
    QByteArray standardError;
    QString errorMessage;
    int exitCode = -1;
    bool canceled = false;
    bool timedOut = false;
    bool succeeded = false;
};

class ProcessRunner
{
public:
    ProcessResult run(
        const QString &program,
        const QStringList &arguments,
        int timeoutMs,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const;
};
