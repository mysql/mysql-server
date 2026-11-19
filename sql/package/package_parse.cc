/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "sql/package/package_parse.h"
#include "sql/package/proc.h"
#include "sql/sql_class.h"

namespace im {

/**
  Generate the proc execution command.

  @param[in]      THD       Thread context

  @retval         Sql_cmd   The Sql_cmd_proc object
*/
Sql_cmd *PT_package_proc::make_cmd(THD *thd) {
  LEX *const lex = thd->lex;
  Parse_context pc(thd, lex->current_query_block());

  if (m_opt_expr_list != nullptr && m_opt_expr_list->contextualize(&pc))
    return nullptr; /* purecov: inspected */

  mem_root_deque<Item *> *proc_args = nullptr;
  if (m_opt_expr_list != nullptr) proc_args = &m_opt_expr_list->value;

  Sql_cmd *sql_cmd = m_proc->evoke_cmd(thd, proc_args);
  lex->sql_command = sql_cmd->sql_command_code();

  return sql_cmd;
}

} /* namespace im */
