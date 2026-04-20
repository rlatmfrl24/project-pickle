#include "CancellationToken.h"

void CancellationToken::cancel()
{
    m_canceled.store(true);
}

void CancellationToken::reset()
{
    m_canceled.store(false);
}

bool CancellationToken::isCancellationRequested() const
{
    return m_canceled.load();
}
