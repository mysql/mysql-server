/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/auth/role_tables.h"

#include <string.h>
#include <utility>

#include "lex_string.h"
#include "m_string.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/psi/psi_base.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "sql/auth/auth_internal.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/auth/sql_user_table.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"
#include "sql/records.h"
#include "sql/sql_base.h"
#include "sql/sql_const.h"
#include "sql/sql_servers.h"
#include "sql/table.h"
#include "thr_lock.h"

class THD;

bool trans_commit_stmt(THD *thd);
extern Granted_roles_graph *g_granted_roles;
extern Default_roles *g_default_roles;
extern Role_index_map *g_authid_to_vertex;

#define MYSQL_ROLE_EDGES_FIELD_FROM_HOST 0
#define MYSQL_ROLE_EDGES_FIELD_FROM_USER 1
#define MYSQL_ROLE_EDGES_FIELD_TO_HOST 2
#define MYSQL_ROLE_EDGES_FIELD_TO_USER 3
#define MYSQL_ROLE_EDGES_FIELD_TO_WITH_ADMIN_OPT 4

#define MYSQL_DEFAULT_ROLE_FIELD_HOST 0
#define MYSQL_DEFAULT_ROLE_FIELD_USER 1
#define MYSQL_DEFAULT_ROLE_FIELD_ROLE_HOST 2
#define MYSQL_DEFAULT_ROLE_FIELD_ROLE_USER 3

