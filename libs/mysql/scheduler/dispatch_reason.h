// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SCHEDULER_DISPATCH_REASON_H
#define MYSQL_SCHEDULER_DISPATCH_REASON_H

namespace mysql::scheduler {

enum class Dispatch_reason {
  normal,
  unblocked,
};

inline thread_local Dispatch_reason current_dispatch_reason_tls =
    Dispatch_reason::normal;

inline Dispatch_reason current_dispatch_reason() {
  return current_dispatch_reason_tls;
}

class Scoped_dispatch_reason {
 public:
  explicit Scoped_dispatch_reason(Dispatch_reason reason)
      : m_previous(current_dispatch_reason_tls) {
    current_dispatch_reason_tls = reason;
  }

  ~Scoped_dispatch_reason() { current_dispatch_reason_tls = m_previous; }

  Scoped_dispatch_reason(const Scoped_dispatch_reason &) = delete;
  Scoped_dispatch_reason &operator=(const Scoped_dispatch_reason &) = delete;

 private:
  Dispatch_reason m_previous;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_DISPATCH_REASON_H
