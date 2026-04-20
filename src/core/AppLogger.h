#pragma once

#include <QString>

class AppLogger
{
public:
    static void initialize();
    static QString logPath();
    static QString rotatedLogPath();
};
