/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "table.h"                      /* TABLE_FIELD_TYPE */
#include "m_string.h"                   /* C_STRING_WITH_LEN */
#include "transaction.h"                /* trans_commit_stmt */
                                        /* trans_commit_implicit */
#include "sql_parse.h"                  /* stmt_causes_implicit_commit */
#include "sql_base.h"                   /* close_thread_tables */
#include "key.h"                        /* key_copy, key_cmp_if_same */
                                        /* key_restore */
#include "log.h"                        /* sql_print_warning */
#include "rpl_filter.h"                 /* rpl_filter */
#include "sql_plugin.h"                 // plugin_is_ready

#include "auth_internal.h"
#include "sql_auth_cache.h"
#include "sql_user_table.h"
#include "sql_authentication.h"

#include "tztime.h"
#include "sql_time.h"
#include "crypt_genhash_impl.h"         /* CRYPT_MAX_PASSWORD_SIZE */
#include "../table.h"
#include "../../include/m_string.h"

#define DEBUG_SE_WRITE_ERROR_PRE(debug_flag)       \
  DBUG_EXECUTE_IF(debug_flag,  \
                  DBUG_SET("+d, inject_error_ha_write_row"););

#define DEBUG_SE_WRITE_ERROR_POST(debug_flag)    \
  DBUG_EXECUTE_IF(debug_flag,  \
                  {  \
                    DBUG_ASSERT(error == HA_ERR_INTERNAL_ERROR);  \
                    DBUG_SET("-d, inject_error_ha_write_row"); \
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;  \
                  });

#define DEBUG_SE_UPDATE_ERROR_PRE(debug_flag)       \
  DBUG_EXECUTE_IF(debug_flag,  \
                  DBUG_SET("+d, inject_error_ha_update_row"););

#define DEBUG_SE_UPDATE_ERROR_POST(debug_flag)    \
  DBUG_EXECUTE_IF(debug_flag,  \
                  {  \
                    DBUG_ASSERT(error == HA_ERR_INTERNAL_ERROR);  \
                    DBUG_SET("-d, inject_error_ha_update_row"); \
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;  \
                  });

#define DEBUG_SE_DELETE_ERROR_PRE(debug_flag)       \
  DBUG_EXECUTE_IF(debug_flag,  \
                  DBUG_SET("+d, inject_error_ha_delete_row"););

#define DEBUG_SE_DELETE_ERROR_POST(debug_flag)    \
  DBUG_EXECUTE_IF(debug_flag,  \
                  {  \
                    DBUG_ASSERT(error == HA_ERR_INTERNAL_ERROR);  \
                    DBUG_SET("-d, inject_error_ha_delete_row"); \
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;  \
                  });

