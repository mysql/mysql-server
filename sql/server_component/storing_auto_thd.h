/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef STORING_AUTO_THD_H
#define STORING_AUTO_THD_H

#include <sql/auto_thd.h>
#include <sql/current_thd.h>
#include <sql/sql_lex.h>
#include <sql/sql_thd_internal_api.h>
#include "sql/auth/auth_acls.h"

THD *create_internal_thd_ctx(Sctx_ptr<Security_context> &ctx);
void destroy_internal_thd_ctx(THD *thd, Sctx_ptr<Security_context> &ctx);

/**
  A version of Auto_THD that:
   - doesn't catch or print the error onto the error log but just passes it up
   - stores and restores the current_thd correctly
*/
class Storing_auto_THD {
  THD *m_previous_thd, *thd;
  Sctx_ptr<Security_context> ctx;

 public:
  Storing_auto_THD() {
    m_previous_thd = current_thd;
    /* Allocate thread local memory if necessary. */
    if (!m_previous_thd) {
      my_thread_init();
    }
    thd = create_internal_thd_ctx(ctx);
  }

  ~Storing_auto_THD() {
    if (m_previous_thd) {
      Diagnostics_area *prev_da = m_previous_thd->get_stmt_da();
      Diagnostics_area *curr_da = thd->get_stmt_da();
      /* We need to put any errors in the DA as well as the condition list. */
      if (curr_da->is_error())
        prev_da->set_error_status(curr_da->mysql_errno(),
                                  curr_da->message_text(),
                                  curr_da->returned_sqlstate());

      prev_da->copy_sql_conditions_from_da(m_previous_thd, curr_da);
    }
    destroy_internal_thd_ctx(thd, ctx);
    if (!m_previous_thd) {
      my_thread_end();
    }
    if (m_previous_thd) {
      m_previous_thd->store_globals();
    }
  }
  THD *get_THD() { return thd; }
};

#endif /* STORING_AUTO_THD_H */
