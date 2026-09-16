/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
#ifndef THD_BACKUP_AND_RESTORE_INCLUDED
#define THD_BACKUP_AND_RESTORE_INCLUDED

#include "mysql/components/services/bits/my_thread_bits.h"
#include "sql/sql_class.h"

/**
  Helper class to switch to a new thread and then go back to the previous one,
  when the object is destroyed using RAII.

  This class is used to temporarily switch to another session (THD
  structure). It will set up thread specific "globals" correctly
  so that the POSIX thread looks exactly like the session attached to.
  However, PSI_thread info is not touched as it is required to show
  the actual physical view in PFS instrumentation i.e., it should
  depict as the real thread doing the work instead of thread it switched
  to.

  On destruction, the original session (which is supplied to the
  constructor) will be re-attached automatically. For example, with
  this code, the value of @c current_thd will be the same before and
  after execution of the code.

  @code
  {
    for (int i = 0 ; i < count ; ++i)
    {
      // here we are attached to current_thd
      // [...]
      Thd_backup_and_restore switch_thd(current_thd, other_thd[i]);
      // [...]
      // here we are attached to other_thd[i]
      // [...]
    }
    // here we are attached to current_thd
  }
  @endcode

  @warning The class is not designed to be inherited from.
 */

class Thd_backup_and_restore {
 public:
  /**
    Try to attach the POSIX thread to a session.

    @param[in] backup_thd    The thd to restore to when object is destructed.
    @param[in] new_thd       The thd to attach to.
   */
  Thd_backup_and_restore(THD *backup_thd, THD *new_thd);

  /**
      Restores to previous thd.
   */
  ~Thd_backup_and_restore();

 private:
  THD *m_backup_thd;
  THD *m_new_thd;
  THD *m_backup_cnt_thd;
  THD *m_dummy_cnt_thd;
  my_thread_t m_new_thd_old_real_id;
  const char *m_new_thd_old_thread_stack;
};

#endif /* THD_BACKUP_AND_RESTORE_INCLUDED */