static const
TABLE_FIELD_TYPE mysql_db_table_fields[MYSQL_DB_FIELD_COUNT] = {
  {
    { C_STRING_WITH_LEN("Host") },            
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  }, 
  {
    { C_STRING_WITH_LEN("Db") },            
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  }, 
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Select_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Insert_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Update_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Delete_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Drop_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Grant_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("References_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Index_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Alter_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_tmp_table_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Lock_tables_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_view_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Show_view_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_routine_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Alter_routine_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Execute_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Event_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Trigger_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  }
};

static const
TABLE_FIELD_TYPE mysql_user_table_fields[MYSQL_USER_FIELD_COUNT] = {
  {
    { C_STRING_WITH_LEN("Host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Select_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Insert_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Update_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Delete_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Drop_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Reload_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Shutdown_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Process_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("File_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Grant_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("References_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Index_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Alter_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Show_db_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Super_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_tmp_table_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Lock_tables_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Execute_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Repl_slave_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Repl_client_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_view_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Show_view_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_routine_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Alter_routine_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_user_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Event_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Trigger_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_tablespace_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("ssl_type") },
    { C_STRING_WITH_LEN("enum('','ANY','X509','SPECIFIED')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("ssl_cipher") },
    { C_STRING_WITH_LEN("blob") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("x509_issuer") },
    { C_STRING_WITH_LEN("blob") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("x509_subject") },
    { C_STRING_WITH_LEN("blob") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("max_questions") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("max_updates") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("max_connections") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("max_user_connections") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("plugin") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("authentication_string") },
    { C_STRING_WITH_LEN("text") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("password_expired") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("password_last_changed") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("password_lifetime") },
    { C_STRING_WITH_LEN("smallint(5)") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("account_locked") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  }
};

static const
TABLE_FIELD_TYPE mysql_proxies_priv_table_fields[MYSQL_PROXIES_PRIV_FIELD_COUNT] = {
  {
   { C_STRING_WITH_LEN("Host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Proxied_host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Proxied_user") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("With_grant") },
    { C_STRING_WITH_LEN("tinyint(1)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Grantor") },
    { C_STRING_WITH_LEN("char(93)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Timestamp") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  }
};

static const
TABLE_FIELD_TYPE mysql_procs_priv_table_fields[MYSQL_PROCS_PRIV_FIELD_COUNT] = {
  {
    { C_STRING_WITH_LEN("Host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Db") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Routine_name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Routine_type") },
    { C_STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Grantor") },
    { C_STRING_WITH_LEN("char(93)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Proc_priv") },
    { C_STRING_WITH_LEN("set('Execute','Alter Routine','Grant')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Timestamp") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  }
};

static const
TABLE_FIELD_TYPE mysql_columns_priv_table_fields[MYSQL_COLUMNS_PRIV_FIELD_COUNT] = {
  {
    { C_STRING_WITH_LEN("Host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Db") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Table_name") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Column_name") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Timestamp") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Column_priv") },
    { C_STRING_WITH_LEN("set('Select','Insert','Update','References')") },
    { C_STRING_WITH_LEN("utf8") }
  }
};

static const
TABLE_FIELD_TYPE mysql_tables_priv_table_fields[MYSQL_TABLES_PRIV_FIELD_COUNT] = {
  {
    { C_STRING_WITH_LEN("Host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Db") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Table_name") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Grantor") },
    { C_STRING_WITH_LEN("char(93)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Timestamp") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Table_priv") },
    { C_STRING_WITH_LEN("set('Select','Insert','Update','Delete','Create',"
                        "'Drop','Grant','References','Index','Alter',"
                        "'Create View','Show view','Trigger')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Column_priv") },
    { C_STRING_WITH_LEN("set('Select','Insert','Update','References')") },
    { C_STRING_WITH_LEN("utf8") }
  }
};

const TABLE_FIELD_DEF
  mysql_db_table_def= {MYSQL_DB_FIELD_COUNT, mysql_db_table_fields};

const TABLE_FIELD_DEF
  mysql_user_table_def= {MYSQL_USER_FIELD_COUNT, mysql_user_table_fields};

const TABLE_FIELD_DEF
  mysql_proxies_priv_table_def= {MYSQL_PROXIES_PRIV_FIELD_COUNT,
                                 mysql_proxies_priv_table_fields};

const TABLE_FIELD_DEF
  mysql_procs_priv_table_def= {MYSQL_PROCS_PRIV_FIELD_COUNT,
                               mysql_procs_priv_table_fields};

const TABLE_FIELD_DEF
  mysql_columns_priv_table_def= {MYSQL_COLUMNS_PRIV_FIELD_COUNT,
                                 mysql_columns_priv_table_fields};

const TABLE_FIELD_DEF
  mysql_tables_priv_table_def= {MYSQL_TABLES_PRIV_FIELD_COUNT,
                                mysql_tables_priv_table_fields};


#ifndef NO_EMBEDDED_ACCESS_CHECKS


/*
  Get all access bits from table after fieldnr

  IMPLEMENTATION
  We know that the access privileges ends when there is no more fields
  or the field is not an enum with two elements.

  SYNOPSIS
    get_access()
    form        an open table to read privileges from.
                The record should be already read in table->record[0]
    fieldnr     number of the first privilege (that is ENUM('N','Y') field
    next_field  on return - number of the field next to the last ENUM
                (unless next_field == 0)

  RETURN VALUE
    privilege mask
*/

ulong get_access(TABLE *form, uint fieldnr, uint *next_field)
{
  ulong access_bits=0,bit;
  char buff[2];
  String res(buff,sizeof(buff),&my_charset_latin1);
  Field **pos;

  for (pos=form->field+fieldnr, bit=1;
       *pos && (*pos)->real_type() == MYSQL_TYPE_ENUM &&
         ((Field_enum*) (*pos))->typelib->count == 2 ;
       pos++, fieldnr++, bit<<=1)
  {
    (*pos)->val_str(&res);
    if (my_toupper(&my_charset_latin1, res[0]) == 'Y')
      access_bits|= bit;
  }
  if (next_field)
    *next_field=fieldnr;
  return access_bits;
}


/**
  A helper function to commit statement transaction and close
  ACL tables after reading some data from them as part of FLUSH
  PRIVILEGES statement or during server initialization.

  @note We assume that we have only read from the tables so commit
        can't fail. @sa close_mysql_tables().

  @note This function also rollbacks the transaction if rollback was
        requested (e.g. as result of deadlock).
*/

void close_acl_tables(THD *thd)
{
  /* Transaction rollback request by SE is unlikely. Still we handle it. */
  if (thd->transaction_rollback_request)
  {
    trans_rollback_stmt(thd);
    trans_rollback_implicit(thd);
  }
  else
  {
#ifndef DBUG_OFF
    bool res=
#endif
      trans_commit_stmt(thd);
    DBUG_ASSERT(res == false);
  }

  close_mysql_tables(thd);
}


/**
  Commit or rollback ACL statement (and transaction),
  close tables which it has opened and release metadata locks.

  @note In case of failure to commit transaction we try to restore correct
        state of in-memory structures by reloading privileges.

  @retval False - Success.
  @retval True  - Error.
*/

bool acl_end_trans_and_close_tables(THD *thd, bool rollback_transaction)
{
  bool result;

  /*
    Try to commit a transaction even if we had some failures.

    Without this step changes to privilege tables will be rolled back at the
    end of mysql_execute_command() in the presence of error, leaving on-disk
    and in-memory descriptions of privileges out of sync and making behavior
    of ACL statements for transactional tables incompatible with legacy
    behavior.

    We need to commit both statement and normal transaction to make behavior
    consistent with both autocommit on and off.

    It is safe to do so since ACL statement always do implicit commit at the
    end of statement.
  */
  DBUG_ASSERT(stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END));

  if (rollback_transaction)
  {
    /*
      Transaction rollback request by SE is unlikely. Still let us
      handle it and also do ACL reload if it happens.
    */
    result= trans_rollback_stmt(thd);
    result|= trans_rollback_implicit(thd);
  }
  else
  {
    result= trans_commit_stmt(thd);
    result|= trans_commit_implicit(thd);
  }
  close_thread_tables(thd);
  thd->mdl_context.release_transactional_locks();

  if (result || rollback_transaction)
  {
    /*
      Try to bring in-memory structures back in sync with on-disk data if we
      have failed to commit our changes.
    */
    (void) acl_reload(thd);
    (void) grant_reload(thd);
  }

  return result;
}


/**
  Notify handlerton(s) that privileges have changed

  Interested handlertons may use this notification to update
  its own privilege structures as well as propagating
  the changing query to other destinations.

*/

void acl_notify_htons(THD* thd, const char* query, size_t query_length)
{
  DBUG_ENTER("acl_notify_htons");
  DBUG_PRINT("enter", ("db: %s", thd->db().str));
  DBUG_PRINT("enter", ("query: '%s', length: %zu", query, query_length));

  ha_binlog_log_query(thd, NULL, LOGCOM_ACL_NOTIFY,
                      query, query_length,
                      thd->db().str, "");
  DBUG_VOID_RETURN;
}


void get_grantor(THD *thd, char *grantor)
{
  const char *user= thd->security_context()->user().str;
  const char *host= thd->security_context()->host_or_ip().str;

#if defined(HAVE_REPLICATION)
  if (thd->slave_thread && thd->has_invoker())
  {
    user= thd->get_invoker_user().str;
    host= thd->get_invoker_host().str;
  }
#endif
  strxmov(grantor, user, "@", host, NullS);
}


/**
  Print error reported by storage engine

  @param TABLE           Handle of TABLE to fetch SE information
  @param  handler_error  an error number resulted from storage engine
 */

void acl_print_ha_error(TABLE *table, int handler_error)
{
  table->file->print_error(handler_error, MYF(0));
}


/**
  Update SSL properties in mysql.user table.

  @param thd          Thread context
  @param tables       table points to mysql.user table.

*/

void update_ssl_properties(THD *thd, TABLE *table)
{
  LEX *lex= thd->lex;
  switch (lex->ssl_type)
  {
    case SSL_TYPE_ANY:
      table->field[MYSQL_USER_FIELD_SSL_TYPE]->store(STRING_WITH_LEN("ANY"),
                                      &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_SSL_CIPHER]->
        store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_ISSUER]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_SUBJECT]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_X509:
      table->field[MYSQL_USER_FIELD_SSL_TYPE]->store(STRING_WITH_LEN("X509"),
                                      &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_SSL_CIPHER]->
        store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_ISSUER]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_SUBJECT]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_SPECIFIED:
      table->field[MYSQL_USER_FIELD_SSL_TYPE]->store(STRING_WITH_LEN("SPECIFIED"),
                                      &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_SSL_CIPHER]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_ISSUER]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_SUBJECT]->store("", 0, &my_charset_latin1);
      if (lex->ssl_cipher)
        table->field[MYSQL_USER_FIELD_SSL_CIPHER]->store(lex->ssl_cipher,
                                strlen(lex->ssl_cipher), system_charset_info);
      if (lex->x509_issuer)
        table->field[MYSQL_USER_FIELD_X509_ISSUER]->store(lex->x509_issuer,
                                strlen(lex->x509_issuer), system_charset_info);
      if (lex->x509_subject)
        table->field[MYSQL_USER_FIELD_X509_SUBJECT]->store(lex->x509_subject,
                                strlen(lex->x509_subject), system_charset_info);
      break;
    case SSL_TYPE_NOT_SPECIFIED:
      break;
    case SSL_TYPE_NONE:
      table->field[MYSQL_USER_FIELD_SSL_TYPE]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_SSL_CIPHER]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_ISSUER]->store("", 0, &my_charset_latin1);
      table->field[MYSQL_USER_FIELD_X509_SUBJECT]->store("", 0, &my_charset_latin1);
      break;
  }
}

/**
  Update user resources in mysql.user table.

  @param tables       table points to mysql.user table.
  @param mqh          user resourses to be updated

*/

void update_user_resource(TABLE *table, USER_RESOURCES *mqh)
{
  if (mqh->specified_limits & USER_RESOURCES::QUERIES_PER_HOUR)
    table->field[MYSQL_USER_FIELD_MAX_QUESTIONS]->
      store((longlong) mqh->questions, TRUE);
  if (mqh->specified_limits & USER_RESOURCES::UPDATES_PER_HOUR)
    table->field[MYSQL_USER_FIELD_MAX_UPDATES]->
      store((longlong) mqh->updates, TRUE);
  if (mqh->specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR)
    table->field[MYSQL_USER_FIELD_MAX_CONNECTIONS]->
      store((longlong) mqh->conn_per_hour, TRUE);
  if (table->s->fields >= 36 &&
      (mqh->specified_limits & USER_RESOURCES::USER_CONNECTIONS))
    table->field[MYSQL_USER_FIELD_MAX_USER_CONNECTIONS]->
      store((longlong) mqh->user_conn, TRUE);
}


