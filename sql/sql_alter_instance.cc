/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_alter_instance.h"         /* Alter_instance class */
#include "sql_class.h"                  /* THD */
#include "my_sys.h"                     /* my_error */
#include "auth_common.h"                /* check_global_access */
#include "handler.h"                    /* ha_resolve_by_legacy_type */
#include "sql_table.h"                  /* write_to_binlog */

/*
  @brief
  Log current command to binlog

  @param [IN] is_transactional - Whether statement is transactional or not

  @returns false on success,
           true on error

  In case of failure, appropriate error is logged.
*/

bool
Alter_instance::log_to_binlog(bool is_transactional)
{
  bool res= false;
  if (!m_thd->lex->no_write_to_binlog)
    res= write_bin_log(m_thd, false, m_thd->query().str, m_thd->query().length);

  return res;
}


/*
  @brief
  Executes master key rotation by calling SE api.

  @returns false on success
           true on error

  In case of failure, appropriate error
  is logged by function.
*/

bool
Rotate_innodb_master_key::execute()
{
  const LEX_STRING storage_engine= { C_STRING_WITH_LEN("innodb") };
  plugin_ref se_plugin;
  handlerton *hton;

  if (!m_thd->security_context()->check_access(SUPER_ACL))
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return true;
  }

  if ((se_plugin= ha_resolve_by_name(m_thd, &storage_engine, false)))
  {
    hton= plugin_data<handlerton *>(se_plugin);
  }
  else
  {
    my_error(ER_MASTER_KEY_ROTATION_SE_UNAVAILABLE, MYF(0));
    return true;
  }

  if (!hton->rotate_encryption_master_key)
  {
    my_error(ER_MASTER_KEY_ROTATION_NOT_SUPPORTED_BY_SE, MYF(0));
    return true;
  }

  if (hton->rotate_encryption_master_key())
  {
    /* SE should have raised error */
    DBUG_ASSERT(m_thd->get_stmt_da()->is_error());
    return true;
  }

  if (log_to_binlog(false))
  {
    /*
      Though we failed to write to binlog,
      there is no way we can undo this operation.
      So, covert error to a warning and let user
      know that something went wrong while trying
      to make entry in binlog.
    */
    m_thd->clear_error();
    m_thd->get_stmt_da()->reset_diagnostics_area();

    push_warning(m_thd, Sql_condition::SL_WARNING,
                 ER_MASTER_KEY_ROTATION_BINLOG_FAILED,
                 ER(ER_MASTER_KEY_ROTATION_BINLOG_FAILED));
  }

  my_ok(m_thd);
  return false;
}
