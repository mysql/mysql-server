/* Copyright (c) 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef THREAD_STAGE_GUARD_H_INCLUDED
#define THREAD_STAGE_GUARD_H_INCLUDED

#include "mysql/components/services/bits/psi_stage_bits.h"  // PSI_stage_info
#include "sql/sql_class.h"                                  // THD

namespace raii {

/// RAII guard that sets a thread stage, and restores the previous
/// stage when going out of scope.
class Thread_stage_guard {
 public:
  Thread_stage_guard() = delete;

  /// Set the given stage for the session, and remember the previous
  /// stage in a member variable.
  ///
  /// @param[in,out] thd Session object that should change stage.
  ///
  /// @param new_stage The new stage to use for THD.
  ///
  /// @param func Name of the calling function.
  ///
  /// @param file Filename of the caller.
  ///
  /// @param line Line number of the caller.
  Thread_stage_guard(THD *thd, const PSI_stage_info &new_stage,
                     const char *func, const char *file,
                     const unsigned int line)
      : m_thd(thd), m_func(func), m_file(file), m_line(line) {
    thd->enter_stage(&new_stage, &old_stage, func, file, line);
  }

  Thread_stage_guard(const Thread_stage_guard &) = delete;
  Thread_stage_guard(Thread_stage_guard &&) = delete;
  Thread_stage_guard &operator=(const Thread_stage_guard &) = delete;
  Thread_stage_guard &operator=(Thread_stage_guard &&) = delete;

  /// Revert the old stage that was used before this object's
  /// constructor was invoked.
  ///
  /// @note This will set the function/filename/line number relating
  /// to where the Thread_stage_guard was created, not where it went
  /// out of scope.
  ~Thread_stage_guard() {
    m_thd->enter_stage(&old_stage, nullptr, m_func, m_file, m_line);
  }

 private:
  /// The previous stage.
  PSI_stage_info old_stage;
  /// The session.
  THD *m_thd;
  /// The name of the calling function.
  const char *m_func;
  /// The filename of the caller.
  const char *m_file;
  /// The Line number of the caller.
  const unsigned int m_line;
};

/// Sets the thread stage for the given thread, and make it restore
/// the previous stage at the end of the code block.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define THD_STAGE_GUARD(thd, new_stage)                      \
  raii::Thread_stage_guard _thread_stage_guard_##new_stage { \
    (thd), (new_stage), __func__, __FILE__, __LINE__         \
  }
// NOLINTEND(cppcoreguidelines-macro-usage)

}  // namespace raii

#endif  /// THREAD_STAGE_GUARD_H_INCLUDED
