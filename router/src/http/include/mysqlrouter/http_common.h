/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_HTTP_COMMON_INCLUDED
#define MYSQLROUTER_HTTP_COMMON_INCLUDED

#include "mysqlrouter/http_common_export.h"

// http_common is a wrapper on `libevent`, thus
// all header from `libevent` should be included in "c" file.
// We need this header to have `evutil_socket_t` because it
// defines socket in different way than most libraries do.
// No function nor structure from "util.h" should be used by
// "wrapper-importers".
#include <event2/util.h>

#include <bitset>
#include <ctime>
#include <functional>  // std::function
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "my_compiler.h"
#include "my_io.h"
#include "my_macros.h"

#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/tls_context.h"

// http_common.cc

/**
 * `libevent` global state management
 */
class HTTP_COMMON_EXPORT Event {
 public:
  enum class Log { Debug, Error, Warning, Message };
  enum class DebugLogLevel : uint32_t { None = 0, All = ~(uint32_t)0 };
  using CallbackLog = void (*)(const Log log, const char *message);
  Event() = delete;

 public:
  static bool initialize_threads();
  static void shutdown();
  static void set_log_callback(const CallbackLog);
  static void enable_debug_logging(const DebugLogLevel which);

  static bool has_ssl();

  static CallbackLog cbLog_;
};

/**
 * Flags that represents which I/O events should be monitored
 */
namespace EventFlags {
using type = int;
using pos_type = unsigned;
namespace Pos {
constexpr pos_type Timeout = 0;
constexpr pos_type Read = 1;
constexpr pos_type Write = 2;
constexpr pos_type Signal = 3;

constexpr pos_type _LAST = Signal;
}  // namespace Pos
using Bitset = std::bitset<Pos::_LAST + 1>;

constexpr type Timeout{1 << Pos::Timeout};
constexpr type Read{1 << Pos::Read};
constexpr type Write{1 << Pos::Write};
constexpr type Signal{1 << Pos::Signal};
}  // namespace EventFlags
using EventBaseSocket = evutil_socket_t;

MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning for now.
// TODO(lkotula) Use proper 64bit/32bit signed/unsigned type based on platform.
// 'initializing': truncation of constant value.
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4309)
MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wconstant-conversion")
const int kEventBaseInvalidSocket = INVALID_SOCKET;
MY_COMPILER_DIAGNOSTIC_POP()

/**
 * Main event registration and dispatch `engine`
 *
 * Wrapper for `event_base` structure from `libevent`.
 */
class HTTP_COMMON_EXPORT EventBase {
 public:
  using SocketHandle = EventBaseSocket;
  using CallbackEvent = void (*)(SocketHandle, short, void *);
  EventBase();
  // Needed because the class defined dtor
  EventBase(EventBase &&event);
  // Needed by pimpl
  ~EventBase();

 public:
  /**
   * Register new event notification
   *
   * Depending on arguments, the function may register notification for:
   * socket read/write, timeout, signal handler.
   *
   * @retval false if `registration` fails
   * @retval true if event was scheduled
   */
  bool once(const SocketHandle fd, const EventFlags::Bitset events,
            CallbackEvent cb, void *arg, const struct timeval *tv);

  /**
   * Stop dispatching
   *
   * While some thread is blocked inside `dispatch` method, some other
   * thread might call this function to notify and break the dispatching
   * loop inside `dispatch`.
   *
   * @retval false if breaking the loop fails
   * @retval true if event breaking the loop was scheduled
   */
  bool loop_exit(const struct timeval *tv);

  /**
   * Wait for registered notifications, and when they become `active`
   * dispatch them. If there is no event registered return.
   *
   * @return 0 if successful, -1 if an error occurred, or 1 if we exited because
   * no events were pending or active.
   */
  int dispatch();

 private:
  struct impl;
  friend class EventHttp;
  friend class EventBuffer;

  EventBase(std::unique_ptr<impl> &&pImpl);

  std::unique_ptr<impl> pImpl_;
};

/**
 * Flags that represents different `bufferevent` options.
 */
namespace EventBufferOptionsFlags {
using type = int;
using pos_type = unsigned;
namespace Pos {
constexpr pos_type CloseOnFree = 0;
constexpr pos_type ThreadSafe = 1;
constexpr pos_type DeferCallbacks = 2;
constexpr pos_type UnlockCallbacks = 3;

constexpr pos_type _LAST = UnlockCallbacks;
}  // namespace Pos
using Bitset = std::bitset<Pos::_LAST + 1>;

constexpr type CloseOnFree{1 << Pos::CloseOnFree};
constexpr type ThreadSafe{1 << Pos::ThreadSafe};
constexpr type DeferCallbacks{1 << Pos::DeferCallbacks};
constexpr type UnlockCallbacks{1 << Pos::UnlockCallbacks};
}  // namespace EventBufferOptionsFlags

/**
 * Enables bufforing of I/O for a socket
 *
 * Wrapper for `bufferevent` structure from `libevent`.
 * Additionally this class allows custom processing, like
 * SSL.
 *
 * Notice: For now the functionality is limited to minimum.
 */
class HTTP_COMMON_EXPORT EventBuffer {
 public:
  // State of SSL-connection that is passed to this structure.
  enum class SslState { Open = 0, Connecting = 1, Accepting = 2 };
  using SocketHandle = EventBaseSocket;

  /**
   * Initialize the buffer, with OpenSSL processing.
   *
   * This c-tor, creates new SSL-connection from `TlsContext`, which means
   * that `state` can only be set to `SslState::Connecting` or
   * `SslState::Accepting`.
   */
  EventBuffer(EventBase *, const SocketHandle socket, TlsContext *tls_context,
              const SslState state,
              const EventBufferOptionsFlags::Bitset &options);
  EventBuffer(EventBuffer &&);
  ~EventBuffer();

 private:
  friend class EventHttp;
  struct impl;

  std::unique_ptr<impl> pImpl_;
};

#endif
