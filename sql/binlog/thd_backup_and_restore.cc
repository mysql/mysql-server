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
#include "thd_backup_and_restore.h"
#include "sql/sql_class.h"

Thd_backup_and_restore::Thd_backup_and_restore(THD *backup_thd, THD *new_thd)
    : m_backup_thd(backup_thd),
      m_new_thd(new_thd),
      m_new_thd_old_real_id(new_thd->real_id),
      m_new_thd_old_thread_stack(new_thd->thread_stack) {
  assert(m_backup_thd != nullptr && m_new_thd != nullptr);
  // Reset the state of the current thd.
  m_backup_thd->restore_globals();

  m_new_thd->thread_stack = m_backup_thd->thread_stack;
  m_new_thd->store_globals();
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_mem_cnt_THD)(m_new_thd, &m_backup_cnt_thd);
#endif
}

/**
Restores to previous thd.
*/
Thd_backup_and_restore::~Thd_backup_and_restore() {
  /*
  Restore the global variables of the thd we previously attached to,
  to its original state. In other words, detach the m_new_thd.
  */
  m_new_thd->restore_globals();
  m_new_thd->real_id = m_new_thd_old_real_id;
  m_new_thd->thread_stack = m_new_thd_old_thread_stack;

  // Reset the global variables to the original state.
  m_backup_thd->store_globals();
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_mem_cnt_THD)(m_backup_cnt_thd, &m_dummy_cnt_thd);
#endif
}
