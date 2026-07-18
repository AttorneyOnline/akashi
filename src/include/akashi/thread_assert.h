#pragma once

#include <QThread>

// Both asserts are debug-only by design: Q_ASSERT compiles out of release
// builds, so a broken threading contract fails loudly in development but
// costs nothing in production.

// Asserts that the caller runs on this QObject's owning thread.
#define AKASHI_ASSERT_THREAD_AFFINITY() \
    Q_ASSERT(QThread::currentThread() == thread())

// Asserts that the caller runs on the thread stored in m_owner_thread.
// The class must initialise m_owner_thread = QThread::currentThread() in its ctor.
#define AKASHI_ASSERT_OWNER_THREAD() \
    Q_ASSERT(QThread::currentThread() == m_owner_thread)
