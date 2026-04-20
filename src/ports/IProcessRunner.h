#pragma once

#include "domain/CancellationToken.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

struct ProcessExecutionResult {
    QByteArray standardOutput;
    QByteArray standardError;
    QString errorMessage;
    int exitCode = -1;
    bool timedOut = false;
    bool canceled = false;
    bool outputLimitExceeded = false;
    bool succeeded = false;
};

class IProcessRunner
{
public:
    virtual ~IProcessRunner() = default;
    virtual ProcessExecutionResult run(
        const QString &program,
        const QStringList &arguments,
        int timeoutMs,
        int maxOutputBytes,
        const std::shared_ptr<CancellationToken> &cancellationToken = nullptr) const = 0;
};