int replace_user_table(THD *thd, TABLE *table, LEX_USER *combo,
                       ulong rights, bool revoke_grant,
                       bool can_create_user, ulong what_to_replace)
{
  int error = -1;
  bool old_row_exists=0;
  bool builtin_plugin= true;
  bool update_password= (what_to_replace & PLUGIN_ATTR);
  char what= (revoke_grant) ? 'N' : 'Y';
  uchar user_key[MAX_KEY_LENGTH];
  LEX *lex= thd->lex;
  LEX_CSTRING old_plugin;
  struct timeval password_change_timestamp= {0, 0};
  Acl_table_intact table_intact;
  DBUG_ENTER("replace_user_table");

  mysql_mutex_assert_owner(&acl_cache->lock);

  if (table_intact.check(table, &mysql_user_table_def))
    goto end;
  
  table->use_all_columns();
  DBUG_ASSERT(combo->host.str != NULL);
  table->field[MYSQL_USER_FIELD_HOST]->store(combo->host.str,combo->host.length,
                                             system_charset_info);
  table->field[MYSQL_USER_FIELD_USER]->store(combo->user.str,combo->user.length,
                                             system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  error= table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT);
  DBUG_EXECUTE_IF("se_error_replace_user_table_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      acl_print_ha_error(table, error);
      DBUG_RETURN(-1);
    }

    /*
      The user record wasn't found; if the intention was to revoke privileges
      (indicated by what == 'N') then execution must fail now.
    */
    if (what == 'N')
    {
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo->user.str, combo->host.str);
      /* 
        Return 1 as an indication that expected error occured during
        handling of REVOKE statement for an unknown user.
      */
      error= 1;
      goto end;
    }

    optimize_plugin_compare_by_pointer(&combo->plugin);
    builtin_plugin= auth_plugin_is_built_in(combo->plugin.str);

    if (!can_create_user)
    {
      my_error(ER_CANT_CREATE_USER_WITH_GRANT, MYF(0));
      error= 1;
      goto end;
    }
    if (thd->lex->sql_command == SQLCOM_GRANT)
    {
      /*
        If NO_AUTO_CREATE_USER SQL mode is set and GRANT is not specified
        with authentication information or the authentication_string
        is empty then report error
      */
      if ((thd->variables.sql_mode & MODE_NO_AUTO_CREATE_USER) &&
          ((what_to_replace & DEFAULT_AUTH_ATTR) || !combo->auth.length))
      {
        my_error(ER_PASSWORD_NO_MATCH, MYF(0), combo->user.str, combo->host.str);
        error= 1;
        goto end;
      }
      push_warning(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                   "Using GRANT for creating new user is deprecated "
                   "and will be removed in future release. "
                   "Create new user with CREATE USER statement.");
    }
    old_row_exists = 0;
    restore_record(table,s->default_values);
    DBUG_ASSERT(combo->host.str != NULL);
    table->field[MYSQL_USER_FIELD_HOST]->store(combo->host.str,combo->host.length,
                                               system_charset_info);
    table->field[MYSQL_USER_FIELD_USER]->store(combo->user.str,combo->user.length,
                                               system_charset_info);
  }
  else // if (table->file->ha_index_read_idx_map [..]
  {
    /*
      There is a matching user record ------------------------------------------
     */

    old_row_exists = 1;
    store_record(table,record[1]);			// Save copy for update

    /* 1. resolve plugins in the LEX_USER struct if needed */

    /* Get old plugin value from storage. */
    old_plugin.str=
      get_field(thd->mem_root, table->field[MYSQL_USER_FIELD_PLUGIN]);

    if (old_plugin.str == NULL || *old_plugin.str == '\0')
    {
      my_error(ER_PASSWORD_NO_MATCH, MYF(0));
      error= 1;
      goto end;
    }

    /* 
      It is important not to include the trailing '\0' in the string length 
      because otherwise the plugin hash search will fail.
    */
    old_plugin.length= strlen(old_plugin.str);

    /* Optimize for pointer comparision of built-in plugin name */
    optimize_plugin_compare_by_pointer(&old_plugin);
    builtin_plugin= auth_plugin_is_built_in(old_plugin.str);

    /* there is nothing to update */
    if ((thd->lex->sql_command != SQLCOM_ALTER_USER) && !rights &&
        lex->ssl_type == SSL_TYPE_NOT_SPECIFIED &&
        !lex->mqh.specified_limits && !revoke_grant &&
        (!builtin_plugin || !update_password))
    {
      DBUG_PRINT("info", ("Proxy user exit path"));
      DBUG_RETURN(0);
    }
    /* GRANT will be used only to specify access rights for existing user */
    if ((thd->lex->sql_command == SQLCOM_GRANT) &&
        (what_to_replace & ~(DEFAULT_AUTH_ATTR | ACCESS_RIGHTS_ATTR)))
    {
      push_warning(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                     "Using GRANT statement to modify existing user's "
                     "properties other than privileges is deprecated and "
                     "will be removed in future release. "
                     "Use ALTER USER statement for this operation.");
    }
  }

  if (what_to_replace & PLUGIN_ATTR ||
      (what_to_replace & DEFAULT_AUTH_ATTR && !old_row_exists))
  {
    if (table->s->fields >= 41)
    {
      table->field[MYSQL_USER_FIELD_PLUGIN]->
        store(combo->plugin.str, combo->plugin.length, system_charset_info);
      table->field[MYSQL_USER_FIELD_PLUGIN]->set_notnull();
      table->field[MYSQL_USER_FIELD_AUTHENTICATION_STRING]->
        store(combo->auth.str, combo->auth.length, &my_charset_utf8_bin);
      table->field[MYSQL_USER_FIELD_AUTHENTICATION_STRING]->set_notnull();
    }
    else
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), "plugin", "mysql.user");
      DBUG_RETURN(-1);
    }
    /* If we change user plugin then check if it is builtin plugin */
    optimize_plugin_compare_by_pointer(&combo->plugin);
    builtin_plugin= auth_plugin_is_built_in(combo->plugin.str);
    /*
      we update the password last changed field whenever there is change
      in auth str and plugin is built in
    */
    if (table->s->fields > MYSQL_USER_FIELD_PASSWORD_LAST_CHANGED)
    {
      if (builtin_plugin)
      {
        /*
          Calculate time stamp up to seconds elapsed
          from 1 Jan 1970 00:00:00.
        */
        password_change_timestamp= thd->query_start_timeval_trunc(0);
        table->field[MYSQL_USER_FIELD_PASSWORD_LAST_CHANGED]->store_timestamp
            (&password_change_timestamp);
        table->field[MYSQL_USER_FIELD_PASSWORD_LAST_CHANGED]->set_notnull();
      }
    }
    else
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), "password_last_changed", "mysql.user");
      DBUG_RETURN(-1);
    }
    /* if we have a password supplied we update the expiration field */
    if (table->s->fields > MYSQL_USER_FIELD_PASSWORD_EXPIRED)
    {
      if (auth_plugin_supports_expiration(combo->plugin.str))
        table->field[MYSQL_USER_FIELD_PASSWORD_EXPIRED]->store("N",
                                       1, system_charset_info);
    }
    else
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), "password_expired", "mysql.user");
      DBUG_RETURN(-1);
    }
  }
  /* Update table columns with new privileges */
  uint next_field;
  if (what_to_replace & ACCESS_RIGHTS_ATTR)
  {
    Field **tmp_field;
    ulong priv;
    for (tmp_field= table->field+2, priv = SELECT_ACL;
         *tmp_field && (*tmp_field)->real_type() == MYSQL_TYPE_ENUM &&
         ((Field_enum*) (*tmp_field))->typelib->count == 2 ;
         tmp_field++, priv <<= 1)
    {
      if (priv & rights)              // set requested privileges
        (*tmp_field)->store(&what, 1, &my_charset_latin1);
    }
  }
  rights= get_access(table, MYSQL_USER_FIELD_SELECT_PRIV, &next_field);
  DBUG_PRINT("info",("table fields: %d",table->s->fields));

  /* We write down SSL related ACL stuff */
  if ((what_to_replace & SSL_ATTR) && (table->s->fields >= 31))
    update_ssl_properties(thd, table);
  next_field+=4;

  if (what_to_replace & RESOURCE_ATTR)
    update_user_resource(table, &lex->mqh);
  mqh_used= mqh_used || lex->mqh.questions || lex->mqh.updates ||
            lex->mqh.conn_per_hour;
  next_field+= 4;

  if (what_to_replace & PASSWORD_EXPIRE_ATTR)
  {
    /*
      ALTER/CREATE USER <user> PASSWORD EXPIRE  (or)
      ALTER USER <user> IDENTIFIED WITH plugin
    */
    if (combo->alter_status.update_password_expired_column)
    {
      if (table->s->fields > MYSQL_USER_FIELD_PASSWORD_EXPIRED)
      {
        table->field[MYSQL_USER_FIELD_PASSWORD_EXPIRED]->
               store("Y", 1, system_charset_info);
      }
      else
      {
        my_error(ER_BAD_FIELD_ERROR, MYF(0), "password_expired", "mysql.user");
        DBUG_RETURN(-1);
      }
    }
    /*
      If password_expired column is not to be updated and only
      password_lifetime is to be updated
    */
    if (table->s->fields > MYSQL_USER_FIELD_PASSWORD_LIFETIME &&
        !combo->alter_status.update_password_expired_column)
    {
      if (!combo->alter_status.use_default_password_lifetime)
      {
        table->field[MYSQL_USER_FIELD_PASSWORD_LIFETIME]->
          store((longlong) combo->alter_status.expire_after_days, TRUE);
        table->field[MYSQL_USER_FIELD_PASSWORD_LIFETIME]->set_notnull();
      }
      else
        table->field[MYSQL_USER_FIELD_PASSWORD_LIFETIME]->set_null();
    }
  }

  if (what_to_replace & ACCOUNT_LOCK_ATTR)
  {
    if (!old_row_exists ||
        (old_row_exists && combo->alter_status.update_account_locked_column))
    {
      if (table->s->fields > MYSQL_USER_FIELD_ACCOUNT_LOCKED)
      {
        /*
          Update the field for a new row and for the row that exists and the
          update was enforced (ACCOUNT [UNLOCK|LOCK]).
        */
        table->field[MYSQL_USER_FIELD_ACCOUNT_LOCKED]->
                      store(combo->alter_status.account_locked ? "Y" : "N", 1,
                            system_charset_info);
      }
      else
      {
        my_error(ER_BAD_FIELD_ERROR, MYF(0), "account_locked", "mysql.user");
        DBUG_RETURN(-1);
      }
    }
  }

  if (old_row_exists)
  {   
    /*
      We should NEVER delete from the user table, as a uses can still
      use mysqld even if he doesn't have any privileges in the user table!
    */
    if (cmp_record(table,record[1]))
    {
      DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_user_table_update");
      error= table->file->ha_update_row(table->record[1],
                                        table->record[0]);
      DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_user_table_update");

      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
      {
        acl_print_ha_error(table, error);
        error= -1;
        goto end;
      }
      else
        error= 0;
    }
  }
  else
  {
    DEBUG_SE_WRITE_ERROR_PRE("se_error_replace_user_table_add");
    error=table->file->ha_write_row(table->record[0]);
    DEBUG_SE_WRITE_ERROR_POST("se_error_replace_user_table_add");
    if (error && !table->file->is_ignorable_error(error))
    {
      acl_print_ha_error(table, error);
      error= -1;
      goto end;
    }
  }
  error=0;					// Privileges granted / revoked

