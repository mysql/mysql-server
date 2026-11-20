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

#include "sql/duckdb/duckdb_proc.h"
#include "sql/auth/auth_acls.h"
#include "sql/duckdb/duckdb_query.h"
#include "sql/protocol.h"
#include "sql/sql_lex.h"
#include "sql/sql_prepare.h"
#include "sql/strfunc.h"

namespace myduck {

/** Uniform schema name for duckdb procedures */
const LEX_CSTRING DUCKDB_PROC_SCHEMA = {STRING_WITH_LEN("dbms_duckdb")};

/**
 Singleton instance for dbms_duckdb.query().
*/
im::Proc *Duckdb_proc_query::instance() {
  static im::Proc *proc = new Duckdb_proc_query();
  return proc;
}

/**
  Create the SQL command for dbms_duckdb.query().
*/
Sql_cmd *Duckdb_proc_query::evoke_cmd(THD *thd,
                                      mem_root_deque<Item *> *list) const {
  return new (thd->mem_root) Sql_cmd_type(thd, list, this);
}

/**
  Execute sql by duckdb

  @param[in]    THD           Thread context

  @retval       true          Failure
  @retval       false         Success
*/
bool Sql_cmd_duckdb_query::pc_execute(THD *) {
  DBUG_ENTER("Sql_cmd_duckdb_query::pc_execute");
  DBUG_RETURN(false);
}

void Sql_cmd_duckdb_query::send_result(THD *thd, bool error) {
  DBUG_ENTER("Sql_cmd_duckdb_query::send_result");
  Protocol *protocol = thd->get_protocol();

  if (error) {
    assert(thd->is_error());
    DBUG_VOID_RETURN;
  }

  /* Parse the input parameters */
  String *input;
  char buff[1024];
  String str(buff, sizeof(buff), system_charset_info);
  input = (*m_list)[0]->val_str(&str);

  std::string sql(input->ptr(), input->length());

  auto res = duckdb_query(thd, sql, false);
  if (res->type == duckdb::QueryResultType::STREAM_RESULT) {
    auto &stream_result = res->Cast<duckdb::StreamQueryResult>();
    res = stream_result.Materialize();
  }
  std::string result = res->ToString();

  if (m_proc->send_result_metadata(thd)) DBUG_VOID_RETURN;
  protocol->start_row();
  protocol->store_string(result.c_str(), result.length(), system_charset_info);
  if (protocol->end_row()) DBUG_VOID_RETURN;
  my_eof(thd);

  DBUG_VOID_RETURN;
}

}  // namespace myduck