TABLE *open_role_edges_table(THD *thd)
{
  DBUG_ENTER("open_role_edges_table");
  TABLE_LIST tablelst;
  tablelst.init_one_table(C_STRING_WITH_LEN("mysql"),
                          C_STRING_WITH_LEN("role_edges"),
                          "role_edges",
                          TL_WRITE,
                          MDL_SHARED_NO_READ_WRITE);
  tablelst.next_local= tablelst.next_global= 0;

  if (open_and_lock_tables(thd, &tablelst, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    DBUG_PRINT("error",("An error occurred while trying to open the "
                        "mysql.roles_edges table"));
    DBUG_RETURN(0);
  }

  DBUG_RETURN(tablelst.table);
}

void close_all_role_tables(THD *thd)
{
  commit_and_close_mysql_tables(thd);
}

TABLE *open_default_role_table(THD *thd)
{
  DBUG_ENTER("open_role_edges_table");
  TABLE_LIST tablelst;
  tablelst.init_one_table(C_STRING_WITH_LEN("mysql"),
                          C_STRING_WITH_LEN("default_roles"),
                          "default_roles",
                          TL_WRITE,
                          MDL_SHARED_NO_READ_WRITE);
  tablelst.next_local= tablelst.next_global= 0;

  if (open_and_lock_tables(thd, &tablelst, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    DBUG_PRINT("error",("En error occurred while trying to open the "
                        "mysql.roles_edges table"));
    DBUG_RETURN(0);
  }

  DBUG_RETURN(tablelst.table);
}

bool modify_role_edges_in_table(THD *thd, TABLE *table,
                                const Auth_id_ref &from_user,
                                const Auth_id_ref &to_user,
                                bool with_admin_option,
                                bool delete_option)
{
  DBUG_ENTER("modify_role_edges_in_table");
  int ret= 0;
  uchar user_key[MAX_KEY_LENGTH];
  Acl_table_intact table_intact(thd);

  if (table_intact.check(table, ACL_TABLES::TABLE_ROLE_EDGES))
    DBUG_RETURN(true);

  table->use_all_columns();

  table->field[MYSQL_ROLE_EDGES_FIELD_FROM_HOST]
       ->store(from_user.second.str, from_user.second.length,
               system_charset_info);
  table->field[MYSQL_ROLE_EDGES_FIELD_FROM_USER]
       ->store(from_user.first.str, from_user.first.length,
               system_charset_info);
  table->field[MYSQL_ROLE_EDGES_FIELD_TO_HOST]
       ->store(to_user.second.str, to_user.second.length,
               system_charset_info);
  table->field[MYSQL_ROLE_EDGES_FIELD_TO_USER]
       ->store(to_user.first.str, to_user.first.length,
               system_charset_info);
  char with_admin_option_char= with_admin_option ? 'Y' : 'N';
  table->field[MYSQL_ROLE_EDGES_FIELD_TO_WITH_ADMIN_OPT]
       ->store(&with_admin_option_char, 1, system_charset_info,
               CHECK_FIELD_IGNORE);

  // How to get edge prop?!
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  ret= table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                          HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT);
  if (delete_option)
  {
    if (ret == 0)
    {
      DBUG_PRINT("note",("Delete role edge (`%s`@`%s`, `%s`@`%s`)",
                         from_user.first.str, from_user.second.str,
                         to_user.first.str, to_user.second.str));
      ret= table->file->ha_delete_row(table->record[0]);
    }
    else if (ret == HA_ERR_KEY_NOT_FOUND)
    {
      /* If the key didn't exist the record is already gone and all is well. */
      DBUG_RETURN(false);
    }
  }
  else if (ret == HA_ERR_KEY_NOT_FOUND && !delete_option)
  {
    /* Insert new edge into table */
    //store_record(table,record[1]);
    DBUG_PRINT("note",("Insert role edge (`%s`@`%s`, `%s`@`%s`)",
                       from_user.first.str, from_user.second.str,
                       to_user.first.str, to_user.second.str));
    ret= table->file->ha_write_row(table->record[0]);
  }
  DBUG_RETURN(ret != 0);
}

bool modify_default_roles_in_table(THD *thd, TABLE *table,
                                   const Auth_id_ref &auth_id,
                                   const Auth_id_ref &role,
                                   bool delete_option)
{
  DBUG_ENTER("modify_default_roles_in_table");
  int ret= 0;
  uchar user_key[MAX_KEY_LENGTH];
  Acl_table_intact table_intact(thd);

  if (table_intact.check(table, ACL_TABLES::TABLE_DEFAULT_ROLES))
    DBUG_RETURN(true);

  table->use_all_columns();
  table->field[MYSQL_DEFAULT_ROLE_FIELD_HOST]
       ->store(auth_id.second.str, auth_id.second.length,
               system_charset_info);
  table->field[MYSQL_DEFAULT_ROLE_FIELD_USER]
       ->store(auth_id.first.str, auth_id.first.length,
               system_charset_info);
  table->field[MYSQL_DEFAULT_ROLE_FIELD_ROLE_HOST]
       ->store(role.second.str, role.second.length,
               system_charset_info);
  table->field[MYSQL_DEFAULT_ROLE_FIELD_ROLE_USER]
       ->store(role.first.str, role.first.length,
               system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  ret= table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                          HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT);
  if (delete_option)
  {
    if (ret == 0)
    {
      DBUG_PRINT("note",("Delete default role `%s`@`%s` for `%s`@`%s`",
                         auth_id.first.str, auth_id.second.str, role.first.str,
                         role.second.str));
      ret= table->file->ha_delete_row(table->record[0]);
    }
  }
  else if (ret == HA_ERR_KEY_NOT_FOUND && !delete_option)
  {
    /* Insert new edge into table */
    //store_record(table,record[1]);
    DBUG_PRINT("note",("Insert default role `%s`@`%s` for `%s`@`%s`",
                       auth_id.first.str, auth_id.second.str, role.first.str,
                       role.second.str));
    ret= table->file->ha_write_row(table->record[0]);
  }
  DBUG_RETURN(false);
}

/*
  Populates caches from roles tables.
  Assumes that tables are opened and requried locks are taken.
  Assumes that caller will close the tables.

  @param [in] thd      Handle to THD object
  @param [in] tablelst Roles tables

  @returns status of cache update
    @retval false Success
    @retval true failure
*/

bool populate_roles_caches(THD *thd, TABLE_LIST *tablelst)
{
  DBUG_ENTER("populate_roles_caches");
  DBUG_ASSERT(assert_acl_cache_write_lock(thd));
  READ_RECORD read_record_info;
  /*
    To avoid any issues with inconsistencies we unconditionally increase
    acl cache version here.
  */
  get_global_acl_cache()->increase_version();
  if (!tablelst[0].table->key_info || !tablelst[1].table->key_info)
  {
    TABLE *table= ((!tablelst[0].table->key_info) ? tablelst[0].table :
                   tablelst[1].table);
    my_error(ER_MISSING_KEY, MYF(0), table->s->db.str,
             table->s->table_name.str);
    DBUG_RETURN(true);
  }

  TABLE *table= tablelst[0].table;
  table->use_all_columns();
  if (init_read_record(&read_record_info, thd, table,
                       NULL, 1, 1, FALSE))
  {
    my_error(ER_TABLE_CORRUPT, MYF(0), table->s->db.str,
             table->s->table_name.str);
    DBUG_RETURN(true);
  }
  ACL_USER *acl_role;
  ACL_USER *acl_user;
  int read_rec_errcode;
  MEM_ROOT tmp_mem;
  init_alloc_root(PSI_NOT_INSTRUMENTED, &tmp_mem, 128, 0);
  g_authid_to_vertex->clear();
  g_granted_roles->clear();
  while (!(read_rec_errcode= read_record_info.read_record(&read_record_info)))
  {
    char *from_host= get_field(&tmp_mem,
                               table->field[MYSQL_ROLE_EDGES_FIELD_FROM_HOST]);
    char *from_user= get_field(&tmp_mem,
                               table->field[MYSQL_ROLE_EDGES_FIELD_FROM_USER]);
    char *to_host= get_field(&tmp_mem,
                             table->field[MYSQL_ROLE_EDGES_FIELD_TO_HOST]);
    char *to_user= get_field(&tmp_mem,
                             table->field[MYSQL_ROLE_EDGES_FIELD_TO_USER]);
    char *with_admin_opt= get_field(&tmp_mem,
                                    table->field[MYSQL_ROLE_EDGES_FIELD_TO_WITH_ADMIN_OPT]);

    acl_role= find_acl_user(from_host, from_user, true);
    acl_user= find_acl_user(to_host, to_user, true);
    if (acl_user == NULL || acl_role == NULL)
    {
      my_printf_error(ER_UNKNOWN_ERROR,
                      "Unknown authorization identifier `%s`@`%s`",
                      MYF(0),
                      to_user,
                      to_host);
      rebuild_vertex_index(thd);
      end_read_record(&read_record_info);
      free_root(&tmp_mem, MYF(0));
      DBUG_RETURN(true);
    }
    grant_role(thd, acl_role, acl_user, *with_admin_opt == 'Y' ? 1 : 0);
  }
  end_read_record(&read_record_info);

  table= tablelst[1].table;
  table->use_all_columns();
  if (init_read_record(&read_record_info, thd, table,
                       NULL, 1, 1, FALSE))
  {
    my_error(ER_TABLE_CORRUPT, MYF(0), table->s->db.str,
             table->s->table_name.str);

    rebuild_vertex_index(thd);
    end_read_record(&read_record_info);
    free_root(&tmp_mem, MYF(0));
    DBUG_RETURN(true);
  }
  g_default_roles->clear();
  while (!(read_rec_errcode= read_record_info.read_record(&read_record_info)))
  {
    char *host= get_field(&tmp_mem,
                          table->field[MYSQL_DEFAULT_ROLE_FIELD_HOST]);
    char *user= get_field(&tmp_mem,
                          table->field[MYSQL_DEFAULT_ROLE_FIELD_USER]);
    char *role_host= get_field(&tmp_mem,
                               table->field[MYSQL_DEFAULT_ROLE_FIELD_ROLE_HOST]);
    char *role_user= get_field(&tmp_mem,
                               table->field[MYSQL_DEFAULT_ROLE_FIELD_ROLE_USER]);
    int user_len= (user ? strlen(user) : 0);
    int host_len= (host ? strlen(host) : 0);
    int role_user_len= (role_user ? strlen(role_user) : 0);
    int role_host_len= (role_host ? strlen(role_host) : 0);
    Role_id user_id(user, user_len, host, host_len);
    Role_id role_id(role_user, role_user_len, role_host, role_host_len);
    g_default_roles->insert(std::make_pair(user_id, role_id));
  }
  end_read_record(&read_record_info);
  free_root(&tmp_mem, MYF(0));
  rebuild_vertex_index(thd);
  opt_mandatory_roles_cache= false;
  DBUG_RETURN(false);
}

bool roles_init_from_tables(THD *thd)
{
  DBUG_ENTER("roles_init_from_tables");
  int ret= 0;

  // open table
  TABLE_LIST tablelst[2];
  tablelst[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("role_edges"),
                             "role_edges",
                             TL_READ,
                             MDL_SHARED_READ_ONLY);
  tablelst[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("default_roles"),
                             "default_roles",
                             TL_READ,
                             MDL_SHARED_READ_ONLY);
  tablelst[0].next_local= tablelst[0].next_global= &tablelst[1];
  tablelst[1].next_local= tablelst[1].next_global= 0;

  if ((ret= open_and_lock_tables(thd, &tablelst[0], MYSQL_LOCK_IGNORE_TIMEOUT)))
  {
    DBUG_PRINT("error",("En error occurred while trying to open the "
                        "mysql.roles_edges table"));
    DBUG_RETURN(true);
  }

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::WRITE_MODE);
  if (!acl_cache_lock.lock())
  {
    close_all_role_tables(thd);
    DBUG_RETURN(true);
  }

  if (populate_roles_caches(thd, tablelst))
  {
    close_all_role_tables(thd);
    DBUG_RETURN(true);
  }

  close_all_role_tables(thd);
  DBUG_RETURN(false);
}