end:
  if (!error)
  {
    /*
       Convert the time when the password was changed from timeval
       structure to MYSQL_TIME format, to store it in cache.
    */
    MYSQL_TIME password_change_time;

    if (builtin_plugin && (update_password || !old_row_exists))
      thd->variables.time_zone->gmt_sec_to_TIME(&password_change_time,
        (my_time_t)password_change_timestamp.tv_sec);
    else
      password_change_time.time_type= MYSQL_TIMESTAMP_ERROR;
    acl_cache->clear(1);			// Clear privilege cache
    if (old_row_exists)
      acl_update_user(combo->user.str, combo->host.str,
		      lex->ssl_type,
		      lex->ssl_cipher,
		      lex->x509_issuer,
		      lex->x509_subject,
		      &lex->mqh,
		      rights,
		      combo->plugin,
		      combo->auth,
                      password_change_time,
                      combo->alter_status,
                      what_to_replace);
    else
      acl_insert_user(combo->user.str, combo->host.str,
		      lex->ssl_type,
		      lex->ssl_cipher,
		      lex->x509_issuer,
		      lex->x509_subject,
		      &lex->mqh,
		      rights,
		      combo->plugin,
		      combo->auth,
                      password_change_time,
                      combo->alter_status);
  }
  DBUG_RETURN(error);
}


/*
  change grants in the mysql.db table
*/

int replace_db_table(TABLE *table, const char *db,
                     const LEX_USER &combo,
                     ulong rights, bool revoke_grant)
{
  uint i;
  ulong priv,store_rights;
  bool old_row_exists=0;
  int error;
  char what= (revoke_grant) ? 'N' : 'Y';
  uchar user_key[MAX_KEY_LENGTH];
  Acl_table_intact table_intact;
  DBUG_ENTER("replace_db_table");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  if (table_intact.check(table, &mysql_db_table_def))
    DBUG_RETURN(-1);

  /* Check if there is such a user in user table in memory? */
  if (!find_acl_user(combo.host.str,combo.user.str, FALSE))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH), MYF(0));
    DBUG_RETURN(1);
  }

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db, strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  error= table->file->ha_index_read_idx_map(table->record[0],0, user_key,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT);

  DBUG_EXECUTE_IF("se_error_replace_db_table_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      goto table_error;

    if (what == 'N')
    { // no row, no revoke
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo.user.str, combo.host.str);
      DBUG_RETURN(1);
    }
    old_row_exists = 0;
    restore_record(table, s->default_values);
    table->field[0]->store(combo.host.str,combo.host.length,
                           system_charset_info);
    table->field[1]->store(db, strlen(db), system_charset_info);
    table->field[2]->store(combo.user.str,combo.user.length,
                           system_charset_info);
  }
  else
  {
    old_row_exists = 1;
    store_record(table,record[1]);
  }

  store_rights=get_rights_for_db(rights);
  for (i= 3, priv= 1; i < table->s->fields; i++, priv <<= 1)
  {
    if (priv & store_rights)                    // do it if priv is chosen
      table->field [i]->store(&what,1, &my_charset_latin1);// set requested privileges
  }
  rights=get_access(table,3,0);
  rights=fix_rights_for_db(rights);

  if (old_row_exists)
  {
    /* update old existing row */
    if (rights)
    {
      DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_db_table_update");
      error= table->file->ha_update_row(table->record[1],
                                        table->record[0]);
      DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_db_table_update");

      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
        goto table_error;
    }
    else        /* must have been a revoke of all privileges */
    {
      DEBUG_SE_DELETE_ERROR_PRE("se_error_replace_db_table_delete");
      error= table->file->ha_delete_row(table->record[1]);
      DEBUG_SE_DELETE_ERROR_POST("se_error_replace_db_table_delete");
      if (error)
        goto table_error;                       /* purecov: deadcode */
    }
  }
  else if (rights)
  {
    DEBUG_SE_WRITE_ERROR_PRE("se_error_replace_db_table_add");
    error= table->file->ha_write_row(table->record[0]);
    DEBUG_SE_WRITE_ERROR_POST("se_error_replace_db_table_add");
    if (error && !table->file->is_ignorable_error(error))
      goto table_error;
  }

  acl_cache->clear(1);                          // Clear privilege cache
  if (old_row_exists)
    acl_update_db(combo.user.str,combo.host.str,db,rights);
  else
  if (rights)
    acl_insert_db(combo.user.str,combo.host.str,db,rights);
  DBUG_RETURN(0);

  /* This could only happen if the grant tables got corrupted */
