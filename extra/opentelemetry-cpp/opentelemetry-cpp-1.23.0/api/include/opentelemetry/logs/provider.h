// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mutex>

#include "opentelemetry/common/macros.h"
#include "opentelemetry/common/spin_lock_mutex.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace logs
{

#if OPENTELEMETRY_ABI_VERSION_NO < 2
class EventLoggerProvider;
#endif
class LoggerProvider;

/**
 * Stores the singleton global LoggerProvider.
 */
class OPENTELEMETRY_EXPORT Provider
{
public:
  /**
   * Returns the singleton LoggerProvider.
   *
   * By default, a no-op LoggerProvider is returned. This will never return a
   * nullptr LoggerProvider.
   */
  static nostd::shared_ptr<LoggerProvider> GetLoggerProvider() noexcept
  {
    std::lock_guard<common::SpinLockMutex> guard(GetLock());
    return nostd::shared_ptr<LoggerProvider>(GetProvider());
  }

  /**
   * Changes the singleton LoggerProvider.
   */
  static void SetLoggerProvider(const nostd::shared_ptr<LoggerProvider> &tp) noexcept
  {
    std::lock_guard<common::SpinLockMutex> guard(GetLock());
    GetProvider() = tp;
  }

#if OPENTELEMETRY_ABI_VERSION_NO < 2
  /**
   * Returns the singleton EventLoggerProvider.
   *
   * By default, a no-op EventLoggerProvider is returned. This will never return a
   * nullptr EventLoggerProvider.
   */
  OPENTELEMETRY_DEPRECATED static nostd::shared_ptr<EventLoggerProvider>
  GetEventLoggerProvider() noexcept
  {
    std::lock_guard<common::SpinLockMutex> guard(GetLock());
    return nostd::shared_ptr<EventLoggerProvider>(GetEventProvider());
  }

  /**
   * Changes the singleton EventLoggerProvider.
   */
  OPENTELEMETRY_DEPRECATED static void SetEventLoggerProvider(
      const nostd::shared_ptr<EventLoggerProvider> &tp) noexcept
  {
    std::lock_guard<common::SpinLockMutex> guard(GetLock());
    GetEventProvider() = tp;
  }
#endif

private:
  OPENTELEMETRY_API_SINGLETON static nostd::shared_ptr<LoggerProvider> &GetProvider() noexcept
  {
    static nostd::shared_ptr<LoggerProvider> provider(new NoopLoggerProvider);
    return provider;
  }

#if OPENTELEMETRY_ABI_VERSION_NO < 2
  OPENTELEMETRY_DEPRECATED
  OPENTELEMETRY_API_SINGLETON static nostd::shared_ptr<EventLoggerProvider> &
  GetEventProvider() noexcept
  {
#  if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#  elif defined(__GNUC__) && !defined(__clang__) && !defined(__apple_build_version__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  elif defined(__clang__) || defined(__apple_build_version__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
#  endif

    static nostd::shared_ptr<EventLoggerProvider> provider(new NoopEventLoggerProvider);
    return provider;

#  if defined(_MSC_VER)
#    pragma warning(pop)
#  elif defined(__GNUC__) && !defined(__clang__) && !defined(__apple_build_version__)
#    pragma GCC diagnostic pop
#  elif defined(__clang__) || defined(__apple_build_version__)
#    pragma clang diagnostic pop
#  endif
  }
#endif

  OPENTELEMETRY_API_SINGLETON static common::SpinLockMutex &GetLock() noexcept
  {
    static common::SpinLockMutex lock;
    return lock;
  }
};

}  // namespace logs
OPENTELEMETRY_END_NAMESPACE
