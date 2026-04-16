#include "MediaLibraryModel.h"

MediaLibraryModel::MediaLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadMockItems();
}

int MediaLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_items.size());
}

QVariant MediaLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    return valueForRole(m_items.at(static_cast<size_t>(index.row())), role);
}

QHash<int, QByteArray> MediaLibraryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {FileNameRole, "fileName"},
        {FilePathRole, "filePath"},
        {FileSizeRole, "fileSize"},
        {DurationRole, "duration"},
        {ResolutionRole, "resolution"},
        {CodecRole, "codec"},
        {DescriptionRole, "description"},
        {TagsRole, "tags"},
        {ReviewStatusRole, "reviewStatus"},
        {RatingRole, "rating"},
        {ModifiedAtRole, "modifiedAt"},
    };
}

QVariantMap MediaLibraryModel::get(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    return toMap(m_items.at(static_cast<size_t>(row)));
}

QVariant MediaLibraryModel::valueForRole(const MediaItem &item, int role) const
{
    switch (role) {
    case IdRole:
        return item.id;
    case FileNameRole:
        return item.fileName;
    case FilePathRole:
        return item.filePath;
    case FileSizeRole:
        return item.fileSize;
    case DurationRole:
        return item.duration;
    case ResolutionRole:
        return item.resolution;
    case CodecRole:
        return item.codec;
    case DescriptionRole:
        return item.description;
    case TagsRole:
        return item.tags;
    case ReviewStatusRole:
        return item.reviewStatus;
    case RatingRole:
        return item.rating;
    case ModifiedAtRole:
        return item.modifiedAt;
    default:
        return {};
    }
}

QVariantMap MediaLibraryModel::toMap(const MediaItem &item) const
{
    return {
        {"id", item.id},
        {"fileName", item.fileName},
        {"filePath", item.filePath},
        {"fileSize", item.fileSize},
        {"duration", item.duration},
        {"resolution", item.resolution},
        {"codec", item.codec},
        {"description", item.description},
        {"tags", item.tags},
        {"reviewStatus", item.reviewStatus},
        {"rating", item.rating},
        {"modifiedAt", item.modifiedAt},
    };
}

void MediaLibraryModel::loadMockItems()
{
    m_items = {
        {1, "morning-review.mp4", "D:/Media/Inbox/morning-review.mp4", "184 MB", "03:42", "1920 x 1080", "H.264 / AAC", "Short capture queued for first-pass review.", {"inbox", "short"}, "New", 2, "2026-04-10"},
        {2, "city-walk-cut.mov", "D:/Media/Projects/city-walk-cut.mov", "1.2 GB", "18:05", "3840 x 2160", "HEVC / AAC", "Stabilized source clip with a few marked sections.", {"project", "4k"}, "Reviewing", 4, "2026-04-09"},
        {3, "interview-angle-a.mkv", "D:/Media/Interviews/interview-angle-a.mkv", "2.4 GB", "42:11", "1920 x 1080", "H.264 / PCM", "Primary interview angle. Needs tag cleanup later.", {"interview"}, "Keep", 5, "2026-04-08"},
        {4, "screen-capture-0412.mp4", "D:/Media/Captures/screen-capture-0412.mp4", "356 MB", "07:33", "2560 x 1440", "H.264 / AAC", "Desktop capture for workflow notes.", {"capture", "notes"}, "New", 3, "2026-04-12"},
        {5, "archive-sample-17.avi", "E:/Archive/sample/archive-sample-17.avi", "728 MB", "12:48", "1280 x 720", "MPEG-4 / MP3", "Older archive item kept as compatibility sample.", {"archive"}, "Maybe", 2, "2026-03-28"},
        {6, "training-segment-b.mp4", "D:/Media/Training/training-segment-b.mp4", "912 MB", "24:06", "1920 x 1080", "H.264 / AAC", "Training clip with useful middle section.", {"training"}, "Reviewing", 4, "2026-04-02"},
        {7, "reference-shot-tabletop.mov", "D:/Media/Reference/reference-shot-tabletop.mov", "640 MB", "05:19", "3840 x 2160", "ProRes / PCM", "Reference lighting sample for tabletop shots.", {"reference", "4k"}, "Keep", 5, "2026-03-30"},
        {8, "phone-import-2219.mp4", "D:/Media/Phone/phone-import-2219.mp4", "98 MB", "01:51", "1920 x 1080", "HEVC / AAC", "Phone import that still needs a better name.", {"phone"}, "Rename", 1, "2026-04-11"},
        {9, "camera-test-lowlight.mkv", "D:/Media/Tests/camera-test-lowlight.mkv", "1.8 GB", "09:27", "3840 x 2160", "HEVC / AAC", "Low-light camera test for quality comparison.", {"test", "4k"}, "Keep", 4, "2026-03-21"},
        {10, "duplicate-candidate.mp4", "E:/Archive/Review/duplicate-candidate.mp4", "184 MB", "03:42", "1920 x 1080", "H.264 / AAC", "Possible duplicate candidate for later tooling.", {"review"}, "Check", 2, "2026-04-01"},
    };
}