table_error:
  acl_print_ha_error(table, error);

  DBUG_RETURN(-1);
}


int replace_proxies_priv_table(THD *thd, TABLE *table, const LEX_USER *user,
                               const LEX_USER *proxied_user,
                               bool with_grant_arg, bool revoke_grant)
{
  bool old_row_exists= 0;
  int error;
  uchar user_key[MAX_KEY_LENGTH];
  ACL_PROXY_USER new_grant;
  char grantor[USER_HOST_BUFF_SIZE];
  Acl_table_intact table_intact;

  DBUG_ENTER("replace_proxies_priv_table");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  if (table_intact.check(table, &mysql_proxies_priv_table_def))
    DBUG_RETURN(-1);

  /* Check if there is such a user in user table in memory? */
  if (!find_acl_user(user->host.str,user->user.str, FALSE))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH), MYF(0));
    DBUG_RETURN(1);
  }

  table->use_all_columns();
  ACL_PROXY_USER::store_pk (table, user->host, user->user,
                            proxied_user->host, proxied_user->user);

  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  get_grantor(thd, grantor);

  if ((error= table->file->ha_index_init(0, 1)))
  {
    acl_print_ha_error(table, error);
    DBUG_PRINT("info", ("ha_index_init error"));
    DBUG_RETURN(-1);
  }

  error= table->file->ha_index_read_map(table->record[0], user_key,
                                        HA_WHOLE_KEY,
                                        HA_READ_KEY_EXACT);

  DBUG_EXECUTE_IF("se_error_replace_proxies_priv_table_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      goto table_error;

    DBUG_PRINT ("info", ("Row not found"));
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_GRANT, MYF(0), user->user.str, user->host.str);
      table->file->ha_index_end();
      DBUG_RETURN(1);
    }
    old_row_exists= 0;
    restore_record(table, s->default_values);
    ACL_PROXY_USER::store_data_record(table, user->host, user->user,
                                      proxied_user->host,
                                      proxied_user->user,
                                      with_grant_arg,
                                      grantor);
  }
  else
  {
    DBUG_PRINT("info", ("Row found"));
    old_row_exists= 1;
    store_record(table,record[1]);                    // copy original row
    ACL_PROXY_USER::store_with_grant(table, with_grant_arg);
  }

  if (old_row_exists)
  {
    /* update old existing row */
    if (!revoke_grant)
    {
      DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_proxies_priv_table_update");
      error= table->file->ha_update_row(table->record[1],
                                        table->record[0]);
      DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_proxies_priv_table_update");
      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
        goto table_error;
    }
    else
    {
      DEBUG_SE_DELETE_ERROR_PRE("se_error_replace_proxies_priv_table_delete");
      error= table->file->ha_delete_row(table->record[1]);
      DEBUG_SE_DELETE_ERROR_POST("se_error_replace_proxies_priv_table_delete");
      if (error)
        goto table_error;                       /* purecov: inspected */
    }
  }
  else
  {
    DEBUG_SE_WRITE_ERROR_PRE("se_error_replace_proxies_priv_table_add");
    error= table->file->ha_write_row(table->record[0]);
    DEBUG_SE_WRITE_ERROR_POST("se_error_replace_proxies_priv_table_add");
    if (error && !table->file->is_ignorable_error(error))
    {
      DBUG_PRINT("info", ("error inserting the row"));
      goto table_error; /* purecov: inspected */
    }
  }

  acl_cache->clear(1);                          // Clear privilege cache
  if (old_row_exists)
  {
    new_grant.init(user->host.str, user->user.str,
                   proxied_user->host.str, proxied_user->user.str,
                   with_grant_arg);
    acl_update_proxy_user(&new_grant, revoke_grant);
  }
  else
  {
    new_grant.init(&global_acl_memory, user->host.str, user->user.str,
                   proxied_user->host.str, proxied_user->user.str,
                   with_grant_arg);
    acl_insert_proxy_user(&new_grant);
  }

  table->file->ha_index_end();
  DBUG_RETURN(0);

  /* This could only happen if the grant tables got corrupted */
table_error:
  DBUG_PRINT("info", ("table error"));
  acl_print_ha_error(table, error);

  DBUG_PRINT("info", ("aborting replace_proxies_priv_table"));
  table->file->ha_index_end();
  DBUG_RETURN(-1);
}


