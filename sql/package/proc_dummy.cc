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

#include "my_dbug.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/protocol_classic.h"

#include "sql/package/package_parse.h"
#include "sql/package/proc.h"
#include "sql/package/proc_dummy.h"
#include "sql/sql_class.h"

/**
  Dummy proc definition.

  It was used to demostrate how to define a native procedure,
  so it only take effect on DBUG mode.
*/
namespace im {

/* The schema of dummy and dummy_2 proc */
const LEX_CSTRING PROC_DUMMY_SCHEMA = {STRING_WITH_LEN("mysql")};

Proc *Proc_dummy::instance() {
  static Proc_dummy *proc = new Proc_dummy(key_memory_package);

  return proc;
}

Sql_cmd *Proc_dummy::evoke_cmd(THD *thd, mem_root_deque<Item *> *list) const {
  return new (thd->mem_root) Sql_cmd_type(thd, list, this);
}

bool Sql_cmd_proc_dummy::pc_execute(THD *) {
  bool error = false;
  DBUG_ENTER("Sql_cmd_proc_dummy::pc_execute");
  assert(m_proc->get_result_type() != Proc::Result_type::RESULT_NONE);
  /**
    Do nothing for dummy:
      - Default access check
      - Default parameter check
      - Default send result
  */
  DBUG_RETURN(error);
}

Proc *Proc_dummy_2::instance() {
  static Proc_dummy_2 *proc = new Proc_dummy_2(key_memory_package);

  return proc;
}

Sql_cmd *Proc_dummy_2::evoke_cmd(THD *thd, mem_root_deque<Item *> *list) const {
  return new (thd->mem_root) Sql_cmd_type(thd, list, this);
}

bool Sql_cmd_proc_dummy_2::pc_execute(THD *) {
  bool error = false;
  DBUG_ENTER("Sql_cmd_proc_dummy_2::pc_execute");
  /**
    Do nothing for dummy_2:
      - Default access check
      - Default parameter check
      - Override send result
  */
  DBUG_RETURN(error);
}

/**
  Dummy2 result format:
    -- name
    -- id
*/
void Sql_cmd_proc_dummy_2::send_result(THD *thd, bool error) {
  Protocol *protocol = thd->get_protocol();

  DBUG_ENTER("Sql_cmd_proc_dummy_2::send_result");

  if (error) {
    assert(thd->is_error());
    DBUG_VOID_RETURN;
  }

  if (m_proc->send_result_metadata(thd)) DBUG_VOID_RETURN;

  protocol->start_row();

  String name;
  String *res = (*m_list)[1]->val_str(&name);
  protocol->store_string(res->ptr(), res->length(), system_charset_info);
  protocol->store((*m_list)[0]->val_int());
  if (protocol->end_row()) DBUG_VOID_RETURN;

  my_eof(thd);
  DBUG_VOID_RETURN;
}

} /* namespace im */
