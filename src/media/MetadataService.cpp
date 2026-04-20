#include "MetadataService.h"

#include "core/ProcessRunner.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include <cmath>

namespace {
constexpr int FfprobeTimeoutMs = 60000;

QString trimmedProcessMessage(const QByteArray &message)
{
    return QString::fromUtf8(message).trimmed();
}

qint64 parseInteger(const QJsonValue &value)
{
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }

    bool ok = false;
    const qint64 parsedValue = value.toString().toLongLong(&ok);
    return ok ? parsedValue : 0;
}

double parseDouble(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toDouble();
    }

    bool ok = false;
    const double parsedValue = value.toString().toDouble(&ok);
    return ok ? parsedValue : 0.0;
}

qint64 parseDurationMs(const QJsonValue &value)
{
    const double seconds = parseDouble(value);
    if (seconds <= 0.0) {
        return 0;
    }

    return static_cast<qint64>(std::llround(seconds * 1000.0));
}

double parseFrameRate(const QString &frameRateText)
{
    if (frameRateText.isEmpty() || frameRateText == QStringLiteral("0/0")) {
        return 0.0;
    }

    const QStringList parts = frameRateText.split(QLatin1Char('/'));
    if (parts.size() == 2) {
        bool numeratorOk = false;
        bool denominatorOk = false;
        const double numerator = parts.at(0).toDouble(&numeratorOk);
        const double denominator = parts.at(1).toDouble(&denominatorOk);
        if (numeratorOk && denominatorOk && denominator != 0.0) {
            return numerator / denominator;
        }
    }

    bool ok = false;
    const double parsedValue = frameRateText.toDouble(&ok);
    return ok ? parsedValue : 0.0;
}

bool hasUsefulMetadata(const MediaMetadata &metadata)
{
    return metadata.durationMs > 0
        || metadata.bitrate > 0
        || metadata.frameRate > 0.0
        || metadata.width > 0
        || metadata.height > 0
        || !metadata.videoCodec.isEmpty()
        || !metadata.audioCodec.isEmpty();
}
}

MetadataExtractionResult MetadataService::extract(
    const QString &filePath,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    return extract(filePath, QStringLiteral("ffprobe"), cancellationToken);
}

MetadataExtractionResult MetadataService::extract(
    const QString &filePath,
    const QString &ffprobeProgram,
    const std::shared_ptr<CancellationToken> &cancellationToken) const
{
    MetadataExtractionResult result;

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        result.errorMessage = QStringLiteral("File does not exist.");
        return result;
    }

    ProcessRunner processRunner;
    const QString program = ffprobeProgram.trimmed().isEmpty()
        ? QStringLiteral("ffprobe")
        : ffprobeProgram.trimmed();
    const ProcessResult processResult = processRunner.run(
        program,
        {
            QStringLiteral("-v"),
            QStringLiteral("error"),
            QStringLiteral("-print_format"),
            QStringLiteral("json"),
            QStringLiteral("-show_format"),
            QStringLiteral("-show_streams"),
            fileInfo.absoluteFilePath(),
        },
        FfprobeTimeoutMs,
        cancellationToken);

    if (processResult.canceled) {
        result.canceled = true;
        result.errorMessage = QStringLiteral("Metadata canceled.");
        return result;
    }

    if (processResult.timedOut) {
        result.errorMessage = QStringLiteral("ffprobe timed out.");
        return result;
    }

    if (!processResult.succeeded) {
        result.errorMessage = processResult.errorMessage.isEmpty()
            ? QStringLiteral("ffprobe failed.")
            : processResult.errorMessage;
        if (result.errorMessage.contains(QStringLiteral("could not be started"))) {
            result.errorMessage = QStringLiteral("ffprobe could not be started. Check the configured ffprobe path or PATH.");
        }
        return result;
    }

    return parseFfprobeJson(processResult.standardOutput, processResult.standardError);
}

MetadataExtractionResult MetadataService::parseFfprobeJson(
    const QByteArray &json,
    const QByteArray &errorOutput)
{
    MetadataExtractionResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString ffprobeError = trimmedProcessMessage(errorOutput);
        result.errorMessage = ffprobeError.isEmpty()
            ? QStringLiteral("Unable to parse ffprobe output: %1").arg(parseError.errorString())
            : ffprobeError;
        return result;
    }

    const QJsonObject rootObject = document.object();
    const QJsonObject formatObject = rootObject.value(QStringLiteral("format")).toObject();
    MediaMetadata metadata;
    metadata.durationMs = parseDurationMs(formatObject.value(QStringLiteral("duration")));
    metadata.bitrate = parseInteger(formatObject.value(QStringLiteral("bit_rate")));

    const QJsonArray streams = rootObject.value(QStringLiteral("streams")).toArray();
    for (const QJsonValue &streamValue : streams) {
        const QJsonObject streamObject = streamValue.toObject();
        const QString codecType = streamObject.value(QStringLiteral("codec_type")).toString();

        if (codecType == QStringLiteral("video") && metadata.videoCodec.isEmpty()) {
            metadata.videoCodec = streamObject.value(QStringLiteral("codec_name")).toString();
            metadata.width = streamObject.value(QStringLiteral("width")).toInt();
            metadata.height = streamObject.value(QStringLiteral("height")).toInt();
            metadata.frameRate = parseFrameRate(streamObject.value(QStringLiteral("avg_frame_rate")).toString());
            if (metadata.frameRate <= 0.0) {
                metadata.frameRate = parseFrameRate(streamObject.value(QStringLiteral("r_frame_rate")).toString());
            }
            if (metadata.durationMs <= 0) {
                metadata.durationMs = parseDurationMs(streamObject.value(QStringLiteral("duration")));
            }
            if (metadata.bitrate <= 0) {
                metadata.bitrate = parseInteger(streamObject.value(QStringLiteral("bit_rate")));
            }
        } else if (codecType == QStringLiteral("audio") && metadata.audioCodec.isEmpty()) {
            metadata.audioCodec = streamObject.value(QStringLiteral("codec_name")).toString();
            if (metadata.durationMs <= 0) {
                metadata.durationMs = parseDurationMs(streamObject.value(QStringLiteral("duration")));
            }
            if (metadata.bitrate <= 0) {
                metadata.bitrate = parseInteger(streamObject.value(QStringLiteral("bit_rate")));
            }
        }
    }

    if (!hasUsefulMetadata(metadata)) {
        result.errorMessage = QStringLiteral("No media metadata found.");
        return result;
    }

    result.metadata = metadata;
    result.succeeded = true;
    return result;
}