int replace_column_table(GRANT_TABLE *g_t,
                         TABLE *table, const LEX_USER &combo,
                         List <LEX_COLUMN> &columns,
                         const char *db, const char *table_name,
                         ulong rights, bool revoke_grant)
{
  int result=0;
  uchar key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  KEY_PART_INFO *key_part;
  Acl_table_intact table_intact;
  DBUG_ENTER("replace_column_table");

  if (table_intact.check(table, &mysql_columns_priv_table_def))
    DBUG_RETURN(-1);

  key_part= table->key_info->key_part;

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db, strlen(db),
                         system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name, strlen(table_name),
                         system_charset_info);

  /* Get length of 4 first key parts */
  key_prefix_length= (key_part[0].store_length + key_part[1].store_length +
                      key_part[2].store_length + key_part[3].store_length);
  key_copy(key, table->record[0], table->key_info, key_prefix_length);

  rights&= COL_ACLS;                            // Only ACL for columns

  /* first fix privileges for all columns in column list */

  List_iterator <LEX_COLUMN> iter(columns);
  class LEX_COLUMN *column;
  int error= table->file->ha_index_init(0, 1);
  if (error)
  {
    acl_print_ha_error(table, error);
    DBUG_RETURN(-1);
  }

  while ((column= iter++))
  {
    ulong privileges= column->rights;
    bool old_row_exists=0;
    uchar user_key[MAX_KEY_LENGTH];

    key_restore(table->record[0],key,table->key_info,
                key_prefix_length);
    table->field[4]->store(column->column.ptr(), column->column.length(),
                           system_charset_info);
    /* Get key for the first 4 columns */
    key_copy(user_key, table->record[0], table->key_info,
             table->key_info->key_length);

    error= table->file->ha_index_read_map(table->record[0],
                                          user_key, HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT);

    DBUG_EXECUTE_IF("se_error_replace_column_table_read",
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;);
    if (error)
    {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        acl_print_ha_error(table, error);
        result= -1;
        goto end;
      }

      if (revoke_grant)
      {
        my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
                 combo.user.str, combo.host.str,
                 table_name);                   /* purecov: inspected */
        result= 1;                              /* purecov: inspected */
        continue;                               /* purecov: inspected */
      }
      old_row_exists = 0;
      restore_record(table, s->default_values);         // Get empty record
      key_restore(table->record[0],key,table->key_info,
                  key_prefix_length);
      table->field[4]->store(column->column.ptr(),column->column.length(),
                             system_charset_info);
    }
    else
    {
      ulong tmp= (ulong) table->field[6]->val_int();
      tmp=fix_rights_for_column(tmp);

      if (revoke_grant)
        privileges = tmp & ~(privileges | rights);
      else
        privileges |= tmp;
      old_row_exists = 1;
      store_record(table,record[1]);                    // copy original row
    }

    table->field[6]->store((longlong) get_rights_for_column(privileges), TRUE);

    if (old_row_exists)
    {
      GRANT_COLUMN *grant_column;
      if (privileges)
      {
        DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_column_table_update");
        error= table->file->ha_update_row(table->record[1],table->record[0]);
        DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_column_table_update");
        if (error && error != HA_ERR_RECORD_IS_THE_SAME)
        {
          acl_print_ha_error(table, error);
          result= -1;
          goto end;
        }
      }
      else
      {
        DEBUG_SE_DELETE_ERROR_PRE("se_error_replace_column_table_delete");
        error= table->file->ha_delete_row(table->record[1]);
        DEBUG_SE_DELETE_ERROR_POST("se_error_replace_column_table_delete");
        if (error)
        {
          acl_print_ha_error(table, error);
          result= -1;
          goto end;
        }
      }
      grant_column= column_hash_search(g_t, column->column.ptr(),
                                       column->column.length());
      if (grant_column)                         // Should always be true
        grant_column->rights= privileges;       // Update hash
    }
    else                                        // new grant
    {
      GRANT_COLUMN *grant_column;
      DEBUG_SE_WRITE_ERROR_PRE("se_error_replace_column_table_add");
      error= table->file->ha_write_row(table->record[0]);
      DEBUG_SE_WRITE_ERROR_POST("se_error_replace_column_table_add");
      if (error)
      {
        acl_print_ha_error(table, error);
        result= -1;
        goto end;
      }
      grant_column= new GRANT_COLUMN(column->column,privileges);
      if (my_hash_insert(&g_t->hash_columns,(uchar*) grant_column))
      {
        result= -1;
        goto end;
      }
    }
  }

  /*
    If revoke of privileges on the table level, remove all such privileges
    for all columns
  */

  if (revoke_grant)
  {
    uchar user_key[MAX_KEY_LENGTH];
    key_copy(user_key, table->record[0], table->key_info,
             key_prefix_length);
    error= table->file->ha_index_read_map(table->record[0], user_key,
                                          (key_part_map)15,
                                          HA_READ_KEY_EXACT);
    DBUG_EXECUTE_IF("se_error_replace_column_table_revoke_read",
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;);
    if (error)
    {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        acl_print_ha_error(table, error);
        result= -1;
      }
      goto end;
    }

    /* Scan through all rows with the same host,db,user and table */
    do
    {
      ulong privileges = (ulong) table->field[6]->val_int();
      privileges=fix_rights_for_column(privileges);
      store_record(table,record[1]);

      if (privileges & rights)  // is in this record the priv to be revoked ??
      {
        GRANT_COLUMN *grant_column = NULL;
        char  colum_name_buf[HOSTNAME_LENGTH+1];
        String column_name(colum_name_buf,sizeof(colum_name_buf),
                           system_charset_info);

        privileges&= ~rights;
        table->field[6]->store((longlong)
                               get_rights_for_column(privileges), TRUE);
        table->field[4]->val_str(&column_name);
        grant_column = column_hash_search(g_t,
                                          column_name.ptr(),
                                          column_name.length());
        if (privileges)
        {
          DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_column_table_revoke_update");
          error= table->file->ha_update_row(table->record[1], table->record[0]);
          DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_column_table_revoke_update");
          if (error && error != HA_ERR_RECORD_IS_THE_SAME)
          {                                        /* purecov: deadcode */
            acl_print_ha_error(table, error);     // deadcode
            result= -1;                         /* purecov: deadcode */
            goto end;                           /* purecov: deadcode */
          }
          if (grant_column)
            grant_column->rights  = privileges; // Update hash
        }
        else
        {
          DEBUG_SE_DELETE_ERROR_PRE("se_error_replace_column_table_revoke_delete");
          error= table->file->ha_delete_row(table->record[1]);
          DEBUG_SE_DELETE_ERROR_POST("se_error_replace_column_table_revoke_delete");
          if (error)
          {
            acl_print_ha_error(table, error);
            result= -1;
            goto end;
          }
          if (grant_column)
            my_hash_delete(&g_t->hash_columns,(uchar*) grant_column);
        }
      }
      error= table->file->ha_index_next(table->record[0]);
      DBUG_EXECUTE_IF("se_error_replace_column_table_revoke_read_next",
                      error= HA_ERR_LOCK_WAIT_TIMEOUT;);
      if (error)
      {
        if (error != HA_ERR_END_OF_FILE)
        {
          acl_print_ha_error(table, error);
          result= -1;
        }
        goto end;
      }
    } while (!key_cmp_if_same(table, key, 0, key_prefix_length));
  }

end:
  table->file->ha_index_end();
  DBUG_RETURN(result);
}

int replace_table_table(THD *thd, GRANT_TABLE *grant_table,
                        TABLE *table, const LEX_USER &combo,
                        const char *db, const char *table_name,
                        ulong rights, ulong col_rights,
                        bool revoke_grant)
{
  char grantor[USER_HOST_BUFF_SIZE];
  int old_row_exists = 1;
  int error=0;
  ulong store_table_rights, store_col_rights;
  uchar user_key[MAX_KEY_LENGTH];
  Acl_table_intact table_intact;
  DBUG_ENTER("replace_table_table");

  if (table_intact.check(table, &mysql_tables_priv_table_def))
    DBUG_RETURN(-1);

  get_grantor(thd, grantor);
  /*
    The following should always succeed as new users are created before
    this function is called!
  */
  if (!find_acl_user(combo.host.str,combo.user.str, FALSE))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH),
               MYF(0)); /* purecov: deadcode */
    DBUG_RETURN(1);                            /* purecov: deadcode */
  }

  if (strlen(grantor) > (table->field[4]->field_length /
                         table->field[4]->charset()->mbmaxlen))
  {
    my_error(ER_USER_COLUMN_OLD_LENGTH, MYF(0), table->field[4]->field_name);
    DBUG_RETURN(1);
  }

  table->use_all_columns();
  restore_record(table, s->default_values);     // Get empty record
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db, strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name, strlen(table_name),
                         system_charset_info);
  store_record(table,record[1]);                        // store at pos 1
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  error= table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT);

  DBUG_EXECUTE_IF("se_error_replace_table_table_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      acl_print_ha_error(table, error);
      DBUG_RETURN(-1);
    }
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
               combo.user.str, combo.host.str,
               table_name);                     /* purecov: deadcode */
      DBUG_RETURN(1);                           /* purecov: deadcode */
    }
    old_row_exists = 0;
    restore_record(table,record[1]);                    // Get saved record
  }

  store_table_rights= get_rights_for_table(rights);
  store_col_rights=   get_rights_for_column(col_rights);
  if (old_row_exists)
  {
    ulong j,k;
    store_record(table,record[1]);
    j = (ulong) table->field[6]->val_int();
    k = (ulong) table->field[7]->val_int();

    if (revoke_grant)
    {
      /* column rights are already fixed in mysql_table_grant */
      store_table_rights=j & ~store_table_rights;
    }
    else
    {
      store_table_rights|= j;
      store_col_rights|=   k;
    }
  }

  table->field[4]->store(grantor,strlen(grantor), system_charset_info);
  table->field[6]->store((longlong) store_table_rights, TRUE);
  table->field[7]->store((longlong) store_col_rights, TRUE);
  rights=fix_rights_for_table(store_table_rights);
  col_rights=fix_rights_for_column(store_col_rights);

  if (old_row_exists)
  {
    if (store_table_rights || store_col_rights)
    {
      DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_table_table_update");
      error=table->file->ha_update_row(table->record[1],
                                       table->record[0]);
      DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_table_table_update");

      if (error &&  error != HA_ERR_RECORD_IS_THE_SAME)
        goto table_error;
    }
    else if ((error = table->file->ha_delete_row(table->record[1])))
      goto table_error;                         /* purecov: deadcode */
  }
  else
  {
    DEBUG_SE_WRITE_ERROR_PRE("se_error_replace_table_table_add");
    error=table->file->ha_write_row(table->record[0]);
    DEBUG_SE_WRITE_ERROR_POST("se_error_replace_table_table_add");
    if (!table->file->is_ignorable_error(error))
      goto table_error;
  }

  if (rights | col_rights)
  {
    grant_table->privs= rights;
    grant_table->cols=  col_rights;
  }
  else
  {
    my_hash_delete(&column_priv_hash,(uchar*) grant_table);
  }
  DBUG_RETURN(0);

  /* This should never happen */
