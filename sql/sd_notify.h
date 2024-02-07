/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <sstream>

// build with -DWITH_SYSTEMD_DEBUG=1 as needed!
namespace sysd {
void notify_connect();
void notify();

template <typename T, typename... Ts>
inline void notify(T, Ts...);

/**
  Class wrapping the "globals" as static members so that they can only
  be accessed from the friend-declared notify functions.
 */
class NotifyGlobals {
  static int socket;
  static std::stringstream fmt;

  NotifyGlobals() = delete;

  friend void notify_connect();
  friend void notify();
  template <typename T, typename... Ts>
  friend void notify(T t, Ts... ts);
};

/**
  Takes a variable number of arguments of different type and formats
  them on NotifyGlobals::fmt, and sends result to notification socket.

  @param t current argument to format
  @param ts remaining args parameter pack for recursive call
*/
template <typename T, typename... Ts>
inline void notify(T t [[maybe_unused]], Ts... ts [[maybe_unused]]) {
#ifndef _WIN32
#ifndef WITH_SYSTEMD_DEBUG
  if (NotifyGlobals::socket == -1) {
    return;
  }
#endif /* WITH_SYSTEMD_DEBUG */
  NotifyGlobals::fmt << t;
  notify(ts...);
#endif /* not defined _WIN32 */
}
}  // namespace sysd
