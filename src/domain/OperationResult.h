#pragma once

#include <QString>

template <typename T>
struct OperationResult {
    T value {};
    QString errorMessage;
    bool succeeded = false;
};

struct VoidResult {
    QString errorMessage;
    bool succeeded = false;
};