table_error:
  acl_print_ha_error(table, error); /* purecov: deadcode */
  DBUG_RETURN(-1); /* purecov: deadcode */
}


/**
  @retval       0  success
  @retval      -1  error
*/
int replace_routine_table(THD *thd, GRANT_NAME *grant_name,
                          TABLE *table, const LEX_USER &combo,
                          const char *db, const char *routine_name,
                          bool is_proc, ulong rights, bool revoke_grant)
{
  char grantor[USER_HOST_BUFF_SIZE];
  int old_row_exists= 1;
  int error=0;
  ulong store_proc_rights;
  Acl_table_intact table_intact;
  DBUG_ENTER("replace_routine_table");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  if (table_intact.check(table, &mysql_procs_priv_table_def))
    DBUG_RETURN(-1);

  get_grantor(thd, grantor);

  if (strlen(grantor) > (table->field[5]->field_length /
                         table->field[5]->charset()->mbmaxlen))
  {
    my_error(ER_USER_COLUMN_OLD_LENGTH, MYF(0), table->field[5]->field_name);
    DBUG_RETURN(-1);
  }

  /*
    New users are created before this function is called.

    There may be some cases where a routine's definer is removed but the
    routine remains.
  */

  table->use_all_columns();
  restore_record(table, s->default_values);             // Get empty record
  table->field[0]->store(combo.host.str,combo.host.length, &my_charset_latin1);
  table->field[1]->store(db, strlen(db), &my_charset_latin1);
  table->field[2]->store(combo.user.str,combo.user.length, &my_charset_latin1);
  table->field[3]->store(routine_name, strlen(routine_name),
                         &my_charset_latin1);
  table->field[4]->store((longlong)(is_proc ?
                                    SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION),
                         TRUE);
  store_record(table,record[1]);                        // store at pos 1

  error= table->file->ha_index_read_idx_map(table->record[0], 0,
                                            table->field[0]->ptr,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT);

  DBUG_EXECUTE_IF("se_error_replace_routine_table_read",
                  error= HA_ERR_LOCK_WAIT_TIMEOUT;);
  if (error)
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      acl_print_ha_error(table, error);
      DBUG_RETURN(-1);
    }
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_PROC_GRANT, MYF(0),
               combo.user.str, combo.host.str, routine_name);
      DBUG_RETURN(1);
    }
    old_row_exists= 0;
    restore_record(table,record[1]);                    // Get saved record
  }

  store_proc_rights= get_rights_for_procedure(rights);
  if (old_row_exists)
  {
    ulong j;
    store_record(table,record[1]);
    j= (ulong) table->field[6]->val_int();

    if (revoke_grant)
    {
      /* column rights are already fixed in mysql_table_grant */
      store_proc_rights=j & ~store_proc_rights;
    }
    else
    {
      store_proc_rights|= j;
    }
  }

  table->field[5]->store(grantor, strlen(grantor), &my_charset_latin1);
  table->field[6]->store((longlong) store_proc_rights, TRUE);
  rights=fix_rights_for_procedure(store_proc_rights);

  if (old_row_exists)
  {
    if (store_proc_rights)
    {
      DEBUG_SE_UPDATE_ERROR_PRE("se_error_replace_routine_table_update");
      error=table->file->ha_update_row(table->record[1],
                                       table->record[0]);
      DEBUG_SE_UPDATE_ERROR_POST("se_error_replace_routine_table_update");
      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
        goto table_error;
    }
    else
    {
      DEBUG_SE_DELETE_ERROR_PRE("se_error_replace_routine_table_delete");
      error= table->file->ha_delete_row(table->record[1]);
      DEBUG_SE_DELETE_ERROR_POST("se_error_replace_routine_table_delete");
      if (error)
        goto table_error;
    }
  }
  else
  {
    DEBUG_SE_WRITE_ERROR_PRE("se_error_replace_routine_table_add");
    error=table->file->ha_write_row(table->record[0]);
    DEBUG_SE_WRITE_ERROR_POST("se_error_replace_routine_table_add");
    if (!table->file->is_ignorable_error(error))
      goto table_error;
  }

  if (rights)
  {
    grant_name->privs= rights;
  }
  else
  {
    my_hash_delete(is_proc ? &proc_priv_hash : &func_priv_hash,(uchar*)
                   grant_name);
  }
  DBUG_RETURN(0);

  /* This should never happen */
table_error:
  acl_print_ha_error(table, error);
  DBUG_RETURN(-1);
}


/**
  Open the grant tables.

  @param          thd                   The current thread.
  @param[in/out]  tables                Array of GRANT_TABLES table list elements
                                        which will be used for opening tables.
  @param[out]     transactional_tables  Set to true if one of grant tables is
                                        transactional, false otherwise.

  @note
    Tables are numbered as follows:
    0 user
    1 db
    2 tables_priv
    3 columns_priv
    4 procs_priv
    5 proxies_priv

  @retval  1    Skip GRANT handling during replication.
  @retval  0    OK.
  @retval  < 0  Error.
*/


int open_grant_tables(THD *thd, TABLE_LIST *tables, bool *transactional_tables)
{
  DBUG_ENTER("open_grant_tables");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  *transactional_tables= false;

  tables->init_one_table(C_STRING_WITH_LEN("mysql"),
                         C_STRING_WITH_LEN("user"), "user", TL_WRITE);
  (tables+1)->init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("db"), "db", TL_WRITE);
  (tables+2)->init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("tables_priv"),
                             "tables_priv", TL_WRITE);
  (tables+3)->init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("columns_priv"),
                             "columns_priv", TL_WRITE);
  (tables+4)->init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("procs_priv"),
                             "procs_priv", TL_WRITE);
  (tables+5)->init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("proxies_priv"),
                             "proxies_priv", TL_WRITE);
  tables[5].open_strategy= TABLE_LIST::OPEN_IF_EXISTS;

  tables->next_local= tables->next_global= tables + 1;
  (tables+1)->next_local= (tables+1)->next_global= tables + 2;
  (tables+2)->next_local= (tables+2)->next_global= tables + 3;
  (tables+3)->next_local= (tables+3)->next_global= tables + 4;
  (tables+4)->next_local= (tables+4)->next_global= tables + 5;

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= tables[2].updating=
      tables[3].updating= tables[4].updating= tables[5].updating= 1;
    if (!(thd->sp_runtime_ctx || rpl_filter->tables_ok(0, tables)))
      DBUG_RETURN(1);
    tables[0].updating= tables[1].updating= tables[2].updating=
      tables[3].updating= tables[4].updating= tables[5].updating= 0;
  }
