/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/sql_alter_instance.h"         /* Alter_instance class */

#include "auth_acls.h"
#include "derror.h"                     /* ER_THD */
#include "handler.h"                    /* ha_resolve_by_legacy_type */
#include "lex_string.h"
#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"                     /* my_error */
#include "mysqld_error.h"
#include "sql_class.h"                  /* THD */
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_plugin.h"
#include "sql_plugin_ref.h"
#include "sql_security_ctx.h"
#include "sql_table.h"                  /* write_to_binlog */
#include "mysql/components/services/dynamic_privilege.h"

/*
  @brief
  Log current command to binlog

  @returns false on success,
           true on error

  In case of failure, appropriate error is logged.
*/

bool
Alter_instance::log_to_binlog()
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

  Security_context *sctx= m_thd->security_context();
  if (!sctx->check_access(SUPER_ACL) &&
      !sctx->has_global_grant(STRING_WITH_LEN("ENCRYPTION_KEY_ADMIN")).first)
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER or ENCRYPTION_KEY_ADMIN");
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

  if (log_to_binlog())
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
                 ER_THD(m_thd, ER_MASTER_KEY_ROTATION_BINLOG_FAILED));
  }

  my_ok(m_thd);
  return false;
}
