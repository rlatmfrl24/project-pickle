#pragma once

#include <atomic>

class CancellationToken
{
public:
    void cancel();
    void reset();
    bool isCancellationRequested() const;

private:
    std::atomic_bool m_canceled = false;
};