#endif

  if (open_and_lock_tables(thd, tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {                                             // This should never happen
    DBUG_RETURN(-1);
  }

  for (uint i= 0; i < GRANT_TABLES; ++i)
    *transactional_tables= (*transactional_tables ||
                            (tables[i].table &&
                             tables[i].table->file->has_transactions()));

  DBUG_RETURN(0);
}


/*
  Modify a privilege table.

  SYNOPSIS
    modify_grant_table()
    table                       The table to modify.
    host_field                  The host name field.
    user_field                  The user name field.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
  Update user/host in the current record if user_to is not NULL.
  Delete the current record if user_to is NULL.

  RETURN
    0           OK.
    != 0        Error.
*/

static int modify_grant_table(TABLE *table, Field *host_field,
                              Field *user_field, LEX_USER *user_to)
{
  int error;
  DBUG_ENTER("modify_grant_table");

  if (user_to)
  {
    /* rename */
    store_record(table, record[1]);
    host_field->store(user_to->host.str, user_to->host.length,
                      system_charset_info);
    user_field->store(user_to->user.str, user_to->user.length,
                      system_charset_info);
    DEBUG_SE_UPDATE_ERROR_PRE("se_error_modify_grant_table_update");
    error= table->file->ha_update_row(table->record[1],
                                      table->record[0]);
    DEBUG_SE_UPDATE_ERROR_POST("se_error_modify_grant_table_update");
    if (error && error != HA_ERR_RECORD_IS_THE_SAME)
      acl_print_ha_error(table, error);
    else
      error= 0;
  }
  else
  {
    /* delete */
    DEBUG_SE_DELETE_ERROR_PRE("se_error_modify_grant_table_delete");
    error= table->file->ha_delete_row(table->record[0]);
    DEBUG_SE_DELETE_ERROR_POST("se_error_modify_grant_table_delete");
    if (error)
      acl_print_ha_error(table, error);
  }

  DBUG_RETURN(error);
}

/*
  Handle a privilege table.

  SYNOPSIS
    handle_grant_table()
    tables                      The array with the four open tables.
    table_no                    The number of the table to handle (0..4).
    drop                        If user_from is to be dropped.
    user_from                   The the user to be searched/dropped/renamed.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
    Scan through all records in a grant table and apply the requested
    operation. For the "user" table, a single index access is sufficient,
    since there is an unique index on (host, user).
    Delete from grant table if drop is true.
    Update in grant table if drop is false and user_to is not NULL.
    Search in grant table if drop is false and user_to is NULL.
    Tables are numbered as follows:
    0 user
    1 db
    2 tables_priv
    3 columns_priv
    4 procs_priv

  RETURN
    > 0         At least one record matched.
    0           OK, but no record matched.
    < 0         Error.
*/

int handle_grant_table(TABLE_LIST *tables, uint table_no, bool drop,
                       LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int error;
  TABLE *table= tables[table_no].table;
  Field *host_field= table->field[0];
  Field *user_field= table->field[table_no && table_no != 5 ? 2 : 1];
  const char *host;
  const char *user;
  uchar user_key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  DBUG_ENTER("handle_grant_table");
  THD *thd= current_thd;

  table->use_all_columns();
  if (! table_no) // mysql.user table
  {
    /*
      The 'user' table has an unique index on (host, user).
      Thus, we can handle everything with a single index access.
      The host- and user fields are consecutive in the user table records.
      So we set host- and user fields of table->record[0] and use the
      pointer to the host field as key.
      index_read_idx() will replace table->record[0] (its first argument)
      by the searched record, if it exists.
    */
    DBUG_PRINT("info",("read table: '%s'  search: '%s'@'%s'",
                       table->s->table_name.str,
                       user_from->user.str,
                       user_from->host.str));
    host_field->store(user_from->host.str,
                      user_from->host.length,
                      system_charset_info);
    user_field->store(user_from->user.str,
                      user_from->user.length,
                      system_charset_info);
    
    key_prefix_length= (table->key_info->key_part[0].store_length +
                        table->key_info->key_part[1].store_length);
    key_copy(user_key, table->record[0], table->key_info, key_prefix_length);

    error= table->file->ha_index_read_idx_map(table->record[0], 0,
                                              user_key, (key_part_map)3,
                                              HA_READ_KEY_EXACT);

    DBUG_EXECUTE_IF("se_error_handle_grant_table_read",
                    error= HA_ERR_LOCK_WAIT_TIMEOUT;);
    if (error)
    {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        acl_print_ha_error(table, error);
        result= -1;
      }
    }
    else
    {
      /* If requested, delete or update the record. */
      result= ((drop || user_to) &&
               modify_grant_table(table, host_field, user_field, user_to)) ?
        -1 : 1; /* Error or found. */
    }
    DBUG_PRINT("info",("read result: %d", result));
  }
  else
  {
    /*
      The non-'user' table do not have indexes on (host, user).
      And their host- and user fields are not consecutive.
      Thus, we need to do a table scan to find all matching records.
    */
    if ((error= table->file->ha_rnd_init(1)))
    {
      acl_print_ha_error(table, error);
      result= -1;
    }
    else
    {
#ifdef EXTRA_DEBUG
      DBUG_PRINT("info",("scan table: '%s'  search: '%s'@'%s'",
                         table->s->table_name.str,
                         user_from->user.str,
                         user_from->host.str));
#endif
      while (true)
      {
        error= table->file->ha_rnd_next(table->record[0]);
        DBUG_EXECUTE_IF("se_error_handle_grant_table_rnd_read",
                        error= HA_ERR_LOCK_WAIT_TIMEOUT;);
        if (error)
        {
          if (error == HA_ERR_RECORD_DELETED)
            continue;

          if (error != HA_ERR_END_OF_FILE)
          {
            acl_print_ha_error(table, error);
            result= -1;
          }
          break;
        }

        if (! (host= get_field(thd->mem_root, host_field)))
          host= "";
        if (! (user= get_field(thd->mem_root, user_field)))
          user= "";

#ifdef EXTRA_DEBUG
        if (table_no != 5)
        {
          DBUG_PRINT("loop",("scan fields: '%s'@'%s' '%s' '%s' '%s'",
                             user, host,
                             get_field(thd->mem_root, table->field[1]) /*db*/,
                             get_field(thd->mem_root, table->field[3]) /*table*/,
                             get_field(thd->mem_root,
                                       table->field[4]) /*column*/));
        }
#endif
        if (strcmp(user_from->user.str, user) ||
            my_strcasecmp(system_charset_info, user_from->host.str, host))
          continue;

        /* If requested, delete or update the record. */
        result= ((drop || user_to) &&
                 modify_grant_table(table, host_field, user_field, user_to)) ?
          -1 : result ? result : 1; /* Error or keep result or found. */
        /* If search is requested, we do not need to search further. */
        if (! drop && ! user_to)
          break ;
      }
      (void) table->file->ha_rnd_end();
      DBUG_PRINT("info",("scan result: %d", result));
    }
  }

  DBUG_RETURN(result);
}


#endif /* NO_EMBEDDED_ACCESS_CHECKS */
