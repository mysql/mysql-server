/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


/*
  The privileges are saved in the following tables:
  mysql/user	 ; super user who are allowed to do almost anything
  mysql/host	 ; host privileges. This is used if host is empty in mysql/db.
  mysql/db	 ; database privileges / user

  data in tables is sorted according to how many not-wild-cards there is
  in the relevant fields. Empty strings comes last.
*/

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "sql_acl.h"         // MYSQL_DB_FIELD_COUNT, ACL_ACCESS
#include "sql_base.h"                           // close_mysql_tables
#include "key.h"             // key_copy, key_cmp_if_same, key_restore
#include "sql_show.h"        // append_identifier
#include "sql_table.h"                         // build_table_filename
#include "hash_filo.h"
#include "sql_parse.h"                          // check_access
#include "sql_view.h"                           // VIEW_ANY_ACL
#include "records.h"              // READ_RECORD, read_record_info,
                                  // init_read_record, end_read_record
#include "rpl_filter.h"           // rpl_filter
#include <m_ctype.h>
#include <stdarg.h>
#include "sp_head.h"
#include "sp.h"
#include "transaction.h"
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT
#include "records.h"             // init_read_record, end_read_record
#include <sql_common.h>
#include <mysql/plugin_auth.h>
#include "sql_connect.h"
#include "hostname.h"
#include "sql_db.h"
#include "sql_array.h"
#include "debug_sync.h"

bool mysql_user_table_is_in_short_password_format= false;

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
    { C_STRING_WITH_LEN("char(16)") },
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
    { C_STRING_WITH_LEN("char(16)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Password") },
    { C_STRING_WITH_LEN("char(41)") },
    { C_STRING_WITH_LEN("latin1") }
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
    { C_STRING_WITH_LEN("char(16)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Proxied_host") },
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Proxied_user") },
    { C_STRING_WITH_LEN("char(16)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("With_grant") },
    { C_STRING_WITH_LEN("tinyint(1)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Grantor") },
    { C_STRING_WITH_LEN("char(77)") },
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
    { C_STRING_WITH_LEN("char(16)") },
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
    { C_STRING_WITH_LEN("char(77)") },
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
    { C_STRING_WITH_LEN("char(16)") },
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
    { C_STRING_WITH_LEN("char(16)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Table_name") },
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Grantor") },
    { C_STRING_WITH_LEN("char(77)") },
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

static LEX_STRING native_password_plugin_name= {
  C_STRING_WITH_LEN("mysql_native_password")
};
  
static LEX_STRING old_password_plugin_name= {
  C_STRING_WITH_LEN("mysql_old_password")
};
  
/// @todo make it configurable
LEX_STRING *default_auth_plugin_name= &native_password_plugin_name;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static plugin_ref old_password_plugin;
#endif
static plugin_ref native_password_plugin;

/* Classes */

struct acl_host_and_ip
{
  char *hostname;
  long ip, ip_mask;                      // Used with masked ip:s
};

class ACL_ACCESS {
public:
  ulong sort;
  ulong access;
};

/* ACL_HOST is used if no host is specified */

class ACL_HOST :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  char *db;
};

class ACL_USER :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  uint hostname_length;
  USER_RESOURCES user_resource;
  char *user;
  uint8 salt[SCRAMBLE_LENGTH + 1];       // scrambled password in binary form
  uint8 salt_len;        // 0 - no password, 4 - 3.20, 8 - 4.0,  20 - 4.1.1 
  enum SSL_type ssl_type;
  const char *ssl_cipher, *x509_issuer, *x509_subject;
  LEX_STRING plugin;
  LEX_STRING auth_string;
  bool can_authenticate;

  ACL_USER *copy(MEM_ROOT *root)
  {
    ACL_USER *dst= (ACL_USER *) alloc_root(root, sizeof(ACL_USER));
    if (!dst)
      return 0;
    *dst= *this;
    dst->user= safe_strdup_root(root, user);
    dst->ssl_cipher= safe_strdup_root(root, ssl_cipher);
    dst->x509_issuer= safe_strdup_root(root, x509_issuer);
    dst->x509_subject= safe_strdup_root(root, x509_subject);
    if (plugin.str == native_password_plugin_name.str ||
        plugin.str == old_password_plugin_name.str)
      dst->plugin= plugin;
    else
      dst->plugin.str= strmake_root(root, plugin.str, plugin.length);
    dst->auth_string.str= safe_strdup_root(root, auth_string.str);
    dst->host.hostname= safe_strdup_root(root, host.hostname);
    return dst;
  }
};

class ACL_DB :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  char *user,*db;
};


#ifndef NO_EMBEDDED_ACCESS_CHECKS
static void update_hostname(acl_host_and_ip *host, const char *hostname);
static ulong get_sort(uint count,...);
static bool compare_hostname(const acl_host_and_ip *host, const char *hostname,
			     const char *ip);
static bool show_proxy_grants (THD *thd, LEX_USER *user,
                               char *buff, size_t buffsize);

class ACL_PROXY_USER :public ACL_ACCESS
{
  acl_host_and_ip host;
  const char *user;
  acl_host_and_ip proxied_host;
  const char *proxied_user;
  bool with_grant;

  typedef enum { 
    MYSQL_PROXIES_PRIV_HOST, 
    MYSQL_PROXIES_PRIV_USER, 
    MYSQL_PROXIES_PRIV_PROXIED_HOST,
    MYSQL_PROXIES_PRIV_PROXIED_USER, 
    MYSQL_PROXIES_PRIV_WITH_GRANT,
    MYSQL_PROXIES_PRIV_GRANTOR,
    MYSQL_PROXIES_PRIV_TIMESTAMP } old_acl_proxy_users;
public:
  ACL_PROXY_USER () {};

  void init(const char *host_arg, const char *user_arg,
       const char *proxied_host_arg, const char *proxied_user_arg,
       bool with_grant_arg)
  {
    user= (user_arg && *user_arg) ? user_arg : NULL;
    update_hostname (&host, 
                     (host_arg && *host_arg) ? host_arg : NULL);
    proxied_user= (proxied_user_arg && *proxied_user_arg) ? 
      proxied_user_arg : NULL;
    update_hostname (&proxied_host, 
                     (proxied_host_arg && *proxied_host_arg) ?
                     proxied_host_arg : NULL);
    with_grant= with_grant_arg;
    sort= get_sort(4, host.hostname, user,
                   proxied_host.hostname, proxied_user);
  }

  void init(MEM_ROOT *mem, const char *host_arg, const char *user_arg,
       const char *proxied_host_arg, const char *proxied_user_arg,
       bool with_grant_arg)
  {
    init ((host_arg && *host_arg) ? strdup_root (mem, host_arg) : NULL,
          (user_arg && *user_arg) ? strdup_root (mem, user_arg) : NULL,
          (proxied_host_arg && *proxied_host_arg) ? 
            strdup_root (mem, proxied_host_arg) : NULL,
          (proxied_user_arg && *proxied_user_arg) ? 
            strdup_root (mem, proxied_user_arg) : NULL,
          with_grant_arg);
  }

  void init(TABLE *table, MEM_ROOT *mem)
  {
    init (get_field(mem, table->field[MYSQL_PROXIES_PRIV_HOST]),
          get_field(mem, table->field[MYSQL_PROXIES_PRIV_USER]),
          get_field(mem, table->field[MYSQL_PROXIES_PRIV_PROXIED_HOST]),
          get_field(mem, table->field[MYSQL_PROXIES_PRIV_PROXIED_USER]),
          table->field[MYSQL_PROXIES_PRIV_WITH_GRANT]->val_int() != 0);
  }

  bool get_with_grant() { return with_grant; }
  const char *get_user() { return user; }
  const char *get_host() { return host.hostname; }
  const char *get_proxied_user() { return proxied_user; }
  const char *get_proxied_host() { return proxied_host.hostname; }
  void set_user(MEM_ROOT *mem, const char *user_arg) 
  { 
    user= user_arg && *user_arg ? strdup_root(mem, user_arg) : NULL;
  }
  void set_host(MEM_ROOT *mem, const char *host_arg) 
  { 
    update_hostname(&host, 
                    (host_arg && *host_arg) ? 
                    strdup_root(mem, host_arg) : NULL);
  }

  bool check_validity(bool check_no_resolve)
  {
    if (check_no_resolve && 
        (hostname_requires_resolving(host.hostname) ||
         hostname_requires_resolving(proxied_host.hostname)))
    {
      sql_print_warning("'proxies_priv' entry '%s@%s %s@%s' "
                        "ignored in --skip-name-resolve mode.",
                        proxied_user ? proxied_user : "",
                        proxied_host.hostname ? proxied_host.hostname : "",
                        user ? user : "",
                        host.hostname ? host.hostname : "");
      return TRUE;
    }
    return FALSE;
  }

  bool matches(const char *host_arg, const char *user_arg, const char *ip_arg,
                const char *proxied_user_arg)
  {
    DBUG_ENTER("ACL_PROXY_USER::matches");
    DBUG_PRINT("info", ("compare_hostname(%s,%s,%s) &&"
                        "compare_hostname(%s,%s,%s) &&"
                        "wild_compare (%s,%s) &&"
                        "wild_compare (%s,%s)",
                        host.hostname ? host.hostname : "<NULL>",
                        host_arg ? host_arg : "<NULL>",
                        ip_arg ? ip_arg : "<NULL>",
                        proxied_host.hostname ? proxied_host.hostname : "<NULL>",
                        host_arg ? host_arg : "<NULL>",
                        ip_arg ? ip_arg : "<NULL>",
                        user_arg ? user_arg : "<NULL>",
                        user ? user : "<NULL>",
                        proxied_user_arg ? proxied_user_arg : "<NULL>",
                        proxied_user ? proxied_user : "<NULL>"));
    DBUG_RETURN(compare_hostname(&host, host_arg, ip_arg) &&
                compare_hostname(&proxied_host, host_arg, ip_arg) &&
                (!user ||
                 (user_arg && !wild_compare(user_arg, user, TRUE))) &&
                (!proxied_user || 
                 (proxied_user && !wild_compare(proxied_user_arg, 
                                                proxied_user, TRUE))));
  }


  inline static bool auth_element_equals(const char *a, const char *b)
  {
    return (a == b || (a != NULL && b != NULL && !strcmp(a,b)));
  }


  bool pk_equals(ACL_PROXY_USER *grant)
  {
    DBUG_ENTER("pk_equals");
    DBUG_PRINT("info", ("strcmp(%s,%s) &&"
                        "strcmp(%s,%s) &&"
                        "wild_compare (%s,%s) &&"
                        "wild_compare (%s,%s)",
                        user ? user : "<NULL>",
                        grant->user ? grant->user : "<NULL>",
                        proxied_user ? proxied_user : "<NULL>",
                        grant->proxied_user ? grant->proxied_user : "<NULL>",
                        host.hostname ? host.hostname : "<NULL>",
                        grant->host.hostname ? grant->host.hostname : "<NULL>",
                        proxied_host.hostname ? proxied_host.hostname : "<NULL>",
                        grant->proxied_host.hostname ? 
                        grant->proxied_host.hostname : "<NULL>"));

    DBUG_RETURN(auth_element_equals(user, grant->user) &&
                auth_element_equals(proxied_user, grant->proxied_user) &&
                auth_element_equals(host.hostname, grant->host.hostname) &&
                auth_element_equals(proxied_host.hostname, 
                                    grant->proxied_host.hostname));
  }


  bool granted_on(const char *host_arg, const char *user_arg)
  {
    return (((!user && (!user_arg || !user_arg[0])) ||
             (user && user_arg && !strcmp(user, user_arg))) &&
            ((!host.hostname && (!host_arg || !host_arg[0])) ||
             (host.hostname && host_arg && !strcmp(host.hostname, host_arg))));
  }


  void print_grant(String *str)
  {
    str->append(STRING_WITH_LEN("GRANT PROXY ON '"));
    if (proxied_user)
      str->append(proxied_user, strlen(proxied_user));
    str->append(STRING_WITH_LEN("'@'"));
    if (proxied_host.hostname)
      str->append(proxied_host.hostname, strlen(proxied_host.hostname));
    str->append(STRING_WITH_LEN("' TO '"));
    if (user)
      str->append(user, strlen(user));
    str->append(STRING_WITH_LEN("'@'"));
    if (host.hostname)
      str->append(host.hostname, strlen(host.hostname));
    str->append(STRING_WITH_LEN("'"));
    if (with_grant)
      str->append(STRING_WITH_LEN(" WITH GRANT OPTION"));
  }

  void set_data(ACL_PROXY_USER *grant)
  {
    with_grant= grant->with_grant;
  }

  static int store_pk(TABLE *table, 
                      const LEX_STRING *host, 
                      const LEX_STRING *user,
                      const LEX_STRING *proxied_host, 
                      const LEX_STRING *proxied_user)
  {
    DBUG_ENTER("ACL_PROXY_USER::store_pk");
    DBUG_PRINT("info", ("host=%s, user=%s, proxied_host=%s, proxied_user=%s",
                        host->str ? host->str : "<NULL>",
                        user->str ? user->str : "<NULL>",
                        proxied_host->str ? proxied_host->str : "<NULL>",
                        proxied_user->str ? proxied_user->str : "<NULL>"));
    if (table->field[MYSQL_PROXIES_PRIV_HOST]->store(host->str, 
                                                   host->length,
                                                   system_charset_info))
      DBUG_RETURN(TRUE);
    if (table->field[MYSQL_PROXIES_PRIV_USER]->store(user->str, 
                                                   user->length,
                                                   system_charset_info))
      DBUG_RETURN(TRUE);
    if (table->field[MYSQL_PROXIES_PRIV_PROXIED_HOST]->store(proxied_host->str,
                                                           proxied_host->length,
                                                           system_charset_info))
      DBUG_RETURN(TRUE);
    if (table->field[MYSQL_PROXIES_PRIV_PROXIED_USER]->store(proxied_user->str,
                                                           proxied_user->length,
                                                           system_charset_info))
      DBUG_RETURN(TRUE);

    DBUG_RETURN(FALSE);
  }

  static int store_data_record(TABLE *table,
                               const LEX_STRING *host,
                               const LEX_STRING *user,
                               const LEX_STRING *proxied_host,
                               const LEX_STRING *proxied_user,
                               bool with_grant,
                               const char *grantor)
  {
    DBUG_ENTER("ACL_PROXY_USER::store_pk");
    if (store_pk(table,  host, user, proxied_host, proxied_user))
      DBUG_RETURN(TRUE);
    DBUG_PRINT("info", ("with_grant=%s", with_grant ? "TRUE" : "FALSE"));
    if (table->field[MYSQL_PROXIES_PRIV_WITH_GRANT]->store(with_grant ? 1 : 0, 
                                                         TRUE))
      DBUG_RETURN(TRUE);
    if (table->field[MYSQL_PROXIES_PRIV_GRANTOR]->store(grantor, 
                                                        strlen(grantor),
                                                        system_charset_info))
      DBUG_RETURN(TRUE);

    DBUG_RETURN(FALSE);
  }
};

#define FIRST_NON_YN_FIELD 26

class acl_entry :public hash_filo_element
{
public:
  ulong access;
  uint16 length;
  char key[1];					// Key will be stored here
};


static uchar* acl_entry_get_key(acl_entry *entry, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  *length=(uint) entry->length;
  return (uchar*) entry->key;
}

/**
  Class to validate the flawlessness of ACL table
  before performing ACL operations.
*/
class Acl_table_intact : public Table_check_intact
{
protected:
  void report_error(uint code, const char *fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    error_log_print(ERROR_LEVEL, fmt, args);
    va_end(args);
    if (code)
    {
      va_start(args, fmt);
      if (code == ER_CANNOT_LOAD_FROM_TABLE)
      {
        char *table_name;
        va_arg(args, char *);
        table_name= va_arg(args, char *);
        my_error(code, MYF(0), table_name);
      }
      else
        my_printv_error(code, ER(code), MYF(0), args);
      va_end(args);
    }
  }
public:
  Acl_table_intact() { has_keys= TRUE; }
};

#define IP_ADDR_STRLEN (3 + 1 + 3 + 1 + 3 + 1 + 3)
#define ACL_KEY_LENGTH (IP_ADDR_STRLEN + 1 + NAME_LEN + \
                        1 + USERNAME_LENGTH + 1)

/** Size of the header fields of an authentication packet. */
#define AUTH_PACKET_HEADER_SIZE_PROTO_41    32
#define AUTH_PACKET_HEADER_SIZE_PROTO_40    5  

static DYNAMIC_ARRAY acl_hosts, acl_users, acl_dbs, acl_proxy_users;
static MEM_ROOT mem, memex;
static bool initialized=0;
static bool allow_all_hosts=1;
static HASH acl_check_hosts, column_priv_hash, proc_priv_hash, func_priv_hash;
static DYNAMIC_ARRAY acl_wild_hosts;
static hash_filo *acl_cache;
static uint grant_version=0; /* Version of priv tables. incremented by acl_load */
static ulong get_access(TABLE *form,uint fieldnr, uint *next_field=0);
static int acl_compare(ACL_ACCESS *a,ACL_ACCESS *b);
static ulong get_sort(uint count,...);
static void init_check_host(void);
static void rebuild_check_host(void);
static ACL_USER *find_acl_user(const char *host, const char *user,
                               my_bool exact);
static bool update_user_table(THD *thd, TABLE *table,
                              const char *host, const char *user,
			      const char *new_password, uint new_password_len);
static my_bool acl_load(THD *thd, TABLE_LIST *tables);
static my_bool grant_load(THD *thd, TABLE_LIST *tables);
static inline void get_grantor(THD *thd, char* grantor);

/*
 Enumeration of various ACL's and Hashes used in handle_grant_struct()
*/
enum enum_acl_lists
{
  USER_ACL= 0,
  DB_ACL,
  COLUMN_PRIVILEGES_HASH,
  PROC_PRIVILEGES_HASH,
  FUNC_PRIVILEGES_HASH,
  PROXY_USERS_ACL
};

/*
  Convert scrambled password to binary form, according to scramble type, 
  Binary form is stored in user.salt.
*/

static
void
set_user_salt(ACL_USER *acl_user, const char *password, uint password_len)
{
  if (password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH)
  {
    get_salt_from_password(acl_user->salt, password);
    acl_user->salt_len= SCRAMBLE_LENGTH;
  }
  else if (password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
  {
    get_salt_from_password_323((ulong *) acl_user->salt, password);
    acl_user->salt_len= SCRAMBLE_LENGTH_323;
  }
  else
    acl_user->salt_len= 0;
}

/*
  Initialize structures responsible for user/db-level privilege checking and
  load privilege information for them from tables in the 'mysql' database.

  SYNOPSIS
    acl_init()
      dont_read_acl_tables  TRUE if we want to skip loading data from
                            privilege tables and disable privilege checking.

  NOTES
    This function is mostly responsible for preparatory steps, main work
    on initialization and grants loading is done in acl_reload().

  RETURN VALUES
    0	ok
    1	Could not initialize grant's
*/

my_bool acl_init(bool dont_read_acl_tables)
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("acl_init");

  acl_cache= new hash_filo(ACL_CACHE_SIZE, 0, 0,
                           (my_hash_get_key) acl_entry_get_key,
                           (my_hash_free_key) free,
                           &my_charset_utf8_bin);

  /*
    cache built-in native authentication plugins,
    to avoid hash searches and a global mutex lock on every connect
  */
  native_password_plugin= my_plugin_lock_by_name(0,
           &native_password_plugin_name, MYSQL_AUTHENTICATION_PLUGIN);
  old_password_plugin= my_plugin_lock_by_name(0,
           &old_password_plugin_name, MYSQL_AUTHENTICATION_PLUGIN);

  if (!native_password_plugin || !old_password_plugin)
    DBUG_RETURN(1);

  if (dont_read_acl_tables)
  {
    DBUG_RETURN(0); /* purecov: tested */
  }

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_RETURN(1); /* purecov: inspected */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  /*
    It is safe to call acl_reload() since acl_* arrays and hashes which
    will be freed there are global static objects and thus are initialized
    by zeros at startup.
  */
  return_val= acl_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_RETURN(return_val);
}

/**
  Choose from either native or old password plugins when assigning a password
*/

static bool
set_user_plugin (ACL_USER *user, int password_len)
{
  switch (password_len) 
  {
  case 0: /* no password */
  case SCRAMBLED_PASSWORD_CHAR_LENGTH:
    user->plugin= native_password_plugin_name;
    return FALSE;
  case SCRAMBLED_PASSWORD_CHAR_LENGTH_323:
    user->plugin= old_password_plugin_name;
    return FALSE;
  case 45: /* 4.1: to be removed */
    sql_print_warning("Found 4.1.0 style password for user '%s@%s'. "
                      "Ignoring user. "
                      "You should change password for this user.",
                      user->user ? user->user : "",
                      user->host.hostname ? user->host.hostname : "");
    return TRUE;
  default:
    sql_print_warning("Found invalid password for user: '%s@%s'; "
                      "Ignoring user", user->user ? user->user : "",
                      user->host.hostname ? user->host.hostname : "");
    return TRUE;
  }
}


/*
  Initialize structures responsible for user/db-level privilege checking
  and load information about grants from open privilege tables.

  SYNOPSIS
    acl_load()
      thd     Current thread
      tables  List containing open "mysql.host", "mysql.user" and
              "mysql.db" tables.

  RETURN VALUES
    FALSE  Success
    TRUE   Error
*/

static my_bool acl_load(THD *thd, TABLE_LIST *tables)
{
  TABLE *table;
  READ_RECORD read_record_info;
  my_bool return_val= TRUE;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  char tmp_name[NAME_LEN+1];
  int password_length;
  ulong old_sql_mode= thd->variables.sql_mode;
  DBUG_ENTER("acl_load");

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  grant_version++; /* Privileges updated */

  acl_cache->clear(1);				// Clear locked hostname cache

  init_sql_alloc(&mem, ACL_ALLOC_BLOCK_SIZE, 0);
  init_read_record(&read_record_info,thd,table= tables[0].table,NULL,1,0, 
                   FALSE);
  table->use_all_columns();
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_HOST host;
    update_hostname(&host.host,get_field(&mem, table->field[0]));
    host.db=	 get_field(&mem, table->field[1]);
    if (lower_case_table_names && host.db)
    {
      /*
        convert db to lower case and give a warning if the db wasn't
        already in lower case
      */
      (void) strmov(tmp_name, host.db);
      my_casedn_str(files_charset_info, host.db);
      if (strcmp(host.db, tmp_name) != 0)
        sql_print_warning("'host' entry '%s|%s' had database in mixed "
                          "case that has been forced to lowercase because "
                          "lower_case_table_names is set. It will not be "
                          "possible to remove this privilege using REVOKE.",
                          host.host.hostname ? host.host.hostname : "",
                          host.db ? host.db : "");
    }
    host.access= get_access(table,2);
    host.access= fix_rights_for_db(host.access);
    host.sort=	 get_sort(2,host.host.hostname,host.db);
    if (check_no_resolve && hostname_requires_resolving(host.host.hostname))
    {
      sql_print_warning("'host' entry '%s|%s' "
		      "ignored in --skip-name-resolve mode.",
			host.host.hostname ? host.host.hostname : "",
			host.db ? host.db : "");
      continue;
    }
#ifndef TO_BE_REMOVED
    if (table->s->fields == 8)
    {						// Without grant
      if (host.access & CREATE_ACL)
	host.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL;
    }
#endif
    (void) push_dynamic(&acl_hosts,(uchar*) &host);
  }
  my_qsort((uchar*) dynamic_element(&acl_hosts,0,ACL_HOST*),acl_hosts.elements,
	   sizeof(ACL_HOST),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_hosts);

  init_read_record(&read_record_info,thd,table=tables[1].table,NULL,1,0,FALSE);
  table->use_all_columns();
  password_length= table->field[2]->field_length /
    table->field[2]->charset()->mbmaxlen;
  if (password_length < SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
  {
    sql_print_error("Fatal error: mysql.user table is damaged or in "
                    "unsupported 3.20 format.");
    goto end;
  }

  DBUG_PRINT("info",("user table fields: %d, password length: %d",
		     table->s->fields, password_length));

  mysql_mutex_lock(&LOCK_global_system_variables);
  if (password_length < SCRAMBLED_PASSWORD_CHAR_LENGTH)
  {
    if (opt_secure_auth)
    {
      mysql_mutex_unlock(&LOCK_global_system_variables);
      sql_print_error("Fatal error: mysql.user table is in old format, "
                      "but server started with --secure-auth option.");
      goto end;
    }
    mysql_user_table_is_in_short_password_format= true;
    if (global_system_variables.old_passwords)
      mysql_mutex_unlock(&LOCK_global_system_variables);
    else
    {
      global_system_variables.old_passwords= 1;
      mysql_mutex_unlock(&LOCK_global_system_variables);
      sql_print_warning("mysql.user table is not updated to new password format; "
                        "Disabling new password usage until "
                        "mysql_fix_privilege_tables is run");
    }
    thd->variables.old_passwords= 1;
  }
  else
  {
    mysql_user_table_is_in_short_password_format= false;
    mysql_mutex_unlock(&LOCK_global_system_variables);
  }

  allow_all_hosts=0;
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_USER user;
    bzero(&user, sizeof(user));

    /*
      All accounts can authenticate per default. This will change when
      we add a new field to the user table.

      Currently this flag is only set to false when authentication is attempted
      using an unknown user name.
    */
    user.can_authenticate= true;

    update_hostname(&user.host, get_field(&mem, table->field[0]));
    user.user= get_field(&mem, table->field[1]);
    if (check_no_resolve && hostname_requires_resolving(user.host.hostname))
    {
      sql_print_warning("'user' entry '%s@%s' "
                        "ignored in --skip-name-resolve mode.",
			user.user ? user.user : "",
			user.host.hostname ? user.host.hostname : "");
      continue;
    }

    char *password= get_field(&mem, table->field[2]);
    uint password_len= password ? strlen(password) : 0;
    set_user_salt(&user, password, password_len);

    if (set_user_plugin(&user, password_len))
      continue;
    
    {
      uint next_field;
      user.access= get_access(table,3,&next_field) & GLOBAL_ACLS;
      /*
        if it is pre 5.0.1 privilege table then map CREATE privilege on
        CREATE VIEW & SHOW VIEW privileges
      */
      if (table->s->fields <= 31 && (user.access & CREATE_ACL))
        user.access|= (CREATE_VIEW_ACL | SHOW_VIEW_ACL);

      /*
        if it is pre 5.0.2 privilege table then map CREATE/ALTER privilege on
        CREATE PROCEDURE & ALTER PROCEDURE privileges
      */
      if (table->s->fields <= 33 && (user.access & CREATE_ACL))
        user.access|= CREATE_PROC_ACL;
      if (table->s->fields <= 33 && (user.access & ALTER_ACL))
        user.access|= ALTER_PROC_ACL;

      /*
        pre 5.0.3 did not have CREATE_USER_ACL
      */
      if (table->s->fields <= 36 && (user.access & GRANT_ACL))
        user.access|= CREATE_USER_ACL;


      /*
        if it is pre 5.1.6 privilege table then map CREATE privilege on
        CREATE|ALTER|DROP|EXECUTE EVENT
      */
      if (table->s->fields <= 37 && (user.access & SUPER_ACL))
        user.access|= EVENT_ACL;

      /*
        if it is pre 5.1.6 privilege then map TRIGGER privilege on CREATE.
      */
      if (table->s->fields <= 38 && (user.access & SUPER_ACL))
        user.access|= TRIGGER_ACL;

      user.sort= get_sort(2,user.host.hostname,user.user);
      user.hostname_length= (user.host.hostname ?
                             (uint) strlen(user.host.hostname) : 0);

      /* Starting from 4.0.2 we have more fields */
      if (table->s->fields >= 31)
      {
        char *ssl_type=get_field(thd->mem_root, table->field[next_field++]);
        if (!ssl_type)
          user.ssl_type=SSL_TYPE_NONE;
        else if (!strcmp(ssl_type, "ANY"))
          user.ssl_type=SSL_TYPE_ANY;
        else if (!strcmp(ssl_type, "X509"))
          user.ssl_type=SSL_TYPE_X509;
        else  /* !strcmp(ssl_type, "SPECIFIED") */
          user.ssl_type=SSL_TYPE_SPECIFIED;

        user.ssl_cipher=   get_field(&mem, table->field[next_field++]);
        user.x509_issuer=  get_field(&mem, table->field[next_field++]);
        user.x509_subject= get_field(&mem, table->field[next_field++]);

        char *ptr = get_field(thd->mem_root, table->field[next_field++]);
        user.user_resource.questions=ptr ? atoi(ptr) : 0;
        ptr = get_field(thd->mem_root, table->field[next_field++]);
        user.user_resource.updates=ptr ? atoi(ptr) : 0;
        ptr = get_field(thd->mem_root, table->field[next_field++]);
        user.user_resource.conn_per_hour= ptr ? atoi(ptr) : 0;
        if (user.user_resource.questions || user.user_resource.updates ||
            user.user_resource.conn_per_hour)
          mqh_used=1;

        if (table->s->fields >= 36)
        {
          /* Starting from 5.0.3 we have max_user_connections field */
          ptr= get_field(thd->mem_root, table->field[next_field++]);
          user.user_resource.user_conn= ptr ? atoi(ptr) : 0;
        }

        if (table->s->fields >= 41)
        {
          /* We may have plugin & auth_String fields */
          char *tmpstr= get_field(&mem, table->field[next_field++]);
          if (tmpstr)
          {
            if (password_len)
            {
              sql_print_warning("'user' entry '%s@%s' has both a password "
                                "and an authentication plugin specified. The "
                                "password will be ignored.",
                                user.user ? user.user : "",
                                user.host.hostname ? user.host.hostname : "");
            }
            if (my_strcasecmp(system_charset_info, tmpstr,
                              native_password_plugin_name.str) == 0)
              user.plugin= native_password_plugin_name;
            else
              if (my_strcasecmp(system_charset_info, tmpstr,
                                old_password_plugin_name.str) == 0)
                user.plugin= old_password_plugin_name;
              else
              {
                user.plugin.str= tmpstr;
                user.plugin.length= strlen(tmpstr);
              }
            user.auth_string.str= get_field(&mem, table->field[next_field++]);
            if (!user.auth_string.str)
              user.auth_string.str= const_cast<char*>("");
            user.auth_string.length= strlen(user.auth_string.str);
          }
        }
      }
      else
      {
        user.ssl_type=SSL_TYPE_NONE;
#ifndef TO_BE_REMOVED
        if (table->s->fields <= 13)
        {						// Without grant
          if (user.access & CREATE_ACL)
            user.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL;
        }
        /* Convert old privileges */
        user.access|= LOCK_TABLES_ACL | CREATE_TMP_ACL | SHOW_DB_ACL;
        if (user.access & FILE_ACL)
          user.access|= REPL_CLIENT_ACL | REPL_SLAVE_ACL;
        if (user.access & PROCESS_ACL)
          user.access|= SUPER_ACL | EXECUTE_ACL;
#endif
      }
      (void) push_dynamic(&acl_users,(uchar*) &user);
      if (!user.host.hostname ||
	  (user.host.hostname[0] == wild_many && !user.host.hostname[1]))
        allow_all_hosts=1;			// Anyone can connect
    }
  }
  my_qsort((uchar*) dynamic_element(&acl_users,0,ACL_USER*),acl_users.elements,
	   sizeof(ACL_USER),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_users);

  init_read_record(&read_record_info,thd,table=tables[2].table,NULL,1,0,FALSE);
  table->use_all_columns();
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_DB db;
    update_hostname(&db.host,get_field(&mem, table->field[MYSQL_DB_FIELD_HOST]));
    db.db=get_field(&mem, table->field[MYSQL_DB_FIELD_DB]);
    if (!db.db)
    {
      sql_print_warning("Found an entry in the 'db' table with empty database name; Skipped");
      continue;
    }
    db.user=get_field(&mem, table->field[MYSQL_DB_FIELD_USER]);
    if (check_no_resolve && hostname_requires_resolving(db.host.hostname))
    {
      sql_print_warning("'db' entry '%s %s@%s' "
		        "ignored in --skip-name-resolve mode.",
		        db.db,
			db.user ? db.user : "",
			db.host.hostname ? db.host.hostname : "");
      continue;
    }
    db.access=get_access(table,3);
    db.access=fix_rights_for_db(db.access);
    if (lower_case_table_names)
    {
      /*
        convert db to lower case and give a warning if the db wasn't
        already in lower case
      */
      (void)strmov(tmp_name, db.db);
      my_casedn_str(files_charset_info, db.db);
      if (strcmp(db.db, tmp_name) != 0)
      {
        sql_print_warning("'db' entry '%s %s@%s' had database in mixed "
                          "case that has been forced to lowercase because "
                          "lower_case_table_names is set. It will not be "
                          "possible to remove this privilege using REVOKE.",
		          db.db,
			  db.user ? db.user : "",
			  db.host.hostname ? db.host.hostname : "");
      }
    }
    db.sort=get_sort(3,db.host.hostname,db.db,db.user);
#ifndef TO_BE_REMOVED
    if (table->s->fields <=  9)
    {						// Without grant
      if (db.access & CREATE_ACL)
	db.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL;
    }
#endif
    (void) push_dynamic(&acl_dbs,(uchar*) &db);
  }
  my_qsort((uchar*) dynamic_element(&acl_dbs,0,ACL_DB*),acl_dbs.elements,
	   sizeof(ACL_DB),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_dbs);

  if (tables[3].table)
  {
    init_read_record(&read_record_info, thd, table= tables[3].table, NULL, 1, 
                     0, FALSE);
    table->use_all_columns();
    while (!(read_record_info.read_record(&read_record_info)))
    {
      ACL_PROXY_USER proxy;
      proxy.init(table, &mem);
      if (proxy.check_validity(check_no_resolve))
        continue;
      if (push_dynamic(&acl_proxy_users, (uchar*) &proxy))
      {
        end_read_record(&read_record_info);
        goto end;
      }
    }
    my_qsort((uchar*) dynamic_element(&acl_proxy_users, 0, ACL_PROXY_USER*),
             acl_proxy_users.elements,
             sizeof(ACL_PROXY_USER), (qsort_cmp) acl_compare);
    end_read_record(&read_record_info);
  }
  else
  {
    sql_print_error("Missing system table mysql.proxies_priv; "
                    "please run mysql_upgrade to create it");
  }
  freeze_size(&acl_proxy_users);

  init_check_host();

  initialized=1;
  return_val= FALSE;

end:
  end_read_record(&read_record_info);
  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(return_val);
}


void acl_free(bool end)
{
  free_root(&mem,MYF(0));
  delete_dynamic(&acl_hosts);
  delete_dynamic(&acl_users);
  delete_dynamic(&acl_dbs);
  delete_dynamic(&acl_wild_hosts);
  delete_dynamic(&acl_proxy_users);
  my_hash_free(&acl_check_hosts);
  if (!end)
    acl_cache->clear(1); /* purecov: inspected */
  else
  {
    plugin_unlock(0, native_password_plugin);
    plugin_unlock(0, old_password_plugin);
    delete acl_cache;
    acl_cache=0;
  }
}


/*
  Forget current user/db-level privileges and read new privileges
  from the privilege tables.

  SYNOPSIS
    acl_reload()
      thd  Current thread

  NOTE
    All tables of calling thread which were open and locked by LOCK TABLES
    statement will be unlocked and closed.
    This function is also used for initialization of structures responsible
    for user/db-level privilege checking.

  RETURN VALUE
    FALSE  Success
    TRUE   Failure
*/

my_bool acl_reload(THD *thd)
{
  TABLE_LIST tables[4];
  DYNAMIC_ARRAY old_acl_hosts, old_acl_users, old_acl_dbs, old_acl_proxy_users;
  MEM_ROOT old_mem;
  bool old_initialized;
  my_bool return_val= TRUE;
  DBUG_ENTER("acl_reload");

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining acl_cache->lock mutex.
  */
  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("host"), "host", TL_READ);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("user"), "user", TL_READ);
  tables[2].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("db"), "db", TL_READ);
  tables[3].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("proxies_priv"), 
                           "proxies_priv", TL_READ);
  tables[0].next_local= tables[0].next_global= tables + 1;
  tables[1].next_local= tables[1].next_global= tables + 2;
  tables[2].next_local= tables[2].next_global= tables + 3;
  tables[0].open_type= tables[1].open_type= tables[2].open_type= 
  tables[3].open_type= OT_BASE_ONLY;
  tables[3].open_strategy= TABLE_LIST::OPEN_IF_EXISTS;

  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    /*
      Execution might have been interrupted; only print the error message
      if an error condition has been raised.
    */
    if (thd->stmt_da->is_error())
      sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
                      thd->stmt_da->message());
    goto end;
  }

  if ((old_initialized=initialized))
    mysql_mutex_lock(&acl_cache->lock);

  old_acl_hosts= acl_hosts;
  old_acl_users= acl_users;
  old_acl_proxy_users= acl_proxy_users;
  old_acl_dbs= acl_dbs;
  my_init_dynamic_array(&acl_hosts, sizeof(ACL_HOST), 20, 50);
  my_init_dynamic_array(&acl_users, sizeof(ACL_USER), 50, 100);
  my_init_dynamic_array(&acl_dbs, sizeof(ACL_DB), 50, 100);
  my_init_dynamic_array(&acl_proxy_users, sizeof(ACL_PROXY_USER), 50, 100);
  old_mem= mem;
  delete_dynamic(&acl_wild_hosts);
  my_hash_free(&acl_check_hosts);

  if ((return_val= acl_load(thd, tables)))
  {					// Error. Revert to old list
    DBUG_PRINT("error",("Reverting to old privileges"));
    acl_free();				/* purecov: inspected */
    acl_hosts= old_acl_hosts;
    acl_users= old_acl_users;
    acl_proxy_users= old_acl_proxy_users;
    acl_dbs= old_acl_dbs;
    mem= old_mem;
    init_check_host();
  }
  else
  {
    free_root(&old_mem,MYF(0));
    delete_dynamic(&old_acl_hosts);
    delete_dynamic(&old_acl_users);
    delete_dynamic(&old_acl_proxy_users);
    delete_dynamic(&old_acl_dbs);
  }
  if (old_initialized)
    mysql_mutex_unlock(&acl_cache->lock);
end:
  close_mysql_tables(thd);

  DEBUG_SYNC(thd, "after_acl_reload");
  DBUG_RETURN(return_val);
}


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

static ulong get_access(TABLE *form, uint fieldnr, uint *next_field)
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


/*
  Return a number which, if sorted 'desc', puts strings in this order:
    no wildcards
    strings containg wildcards and non-wildcard characters
    single muilt-wildcard character('%')
    empty string
*/

static ulong get_sort(uint count,...)
{
  va_list args;
  va_start(args,count);
  ulong sort=0;

  /* Should not use this function with more than 4 arguments for compare. */
  DBUG_ASSERT(count <= 4);

  while (count--)
  {
    char *start, *str= va_arg(args,char*);
    uint chars= 0;
    uint wild_pos= 0;

    /*
      wild_pos
        0                            if string is empty
        1                            if string is a single muilt-wildcard
                                     character('%')
        first wildcard position + 1  if string containg wildcards and
                                     non-wildcard characters
    */

    if ((start= str))
    {
      for (; *str ; str++)
      {
        if (*str == wild_prefix && str[1])
          str++;
        else if (*str == wild_many || *str == wild_one)
        {
          wild_pos= (uint) (str - start) + 1;
          if (!(wild_pos == 1 && *str == wild_many && *(++str) == '\0'))
            wild_pos++;
          break;
        }
        chars= 128;                             // Marker that chars existed
      }
    }
    sort= (sort << 8) + (wild_pos ? min(wild_pos, 127) : chars);
  }
  va_end(args);
  return sort;
}


static int acl_compare(ACL_ACCESS *a,ACL_ACCESS *b)
{
  if (a->sort > b->sort)
    return -1;
  if (a->sort < b->sort)
    return 1;
  return 0;
}


/*
  Gets user credentials without authentication and resource limit checks.

  SYNOPSIS
    acl_getroot()
      sctx               Context which should be initialized
      user               user name
      host               host name
      ip                 IP
      db                 current data base name

  RETURN
    FALSE  OK
    TRUE   Error
*/

bool acl_getroot(Security_context *sctx, char *user, char *host,
                 char *ip, char *db)
{
  int res= 1;
  uint i;
  ACL_USER *acl_user= 0;
  DBUG_ENTER("acl_getroot");

  DBUG_PRINT("enter", ("Host: '%s', Ip: '%s', User: '%s', db: '%s'",
                       (host ? host : "(NULL)"), (ip ? ip : "(NULL)"),
                       user, (db ? db : "(NULL)")));
  sctx->user= user;
  sctx->set_host(host);
  sctx->set_ip(ip);
  sctx->host_or_ip= host ? host : (ip ? ip : "");

  if (!initialized)
  {
    /*
      here if mysqld's been started with --skip-grant-tables option.
    */
    sctx->skip_grants();
    DBUG_RETURN(FALSE);
  }

  mysql_mutex_lock(&acl_cache->lock);

  sctx->master_access= 0;
  sctx->db_access= 0;
  *sctx->priv_user= *sctx->priv_host= 0;

  /*
     Find acl entry in user database.
     This is specially tailored to suit the check we do for CALL of
     a stored procedure; user is set to what is actually a
     priv_user, which can be ''.
  */
  for (i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user_tmp= dynamic_element(&acl_users,i,ACL_USER*);
    if ((!acl_user_tmp->user && !user[0]) ||
        (acl_user_tmp->user && strcmp(user, acl_user_tmp->user) == 0))
    {
      if (compare_hostname(&acl_user_tmp->host, host, ip))
      {
        acl_user= acl_user_tmp;
        res= 0;
        break;
      }
    }
  }

  if (acl_user)
  {
    for (i=0 ; i < acl_dbs.elements ; i++)
    {
      ACL_DB *acl_db= dynamic_element(&acl_dbs, i, ACL_DB*);
      if (!acl_db->user ||
	  (user && user[0] && !strcmp(user, acl_db->user)))
      {
	if (compare_hostname(&acl_db->host, host, ip))
	{
	  if (!acl_db->db || (db && !wild_compare(db, acl_db->db, 0)))
	  {
	    sctx->db_access= acl_db->access;
	    break;
	  }
	}
      }
    }
    sctx->master_access= acl_user->access;

    if (acl_user->user)
      strmake(sctx->priv_user, user, USERNAME_LENGTH);
    else
      *sctx->priv_user= 0;

    if (acl_user->host.hostname)
      strmake(sctx->priv_host, acl_user->host.hostname, MAX_HOSTNAME - 1);
    else
      *sctx->priv_host= 0;
  }
  mysql_mutex_unlock(&acl_cache->lock);
  DBUG_RETURN(res);
}

static uchar* check_get_key(ACL_USER *buff, size_t *length,
                            my_bool not_used __attribute__((unused)))
{
  *length=buff->hostname_length;
  return (uchar*) buff->host.hostname;
}


static void acl_update_user(const char *user, const char *host,
			    const char *password, uint password_len,
			    enum SSL_type ssl_type,
			    const char *ssl_cipher,
			    const char *x509_issuer,
			    const char *x509_subject,
			    USER_RESOURCES  *mqh,
			    ulong privileges,
			    const LEX_STRING *plugin,
			    const LEX_STRING *auth)
{
  mysql_mutex_assert_owner(&acl_cache->lock);

  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
    if ((!acl_user->user && !user[0]) ||
	(acl_user->user && !strcmp(user,acl_user->user)))
    {
      if ((!acl_user->host.hostname && !host[0]) ||
	  (acl_user->host.hostname &&
	  !my_strcasecmp(system_charset_info, host, acl_user->host.hostname)))
      {
        if (plugin->str[0])
        {
          acl_user->plugin.str= strmake_root(&mem, plugin->str, plugin->length);
          acl_user->plugin.length= plugin->length;
          acl_user->auth_string.str= auth->str ?
            strmake_root(&mem, auth->str, auth->length) : const_cast<char*>("");
          acl_user->auth_string.length= auth->length;
        }
	acl_user->access=privileges;
	if (mqh->specified_limits & USER_RESOURCES::QUERIES_PER_HOUR)
	  acl_user->user_resource.questions=mqh->questions;
	if (mqh->specified_limits & USER_RESOURCES::UPDATES_PER_HOUR)
	  acl_user->user_resource.updates=mqh->updates;
	if (mqh->specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR)
	  acl_user->user_resource.conn_per_hour= mqh->conn_per_hour;
	if (mqh->specified_limits & USER_RESOURCES::USER_CONNECTIONS)
	  acl_user->user_resource.user_conn= mqh->user_conn;
	if (ssl_type != SSL_TYPE_NOT_SPECIFIED)
	{
	  acl_user->ssl_type= ssl_type;
	  acl_user->ssl_cipher= (ssl_cipher ? strdup_root(&mem,ssl_cipher) :
				 0);
	  acl_user->x509_issuer= (x509_issuer ? strdup_root(&mem,x509_issuer) :
				  0);
	  acl_user->x509_subject= (x509_subject ?
				   strdup_root(&mem,x509_subject) : 0);
	}
	if (password)
	  set_user_salt(acl_user, password, password_len);
        /* search complete: */
	break;
      }
    }
  }
}


static void acl_insert_user(const char *user, const char *host,
			    const char *password, uint password_len,
			    enum SSL_type ssl_type,
			    const char *ssl_cipher,
			    const char *x509_issuer,
			    const char *x509_subject,
			    USER_RESOURCES *mqh,
			    ulong privileges,
			    const LEX_STRING *plugin,
			    const LEX_STRING *auth)
{
  ACL_USER acl_user;

  mysql_mutex_assert_owner(&acl_cache->lock);
  /*
     All accounts can authenticate per default. This will change when
     we add a new field to the user table.

     Currently this flag is only set to false when authentication is attempted
     using an unknown user name.
  */
  acl_user.can_authenticate= true;

  acl_user.user=*user ? strdup_root(&mem,user) : 0;
  update_hostname(&acl_user.host, *host ? strdup_root(&mem, host): 0);
  if (plugin->str[0])
  {
    acl_user.plugin.str= strmake_root(&mem, plugin->str, plugin->length);
    acl_user.plugin.length= plugin->length;
    acl_user.auth_string.str= auth->str ?
      strmake_root(&mem, auth->str, auth->length) : const_cast<char*>("");
    acl_user.auth_string.length= auth->length;
  }
  else
  {
    acl_user.plugin= password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH_323 ?
      old_password_plugin_name : native_password_plugin_name;
    acl_user.auth_string.str= const_cast<char*>("");
    acl_user.auth_string.length= 0;
  }

  acl_user.access=privileges;
  acl_user.user_resource = *mqh;
  acl_user.sort=get_sort(2,acl_user.host.hostname,acl_user.user);
  acl_user.hostname_length=(uint) strlen(host);
  acl_user.ssl_type= (ssl_type != SSL_TYPE_NOT_SPECIFIED ?
		      ssl_type : SSL_TYPE_NONE);
  acl_user.ssl_cipher=	ssl_cipher   ? strdup_root(&mem,ssl_cipher) : 0;
  acl_user.x509_issuer= x509_issuer  ? strdup_root(&mem,x509_issuer) : 0;
  acl_user.x509_subject=x509_subject ? strdup_root(&mem,x509_subject) : 0;

  set_user_salt(&acl_user, password, password_len);

  (void) push_dynamic(&acl_users,(uchar*) &acl_user);
  if (!acl_user.host.hostname ||
      (acl_user.host.hostname[0] == wild_many && !acl_user.host.hostname[1]))
    allow_all_hosts=1;		// Anyone can connect /* purecov: tested */
  my_qsort((uchar*) dynamic_element(&acl_users,0,ACL_USER*),acl_users.elements,
	   sizeof(ACL_USER),(qsort_cmp) acl_compare);

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();
}


static void acl_update_db(const char *user, const char *host, const char *db,
			  ulong privileges)
{
  mysql_mutex_assert_owner(&acl_cache->lock);

  for (uint i=0 ; i < acl_dbs.elements ; i++)
  {
    ACL_DB *acl_db=dynamic_element(&acl_dbs,i,ACL_DB*);
    if ((!acl_db->user && !user[0]) ||
	(acl_db->user &&
	!strcmp(user,acl_db->user)))
    {
      if ((!acl_db->host.hostname && !host[0]) ||
	  (acl_db->host.hostname &&
          !strcmp(host, acl_db->host.hostname)))
      {
	if ((!acl_db->db && !db[0]) ||
	    (acl_db->db && !strcmp(db,acl_db->db)))
	{
	  if (privileges)
	    acl_db->access=privileges;
	  else
	    delete_dynamic_element(&acl_dbs,i);
	}
      }
    }
  }
}


/*
  Insert a user/db/host combination into the global acl_cache

  SYNOPSIS
    acl_insert_db()
    user		User name
    host		Host name
    db			Database name
    privileges		Bitmap of privileges

  NOTES
    acl_cache->lock must be locked when calling this
*/

static void acl_insert_db(const char *user, const char *host, const char *db,
			  ulong privileges)
{
  ACL_DB acl_db;
  mysql_mutex_assert_owner(&acl_cache->lock);
  acl_db.user=strdup_root(&mem,user);
  update_hostname(&acl_db.host, *host ? strdup_root(&mem,host) : 0);
  acl_db.db=strdup_root(&mem,db);
  acl_db.access=privileges;
  acl_db.sort=get_sort(3,acl_db.host.hostname,acl_db.db,acl_db.user);
  (void) push_dynamic(&acl_dbs,(uchar*) &acl_db);
  my_qsort((uchar*) dynamic_element(&acl_dbs,0,ACL_DB*),acl_dbs.elements,
	   sizeof(ACL_DB),(qsort_cmp) acl_compare);
}



/*
  Get privilege for a host, user and db combination

  as db_is_pattern changes the semantics of comparison,
  acl_cache is not used if db_is_pattern is set.
*/

ulong acl_get(const char *host, const char *ip,
              const char *user, const char *db, my_bool db_is_pattern)
{
  ulong host_access= ~(ulong)0, db_access= 0;
  uint i;
  size_t key_length, copy_length;
  char key[ACL_KEY_LENGTH],*tmp_db,*end;
  acl_entry *entry;
  DBUG_ENTER("acl_get");

  copy_length= (size_t) (strlen(ip ? ip : "") +
                 strlen(user ? user : "") +
                 strlen(db ? db : "")) + 2; /* Added 2 at the end to avoid  
                                               buffer overflow at strmov()*/
  /*
    Make sure that strmov() operations do not result in buffer overflow.
  */
  if (copy_length >= ACL_KEY_LENGTH)
    DBUG_RETURN(0);

  mysql_mutex_lock(&acl_cache->lock);
  end=strmov((tmp_db=strmov(strmov(key, ip ? ip : "")+1,user)+1),db);
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, tmp_db);
    db=tmp_db;
  }
  key_length= (size_t) (end-key);
  if (!db_is_pattern && (entry=(acl_entry*) acl_cache->search((uchar*) key,
                                                              key_length)))
  {
    db_access=entry->access;
    mysql_mutex_unlock(&acl_cache->lock);
    DBUG_PRINT("exit", ("access: 0x%lx", db_access));
    DBUG_RETURN(db_access);
  }

  /*
    Check if there are some access rights for database and user
  */
  for (i=0 ; i < acl_dbs.elements ; i++)
  {
    ACL_DB *acl_db=dynamic_element(&acl_dbs,i,ACL_DB*);
    if (!acl_db->user || !strcmp(user,acl_db->user))
    {
      if (compare_hostname(&acl_db->host,host,ip))
      {
	if (!acl_db->db || !wild_compare(db,acl_db->db,db_is_pattern))
	{
	  db_access=acl_db->access;
	  if (acl_db->host.hostname)
	    goto exit;				// Fully specified. Take it
	  break; /* purecov: tested */
	}
      }
    }
  }
  if (!db_access)
    goto exit;					// Can't be better

  /*
    No host specified for user. Get hostdata from host table
  */
  host_access=0;				// Host must be found
  for (i=0 ; i < acl_hosts.elements ; i++)
  {
    ACL_HOST *acl_host=dynamic_element(&acl_hosts,i,ACL_HOST*);
    if (compare_hostname(&acl_host->host,host,ip))
    {
      if (!acl_host->db || !wild_compare(db,acl_host->db,db_is_pattern))
      {
	host_access=acl_host->access;		// Fully specified. Take it
	break;
      }
    }
  }
exit:
  /* Save entry in cache for quick retrieval */
  if (!db_is_pattern &&
      (entry= (acl_entry*) malloc(sizeof(acl_entry)+key_length)))
  {
    entry->access=(db_access & host_access);
    entry->length=key_length;
    memcpy((uchar*) entry->key,key,key_length);
    acl_cache->add(entry);
  }
  mysql_mutex_unlock(&acl_cache->lock);
  DBUG_PRINT("exit", ("access: 0x%lx", db_access & host_access));
  DBUG_RETURN(db_access & host_access);
}

/*
  Check if there are any possible matching entries for this host

  NOTES
    All host names without wild cards are stored in a hash table,
    entries with wildcards are stored in a dynamic array
*/

static void init_check_host(void)
{
  DBUG_ENTER("init_check_host");
  (void) my_init_dynamic_array(&acl_wild_hosts,sizeof(struct acl_host_and_ip),
			  acl_users.elements,1);
  (void) my_hash_init(&acl_check_hosts,system_charset_info,
                      acl_users.elements, 0, 0,
                      (my_hash_get_key) check_get_key, 0, 0);
  if (!allow_all_hosts)
  {
    for (uint i=0 ; i < acl_users.elements ; i++)
    {
      ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
      if (strchr(acl_user->host.hostname,wild_many) ||
	  strchr(acl_user->host.hostname,wild_one) ||
	  acl_user->host.ip_mask)
      {						// Has wildcard
	uint j;
	for (j=0 ; j < acl_wild_hosts.elements ; j++)
	{					// Check if host already exists
	  acl_host_and_ip *acl=dynamic_element(&acl_wild_hosts,j,
					       acl_host_and_ip *);
	  if (!my_strcasecmp(system_charset_info,
                             acl_user->host.hostname, acl->hostname))
	    break;				// already stored
	}
	if (j == acl_wild_hosts.elements)	// If new
	  (void) push_dynamic(&acl_wild_hosts,(uchar*) &acl_user->host);
      }
      else if (!my_hash_search(&acl_check_hosts,(uchar*)
                               acl_user->host.hostname,
                               strlen(acl_user->host.hostname)))
      {
	if (my_hash_insert(&acl_check_hosts,(uchar*) acl_user))
	{					// End of memory
	  allow_all_hosts=1;			// Should never happen
	  DBUG_VOID_RETURN;
	}
      }
    }
  }
  freeze_size(&acl_wild_hosts);
  freeze_size(&acl_check_hosts.array);
  DBUG_VOID_RETURN;
}


/*
  Rebuild lists used for checking of allowed hosts

  We need to rebuild 'acl_check_hosts' and 'acl_wild_hosts' after adding,
  dropping or renaming user, since they contain pointers to elements of
  'acl_user' array, which are invalidated by drop operation, and use
  ACL_USER::host::hostname as a key, which is changed by rename.
*/
void rebuild_check_host(void)
{
  delete_dynamic(&acl_wild_hosts);
  my_hash_free(&acl_check_hosts);
  init_check_host();
}


/* Return true if there is no users that can match the given host */

bool acl_check_host(const char *host, const char *ip)
{
  if (allow_all_hosts)
    return 0;
  mysql_mutex_lock(&acl_cache->lock);

  if ((host && my_hash_search(&acl_check_hosts,(uchar*) host,strlen(host))) ||
      (ip && my_hash_search(&acl_check_hosts,(uchar*) ip, strlen(ip))))
  {
    mysql_mutex_unlock(&acl_cache->lock);
    return 0;					// Found host
  }
  for (uint i=0 ; i < acl_wild_hosts.elements ; i++)
  {
    acl_host_and_ip *acl=dynamic_element(&acl_wild_hosts,i,acl_host_and_ip*);
    if (compare_hostname(acl, host, ip))
    {
      mysql_mutex_unlock(&acl_cache->lock);
      return 0;					// Host ok
    }
  }
  mysql_mutex_unlock(&acl_cache->lock);
  return 1;					// Host is not allowed
}


/*
  Check if the user is allowed to change password

  SYNOPSIS:
    check_change_password()
    thd		THD
    host	hostname for the user
    user	user name
    new_password new password

  NOTE:
    new_password cannot be NULL

    RETURN VALUE
      0		OK
      1		ERROR  ; In this case the error is sent to the client.
*/

int check_change_password(THD *thd, const char *host, const char *user,
                           char *new_password, uint new_password_len)
{
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    return(1);
  }
  if (!thd->slave_thread &&
      (strcmp(thd->security_ctx->user, user) ||
       my_strcasecmp(system_charset_info, host,
                     thd->security_ctx->priv_host)))
  {
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 0))
      return(1);
  }
  if (!thd->slave_thread && !thd->security_ctx->user[0])
  {
    my_message(ER_PASSWORD_ANONYMOUS_USER, ER(ER_PASSWORD_ANONYMOUS_USER),
               MYF(0));
    return(1);
  }
  size_t len= strlen(new_password);
  if (len && len != SCRAMBLED_PASSWORD_CHAR_LENGTH &&
      len != SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
  {
    my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH);
    return -1;
  }
  return(0);
}


/*
  Change a password for a user

  SYNOPSIS
    change_password()
    thd			Thread handle
    host		Hostname
    user		User name
    new_password	New password for host@user

  RETURN VALUES
    0	ok
    1	ERROR; In this case the error is sent to the client.
*/

bool change_password(THD *thd, const char *host, const char *user,
		     char *new_password)
{
  TABLE_LIST tables;
  TABLE *table;
  Acl_table_intact table_intact;
  /* Buffer should be extended when password length is extended. */
  char buff[512];
  ulong query_length;
  bool save_binlog_row_based;
  uint new_password_len= (uint) strlen(new_password);
  bool result= 1;
  DBUG_ENTER("change_password");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'  new_password: '%s'",
		      host,user,new_password));
  DBUG_ASSERT(host != 0);			// Ensured by parent

  if (check_change_password(thd, host, user, new_password, new_password_len))
    DBUG_RETURN(1);

  tables.init_one_table("mysql", 5, "user", 4, "user", TL_WRITE);

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.  It's ok to leave 'updating' set after tables_ok.
    */
    tables.updating= 1;
    /* Thanks to bzero, tables.next==0 */
    if (!(thd->spcont || rpl_filter->tables_ok(0, &tables)))
      DBUG_RETURN(0);
  }
#endif
  if (!(table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(1);

  if (table_intact.check(table, &mysql_user_table_def))
    DBUG_RETURN(1);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  mysql_mutex_lock(&acl_cache->lock);
  ACL_USER *acl_user;
  if (!(acl_user= find_acl_user(host, user, TRUE)))
  {
    mysql_mutex_unlock(&acl_cache->lock);
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH), MYF(0));
    goto end;
  }

  /* update loaded acl entry: */
  set_user_salt(acl_user, new_password, new_password_len);

  if (my_strcasecmp(system_charset_info, acl_user->plugin.str,
                    native_password_plugin_name.str) &&
      my_strcasecmp(system_charset_info, acl_user->plugin.str,
                    old_password_plugin_name.str))
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                 ER_SET_PASSWORD_AUTH_PLUGIN, ER(ER_SET_PASSWORD_AUTH_PLUGIN));
  else
    set_user_plugin(acl_user, new_password_len);

  if (update_user_table(thd, table,
			acl_user->host.hostname ? acl_user->host.hostname : "",
			acl_user->user ? acl_user->user : "",
			new_password, new_password_len))
  {
    mysql_mutex_unlock(&acl_cache->lock); /* purecov: deadcode */
    goto end;
  }

  acl_cache->clear(1);				// Clear locked hostname cache
  mysql_mutex_unlock(&acl_cache->lock);
  result= 0;
  if (mysql_bin_log.is_open())
  {
    query_length= sprintf(buff, "SET PASSWORD FOR '%-.120s'@'%-.120s'='%-.120s'",
                          acl_user->user ? acl_user->user : "",
                          acl_user->host.hostname ? acl_user->host.hostname : "",
                          new_password);
    thd->clear_error();
    result= thd->binlog_query(THD::STMT_QUERY_TYPE, buff, query_length,
                              FALSE, FALSE, FALSE, 0);
  }
end:
  close_mysql_tables(thd);

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(result);
}


/*
  Find user in ACL

  SYNOPSIS
    is_acl_user()
    host                 host name
    user                 user name

  RETURN
   FALSE  user not fond
   TRUE   there are such user
*/

bool is_acl_user(const char *host, const char *user)
{
  bool res;

  /* --skip-grants */
  if (!initialized)
    return TRUE;

  mysql_mutex_lock(&acl_cache->lock);
  res= find_acl_user(host, user, TRUE) != NULL;
  mysql_mutex_unlock(&acl_cache->lock);
  return res;
}


/*
  Find first entry that matches the current user
*/

static ACL_USER *
find_acl_user(const char *host, const char *user, my_bool exact)
{
  DBUG_ENTER("find_acl_user");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'",host,user));

  mysql_mutex_assert_owner(&acl_cache->lock);

  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
    DBUG_PRINT("info",("strcmp('%s','%s'), compare_hostname('%s','%s'),",
                       user, acl_user->user ? acl_user->user : "",
                       host,
                       acl_user->host.hostname ? acl_user->host.hostname :
                       ""));
    if ((!acl_user->user && !user[0]) ||
	(acl_user->user && !strcmp(user,acl_user->user)))
    {
      if (exact ? !my_strcasecmp(system_charset_info, host,
                                 acl_user->host.hostname ?
				 acl_user->host.hostname : "") :
          compare_hostname(&acl_user->host,host,host))
      {
	DBUG_RETURN(acl_user);
      }
    }
  }
  DBUG_RETURN(0);
}


/*
  Comparing of hostnames

  NOTES
  A hostname may be of type:
  hostname   (May include wildcards);   monty.pp.sci.fi
  ip	   (May include wildcards);   192.168.0.0
  ip/netmask			      192.168.0.0/255.255.255.0

  A net mask of 0.0.0.0 is not allowed.
*/

static const char *calc_ip(const char *ip, long *val, char end)
{
  long ip_val,tmp;
  if (!(ip=str2int(ip,10,0,255,&ip_val)) || *ip != '.')
    return 0;
  ip_val<<=24;
  if (!(ip=str2int(ip+1,10,0,255,&tmp)) || *ip != '.')
    return 0;
  ip_val+=tmp<<16;
  if (!(ip=str2int(ip+1,10,0,255,&tmp)) || *ip != '.')
    return 0;
  ip_val+=tmp<<8;
  if (!(ip=str2int(ip+1,10,0,255,&tmp)) || *ip != end)
    return 0;
  *val=ip_val+tmp;
  return ip;
}


static void update_hostname(acl_host_and_ip *host, const char *hostname)
{
  host->hostname=(char*) hostname;             // This will not be modified!
  if (!hostname ||
      (!(hostname=calc_ip(hostname,&host->ip,'/')) ||
       !(hostname=calc_ip(hostname+1,&host->ip_mask,'\0'))))
  {
    host->ip= host->ip_mask=0;			// Not a masked ip
  }
}


static bool compare_hostname(const acl_host_and_ip *host, const char *hostname,
			     const char *ip)
{
  long tmp;
  if (host->ip_mask && ip && calc_ip(ip,&tmp,'\0'))
  {
    return (tmp & host->ip_mask) == host->ip;
  }
  return (!host->hostname ||
	  (hostname && !wild_case_compare(system_charset_info,
                                          hostname, host->hostname)) ||
	  (ip && !wild_compare(ip, host->hostname, 0)));
}

/**
  Check if the given host name needs to be resolved or not.
  Host name has to be resolved if it actually contains *name*.

  For example:
    192.168.1.1               --> FALSE
    192.168.1.0/255.255.255.0 --> FALSE
    %                         --> FALSE
    192.168.1.%               --> FALSE
    AB%                       --> FALSE

    AAAAFFFF                  --> TRUE (Hostname)
    AAAA:FFFF:1234:5678       --> FALSE
    ::1                       --> FALSE

  This function does not check if the given string is a valid host name or
  not. It assumes that the argument is a valid host name.

  @param hostname   the string to check.

  @return a flag telling if the argument needs to be resolved or not.
  @retval TRUE the argument is a host name and needs to be resolved.
  @retval FALSE the argument is either an IP address, or a patter and
          should not be resolved.
*/

bool hostname_requires_resolving(const char *hostname)
{
  if (!hostname)
    return FALSE;

  /* Check if hostname is the localhost. */

  size_t hostname_len= strlen(hostname);
  size_t localhost_len= strlen(my_localhost);

  if (hostname == my_localhost ||
      (hostname_len == localhost_len &&
       !my_strnncoll(system_charset_info,
                     (const uchar *) hostname,  hostname_len,
                     (const uchar *) my_localhost, strlen(my_localhost))))
  {
    return FALSE;
  }

  /*
    If the string contains any of {':', '%', '_', '/'}, it is definitely
    not a host name:
      - ':' means that the string is an IPv6 address;
      - '%' or '_' means that the string is a pattern;
      - '/' means that the string is an IPv4 network address;
  */

  for (const char *p= hostname; *p; ++p)
  {
    switch (*p) {
      case ':':
      case '%':
      case '_':
      case '/':
        return FALSE;
    }
  }

  /*
    Now we have to tell a host name (ab.cd, 12.ab) from an IPv4 address
    (12.34.56.78). The assumption is that if the string contains only
    digits and dots, it is an IPv4 address. Otherwise -- a host name.
  */

  for (const char *p= hostname; *p; ++p)
  {
    if (*p != '.' && !my_isdigit(&my_charset_latin1, *p))
      return TRUE; /* a "letter" has been found. */
  }

  return FALSE; /* all characters are either dots or digits. */
}


/*
  Update record for user in mysql.user privilege table with new password.

  SYNOPSIS
    update_user_table()
      thd               Thread handle
      table             Pointer to TABLE object for open mysql.user table
      host/user         Hostname/username pair identifying user for which
                        new password should be set
      new_password      New password
      new_password_len  Length of new password
*/

static bool update_user_table(THD *thd, TABLE *table,
                              const char *host, const char *user,
			      const char *new_password, uint new_password_len)
{
  char user_key[MAX_KEY_LENGTH];
  int error;
  DBUG_ENTER("update_user_table");
  DBUG_PRINT("enter",("user: %s  host: %s",user,host));

  table->use_all_columns();
  table->field[0]->store(host,(uint) strlen(host), system_charset_info);
  table->field[1]->store(user,(uint) strlen(user), system_charset_info);
  key_copy((uchar *) user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0,
                                      (uchar *) user_key, HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH),
               MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(1);				/* purecov: deadcode */
  }
  store_record(table,record[1]);
  table->field[2]->store(new_password, new_password_len, system_charset_info);
  if ((error=table->file->ha_update_row(table->record[1],table->record[0])) &&
      error != HA_ERR_RECORD_IS_THE_SAME)
  {
    table->file->print_error(error,MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Return 1 if we are allowed to create new users
  the logic here is: INSERT_ACL is sufficient.
  It's also a requirement in opt_safe_user_create,
  otherwise CREATE_USER_ACL is enough.
*/

static bool test_if_create_new_users(THD *thd)
{
  Security_context *sctx= thd->security_ctx;
  bool create_new_users= test(sctx->master_access & INSERT_ACL) ||
                         (!opt_safe_user_create &&
                          test(sctx->master_access & CREATE_USER_ACL));
  if (!create_new_users)
  {
    TABLE_LIST tl;
    ulong db_access;
    tl.init_one_table(C_STRING_WITH_LEN("mysql"),
                      C_STRING_WITH_LEN("user"), "user", TL_WRITE);
    create_new_users= 1;

    db_access=acl_get(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
		      sctx->priv_user, tl.db, 0);
    if (!(db_access & INSERT_ACL))
    {
      if (check_grant(thd, INSERT_ACL, &tl, FALSE, UINT_MAX, TRUE))
	create_new_users=0;
    }
  }
  return create_new_users;
}


/****************************************************************************
  Handle GRANT commands
****************************************************************************/

static int replace_user_table(THD *thd, TABLE *table, const LEX_USER &combo,
			      ulong rights, bool revoke_grant,
			      bool can_create_user, bool no_auto_create)
{
  int error = -1;
  bool old_row_exists=0;
  const char *password= "";
  uint password_len= 0;
  char what= (revoke_grant) ? 'N' : 'Y';
  uchar user_key[MAX_KEY_LENGTH];
  LEX *lex= thd->lex;
  Acl_table_intact table_intact;
  DBUG_ENTER("replace_user_table");

  mysql_mutex_assert_owner(&acl_cache->lock);

  if (table_intact.check(table, &mysql_user_table_def))
    goto end;

  if (combo.password.str && combo.password.str[0])
  {
    if (combo.password.length != SCRAMBLED_PASSWORD_CHAR_LENGTH &&
        combo.password.length != SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
    {
      my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH);
      DBUG_RETURN(-1);
    }
    password_len= combo.password.length;
    password=combo.password.str;
  }

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    /* what == 'N' means revoke */
    if (what == 'N')
    {
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo.user.str, combo.host.str);
      goto end;
    }
    /*
      There are four options which affect the process of creation of
      a new user (mysqld option --safe-create-user, 'insert' privilege
      on 'mysql.user' table, using 'GRANT' with 'IDENTIFIED BY' and
      SQL_MODE flag NO_AUTO_CREATE_USER). Below is the simplified rule
      how it should work.
      if (safe-user-create && ! INSERT_priv) => reject
      else if (identified_by) => create
      else if (no_auto_create_user) => reject
      else create

      see also test_if_create_new_users()
    */
    else if (!password_len && !combo.plugin.length && no_auto_create)
    {
      my_error(ER_PASSWORD_NO_MATCH, MYF(0));
      goto end;
    }
    else if (!can_create_user)
    {
      my_error(ER_CANT_CREATE_USER_WITH_GRANT, MYF(0));
      goto end;
    }
    else if (combo.plugin.str[0])
    {
      if (!plugin_is_ready(&combo.plugin, MYSQL_AUTHENTICATION_PLUGIN))
      {
        my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), combo.plugin.str);
        goto end;
      }
    }

    old_row_exists = 0;
    restore_record(table,s->default_values);
    table->field[0]->store(combo.host.str,combo.host.length,
                           system_charset_info);
    table->field[1]->store(combo.user.str,combo.user.length,
                           system_charset_info);
    table->field[2]->store(password, password_len,
                           system_charset_info);
  }
  else
  {
    old_row_exists = 1;
    store_record(table,record[1]);			// Save copy for update
    /* what == 'N' means revoke */
    if (combo.plugin.length && what != 'N')
    {
        my_error(ER_GRANT_PLUGIN_USER_EXISTS, MYF(0),
                 static_cast<int>(combo.user.length), combo.user.str);
        goto end;
    }
    if (combo.password.str)                             // If password given
      table->field[2]->store(password, password_len, system_charset_info);
    else if (!rights && !revoke_grant &&
             lex->ssl_type == SSL_TYPE_NOT_SPECIFIED &&
             !lex->mqh.specified_limits)
    {
      DBUG_RETURN(0);
    }
  }

  /* Update table columns with new privileges */

  Field **tmp_field;
  ulong priv;
  uint next_field;
  for (tmp_field= table->field+3, priv = SELECT_ACL;
       *tmp_field && (*tmp_field)->real_type() == MYSQL_TYPE_ENUM &&
	 ((Field_enum*) (*tmp_field))->typelib->count == 2 ;
       tmp_field++, priv <<= 1)
  {
    if (priv & rights)				 // set requested privileges
      (*tmp_field)->store(&what, 1, &my_charset_latin1);
  }
  rights= get_access(table, 3, &next_field);
  DBUG_PRINT("info",("table fields: %d",table->s->fields));
  if (table->s->fields >= 31)		/* From 4.0.0 we have more fields */
  {
    /* We write down SSL related ACL stuff */
    switch (lex->ssl_type) {
    case SSL_TYPE_ANY:
      table->field[next_field]->store(STRING_WITH_LEN("ANY"),
                                      &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_X509:
      table->field[next_field]->store(STRING_WITH_LEN("X509"),
                                      &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_SPECIFIED:
      table->field[next_field]->store(STRING_WITH_LEN("SPECIFIED"),
                                      &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      if (lex->ssl_cipher)
        table->field[next_field+1]->store(lex->ssl_cipher,
                                strlen(lex->ssl_cipher), system_charset_info);
      if (lex->x509_issuer)
        table->field[next_field+2]->store(lex->x509_issuer,
                                strlen(lex->x509_issuer), system_charset_info);
      if (lex->x509_subject)
        table->field[next_field+3]->store(lex->x509_subject,
                                strlen(lex->x509_subject), system_charset_info);
      break;
    case SSL_TYPE_NOT_SPECIFIED:
      break;
    case SSL_TYPE_NONE:
      table->field[next_field]->store("", 0, &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    }
    next_field+=4;

    USER_RESOURCES mqh= lex->mqh;
    if (mqh.specified_limits & USER_RESOURCES::QUERIES_PER_HOUR)
      table->field[next_field]->store((longlong) mqh.questions, TRUE);
    if (mqh.specified_limits & USER_RESOURCES::UPDATES_PER_HOUR)
      table->field[next_field+1]->store((longlong) mqh.updates, TRUE);
    if (mqh.specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR)
      table->field[next_field+2]->store((longlong) mqh.conn_per_hour, TRUE);
    if (table->s->fields >= 36 &&
        (mqh.specified_limits & USER_RESOURCES::USER_CONNECTIONS))
      table->field[next_field+3]->store((longlong) mqh.user_conn, TRUE);
    mqh_used= mqh_used || mqh.questions || mqh.updates || mqh.conn_per_hour;

    next_field+= 4;
    if (combo.plugin.str[0])
    {
      if (table->s->fields >= 41 && combo.plugin.str[0])
      {
        table->field[next_field]->store(combo.plugin.str, combo.plugin.length,
                                        system_charset_info);
        table->field[next_field]->set_notnull();
        table->field[next_field + 1]->store(combo.auth.str, combo.auth.length,
                                            system_charset_info);
        table->field[next_field + 1]->set_notnull();
      }
      else
      {
        my_error(ER_BAD_FIELD_ERROR, MYF(0), "plugin", "mysql.user");
        goto end;
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
      if ((error=
           table->file->ha_update_row(table->record[1],table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
      {						// This should never happen
        table->file->print_error(error,MYF(0));	/* purecov: deadcode */
        error= -1;				/* purecov: deadcode */
        goto end;				/* purecov: deadcode */
      }
      else
        error= 0;
    }
  }
  else if ((error=table->file->ha_write_row(table->record[0]))) // insert
  {						// This should never happen
    if (table->file->is_fatal_error(error, HA_CHECK_DUP))
    {
      table->file->print_error(error,MYF(0));	/* purecov: deadcode */
      error= -1;				/* purecov: deadcode */
      goto end;					/* purecov: deadcode */
    }
  }
  error=0;					// Privileges granted / revoked

end:
  if (!error)
  {
    acl_cache->clear(1);			// Clear privilege cache
    if (old_row_exists)
      acl_update_user(combo.user.str, combo.host.str,
                      combo.password.str, password_len,
		      lex->ssl_type,
		      lex->ssl_cipher,
		      lex->x509_issuer,
		      lex->x509_subject,
		      &lex->mqh,
		      rights,
		      &combo.plugin,
		      &combo.auth);
    else
      acl_insert_user(combo.user.str, combo.host.str, password, password_len,
		      lex->ssl_type,
		      lex->ssl_cipher,
		      lex->x509_issuer,
		      lex->x509_subject,
		      &lex->mqh,
		      rights,
		      &combo.plugin,
		      &combo.auth);
  }
  DBUG_RETURN(error);
}


/*
  change grants in the mysql.db table
*/

static int replace_db_table(TABLE *table, const char *db,
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
    DBUG_RETURN(-1);
  }

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0],0, user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    if (what == 'N')
    { // no row, no revoke
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo.user.str, combo.host.str);
      goto abort;
    }
    old_row_exists = 0;
    restore_record(table, s->default_values);
    table->field[0]->store(combo.host.str,combo.host.length,
                           system_charset_info);
    table->field[1]->store(db,(uint) strlen(db), system_charset_info);
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
    if (priv & store_rights)			// do it if priv is chosen
      table->field [i]->store(&what,1, &my_charset_latin1);// set requested privileges
  }
  rights=get_access(table,3);
  rights=fix_rights_for_db(rights);

  if (old_row_exists)
  {
    /* update old existing row */
    if (rights)
    {
      if ((error= table->file->ha_update_row(table->record[1],
                                             table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;			/* purecov: deadcode */
    }
    else	/* must have been a revoke of all privileges */
    {
      if ((error= table->file->ha_delete_row(table->record[1])))
	goto table_error;			/* purecov: deadcode */
    }
  }
  else if (rights && (error= table->file->ha_write_row(table->record[0])))
  {
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
      goto table_error; /* purecov: deadcode */
  }

  acl_cache->clear(1);				// Clear privilege cache
  if (old_row_exists)
    acl_update_db(combo.user.str,combo.host.str,db,rights);
  else
  if (rights)
    acl_insert_db(combo.user.str,combo.host.str,db,rights);
  DBUG_RETURN(0);

  /* This could only happen if the grant tables got corrupted */
table_error:
  table->file->print_error(error,MYF(0));	/* purecov: deadcode */

abort:
  DBUG_RETURN(-1);
}


static void  
acl_update_proxy_user(ACL_PROXY_USER *new_value, bool is_revoke)
{
  mysql_mutex_assert_owner(&acl_cache->lock);

  DBUG_ENTER("acl_update_proxy_user");
  for (uint i= 0; i < acl_proxy_users.elements; i++)
  {
    ACL_PROXY_USER *acl_user= 
      dynamic_element(&acl_proxy_users, i, ACL_PROXY_USER *);

    if (acl_user->pk_equals(new_value))
    {
      if (is_revoke)
      {
        DBUG_PRINT("info", ("delting ACL_PROXY_USER"));
        delete_dynamic_element(&acl_proxy_users, i);
      }
      else
      {
        DBUG_PRINT("info", ("updating ACL_PROXY_USER"));
        acl_user->set_data(new_value);
      }
      break;
    }
  }
  DBUG_VOID_RETURN;
}


static void  
acl_insert_proxy_user(ACL_PROXY_USER *new_value)
{
  DBUG_ENTER("acl_insert_proxy_user");
  mysql_mutex_assert_owner(&acl_cache->lock);
  (void) push_dynamic(&acl_proxy_users, (uchar *) new_value);
  my_qsort((uchar*) dynamic_element(&acl_proxy_users, 0, ACL_PROXY_USER *),
           acl_proxy_users.elements,
           sizeof(ACL_PROXY_USER), (qsort_cmp) acl_compare);
  DBUG_VOID_RETURN;
}


static int 
replace_proxies_priv_table(THD *thd, TABLE *table, const LEX_USER *user,
                         const LEX_USER *proxied_user, bool with_grant_arg, 
                         bool revoke_grant)
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
    DBUG_RETURN(-1);
  }

  table->use_all_columns();
  ACL_PROXY_USER::store_pk (table, &user->host, &user->user, 
                            &proxied_user->host, &proxied_user->user);

  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  get_grantor(thd, grantor);

  if ((error= table->file->ha_index_init(0, 1)))
  {
    table->file->print_error(error, MYF(0));
    DBUG_PRINT("info", ("ha_index_init error"));
    DBUG_RETURN(-1);
  }

  if (table->file->index_read_map(table->record[0], user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    DBUG_PRINT ("info", ("Row not found"));
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_GRANT, MYF(0), user->user.str, user->host.str);
      goto abort;
    }
    old_row_exists= 0;
    restore_record(table, s->default_values);
    ACL_PROXY_USER::store_data_record(table, &user->host, &user->user,
                                      &proxied_user->host,
                                      &proxied_user->user,
                                      with_grant_arg,
                                      grantor);
  }
  else
  {
    DBUG_PRINT("info", ("Row found"));
    old_row_exists= 1;
    store_record(table, record[1]);
  }

  if (old_row_exists)
  {
    /* update old existing row */
    if (!revoke_grant)
    {
      if ((error= table->file->ha_update_row(table->record[1],
                                             table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;			/* purecov: inspected */
    }
    else
    {
      if ((error= table->file->ha_delete_row(table->record[1])))
	goto table_error;			/* purecov: inspected */
    }
  }
  else if ((error= table->file->ha_write_row(table->record[0])))
  {
    DBUG_PRINT("info", ("error inserting the row"));
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
      goto table_error; /* purecov: inspected */
  }

  acl_cache->clear(1);				// Clear privilege cache
  if (old_row_exists)
  {
    new_grant.init(user->host.str, user->user.str,
                   proxied_user->host.str, proxied_user->user.str,
                   with_grant_arg);
    acl_update_proxy_user(&new_grant, revoke_grant);
  }
  else
  {
    new_grant.init(&mem, user->host.str, user->user.str,
                   proxied_user->host.str, proxied_user->user.str,
                   with_grant_arg);
    acl_insert_proxy_user(&new_grant);
  }

  table->file->ha_index_end();
  DBUG_RETURN(0);

  /* This could only happen if the grant tables got corrupted */
table_error:
  DBUG_PRINT("info", ("table error"));
  table->file->print_error(error, MYF(0));	/* purecov: inspected */

abort:
  DBUG_PRINT("info", ("aborting replace_proxies_priv_table"));
  table->file->ha_index_end();
  DBUG_RETURN(-1);
}


class GRANT_COLUMN :public Sql_alloc
{
public:
  char *column;
  ulong rights;
  uint key_length;
  GRANT_COLUMN(String &c,  ulong y) :rights (y)
  {
    column= (char*) memdup_root(&memex,c.ptr(), key_length=c.length());
  }
};


static uchar* get_key_column(GRANT_COLUMN *buff, size_t *length,
			    my_bool not_used __attribute__((unused)))
{
  *length=buff->key_length;
  return (uchar*) buff->column;
}


class GRANT_NAME :public Sql_alloc
{
public:
  acl_host_and_ip host;
  char *db, *user, *tname, *hash_key;
  ulong privs;
  ulong sort;
  size_t key_length;
  GRANT_NAME(const char *h, const char *d,const char *u,
             const char *t, ulong p, bool is_routine);
  GRANT_NAME (TABLE *form, bool is_routine);
  virtual ~GRANT_NAME() {};
  virtual bool ok() { return privs != 0; }
  void set_user_details(const char *h, const char *d,
                        const char *u, const char *t,
                        bool is_routine);
};


class GRANT_TABLE :public GRANT_NAME
{
public:
  ulong cols;
  HASH hash_columns;

  GRANT_TABLE(const char *h, const char *d,const char *u,
              const char *t, ulong p, ulong c);
  GRANT_TABLE (TABLE *form, TABLE *col_privs);
  ~GRANT_TABLE();
  bool ok() { return privs != 0 || cols != 0; }
};


void GRANT_NAME::set_user_details(const char *h, const char *d,
                                  const char *u, const char *t,
                                  bool is_routine)
{
  /* Host given by user */
  update_hostname(&host, strdup_root(&memex, h));
  if (db != d)
  {
    db= strdup_root(&memex, d);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, db);
  }
  user = strdup_root(&memex,u);
  sort=  get_sort(3,host.hostname,db,user);
  if (tname != t)
  {
    tname= strdup_root(&memex, t);
    if (lower_case_table_names || is_routine)
      my_casedn_str(files_charset_info, tname);
  }
  key_length= strlen(d) + strlen(u)+ strlen(t)+3;
  hash_key=   (char*) alloc_root(&memex,key_length);
  strmov(strmov(strmov(hash_key,user)+1,db)+1,tname);
}

GRANT_NAME::GRANT_NAME(const char *h, const char *d,const char *u,
                       const char *t, ulong p, bool is_routine)
  :db(0), tname(0), privs(p)
{
  set_user_details(h, d, u, t, is_routine);
}

GRANT_TABLE::GRANT_TABLE(const char *h, const char *d,const char *u,
                	 const char *t, ulong p, ulong c)
  :GRANT_NAME(h,d,u,t,p, FALSE), cols(c)
{
  (void) my_hash_init2(&hash_columns,4,system_charset_info,
                   0,0,0, (my_hash_get_key) get_key_column,0,0);
}


GRANT_NAME::GRANT_NAME(TABLE *form, bool is_routine)
{
  update_hostname(&host, get_field(&memex, form->field[0]));
  db=    get_field(&memex,form->field[1]);
  user=  get_field(&memex,form->field[2]);
  if (!user)
    user= (char*) "";
  sort=  get_sort(3, host.hostname, db, user);
  tname= get_field(&memex,form->field[3]);
  if (!db || !tname)
  {
    /* Wrong table row; Ignore it */
    privs= 0;
    return;					/* purecov: inspected */
  }
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, db);
  }
  if (lower_case_table_names || is_routine)
  {
    my_casedn_str(files_charset_info, tname);
  }
  key_length= (strlen(db) + strlen(user) + strlen(tname) + 3);
  hash_key=   (char*) alloc_root(&memex, key_length);
  strmov(strmov(strmov(hash_key,user)+1,db)+1,tname);
  privs = (ulong) form->field[6]->val_int();
  privs = fix_rights_for_table(privs);
}


GRANT_TABLE::GRANT_TABLE(TABLE *form, TABLE *col_privs)
  :GRANT_NAME(form, FALSE)
{
  uchar key[MAX_KEY_LENGTH];

  if (!db || !tname)
  {
    /* Wrong table row; Ignore it */
    my_hash_clear(&hash_columns);               /* allow for destruction */
    cols= 0;
    return;
  }
  cols= (ulong) form->field[7]->val_int();
  cols =  fix_rights_for_column(cols);

  (void) my_hash_init2(&hash_columns,4,system_charset_info,
                   0,0,0, (my_hash_get_key) get_key_column,0,0);
  if (cols)
  {
    uint key_prefix_len;
    KEY_PART_INFO *key_part= col_privs->key_info->key_part;
    col_privs->field[0]->store(host.hostname,
                               host.hostname ? (uint) strlen(host.hostname) :
                               0,
                               system_charset_info);
    col_privs->field[1]->store(db,(uint) strlen(db), system_charset_info);
    col_privs->field[2]->store(user,(uint) strlen(user), system_charset_info);
    col_privs->field[3]->store(tname,(uint) strlen(tname), system_charset_info);

    key_prefix_len= (key_part[0].store_length +
                     key_part[1].store_length +
                     key_part[2].store_length +
                     key_part[3].store_length);
    key_copy(key, col_privs->record[0], col_privs->key_info, key_prefix_len);
    col_privs->field[4]->store("",0, &my_charset_latin1);

    if (col_privs->file->ha_index_init(0, 1))
    {
      cols= 0;
      return;
    }

    if (col_privs->file->index_read_map(col_privs->record[0], (uchar*) key,
                                        (key_part_map)15, HA_READ_KEY_EXACT))
    {
      cols = 0; /* purecov: deadcode */
      col_privs->file->ha_index_end();
      return;
    }
    do
    {
      String *res,column_name;
      GRANT_COLUMN *mem_check;
      /* As column name is a string, we don't have to supply a buffer */
      res=col_privs->field[4]->val_str(&column_name);
      ulong priv= (ulong) col_privs->field[6]->val_int();
      if (!(mem_check = new GRANT_COLUMN(*res,
                                         fix_rights_for_column(priv))))
      {
        /* Don't use this entry */
        privs = cols = 0;			/* purecov: deadcode */
        return;				/* purecov: deadcode */
      }
      if (my_hash_insert(&hash_columns, (uchar *) mem_check))
      {
        /* Invalidate this entry */
        privs= cols= 0;
        return;
      }
    } while (!col_privs->file->index_next(col_privs->record[0]) &&
             !key_cmp_if_same(col_privs,key,0,key_prefix_len));
    col_privs->file->ha_index_end();
  }
}


GRANT_TABLE::~GRANT_TABLE()
{
  my_hash_free(&hash_columns);
}


static uchar* get_grant_table(GRANT_NAME *buff, size_t *length,
			     my_bool not_used __attribute__((unused)))
{
  *length=buff->key_length;
  return (uchar*) buff->hash_key;
}


void free_grant_table(GRANT_TABLE *grant_table)
{
  my_hash_free(&grant_table->hash_columns);
}


/* Search after a matching grant. Prefer exact grants before not exact ones */

static GRANT_NAME *name_hash_search(HASH *name_hash,
                                    const char *host,const char* ip,
                                    const char *db,
                                    const char *user, const char *tname,
                                    bool exact, bool name_tolower)
{
  char helping [NAME_LEN*2+USERNAME_LENGTH+3], *name_ptr;
  uint len;
  GRANT_NAME *grant_name,*found=0;
  HASH_SEARCH_STATE state;

  name_ptr= strmov(strmov(helping, user) + 1, db) + 1;
  len  = (uint) (strmov(name_ptr, tname) - helping) + 1;
  if (name_tolower)
    my_casedn_str(files_charset_info, name_ptr);
  for (grant_name= (GRANT_NAME*) my_hash_first(name_hash, (uchar*) helping,
                                               len, &state);
       grant_name ;
       grant_name= (GRANT_NAME*) my_hash_next(name_hash,(uchar*) helping,
                                              len, &state))
  {
    if (exact)
    {
      if (!grant_name->host.hostname ||
          (host &&
	   !my_strcasecmp(system_charset_info, host,
                          grant_name->host.hostname)) ||
	  (ip && !strcmp(ip, grant_name->host.hostname)))
	return grant_name;
    }
    else
    {
      if (compare_hostname(&grant_name->host, host, ip) &&
          (!found || found->sort < grant_name->sort))
	found=grant_name;					// Host ok
    }
  }
  return found;
}


inline GRANT_NAME *
routine_hash_search(const char *host, const char *ip, const char *db,
                 const char *user, const char *tname, bool proc, bool exact)
{
  return (GRANT_TABLE*)
    name_hash_search(proc ? &proc_priv_hash : &func_priv_hash,
		     host, ip, db, user, tname, exact, TRUE);
}


inline GRANT_TABLE *
table_hash_search(const char *host, const char *ip, const char *db,
		  const char *user, const char *tname, bool exact)
{
  return (GRANT_TABLE*) name_hash_search(&column_priv_hash, host, ip, db,
					 user, tname, exact, FALSE);
}


inline GRANT_COLUMN *
column_hash_search(GRANT_TABLE *t, const char *cname, uint length)
{
  return (GRANT_COLUMN*) my_hash_search(&t->hash_columns,
                                        (uchar*) cname, length);
}


static int replace_column_table(GRANT_TABLE *g_t,
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

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db),
                         system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name,(uint) strlen(table_name),
                         system_charset_info);

  /* Get length of 4 first key parts */
  key_part= table->key_info->key_part;
  key_prefix_length= (key_part[0].store_length + key_part[1].store_length +
                      key_part[2].store_length + key_part[3].store_length);
  key_copy(key, table->record[0], table->key_info, key_prefix_length);

  rights&= COL_ACLS;				// Only ACL for columns

  /* first fix privileges for all columns in column list */

  List_iterator <LEX_COLUMN> iter(columns);
  class LEX_COLUMN *column;
  int error= table->file->ha_index_init(0, 1);
  if (error)
  {
    table->file->print_error(error, MYF(0));
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

    if (table->file->index_read_map(table->record[0], user_key, HA_WHOLE_KEY,
                                    HA_READ_KEY_EXACT))
    {
      if (revoke_grant)
      {
	my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
                 combo.user.str, combo.host.str,
                 table_name);                   /* purecov: inspected */
	result= -1;                             /* purecov: inspected */
	continue;                               /* purecov: inspected */
      }
      old_row_exists = 0;
      restore_record(table, s->default_values);		// Get empty record
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
      store_record(table,record[1]);			// copy original row
    }

    table->field[6]->store((longlong) get_rights_for_column(privileges), TRUE);

    if (old_row_exists)
    {
      GRANT_COLUMN *grant_column;
      if (privileges)
	error=table->file->ha_update_row(table->record[1],table->record[0]);
      else
	error=table->file->ha_delete_row(table->record[1]);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
      {
	table->file->print_error(error,MYF(0)); /* purecov: inspected */
	result= -1;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }
      else
        error= 0;
      grant_column= column_hash_search(g_t, column->column.ptr(),
                                       column->column.length());
      if (grant_column)				// Should always be true
	grant_column->rights= privileges;	// Update hash
    }
    else					// new grant
    {
      GRANT_COLUMN *grant_column;
      if ((error=table->file->ha_write_row(table->record[0])))
      {
	table->file->print_error(error,MYF(0)); /* purecov: inspected */
	result= -1;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
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

    if (table->file->index_read_map(table->record[0], user_key,
                                    (key_part_map)15,
                                    HA_READ_KEY_EXACT))
      goto end;

    /* Scan through all rows with the same host,db,user and table */
    do
    {
      ulong privileges = (ulong) table->field[6]->val_int();
      privileges=fix_rights_for_column(privileges);
      store_record(table,record[1]);

      if (privileges & rights)	// is in this record the priv to be revoked ??
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
	  int tmp_error;
	  if ((tmp_error=table->file->ha_update_row(table->record[1],
						    table->record[0])) &&
              tmp_error != HA_ERR_RECORD_IS_THE_SAME)
	  {					/* purecov: deadcode */
	    table->file->print_error(tmp_error,MYF(0)); /* purecov: deadcode */
	    result= -1;				/* purecov: deadcode */
	    goto end;				/* purecov: deadcode */
	  }
	  if (grant_column)
	    grant_column->rights  = privileges; // Update hash
	}
	else
	{
	  int tmp_error;
	  if ((tmp_error = table->file->ha_delete_row(table->record[1])))
	  {					/* purecov: deadcode */
	    table->file->print_error(tmp_error,MYF(0)); /* purecov: deadcode */
	    result= -1;				/* purecov: deadcode */
	    goto end;				/* purecov: deadcode */
	  }
	  if (grant_column)
	    my_hash_delete(&g_t->hash_columns,(uchar*) grant_column);
	}
      }
    } while (!table->file->index_next(table->record[0]) &&
	     !key_cmp_if_same(table, key, 0, key_prefix_length));
  }

end:
  table->file->ha_index_end();
  DBUG_RETURN(result);
}

static inline void get_grantor(THD *thd, char *grantor)
{
  const char *user= thd->security_ctx->user;
  const char *host= thd->security_ctx->host_or_ip;

#if defined(HAVE_REPLICATION)
  if (thd->slave_thread && thd->has_invoker())
  {
    user= thd->get_invoker_user().str;
    host= thd->get_invoker_host().str;
  }
#endif
  strxmov(grantor, user, "@", host, NullS);
}

static int replace_table_table(THD *thd, GRANT_TABLE *grant_table,
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
               MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(-1);				/* purecov: deadcode */
  }

  table->use_all_columns();
  restore_record(table, s->default_values);     // Get empty record
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name,(uint) strlen(table_name),
                         system_charset_info);
  store_record(table,record[1]);			// store at pos 1
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
               combo.user.str, combo.host.str,
               table_name);		        /* purecov: deadcode */
      DBUG_RETURN(-1);				/* purecov: deadcode */
    }
    old_row_exists = 0;
    restore_record(table,record[1]);			// Get saved record
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

  table->field[4]->store(grantor,(uint) strlen(grantor), system_charset_info);
  table->field[6]->store((longlong) store_table_rights, TRUE);
  table->field[7]->store((longlong) store_col_rights, TRUE);
  rights=fix_rights_for_table(store_table_rights);
  col_rights=fix_rights_for_column(store_col_rights);

  if (old_row_exists)
  {
    if (store_table_rights || store_col_rights)
    {
      if ((error=table->file->ha_update_row(table->record[1],
                                            table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;			/* purecov: deadcode */
    }
    else if ((error = table->file->ha_delete_row(table->record[1])))
      goto table_error;				/* purecov: deadcode */
  }
  else
  {
    error=table->file->ha_write_row(table->record[0]);
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
      goto table_error;				/* purecov: deadcode */
  }

  if (rights | col_rights)
  {
    grant_table->privs= rights;
    grant_table->cols=	col_rights;
  }
  else
  {
    my_hash_delete(&column_priv_hash,(uchar*) grant_table);
  }
  DBUG_RETURN(0);

  /* This should never happen */
table_error:
  table->file->print_error(error,MYF(0)); /* purecov: deadcode */
  DBUG_RETURN(-1); /* purecov: deadcode */
}


/**
  @retval       0  success
  @retval      -1  error
*/
static int replace_routine_table(THD *thd, GRANT_NAME *grant_name,
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
  /*
    New users are created before this function is called.

    There may be some cases where a routine's definer is removed but the
    routine remains.
  */

  table->use_all_columns();
  restore_record(table, s->default_values);		// Get empty record
  table->field[0]->store(combo.host.str,combo.host.length, &my_charset_latin1);
  table->field[1]->store(db,(uint) strlen(db), &my_charset_latin1);
  table->field[2]->store(combo.user.str,combo.user.length, &my_charset_latin1);
  table->field[3]->store(routine_name,(uint) strlen(routine_name),
                         &my_charset_latin1);
  table->field[4]->store((longlong)(is_proc ?
                                    TYPE_ENUM_PROCEDURE : TYPE_ENUM_FUNCTION),
                         TRUE);
  store_record(table,record[1]);			// store at pos 1

  if (table->file->index_read_idx_map(table->record[0], 0,
                                      (uchar*) table->field[0]->ptr,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_PROC_GRANT, MYF(0),
               combo.user.str, combo.host.str, routine_name);
      DBUG_RETURN(-1);
    }
    old_row_exists= 0;
    restore_record(table,record[1]);			// Get saved record
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

  table->field[5]->store(grantor,(uint) strlen(grantor), &my_charset_latin1);
  table->field[6]->store((longlong) store_proc_rights, TRUE);
  rights=fix_rights_for_procedure(store_proc_rights);

  if (old_row_exists)
  {
    if (store_proc_rights)
    {
      if ((error=table->file->ha_update_row(table->record[1],
                                            table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;
    }
    else if ((error= table->file->ha_delete_row(table->record[1])))
      goto table_error;
  }
  else
  {
    error=table->file->ha_write_row(table->record[0]);
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
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
  table->file->print_error(error,MYF(0));
  DBUG_RETURN(-1);
}


/*
  Store table level and column level grants in the privilege tables

  SYNOPSIS
    mysql_table_grant()
    thd			Thread handle
    table_list		List of tables to give grant
    user_list		List of users to give grant
    columns		List of columns to give grant
    rights		Table level grant
    revoke_grant	Set to 1 if this is a REVOKE command

  RETURN
    FALSE ok
    TRUE  error
*/

int mysql_table_grant(THD *thd, TABLE_LIST *table_list,
		      List <LEX_USER> &user_list,
		      List <LEX_COLUMN> &columns, ulong rights,
		      bool revoke_grant)
{
  ulong column_priv= 0;
  List_iterator <LEX_USER> str_list (user_list);
  LEX_USER *Str, *tmp_Str;
  TABLE_LIST tables[3];
  bool create_new_users=0;
  char *db_name, *table_name;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_table_grant");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--skip-grant-tables");	/* purecov: inspected */
    DBUG_RETURN(TRUE);				/* purecov: inspected */
  }
  if (rights & ~TABLE_ACLS)
  {
    my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
  {
    if (columns.elements)
    {
      class LEX_COLUMN *column;
      List_iterator <LEX_COLUMN> column_iter(columns);

      if (open_normal_and_derived_tables(thd, table_list, 0))
        DBUG_RETURN(TRUE);

      while ((column = column_iter++))
      {
        uint unused_field_idx= NO_CACHED_FIELD_INDEX;
        TABLE_LIST *dummy;
        Field *f=find_field_in_table_ref(thd, table_list, column->column.ptr(),
                                         column->column.length(),
                                         column->column.ptr(), NULL, NULL,
                                         NULL, TRUE, FALSE,
                                         &unused_field_idx, FALSE, &dummy);
        if (f == (Field*)0)
        {
          my_error(ER_BAD_FIELD_ERROR, MYF(0),
                   column->column.c_ptr(), table_list->alias);
          DBUG_RETURN(TRUE);
        }
        if (f == (Field *)-1)
          DBUG_RETURN(TRUE);
        column_priv|= column->rights;
      }
      close_mysql_tables(thd);
    }
    else
    {
      if (!(rights & CREATE_ACL))
      {
        char buf[FN_REFLEN + 1];
        build_table_filename(buf, sizeof(buf) - 1, table_list->db,
                             table_list->table_name, reg_ext, 0);
        fn_format(buf, buf, "", "", MY_UNPACK_FILENAME  | MY_RESOLVE_SYMLINKS |
                                    MY_RETURN_REAL_PATH | MY_APPEND_EXT);
        if (access(buf,F_OK))
        {
          my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db, table_list->alias);
          DBUG_RETURN(TRUE);
        }
      }
      if (table_list->grant.want_privilege)
      {
        char command[128];
        get_privilege_desc(command, sizeof(command),
                           table_list->grant.want_privilege);
        my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                 command, thd->security_ctx->priv_user,
                 thd->security_ctx->host_or_ip, table_list->alias);
        DBUG_RETURN(-1);
      }
    }
  }

  /* open the mysql.tables_priv and mysql.columns_priv tables */

  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("user"), "user", TL_WRITE);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("tables_priv"),
                           "tables_priv", TL_WRITE);
  tables[2].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("columns_priv"),
                           "columns_priv", TL_WRITE);
  tables[0].next_local= tables[0].next_global= tables+1;
  /* Don't open column table if we don't need it ! */
  if (column_priv || (revoke_grant && ((rights & COL_ACLS) || columns.elements)))
    tables[1].next_local= tables[1].next_global= tables+2;

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

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
    tables[0].updating= tables[1].updating= tables[2].updating= 1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
    {
      /* Restore the state of binlog format */
      DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
      if (save_binlog_row_based)
        thd->set_current_stmt_binlog_format_row();
      DBUG_RETURN(FALSE);
    }
  }
#endif

  /* 
    The lock api is depending on the thd->lex variable which needs to be
    re-initialized.
  */
  Query_tables_list backup;
  thd->lex->reset_n_backup_query_tables_list(&backup);
  /*
    Restore Query_tables_list::sql_command value, which was reset
    above, as the code writing query to the binary log assumes that
    this value corresponds to the statement being executed.
  */
  thd->lex->sql_command= backup.sql_command;
  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {						// Should never happen
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    thd->lex->restore_backup_query_tables_list(&backup);
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(TRUE);				/* purecov: deadcode */
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);
  bool result= FALSE;
  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);
  MEM_ROOT *old_root= thd->mem_root;
  thd->mem_root= &memex;
  grant_version++;

  while ((tmp_Str = str_list++))
  {
    int error;
    GRANT_TABLE *grant_table;
    if (!(Str= get_current_user(thd, tmp_Str)))
    {
      result= TRUE;
      continue;
    }  
    /* Create user if needed */
    error=replace_user_table(thd, tables[0].table, *Str,
			     0, revoke_grant, create_new_users,
                             test(thd->variables.sql_mode &
                                  MODE_NO_AUTO_CREATE_USER));
    if (error)
    {
      result= TRUE;				// Remember error
      continue;					// Add next user
    }

    db_name= table_list->get_db_name();
    table_name= table_list->get_table_name();

    /* Find/create cached table grant */
    grant_table= table_hash_search(Str->host.str, NullS, db_name,
				   Str->user.str, table_name, 1);
    if (!grant_table)
    {
      if (revoke_grant)
      {
	my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
                 Str->user.str, Str->host.str, table_list->table_name);
	result= TRUE;
	continue;
      }
      grant_table = new GRANT_TABLE (Str->host.str, db_name,
				     Str->user.str, table_name,
				     rights,
				     column_priv);
      if (!grant_table ||
        my_hash_insert(&column_priv_hash,(uchar*) grant_table))
      {
	result= TRUE;				/* purecov: deadcode */
	continue;				/* purecov: deadcode */
      }
    }

    /* If revoke_grant, calculate the new column privilege for tables_priv */
    if (revoke_grant)
    {
      class LEX_COLUMN *column;
      List_iterator <LEX_COLUMN> column_iter(columns);
      GRANT_COLUMN *grant_column;

      /* Fix old grants */
      while ((column = column_iter++))
      {
	grant_column = column_hash_search(grant_table,
					  column->column.ptr(),
					  column->column.length());
	if (grant_column)
	  grant_column->rights&= ~(column->rights | rights);
      }
      /* scan trough all columns to get new column grant */
      column_priv= 0;
      for (uint idx=0 ; idx < grant_table->hash_columns.records ; idx++)
      {
        grant_column= (GRANT_COLUMN*)
          my_hash_element(&grant_table->hash_columns, idx);
	grant_column->rights&= ~rights;		// Fix other columns
	column_priv|= grant_column->rights;
      }
    }
    else
    {
      column_priv|= grant_table->cols;
    }


    /* update table and columns */

    if (replace_table_table(thd, grant_table, tables[1].table, *Str,
			    db_name, table_name,
			    rights, column_priv, revoke_grant))
    {
      /* Should only happen if table is crashed */
      result= TRUE;			       /* purecov: deadcode */
    }
    else if (tables[2].table)
    {
      if ((replace_column_table(grant_table, tables[2].table, *Str,
				columns,
				db_name, table_name,
				rights, revoke_grant)))
      {
	result= TRUE;
      }
    }
  }
  thd->mem_root= old_root;
  mysql_mutex_unlock(&acl_cache->lock);

  if (!result) /* success */
  {
    result= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
  }

  mysql_rwlock_unlock(&LOCK_grant);

  if (!result) /* success */
    my_ok(thd);

  /* Tables are automatically closed */
  thd->lex->restore_backup_query_tables_list(&backup);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


/**
  Store routine level grants in the privilege tables

  @param thd Thread handle
  @param table_list List of routines to give grant
  @param is_proc Is this a list of procedures?
  @param user_list List of users to give grant
  @param rights Table level grant
  @param revoke_grant Is this is a REVOKE command?

  @return
    @retval FALSE Success.
    @retval TRUE An error occurred.
*/

bool mysql_routine_grant(THD *thd, TABLE_LIST *table_list, bool is_proc,
			 List <LEX_USER> &user_list, ulong rights,
			 bool revoke_grant, bool write_to_binlog)
{
  List_iterator <LEX_USER> str_list (user_list);
  LEX_USER *Str, *tmp_Str;
  TABLE_LIST tables[2];
  bool create_new_users=0, result=0;
  char *db_name, *table_name;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_routine_grant");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }
  if (rights & ~PROC_ACLS)
  {
    my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
  {
    if (sp_exist_routines(thd, table_list, is_proc))
      DBUG_RETURN(TRUE);
  }

  /* open the mysql.user and mysql.procs_priv tables */

  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("user"), "user", TL_WRITE);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("procs_priv"), "procs_priv", TL_WRITE);
  tables[0].next_local= tables[0].next_global= tables+1;

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

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
    tables[0].updating= tables[1].updating= 1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
    {
      /* Restore the state of binlog format */
      DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
      if (save_binlog_row_based)
        thd->set_current_stmt_binlog_format_row();
      DBUG_RETURN(FALSE);
    }
  }
#endif

  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {						// Should never happen
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);
  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);
  MEM_ROOT *old_root= thd->mem_root;
  thd->mem_root= &memex;

  DBUG_PRINT("info",("now time to iterate and add users"));

  while ((tmp_Str= str_list++))
  {
    int error;
    GRANT_NAME *grant_name;
    if (!(Str= get_current_user(thd, tmp_Str)))
    {
      result= TRUE;
      continue;
    }  
    /* Create user if needed */
    error=replace_user_table(thd, tables[0].table, *Str,
			     0, revoke_grant, create_new_users,
                             test(thd->variables.sql_mode &
                                  MODE_NO_AUTO_CREATE_USER));
    if (error)
    {
      result= TRUE;				// Remember error
      continue;					// Add next user
    }

    db_name= table_list->db;
    table_name= table_list->table_name;

    grant_name= routine_hash_search(Str->host.str, NullS, db_name,
                                    Str->user.str, table_name, is_proc, 1);
    if (!grant_name)
    {
      if (revoke_grant)
      {
        my_error(ER_NONEXISTING_PROC_GRANT, MYF(0),
	         Str->user.str, Str->host.str, table_name);
	result= TRUE;
	continue;
      }
      grant_name= new GRANT_NAME(Str->host.str, db_name,
				 Str->user.str, table_name,
				 rights, TRUE);
      if (!grant_name ||
        my_hash_insert(is_proc ?
                       &proc_priv_hash : &func_priv_hash,(uchar*) grant_name))
      {
        result= TRUE;
	continue;
      }
    }

    if (replace_routine_table(thd, grant_name, tables[1].table, *Str,
                              db_name, table_name, is_proc, rights, 
                              revoke_grant) != 0)
    {
      result= TRUE;
      continue;
    }
  }
  thd->mem_root= old_root;
  mysql_mutex_unlock(&acl_cache->lock);

  if (write_to_binlog)
  {
    if (write_bin_log(thd, FALSE, thd->query(), thd->query_length()))
      result= TRUE;
  }

  mysql_rwlock_unlock(&LOCK_grant);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  /* Tables are automatically closed */
  DBUG_RETURN(result);
}


bool mysql_grant(THD *thd, const char *db, List <LEX_USER> &list,
                 ulong rights, bool revoke_grant, bool is_proxy)
{
  List_iterator <LEX_USER> str_list (list);
  LEX_USER *Str, *tmp_Str, *proxied_user= NULL;
  char tmp_db[NAME_LEN+1];
  bool create_new_users=0;
  TABLE_LIST tables[2];
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_grant");
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--skip-grant-tables");	/* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if (lower_case_table_names && db)
  {
    strnmov(tmp_db,db,NAME_LEN);
    tmp_db[NAME_LEN]= '\0';
    my_casedn_str(files_charset_info, tmp_db);
    db=tmp_db;
  }

  if (is_proxy)
  {
    DBUG_ASSERT(!db);
    proxied_user= str_list++;
  }

  /* open the mysql.user and mysql.db or mysql.proxies_priv tables */
  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("user"), "user", TL_WRITE);
  if (is_proxy)

    tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("proxies_priv"),
                             "proxies_priv", 
                             TL_WRITE);
  else
    tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                             C_STRING_WITH_LEN("db"), 
                             "db", 
                             TL_WRITE);
  tables[0].next_local= tables[0].next_global= tables+1;

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

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
    tables[0].updating= tables[1].updating= 1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
    {
      /* Restore the state of binlog format */
      DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
      if (save_binlog_row_based)
        thd->set_current_stmt_binlog_format_row();
      DBUG_RETURN(FALSE);
    }
  }
#endif

  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {						// This should never happen
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(TRUE);				/* purecov: deadcode */
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);

  /* go through users in user_list */
  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);
  grant_version++;

  int result=0;
  while ((tmp_Str = str_list++))
  {
    if (!(Str= get_current_user(thd, tmp_Str)))
    {
      result= TRUE;
      continue;
    }
    /*
      No User, but a password?
      They did GRANT ... TO CURRENT_USER() IDENTIFIED BY ... !
      Get the current user, and shallow-copy the new password to them!
    */
    if (!tmp_Str->user.str && tmp_Str->password.str)
      Str->password= tmp_Str->password;
    if (replace_user_table(thd, tables[0].table, *Str,
                           (!db ? rights : 0), revoke_grant, create_new_users,
                           test(thd->variables.sql_mode &
                                MODE_NO_AUTO_CREATE_USER)))
      result= -1;
    else if (db)
    {
      ulong db_rights= rights & DB_ACLS;
      if (db_rights  == rights)
      {
	if (replace_db_table(tables[1].table, db, *Str, db_rights,
			     revoke_grant))
	  result= -1;
      }
      else
      {
	my_error(ER_WRONG_USAGE, MYF(0), "DB GRANT", "GLOBAL PRIVILEGES");
	result= -1;
      }
    }
    else if (is_proxy)
    {
      if (replace_proxies_priv_table (thd, tables[1].table, Str, proxied_user,
                                    rights & GRANT_ACL ? TRUE : FALSE, 
                                    revoke_grant))
        result= -1;
    }
  }
  mysql_mutex_unlock(&acl_cache->lock);

  if (!result)
  {
    result= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
  }

  mysql_rwlock_unlock(&LOCK_grant);

  if (!result)
    my_ok(thd);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(result);
}


/* Free grant array if possible */

void  grant_free(void)
{
  DBUG_ENTER("grant_free");
  my_hash_free(&column_priv_hash);
  my_hash_free(&proc_priv_hash);
  my_hash_free(&func_priv_hash);
  free_root(&memex,MYF(0));
  DBUG_VOID_RETURN;
}


/**
  @brief Initialize structures responsible for table/column-level privilege
   checking and load information for them from tables in the 'mysql' database.

  @return Error status
    @retval 0 OK
    @retval 1 Could not initialize grant subsystem.
*/

my_bool grant_init()
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("grant_init");

  if (!(thd= new THD))
    DBUG_RETURN(1);				/* purecov: deadcode */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  return_val=  grant_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_RETURN(return_val);
}


/**
  @brief Helper function to grant_reload_procs_priv

  Reads the procs_priv table into memory hash.

  @param table A pointer to the procs_priv table structure.

  @see grant_reload
  @see grant_reload_procs_priv

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/

static my_bool grant_load_procs_priv(TABLE *p_table)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**,
                                                           THR_MALLOC);
  DBUG_ENTER("grant_load_procs_priv");
  (void) my_hash_init(&proc_priv_hash, &my_charset_utf8_bin,
                      0,0,0, (my_hash_get_key) get_grant_table,
                      0,0);
  (void) my_hash_init(&func_priv_hash, &my_charset_utf8_bin,
                      0,0,0, (my_hash_get_key) get_grant_table,
                      0,0);

  if (p_table->file->ha_index_init(0, 1))
    DBUG_RETURN(TRUE);

  p_table->use_all_columns();

  if (!p_table->file->index_first(p_table->record[0]))
  {
    memex_ptr= &memex;
    my_pthread_setspecific_ptr(THR_MALLOC, &memex_ptr);
    do
    {
      GRANT_NAME *mem_check;
      HASH *hash;
      if (!(mem_check=new (memex_ptr) GRANT_NAME(p_table, TRUE)))
      {
        /* This could only happen if we are out memory */
        goto end_unlock;
      }

      if (check_no_resolve)
      {
	if (hostname_requires_resolving(mem_check->host.hostname))
	{
          sql_print_warning("'procs_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname, mem_check->user,
                            mem_check->host.hostname ?
                            mem_check->host.hostname : "");
          continue;
        }
      }
      if (p_table->field[4]->val_int() == TYPE_ENUM_PROCEDURE)
      {
        hash= &proc_priv_hash;
      }
      else
      if (p_table->field[4]->val_int() == TYPE_ENUM_FUNCTION)
      {
        hash= &func_priv_hash;
      }
      else
      {
        sql_print_warning("'procs_priv' entry '%s' "
                          "ignored, bad routine type",
                          mem_check->tname);
        continue;
      }

      mem_check->privs= fix_rights_for_procedure(mem_check->privs);
      if (! mem_check->ok())
        delete mem_check;
      else if (my_hash_insert(hash, (uchar*) mem_check))
      {
        delete mem_check;
        goto end_unlock;
      }
    }
    while (!p_table->file->index_next(p_table->record[0]));
  }
  /* Return ok */
  return_val= 0;

end_unlock:
  p_table->file->ha_index_end();
  my_pthread_setspecific_ptr(THR_MALLOC, save_mem_root_ptr);
  DBUG_RETURN(return_val);
}


/**
  @brief Initialize structures responsible for table/column-level privilege
    checking and load information about grants from open privilege tables.

  @param thd Current thread
  @param tables List containing open "mysql.tables_priv" and
    "mysql.columns_priv" tables.

  @see grant_reload

  @return Error state
    @retval FALSE Success
    @retval TRUE Error
*/

static my_bool grant_load(THD *thd, TABLE_LIST *tables)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  TABLE *t_table= 0, *c_table= 0;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**,
                                                           THR_MALLOC);
  ulong old_sql_mode= thd->variables.sql_mode;
  DBUG_ENTER("grant_load");

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  (void) my_hash_init(&column_priv_hash, &my_charset_utf8_bin,
                      0,0,0, (my_hash_get_key) get_grant_table,
                      (my_hash_free_key) free_grant_table,0);

  t_table = tables[0].table;
  c_table = tables[1].table;

  if (t_table->file->ha_index_init(0, 1))
    goto end_index_init;

  t_table->use_all_columns();
  c_table->use_all_columns();

  if (!t_table->file->index_first(t_table->record[0]))
  {
    memex_ptr= &memex;
    my_pthread_setspecific_ptr(THR_MALLOC, &memex_ptr);
    do
    {
      GRANT_TABLE *mem_check;
      if (!(mem_check=new (memex_ptr) GRANT_TABLE(t_table,c_table)))
      {
	/* This could only happen if we are out memory */
	goto end_unlock;
      }

      if (check_no_resolve)
      {
	if (hostname_requires_resolving(mem_check->host.hostname))
	{
          sql_print_warning("'tables_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname,
                            mem_check->user ? mem_check->user : "",
                            mem_check->host.hostname ?
                            mem_check->host.hostname : "");
	  continue;
	}
      }

      if (! mem_check->ok())
	delete mem_check;
      else if (my_hash_insert(&column_priv_hash,(uchar*) mem_check))
      {
	delete mem_check;
	goto end_unlock;
      }
    }
    while (!t_table->file->index_next(t_table->record[0]));
  }

  return_val=0;					// Return ok

end_unlock:
  t_table->file->ha_index_end();
  my_pthread_setspecific_ptr(THR_MALLOC, save_mem_root_ptr);
end_index_init:
  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(return_val);
}


/**
  @brief Helper function to grant_reload. Reloads procs_priv table is it
    exists.

  @param thd A pointer to the thread handler object.
  @param table A pointer to the table list.

  @see grant_reload

  @return Error state
    @retval FALSE Success
    @retval TRUE An error has occurred.
*/

static my_bool grant_reload_procs_priv(THD *thd, TABLE_LIST *table)
{
  HASH old_proc_priv_hash, old_func_priv_hash;
  my_bool return_val= FALSE;
  DBUG_ENTER("grant_reload_procs_priv");

  /* Save a copy of the current hash if we need to undo the grant load */
  old_proc_priv_hash= proc_priv_hash;
  old_func_priv_hash= func_priv_hash;

  if ((return_val= grant_load_procs_priv(table->table)))
  {
    /* Error; Reverting to old hash */
    DBUG_PRINT("error",("Reverting to old privileges"));
    my_hash_free(&proc_priv_hash);
    my_hash_free(&func_priv_hash);
    proc_priv_hash= old_proc_priv_hash;
    func_priv_hash= old_func_priv_hash;
  }
  else
  {
    my_hash_free(&old_proc_priv_hash);
    my_hash_free(&old_func_priv_hash);
  }

  DBUG_RETURN(return_val);
}


/**
  @brief Reload information about table and column level privileges if possible

  @param thd Current thread

  Locked tables are checked by acl_reload() and doesn't have to be checked
  in this call.
  This function is also used for initialization of structures responsible
  for table/column-level privilege checking.

  @return Error state
    @retval FALSE Success
    @retval TRUE  Error
*/

my_bool grant_reload(THD *thd)
{
  TABLE_LIST tables[3];
  HASH old_column_priv_hash;
  MEM_ROOT old_mem;
  my_bool return_val= 1;
  DBUG_ENTER("grant_reload");

  /* Don't do anything if running with --skip-grant-tables */
  if (!initialized)
    DBUG_RETURN(0);

  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("tables_priv"),
                           "tables_priv", TL_READ);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("columns_priv"),
                           "columns_priv", TL_READ);
  tables[2].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("procs_priv"),
                           "procs_priv", TL_READ);

  tables[0].next_local= tables[0].next_global= tables+1;
  tables[1].next_local= tables[1].next_global= tables+2;
  tables[0].open_type= tables[1].open_type= tables[2].open_type= OT_BASE_ONLY;

  /*
    Reload will work in the following manner:-

                             proc_priv_hash structure
                              /                     \
                    not initialized                 initialized
                   /               \                     |
    mysql.procs_priv table        Server Startup         |
        is missing                      \                |
             |                         open_and_lock_tables()
    Assume we are working on           /success             \failure
    pre 4.1 system tables.        Normal Scenario.          An error is thrown.
    A warning is printed          Reload column privilege.  Retain the old hash.
    and continue with             Reload function and
    reloading the column          procedure privileges,
    privileges.                   if available.
  */

  if (!(my_hash_inited(&proc_priv_hash)))
    tables[2].open_strategy= TABLE_LIST::OPEN_IF_EXISTS;

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining LOCK_grant rwlock.
  */
  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    if (thd->stmt_da->is_error())
    {
      sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
                      thd->stmt_da->message());
    }
    goto end;
  }

  if (tables[2].table == NULL)
  {
    sql_print_warning("Table 'mysql.procs_priv' does not exist. "
                      "Please run mysql_upgrade.");
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_NO_SUCH_TABLE,
                        ER(ER_NO_SUCH_TABLE), tables[2].db,
                        tables[2].table_name);
  }

  mysql_rwlock_wrlock(&LOCK_grant);
  old_column_priv_hash= column_priv_hash;

  /*
    Create a new memory pool but save the current memory pool to make an undo
    opertion possible in case of failure.
  */
  old_mem= memex;
  init_sql_alloc(&memex, ACL_ALLOC_BLOCK_SIZE, 0);

  /*
    tables[2].table i.e. procs_priv can be null if we are working with
    pre 4.1 privilage tables
  */
  if ((return_val= (grant_load(thd, tables) ||
                    (tables[2].table != NULL &&
                     grant_reload_procs_priv(thd, &tables[2])))
     ))
  {						// Error. Revert to old hash
    DBUG_PRINT("error",("Reverting to old privileges"));
    my_hash_free(&column_priv_hash);
    free_root(&memex,MYF(0));
    column_priv_hash= old_column_priv_hash;	/* purecov: deadcode */
    memex= old_mem;				/* purecov: deadcode */
  }
  else
  {
    my_hash_free(&old_column_priv_hash);
    free_root(&old_mem,MYF(0));
    grant_version++;
  }
  mysql_rwlock_unlock(&LOCK_grant);

end:
  close_mysql_tables(thd);
  DBUG_RETURN(return_val);
}


/**
  @brief Check table level grants

  @param thd          Thread handler
  @param want_access  Bits of privileges user needs to have.
  @param tables       List of tables to check. The user should have
                      'want_access' to all tables in list.
  @param any_combination_will_do TRUE if it's enough to have any privilege for
    any combination of the table columns.
  @param number       Check at most this number of tables.
  @param no_errors    TRUE if no error should be sent directly to the client.

  If table->grant.want_privilege != 0 then the requested privileges where
  in the set of COL_ACLS but access was not granted on the table level. As
  a consequence an extra check of column privileges is required.

  Specifically if this function returns FALSE the user has some kind of
  privilege on a combination of columns in each table.

  This function is usually preceeded by check_access which establish the
  User-, Db- and Host access rights.

  @see check_access
  @see check_table_access

  @note This functions assumes that either number of tables to be inspected
     by it is limited explicitly (i.e. is is not UINT_MAX) or table list
     used and thd->lex->query_tables_own_last value correspond to each
     other (the latter should be either 0 or point to next_global member
     of one of elements of this table list).

   @return Access status
     @retval FALSE Access granted; But column privileges might need to be
      checked.
     @retval TRUE The user did not have the requested privileges on any of the
      tables.

*/

bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
                 bool any_combination_will_do, uint number, bool no_errors)
{
  TABLE_LIST *tl;
  TABLE_LIST *first_not_own_table= thd->lex->first_not_own_table();
  Security_context *sctx= thd->security_ctx;
  uint i;
  ulong orig_want_access= want_access;
  DBUG_ENTER("check_grant");
  DBUG_ASSERT(number > 0);

  /*
    Walk through the list of tables that belong to the query and save the
    requested access (orig_want_privilege) to be able to use it when
    checking access rights to the underlying tables of a view. Our grant
    system gradually eliminates checked bits from want_privilege and thus
    after all checks are done we can no longer use it.
    The check that first_not_own_table is not reached is for the case when
    the given table list refers to the list for prelocking (contains tables
    of other queries). For simple queries first_not_own_table is 0.
  */
  for (i= 0, tl= tables;
       i < number  && tl != first_not_own_table;
       tl= tl->next_global, i++)
  {
    /*
      Save a copy of the privileges without the SHOW_VIEW_ACL attribute.
      It will be checked during making view.
    */
    tl->grant.orig_want_privilege= (want_access & ~SHOW_VIEW_ACL);
  }

  mysql_rwlock_rdlock(&LOCK_grant);
  for (tl= tables;
       tl && number-- && tl != first_not_own_table;
       tl= tl->next_global)
  {
    TABLE_LIST *const t_ref=
      tl->correspondent_table ? tl->correspondent_table : tl;
    sctx = test(t_ref->security_ctx) ? t_ref->security_ctx :
                                       thd->security_ctx;

    const ACL_internal_table_access *access=
      get_cached_table_access(&t_ref->grant.m_internal,
                              t_ref->get_db_name(),
                              t_ref->get_table_name());

    if (access)
    {
      switch(access->check(orig_want_access, &t_ref->grant.privilege))
      {
      case ACL_INTERNAL_ACCESS_GRANTED:
        /*
          Currently,
          -  the information_schema does not subclass ACL_internal_table_access,
          there are no per table privilege checks for I_S,
          - the performance schema does use per tables checks, but at most
          returns 'CHECK_GRANT', and never 'ACCESS_GRANTED'.
          so this branch is not used.
        */
        DBUG_ASSERT(0);
      case ACL_INTERNAL_ACCESS_DENIED:
        goto err;
      case ACL_INTERNAL_ACCESS_CHECK_GRANT:
        break;
      }
    }

    want_access= orig_want_access;
    want_access&= ~sctx->master_access;
    if (!want_access)
      continue;                                 // ok

    if (!(~t_ref->grant.privilege & want_access) ||
        t_ref->is_anonymous_derived_table() || t_ref->schema_table)
    {
      /*
        It is subquery in the FROM clause. VIEW set t_ref->derived after
        table opening, but this function always called before table opening.
      */
      if (!t_ref->referencing_view)
      {
        /*
          If it's a temporary table created for a subquery in the FROM
          clause, or an INFORMATION_SCHEMA table, drop the request for
          a privilege.
        */
        t_ref->grant.want_privilege= 0;
      }
      continue;
    }
    GRANT_TABLE *grant_table= table_hash_search(sctx->get_host()->ptr(),
                                                sctx->get_ip()->ptr(),
                                                t_ref->get_db_name(),
                                                sctx->priv_user,
                                                t_ref->get_table_name(),
                                                FALSE);

    if (!grant_table)
    {
      want_access &= ~t_ref->grant.privilege;
      goto err;					// No grants
    }

    /*
      For SHOW COLUMNS, SHOW INDEX it is enough to have some
      privileges on any column combination on the table.
    */
    if (any_combination_will_do)
      continue;

    t_ref->grant.grant_table= grant_table; // Remember for column test
    t_ref->grant.version= grant_version;
    t_ref->grant.privilege|= grant_table->privs;
    t_ref->grant.want_privilege= ((want_access & COL_ACLS) & ~t_ref->grant.privilege);

    if (!(~t_ref->grant.privilege & want_access))
      continue;

    if (want_access & ~(grant_table->cols | t_ref->grant.privilege))
    {
      want_access &= ~(grant_table->cols | t_ref->grant.privilege);
      goto err;					// impossible
    }
  }
  mysql_rwlock_unlock(&LOCK_grant);
  DBUG_RETURN(FALSE);

err:
  mysql_rwlock_unlock(&LOCK_grant);
  if (!no_errors)				// Not a silent skip of table
  {
    char command[128];
    get_privilege_desc(command, sizeof(command), want_access);
    my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
             command,
             sctx->priv_user,
             sctx->host_or_ip,
             tl ? tl->get_table_name() : "unknown");
  }
  DBUG_RETURN(TRUE);
}


/*
  Check column rights in given security context

  SYNOPSIS
    check_grant_column()
    thd                  thread handler
    grant                grant information structure
    db_name              db name
    table_name           table  name
    name                 column name
    length               column name length
    sctx                 security context

  RETURN
    FALSE OK
    TRUE  access denied
*/

bool check_grant_column(THD *thd, GRANT_INFO *grant,
			const char *db_name, const char *table_name,
			const char *name, uint length,  Security_context *sctx)
{
  GRANT_TABLE *grant_table;
  GRANT_COLUMN *grant_column;
  ulong want_access= grant->want_privilege & ~grant->privilege;
  DBUG_ENTER("check_grant_column");
  DBUG_PRINT("enter", ("table: %s  want_access: %lu", table_name, want_access));

  if (!want_access)
    DBUG_RETURN(0);				// Already checked

  mysql_rwlock_rdlock(&LOCK_grant);

  /* reload table if someone has modified any grants */

  if (grant->version != grant_version)
  {
    grant->grant_table=
      table_hash_search(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                        db_name, sctx->priv_user,
			table_name, 0);         /* purecov: inspected */
    grant->version= grant_version;	        /* purecov: inspected */
  }
  if (!(grant_table= grant->grant_table))
    goto err;					/* purecov: deadcode */

  grant_column=column_hash_search(grant_table, name, length);
  if (grant_column && !(~grant_column->rights & want_access))
  {
    mysql_rwlock_unlock(&LOCK_grant);
    DBUG_RETURN(0);
  }

err:
  mysql_rwlock_unlock(&LOCK_grant);
  char command[128];
  get_privilege_desc(command, sizeof(command), want_access);
  my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
           command,
           sctx->priv_user,
           sctx->host_or_ip,
           name,
           table_name);
  DBUG_RETURN(1);
}


/*
  Check the access right to a column depending on the type of table.

  SYNOPSIS
    check_column_grant_in_table_ref()
    thd              thread handler
    table_ref        table reference where to check the field
    name             name of field to check
    length           length of name

  DESCRIPTION
    Check the access rights to a column depending on the type of table
    reference where the column is checked. The function provides a
    generic interface to check column access rights that hides the
    heterogeneity of the column representation - whether it is a view
    or a stored table colum.

  RETURN
    FALSE OK
    TRUE  access denied
*/

bool check_column_grant_in_table_ref(THD *thd, TABLE_LIST * table_ref,
                                     const char *name, uint length)
{
  GRANT_INFO *grant;
  const char *db_name;
  const char *table_name;
  Security_context *sctx= test(table_ref->security_ctx) ?
                          table_ref->security_ctx : thd->security_ctx;

  if (table_ref->view || table_ref->field_translation)
  {
    /* View or derived information schema table. */
    ulong view_privs;
    grant= &(table_ref->grant);
    db_name= table_ref->view_db.str;
    table_name= table_ref->view_name.str;
    if (table_ref->belong_to_view && 
        thd->lex->sql_command == SQLCOM_SHOW_FIELDS)
    {
      view_privs= get_column_grant(thd, grant, db_name, table_name, name);
      if (view_privs & VIEW_ANY_ACL)
      {
        table_ref->belong_to_view->allowed_show= TRUE;
        return FALSE;
      }
      table_ref->belong_to_view->allowed_show= FALSE;
      my_message(ER_VIEW_NO_EXPLAIN, ER(ER_VIEW_NO_EXPLAIN), MYF(0));
      return TRUE;
    }
  }
  else
  {
    /* Normal or temporary table. */
    TABLE *table= table_ref->table;
    grant= &(table->grant);
    db_name= table->s->db.str;
    table_name= table->s->table_name.str;
  }

  if (grant->want_privilege)
    return check_grant_column(thd, grant, db_name, table_name, name,
                              length, sctx);
  else
    return FALSE;

}


/** 
  @brief check if a query can access a set of columns

  @param  thd  the current thread
  @param  want_access_arg  the privileges requested
  @param  fields an iterator over the fields of a table reference.
  @return Operation status
    @retval 0 Success
    @retval 1 Falure
  @details This function walks over the columns of a table reference 
   The columns may originate from different tables, depending on the kind of
   table reference, e.g. join, view.
   For each table it will retrieve the grant information and will use it
   to check the required access privileges for the fields requested from it.
*/    
bool check_grant_all_columns(THD *thd, ulong want_access_arg, 
                             Field_iterator_table_ref *fields)
{
  Security_context *sctx= thd->security_ctx;
  ulong want_access= want_access_arg;
  const char *table_name= NULL;

  const char* db_name; 
  GRANT_INFO *grant;
  /* Initialized only to make gcc happy */
  GRANT_TABLE *grant_table= NULL;
  /* 
     Flag that gets set if privilege checking has to be performed on column
     level.
  */
  bool using_column_privileges= FALSE;

  mysql_rwlock_rdlock(&LOCK_grant);

  for (; !fields->end_of_fields(); fields->next())
  {
    const char *field_name= fields->name();

    if (table_name != fields->get_table_name())
    {
      table_name= fields->get_table_name();
      db_name= fields->get_db_name();
      grant= fields->grant();
      /* get a fresh one for each table */
      want_access= want_access_arg & ~grant->privilege;
      if (want_access)
      {
        /* reload table if someone has modified any grants */
        if (grant->version != grant_version)
        {
          grant->grant_table=
            table_hash_search(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                              db_name, sctx->priv_user,
                              table_name, 0);	/* purecov: inspected */
          grant->version= grant_version;	/* purecov: inspected */
        }

        grant_table= grant->grant_table;
        DBUG_ASSERT (grant_table);
      }
    }

    if (want_access)
    {
      GRANT_COLUMN *grant_column= 
        column_hash_search(grant_table, field_name,
                           (uint) strlen(field_name));
      if (grant_column)
        using_column_privileges= TRUE;
      if (!grant_column || (~grant_column->rights & want_access))
        goto err;
    }
  }
  mysql_rwlock_unlock(&LOCK_grant);
  return 0;

err:
  mysql_rwlock_unlock(&LOCK_grant);

  char command[128];
  get_privilege_desc(command, sizeof(command), want_access);
  /*
    Do not give an error message listing a column name unless the user has
    privilege to see all columns.
  */
  if (using_column_privileges)
    my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
             command, sctx->priv_user,
             sctx->host_or_ip, table_name); 
  else
    my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
             command,
             sctx->priv_user,
             sctx->host_or_ip,
             fields->name(),
             table_name);
  return 1;
}


static bool check_grant_db_routine(THD *thd, const char *db, HASH *hash)
{
  Security_context *sctx= thd->security_ctx;

  for (uint idx= 0; idx < hash->records; ++idx)
  {
    GRANT_NAME *item= (GRANT_NAME*) my_hash_element(hash, idx);

    if (strcmp(item->user, sctx->priv_user) == 0 &&
        strcmp(item->db, db) == 0 &&
        compare_hostname(&item->host, sctx->get_host()->ptr(),
                         sctx->get_ip()->ptr()))
    {
      return FALSE;
    }
  }

  return TRUE;
}


/*
  Check if a user has the right to access a database
  Access is accepted if he has a grant for any table/routine in the database
  Return 1 if access is denied
*/

bool check_grant_db(THD *thd,const char *db)
{
  Security_context *sctx= thd->security_ctx;
  char helping [NAME_LEN+USERNAME_LENGTH+2];
  uint len;
  bool error= TRUE;
  size_t copy_length;

  copy_length= (size_t) (strlen(sctx->priv_user ? sctx->priv_user : "") +
                 strlen(db ? db : "")) + 1; /* Added 1 at the end to avoid  
                                               buffer overflow at strmov()*/

  /*
    Make sure that strmov() operations do not result in buffer overflow.
  */
  if (copy_length >= (NAME_LEN+USERNAME_LENGTH+2))
    return 1;

  len= (uint) (strmov(strmov(helping, sctx->priv_user) + 1, db) - helping) + 1;

  mysql_rwlock_rdlock(&LOCK_grant);

  for (uint idx=0 ; idx < column_priv_hash.records ; idx++)
  {
    GRANT_TABLE *grant_table= (GRANT_TABLE*)
      my_hash_element(&column_priv_hash,
                      idx);
    if (len < grant_table->key_length &&
	!memcmp(grant_table->hash_key,helping,len) &&
        compare_hostname(&grant_table->host, sctx->get_host()->ptr(),
                         sctx->get_ip()->ptr()))
    {
      error= FALSE; /* Found match. */
      break;
    }
  }

  if (error)
    error= check_grant_db_routine(thd, db, &proc_priv_hash) &&
           check_grant_db_routine(thd, db, &func_priv_hash);

  mysql_rwlock_unlock(&LOCK_grant);

  return error;
}


/****************************************************************************
  Check routine level grants

  SYNPOSIS
   bool check_grant_routine()
   thd		Thread handler
   want_access  Bits of privileges user needs to have
   procs	List of routines to check. The user should have 'want_access'
   is_proc	True if the list is all procedures, else functions
   no_errors	If 0 then we write an error. The error is sent directly to
		the client

   RETURN
     0  ok
     1  Error: User did not have the requested privielges
****************************************************************************/

bool check_grant_routine(THD *thd, ulong want_access,
			 TABLE_LIST *procs, bool is_proc, bool no_errors)
{
  TABLE_LIST *table;
  Security_context *sctx= thd->security_ctx;
  char *user= sctx->priv_user;
  char *host= sctx->priv_host;
  DBUG_ENTER("check_grant_routine");

  want_access&= ~sctx->master_access;
  if (!want_access)
    DBUG_RETURN(0);                             // ok

  mysql_rwlock_rdlock(&LOCK_grant);
  for (table= procs; table; table= table->next_global)
  {
    GRANT_NAME *grant_proc;
    if ((grant_proc= routine_hash_search(host, sctx->get_ip()->ptr(), table->db,
                                         user, table->table_name, is_proc, 0)))
      table->grant.privilege|= grant_proc->privs;

    if (want_access & ~table->grant.privilege)
    {
      want_access &= ~table->grant.privilege;
      goto err;
    }
  }
  mysql_rwlock_unlock(&LOCK_grant);
  DBUG_RETURN(0);
err:
  mysql_rwlock_unlock(&LOCK_grant);
  if (!no_errors)
  {
    char buff[1024];
    const char *command="";
    if (table)
      strxmov(buff, table->db, ".", table->table_name, NullS);
    if (want_access & EXECUTE_ACL)
      command= "execute";
    else if (want_access & ALTER_PROC_ACL)
      command= "alter routine";
    else if (want_access & GRANT_ACL)
      command= "grant";
    my_error(ER_PROCACCESS_DENIED_ERROR, MYF(0),
             command, user, host, table ? buff : "unknown");
  }
  DBUG_RETURN(1);
}


/*
  Check if routine has any of the 
  routine level grants
  
  SYNPOSIS
   bool    check_routine_level_acl()
   thd	        Thread handler
   db           Database name
   name         Routine name

  RETURN
   0            Ok 
   1            error
*/

bool check_routine_level_acl(THD *thd, const char *db, const char *name, 
                             bool is_proc)
{
  bool no_routine_acl= 1;
  GRANT_NAME *grant_proc;
  Security_context *sctx= thd->security_ctx;
  mysql_rwlock_rdlock(&LOCK_grant);
  if ((grant_proc= routine_hash_search(sctx->priv_host,
                                       sctx->get_ip()->ptr(), db,
                                       sctx->priv_user,
                                       name, is_proc, 0)))
    no_routine_acl= !(grant_proc->privs & SHOW_PROC_ACLS);
  mysql_rwlock_unlock(&LOCK_grant);
  return no_routine_acl;
}


/*****************************************************************************
  Functions to retrieve the grant for a table/column  (for SHOW functions)
*****************************************************************************/

ulong get_table_grant(THD *thd, TABLE_LIST *table)
{
  ulong privilege;
  Security_context *sctx= thd->security_ctx;
  const char *db = table->db ? table->db : thd->db;
  GRANT_TABLE *grant_table;

  mysql_rwlock_rdlock(&LOCK_grant);
#ifdef EMBEDDED_LIBRARY
  grant_table= NULL;
#else
  grant_table= table_hash_search(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                                 db, sctx->priv_user, table->table_name, 0);
#endif
  table->grant.grant_table=grant_table; // Remember for column test
  table->grant.version=grant_version;
  if (grant_table)
    table->grant.privilege|= grant_table->privs;
  privilege= table->grant.privilege;
  mysql_rwlock_unlock(&LOCK_grant);
  return privilege;
}


/*
  Determine the access priviliges for a field.

  SYNOPSIS
    get_column_grant()
    thd         thread handler
    grant       grants table descriptor
    db_name     name of database that the field belongs to
    table_name  name of table that the field belongs to
    field_name  name of field

  DESCRIPTION
    The procedure may also modify: grant->grant_table and grant->version.

  RETURN
    The access priviliges for the field db_name.table_name.field_name
*/

ulong get_column_grant(THD *thd, GRANT_INFO *grant,
                       const char *db_name, const char *table_name,
                       const char *field_name)
{
  GRANT_TABLE *grant_table;
  GRANT_COLUMN *grant_column;
  ulong priv;

  mysql_rwlock_rdlock(&LOCK_grant);
  /* reload table if someone has modified any grants */
  if (grant->version != grant_version)
  {
    Security_context *sctx= thd->security_ctx;
    grant->grant_table=
      table_hash_search(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                        db_name, sctx->priv_user,
			table_name, 0);	        /* purecov: inspected */
    grant->version= grant_version;              /* purecov: inspected */
  }

  if (!(grant_table= grant->grant_table))
    priv= grant->privilege;
  else
  {
    grant_column= column_hash_search(grant_table, field_name,
                                     (uint) strlen(field_name));
    if (!grant_column)
      priv= (grant->privilege | grant_table->privs);
    else
      priv= (grant->privilege | grant_table->privs | grant_column->rights);
  }
  mysql_rwlock_unlock(&LOCK_grant);
  return priv;
}


/* Help function for mysql_show_grants */

static void add_user_option(String *grant, ulong value, const char *name)
{
  if (value)
  {
    char buff[22], *p; // just as in int2str
    grant->append(' ');
    grant->append(name, strlen(name));
    grant->append(' ');
    p=int10_to_str(value, buff, 10);
    grant->append(buff,p-buff);
  }
}

static const char *command_array[]=
{
  "SELECT", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP", "RELOAD",
  "SHUTDOWN", "PROCESS","FILE", "GRANT", "REFERENCES", "INDEX",
  "ALTER", "SHOW DATABASES", "SUPER", "CREATE TEMPORARY TABLES",
  "LOCK TABLES", "EXECUTE", "REPLICATION SLAVE", "REPLICATION CLIENT",
  "CREATE VIEW", "SHOW VIEW", "CREATE ROUTINE", "ALTER ROUTINE",
  "CREATE USER", "EVENT", "TRIGGER", "CREATE TABLESPACE"
};

static uint command_lengths[]=
{
  6, 6, 6, 6, 6, 4, 6, 8, 7, 4, 5, 10, 5, 5, 14, 5, 23, 11, 7, 17, 18, 11, 9,
  14, 13, 11, 5, 7, 17
};


static int show_routine_grants(THD *thd, LEX_USER *lex_user, HASH *hash,
                               const char *type, int typelen,
                               char *buff, int buffsize);


/*
  SHOW GRANTS;  Send grants for a user to the client

  IMPLEMENTATION
   Send to client grant-like strings depicting user@host privileges
*/

bool mysql_show_grants(THD *thd,LEX_USER *lex_user)
{
  ulong want_access;
  uint counter,index;
  int  error = 0;
  ACL_USER *acl_user;
  ACL_DB *acl_db;
  char buff[1024];
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysql_show_grants");

  LINT_INIT(acl_user);
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }

  mysql_rwlock_rdlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  acl_user= find_acl_user(lex_user->host.str, lex_user->user.str, TRUE);
  if (!acl_user)
  {
    mysql_mutex_unlock(&acl_cache->lock);
    mysql_rwlock_unlock(&LOCK_grant);

    my_error(ER_NONEXISTING_GRANT, MYF(0),
             lex_user->user.str, lex_user->host.str);
    DBUG_RETURN(TRUE);
  }

  Item_string *field=new Item_string("",0,&my_charset_latin1);
  List<Item> field_list;
  field->name=buff;
  field->max_length=1024;
  strxmov(buff,"Grants for ",lex_user->user.str,"@",
	  lex_user->host.str,NullS);
  field_list.push_back(field);
  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    mysql_mutex_unlock(&acl_cache->lock);
    mysql_rwlock_unlock(&LOCK_grant);

    DBUG_RETURN(TRUE);
  }

  /* Add first global access grants */
  {
    String global(buff,sizeof(buff),system_charset_info);
    global.length(0);
    global.append(STRING_WITH_LEN("GRANT "));

    want_access= acl_user->access;
    if (test_all_bits(want_access, (GLOBAL_ACLS & ~ GRANT_ACL)))
      global.append(STRING_WITH_LEN("ALL PRIVILEGES"));
    else if (!(want_access & ~GRANT_ACL))
      global.append(STRING_WITH_LEN("USAGE"));
    else
    {
      bool found=0;
      ulong j,test_access= want_access & ~GRANT_ACL;
      for (counter=0, j = SELECT_ACL;j <= GLOBAL_ACLS;counter++,j <<= 1)
      {
	if (test_access & j)
	{
	  if (found)
	    global.append(STRING_WITH_LEN(", "));
	  found=1;
	  global.append(command_array[counter],command_lengths[counter]);
	}
      }
    }
    global.append (STRING_WITH_LEN(" ON *.* TO '"));
    global.append(lex_user->user.str, lex_user->user.length,
		  system_charset_info);
    global.append (STRING_WITH_LEN("'@'"));
    global.append(lex_user->host.str,lex_user->host.length,
		  system_charset_info);
    global.append ('\'');
    if (acl_user->salt_len)
    {
      global.append(STRING_WITH_LEN(" IDENTIFIED BY PASSWORD"));
      if ((thd->security_ctx->master_access & SUPER_ACL) == SUPER_ACL)
      {
        char passwd_buff[SCRAMBLED_PASSWORD_CHAR_LENGTH+1];
        if (acl_user->salt_len == SCRAMBLE_LENGTH)
          make_password_from_salt(passwd_buff, acl_user->salt);
        else
          make_password_from_salt_323(passwd_buff, (ulong *) acl_user->salt);

        global.append(" \'");
        global.append(passwd_buff);
        global.append('\'');
      }
      else
        global.append(" <secret>");
    }
    /* "show grants" SSL related stuff */
    if (acl_user->ssl_type == SSL_TYPE_ANY)
      global.append(STRING_WITH_LEN(" REQUIRE SSL"));
    else if (acl_user->ssl_type == SSL_TYPE_X509)
      global.append(STRING_WITH_LEN(" REQUIRE X509"));
    else if (acl_user->ssl_type == SSL_TYPE_SPECIFIED)
    {
      int ssl_options = 0;
      global.append(STRING_WITH_LEN(" REQUIRE "));
      if (acl_user->x509_issuer)
      {
	ssl_options++;
	global.append(STRING_WITH_LEN("ISSUER \'"));
	global.append(acl_user->x509_issuer,strlen(acl_user->x509_issuer));
	global.append('\'');
      }
      if (acl_user->x509_subject)
      {
	if (ssl_options++)
	  global.append(' ');
	global.append(STRING_WITH_LEN("SUBJECT \'"));
	global.append(acl_user->x509_subject,strlen(acl_user->x509_subject),
                      system_charset_info);
	global.append('\'');
      }
      if (acl_user->ssl_cipher)
      {
	if (ssl_options++)
	  global.append(' ');
	global.append(STRING_WITH_LEN("CIPHER '"));
	global.append(acl_user->ssl_cipher,strlen(acl_user->ssl_cipher),
                      system_charset_info);
	global.append('\'');
      }
    }
    if ((want_access & GRANT_ACL) ||
	(acl_user->user_resource.questions ||
         acl_user->user_resource.updates ||
         acl_user->user_resource.conn_per_hour ||
         acl_user->user_resource.user_conn))
    {
      global.append(STRING_WITH_LEN(" WITH"));
      if (want_access & GRANT_ACL)
	global.append(STRING_WITH_LEN(" GRANT OPTION"));
      add_user_option(&global, acl_user->user_resource.questions,
		      "MAX_QUERIES_PER_HOUR");
      add_user_option(&global, acl_user->user_resource.updates,
		      "MAX_UPDATES_PER_HOUR");
      add_user_option(&global, acl_user->user_resource.conn_per_hour,
		      "MAX_CONNECTIONS_PER_HOUR");
      add_user_option(&global, acl_user->user_resource.user_conn,
		      "MAX_USER_CONNECTIONS");
    }
    protocol->prepare_for_resend();
    protocol->store(global.ptr(),global.length(),global.charset());
    if (protocol->write())
    {
      error= -1;
      goto end;
    }
  }

  /* Add database access */
  for (counter=0 ; counter < acl_dbs.elements ; counter++)
  {
    const char *user, *host;

    acl_db=dynamic_element(&acl_dbs,counter,ACL_DB*);
    if (!(user=acl_db->user))
      user= "";
    if (!(host=acl_db->host.hostname))
      host= "";

    /*
      We do not make SHOW GRANTS case-sensitive here (like REVOKE),
      but make it case-insensitive because that's the way they are
      actually applied, and showing fewer privileges than are applied
      would be wrong from a security point of view.
    */

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      want_access=acl_db->access;
      if (want_access)
      {
	String db(buff,sizeof(buff),system_charset_info);
	db.length(0);
	db.append(STRING_WITH_LEN("GRANT "));

	if (test_all_bits(want_access,(DB_ACLS & ~GRANT_ACL)))
	  db.append(STRING_WITH_LEN("ALL PRIVILEGES"));
	else if (!(want_access & ~GRANT_ACL))
	  db.append(STRING_WITH_LEN("USAGE"));
	else
	{
	  int found=0, cnt;
	  ulong j,test_access= want_access & ~GRANT_ACL;
	  for (cnt=0, j = SELECT_ACL; j <= DB_ACLS; cnt++,j <<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		db.append(STRING_WITH_LEN(", "));
	      found = 1;
	      db.append(command_array[cnt],command_lengths[cnt]);
	    }
	  }
	}
	db.append (STRING_WITH_LEN(" ON "));
	append_identifier(thd, &db, acl_db->db, strlen(acl_db->db));
	db.append (STRING_WITH_LEN(".* TO '"));
	db.append(lex_user->user.str, lex_user->user.length,
		  system_charset_info);
	db.append (STRING_WITH_LEN("'@'"));
	// host and lex_user->host are equal except for case
	db.append(host, strlen(host), system_charset_info);
	db.append ('\'');
	if (want_access & GRANT_ACL)
	  db.append(STRING_WITH_LEN(" WITH GRANT OPTION"));
	protocol->prepare_for_resend();
	protocol->store(db.ptr(),db.length(),db.charset());
	if (protocol->write())
	{
	  error= -1;
	  goto end;
	}
      }
    }
  }

  /* Add table & column access */
  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *host;
    GRANT_TABLE *grant_table= (GRANT_TABLE*)
      my_hash_element(&column_priv_hash, index);

    if (!(user=grant_table->user))
      user= "";
    if (!(host= grant_table->host.hostname))
      host= "";

    /*
      We do not make SHOW GRANTS case-sensitive here (like REVOKE),
      but make it case-insensitive because that's the way they are
      actually applied, and showing fewer privileges than are applied
      would be wrong from a security point of view.
    */

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      ulong table_access= grant_table->privs;
      if ((table_access | grant_table->cols) != 0)
      {
	String global(buff, sizeof(buff), system_charset_info);
	ulong test_access= (table_access | grant_table->cols) & ~GRANT_ACL;

	global.length(0);
	global.append(STRING_WITH_LEN("GRANT "));

	if (test_all_bits(table_access, (TABLE_ACLS & ~GRANT_ACL)))
	  global.append(STRING_WITH_LEN("ALL PRIVILEGES"));
	else if (!test_access)
	  global.append(STRING_WITH_LEN("USAGE"));
	else
	{
          /* Add specific column access */
	  int found= 0;
	  ulong j;

	  for (counter= 0, j= SELECT_ACL; j <= TABLE_ACLS; counter++, j<<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		global.append(STRING_WITH_LEN(", "));
	      found= 1;
	      global.append(command_array[counter],command_lengths[counter]);

	      if (grant_table->cols)
	      {
		uint found_col= 0;
		for (uint col_index=0 ;
		     col_index < grant_table->hash_columns.records ;
		     col_index++)
		{
		  GRANT_COLUMN *grant_column = (GRANT_COLUMN*)
                    my_hash_element(&grant_table->hash_columns,col_index);
		  if (grant_column->rights & j)
		  {
		    if (!found_col)
		    {
		      found_col= 1;
		      /*
			If we have a duplicated table level privilege, we
			must write the access privilege name again.
		      */
		      if (table_access & j)
		      {
			global.append(STRING_WITH_LEN(", "));
			global.append(command_array[counter],
				      command_lengths[counter]);
		      }
		      global.append(STRING_WITH_LEN(" ("));
		    }
		    else
		      global.append(STRING_WITH_LEN(", "));
		    global.append(grant_column->column,
				  grant_column->key_length,
				  system_charset_info);
		  }
		}
		if (found_col)
		  global.append(')');
	      }
	    }
	  }
	}
	global.append(STRING_WITH_LEN(" ON "));
	append_identifier(thd, &global, grant_table->db,
			  strlen(grant_table->db));
	global.append('.');
	append_identifier(thd, &global, grant_table->tname,
			  strlen(grant_table->tname));
	global.append(STRING_WITH_LEN(" TO '"));
	global.append(lex_user->user.str, lex_user->user.length,
		      system_charset_info);
	global.append(STRING_WITH_LEN("'@'"));
	// host and lex_user->host are equal except for case
	global.append(host, strlen(host), system_charset_info);
	global.append('\'');
	if (table_access & GRANT_ACL)
	  global.append(STRING_WITH_LEN(" WITH GRANT OPTION"));
	protocol->prepare_for_resend();
	protocol->store(global.ptr(),global.length(),global.charset());
	if (protocol->write())
	{
	  error= -1;
	  break;
	}
      }
    }
  }

  if (show_routine_grants(thd, lex_user, &proc_priv_hash, 
                          STRING_WITH_LEN("PROCEDURE"), buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

  if (show_routine_grants(thd, lex_user, &func_priv_hash,
                          STRING_WITH_LEN("FUNCTION"), buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

  if (show_proxy_grants(thd, lex_user, buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

end:
  mysql_mutex_unlock(&acl_cache->lock);
  mysql_rwlock_unlock(&LOCK_grant);

  my_eof(thd);
  DBUG_RETURN(error);
}

static int show_routine_grants(THD* thd, LEX_USER *lex_user, HASH *hash,
                               const char *type, int typelen,
                               char *buff, int buffsize)
{
  uint counter, index;
  int error= 0;
  Protocol *protocol= thd->protocol;
  /* Add routine access */
  for (index=0 ; index < hash->records ; index++)
  {
    const char *user, *host;
    GRANT_NAME *grant_proc= (GRANT_NAME*) my_hash_element(hash, index);

    if (!(user=grant_proc->user))
      user= "";
    if (!(host= grant_proc->host.hostname))
      host= "";

    /*
      We do not make SHOW GRANTS case-sensitive here (like REVOKE),
      but make it case-insensitive because that's the way they are
      actually applied, and showing fewer privileges than are applied
      would be wrong from a security point of view.
    */

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      ulong proc_access= grant_proc->privs;
      if (proc_access != 0)
      {
	String global(buff, buffsize, system_charset_info);
	ulong test_access= proc_access & ~GRANT_ACL;

	global.length(0);
	global.append(STRING_WITH_LEN("GRANT "));

	if (!test_access)
 	  global.append(STRING_WITH_LEN("USAGE"));
	else
	{
          /* Add specific procedure access */
	  int found= 0;
	  ulong j;

	  for (counter= 0, j= SELECT_ACL; j <= PROC_ACLS; counter++, j<<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		global.append(STRING_WITH_LEN(", "));
	      found= 1;
	      global.append(command_array[counter],command_lengths[counter]);
	    }
	  }
	}
	global.append(STRING_WITH_LEN(" ON "));
        global.append(type,typelen);
        global.append(' ');
	append_identifier(thd, &global, grant_proc->db,
			  strlen(grant_proc->db));
	global.append('.');
	append_identifier(thd, &global, grant_proc->tname,
			  strlen(grant_proc->tname));
	global.append(STRING_WITH_LEN(" TO '"));
	global.append(lex_user->user.str, lex_user->user.length,
		      system_charset_info);
	global.append(STRING_WITH_LEN("'@'"));
	// host and lex_user->host are equal except for case
	global.append(host, strlen(host), system_charset_info);
	global.append('\'');
	if (proc_access & GRANT_ACL)
	  global.append(STRING_WITH_LEN(" WITH GRANT OPTION"));
	protocol->prepare_for_resend();
	protocol->store(global.ptr(),global.length(),global.charset());
	if (protocol->write())
	{
	  error= -1;
	  break;
	}
      }
    }
  }
  return error;
}

/*
  Make a clear-text version of the requested privilege.
*/

void get_privilege_desc(char *to, uint max_length, ulong access)
{
  uint pos;
  char *start=to;
  DBUG_ASSERT(max_length >= 30);		// For end ',' removal

  if (access)
  {
    max_length--;				// Reserve place for end-zero
    for (pos=0 ; access ; pos++, access>>=1)
    {
      if ((access & 1) &&
	  command_lengths[pos] + (uint) (to-start) < max_length)
      {
	to= strmov(to, command_array[pos]);
	*to++=',';
      }
    }
    to--;					// Remove end ','
  }
  *to=0;
}


void get_mqh(const char *user, const char *host, USER_CONN *uc)
{
  ACL_USER *acl_user;

  mysql_mutex_lock(&acl_cache->lock);

  if (initialized && (acl_user= find_acl_user(host,user, FALSE)))
    uc->user_resources= acl_user->user_resource;
  else
    bzero((char*) &uc->user_resources, sizeof(uc->user_resources));

  mysql_mutex_unlock(&acl_cache->lock);
}

/*
  Open the grant tables.

  SYNOPSIS
    open_grant_tables()
    thd                         The current thread.
    tables (out)                The 4 elements array for the opened tables.

  DESCRIPTION
    Tables are numbered as follows:
    0 user
    1 db
    2 tables_priv
    3 columns_priv

  RETURN
    1           Skip GRANT handling during replication.
    0           OK.
    < 0         Error.
*/

#define GRANT_TABLES 6
int open_grant_tables(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("open_grant_tables");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

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
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
      DBUG_RETURN(1);
    tables[0].updating= tables[1].updating= tables[2].updating=
      tables[3].updating= tables[4].updating= tables[5].updating= 0;
  }
#endif

  if (open_and_lock_tables(thd, tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT))
  {						// This should never happen
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

ACL_USER *check_acl_user(LEX_USER *user_name,
			 uint *acl_acl_userdx)
{
  ACL_USER *acl_user= 0;
  uint counter;

  mysql_mutex_assert_owner(&acl_cache->lock);

  for (counter= 0 ; counter < acl_users.elements ; counter++)
  {
    const char *user,*host;
    acl_user= dynamic_element(&acl_users, counter, ACL_USER*);
    if (!(user=acl_user->user))
      user= "";
    if (!(host=acl_user->host.hostname))
      host= "";
    if (!strcmp(user_name->user.str,user) &&
	!my_strcasecmp(system_charset_info, user_name->host.str, host))
      break;
  }
  if (counter == acl_users.elements)
    return 0;

  *acl_acl_userdx= counter;
  return acl_user;
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
    if ((error= table->file->ha_update_row(table->record[1], 
                                           table->record[0])) &&
        error != HA_ERR_RECORD_IS_THE_SAME)
      table->file->print_error(error, MYF(0));
    else
      error= 0;
  }
  else
  {
    /* delete */
    if ((error=table->file->ha_delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
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

static int handle_grant_table(TABLE_LIST *tables, uint table_no, bool drop,
                              LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int error;
  TABLE *table= tables[table_no].table;
  Field *host_field= table->field[0];
  Field *user_field= table->field[table_no && table_no != 5 ? 2 : 1];
  char *host_str= user_from->host.str;
  char *user_str= user_from->user.str;
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
                       table->s->table_name.str, user_str, host_str));
    host_field->store(host_str, user_from->host.length, system_charset_info);
    user_field->store(user_str, user_from->user.length, system_charset_info);

    key_prefix_length= (table->key_info->key_part[0].store_length +
                        table->key_info->key_part[1].store_length);
    key_copy(user_key, table->record[0], table->key_info, key_prefix_length);

    if ((error= table->file->index_read_idx_map(table->record[0], 0,
                                                user_key, (key_part_map)3,
                                                HA_READ_KEY_EXACT)))
    {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        table->file->print_error(error, MYF(0));
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
      table->file->print_error(error, MYF(0));
      result= -1;
    }
    else
    {
#ifdef EXTRA_DEBUG
      DBUG_PRINT("info",("scan table: '%s'  search: '%s'@'%s'",
                         table->s->table_name.str, user_str, host_str));
#endif
      while ((error= table->file->rnd_next(table->record[0])) != 
             HA_ERR_END_OF_FILE)
      {
        if (error)
        {
          /* Most probable 'deleted record'. */
          DBUG_PRINT("info",("scan error: %d", error));
          continue;
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
        if (strcmp(user_str, user) ||
            my_strcasecmp(system_charset_info, host_str, host))
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


/**
  Handle an in-memory privilege structure.

  @param struct_no  The number of the structure to handle (0..5).
  @param drop       If user_from is to be dropped.
  @param user_from  The the user to be searched/dropped/renamed.
  @param user_to    The new name for the user if to be renamed, NULL otherwise.

  @note
    Scan through all elements in an in-memory grant structure and apply
    the requested operation.
    Delete from grant structure if drop is true.
    Update in grant structure if drop is false and user_to is not NULL.
    Search in grant structure if drop is false and user_to is NULL.
    Structures are enumerated as follows:
    0 ACL_USER
    1 ACL_DB
    2 COLUMN_PRIVILIGES_HASH
    3 PROC_PRIVILEGES_HASH
    4 FUNC_PRIVILEGES_HASH
    5 ACL_PROXY_USERS

  @retval > 0  At least one element matched.
  @retval 0    OK, but no element matched.
  @retval -1   Wrong arguments to function or Out of Memory.
*/

static int handle_grant_struct(enum enum_acl_lists struct_no, bool drop,
                               LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  uint idx;
  uint elements;
  const char *user;
  const char *host;
  ACL_USER *acl_user= NULL;
  ACL_DB *acl_db= NULL;
  ACL_PROXY_USER *acl_proxy_user= NULL;
  GRANT_NAME *grant_name= NULL;
  /*
    Dynamic array acl_grant_name used to store pointers to all
    GRANT_NAME objects
  */
  Dynamic_array<GRANT_NAME *> acl_grant_name;
  HASH *grant_name_hash= NULL;
  DBUG_ENTER("handle_grant_struct");
  DBUG_PRINT("info",("scan struct: %u  search: '%s'@'%s'",
                     struct_no, user_from->user.str, user_from->host.str));

  LINT_INIT(user);
  LINT_INIT(host);

  mysql_mutex_assert_owner(&acl_cache->lock);

  /* Get the number of elements in the in-memory structure. */
  switch (struct_no) {
  case USER_ACL:
    elements= acl_users.elements;
    break;
  case DB_ACL:
    elements= acl_dbs.elements;
    break;
  case COLUMN_PRIVILEGES_HASH:
    elements= column_priv_hash.records;
    grant_name_hash= &column_priv_hash;
    break;
  case PROC_PRIVILEGES_HASH:
    elements= proc_priv_hash.records;
    grant_name_hash= &proc_priv_hash;
    break;
  case FUNC_PRIVILEGES_HASH:
    elements= func_priv_hash.records;
    grant_name_hash= &func_priv_hash;
    break;
  case PROXY_USERS_ACL:
    elements= acl_proxy_users.elements;
    break;
  default:
    return -1;
  }

#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("scan struct: %u  search    user: '%s'  host: '%s'",
                       struct_no, user_from->user.str, user_from->host.str));
#endif
  /* Loop over all elements. */
  for (idx= 0; idx < elements; idx++)
  {
    /*
      Get a pointer to the element.
    */
    switch (struct_no) {
    case USER_ACL:
      acl_user= dynamic_element(&acl_users, idx, ACL_USER*);
      user= acl_user->user;
      host= acl_user->host.hostname;
    break;

    case DB_ACL:
      acl_db= dynamic_element(&acl_dbs, idx, ACL_DB*);
      user= acl_db->user;
      host= acl_db->host.hostname;
      break;

    case COLUMN_PRIVILEGES_HASH:
    case PROC_PRIVILEGES_HASH:
    case FUNC_PRIVILEGES_HASH:
      grant_name= (GRANT_NAME*) my_hash_element(grant_name_hash, idx);
      user= grant_name->user;
      host= grant_name->host.hostname;
      break;

    case PROXY_USERS_ACL:
      acl_proxy_user= dynamic_element(&acl_proxy_users, idx, ACL_PROXY_USER*);
      user= acl_proxy_user->get_user();
      host= acl_proxy_user->get_host();
      break;

    default:
      MY_ASSERT_UNREACHABLE();
    }
    if (! user)
      user= "";
    if (! host)
      host= "";

#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("scan struct: %u  index: %u  user: '%s'  host: '%s'",
                       struct_no, idx, user, host));
#endif
    if (strcmp(user_from->user.str, user) ||
        my_strcasecmp(system_charset_info, user_from->host.str, host))
      continue;

    result= 1; /* At least one element found. */
    if ( drop )
    {
      switch ( struct_no ) {
      case USER_ACL:
        delete_dynamic_element(&acl_users, idx);
        elements--;
        /*
        - If we are iterating through an array then we just have moved all
          elements after the current element one position closer to its head.
          This means that we have to take another look at the element at
          current position as it is a new element from the array's tail.
        - This is valid for USER_ACL, DB_ACL and PROXY_USERS_ACL.
        */
        idx--;
        break;

      case DB_ACL:
        delete_dynamic_element(&acl_dbs, idx);
        elements--;
        idx--;
        break;

      case COLUMN_PRIVILEGES_HASH:
      case PROC_PRIVILEGES_HASH:
      case FUNC_PRIVILEGES_HASH:
        /*
          Deleting while traversing a hash table is not valid procedure and
          hence we save pointers to GRANT_NAME objects for later processing.
        */
        if (acl_grant_name.append(grant_name))
          DBUG_RETURN(-1);
	break;

      case PROXY_USERS_ACL:
        delete_dynamic_element(&acl_proxy_users, idx);
        elements--;
        idx--;
        break;

      }
    }
    else if ( user_to )
    {
      switch ( struct_no ) {
      case USER_ACL:
        acl_user->user= strdup_root(&mem, user_to->user.str);
        acl_user->host.hostname= strdup_root(&mem, user_to->host.str);
        break;

      case DB_ACL:
        acl_db->user= strdup_root(&mem, user_to->user.str);
        acl_db->host.hostname= strdup_root(&mem, user_to->host.str);
        break;

      case COLUMN_PRIVILEGES_HASH:
      case PROC_PRIVILEGES_HASH:
      case FUNC_PRIVILEGES_HASH:
        /*
          Updating while traversing a hash table is not valid procedure and
          hence we save pointers to GRANT_NAME objects for later processing.
        */
        if (acl_grant_name.append(grant_name))
          DBUG_RETURN(-1);
        break;

      case PROXY_USERS_ACL:
        acl_proxy_user->set_user (&mem, user_to->user.str);
        acl_proxy_user->set_host (&mem, user_to->host.str);
        break;
      }
    }
    else
    {
      /* If search is requested, we do not need to search further. */
      break;
    }
  }

  if (drop || user_to)
  {
    /*
      Traversing the elements stored in acl_grant_name dynamic array
      to either delete or update them.
    */
    for (int i= 0; i < acl_grant_name.elements(); ++i)
    {
      grant_name= acl_grant_name.at(i);

      if (drop)
      {
        my_hash_delete(grant_name_hash, (uchar *) grant_name);
      }
      else
      {
        /*
          Save old hash key and its length to be able properly update
          element position in hash.
        */
        char *old_key= grant_name->hash_key;
        size_t old_key_length= grant_name->key_length;

        /*
          Update the grant structure with the new user name and host name.
        */
        grant_name->set_user_details(user_to->host.str, grant_name->db,
                                     user_to->user.str, grant_name->tname,
                                     TRUE);

        /*
          Since username is part of the hash key, when the user name
          is renamed, the hash key is changed. Update the hash to
          ensure that the position matches the new hash key value
        */
        my_hash_update(grant_name_hash, (uchar*) grant_name, (uchar*) old_key,
                       old_key_length);
      }
    }
  }

#ifdef EXTRA_DEBUG
  DBUG_PRINT("loop",("scan struct: %u  result %d", struct_no, result));
#endif

  DBUG_RETURN(result);
}


/*
  Handle all privilege tables and in-memory privilege structures.

  SYNOPSIS
    handle_grant_data()
    tables                      The array with the four open tables.
    drop                        If user_from is to be dropped.
    user_from                   The the user to be searched/dropped/renamed.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
    Go through all grant tables and in-memory grant structures and apply
    the requested operation.
    Delete from grant data if drop is true.
    Update in grant data if drop is false and user_to is not NULL.
    Search in grant data if drop is false and user_to is NULL.

  RETURN
    > 0         At least one element matched.
    0           OK, but no element matched.
    < 0         Error.
*/

static int handle_grant_data(TABLE_LIST *tables, bool drop,
                             LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int found;
  int ret;
  Acl_table_intact table_intact;
  DBUG_ENTER("handle_grant_data");

  /* Handle user table. */
  if (table_intact.check(tables[0].table, &mysql_user_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(tables, 0, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle user array. */
    if (((ret= handle_grant_struct(USER_ACL, drop, user_from, user_to) > 0) &&
         ! result) || found)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle db table. */
  if (table_intact.check(tables[1].table, &mysql_db_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(tables, 1, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle db array. */
    if ((((ret= handle_grant_struct(DB_ACL, drop, user_from, user_to) > 0) &&
          ! result) || found) && ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle stored routines table. */
  if (table_intact.check(tables[4].table, &mysql_procs_priv_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(tables, 4, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle procs array. */
    if ((((ret= handle_grant_struct(PROC_PRIVILEGES_HASH, drop, user_from,
                                    user_to) > 0) && ! result) || found) &&
        ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
    /* Handle funcs array. */
    if ((((ret= handle_grant_struct(FUNC_PRIVILEGES_HASH, drop, user_from,
                                    user_to) > 0) && ! result) || found) &&
        ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle tables table. */
  if (table_intact.check(tables[2].table, &mysql_tables_priv_table_def))
  {
    result= -1;
    goto end;
  }

  if ((found= handle_grant_table(tables, 2, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch columns and in-memory array. */
    result= -1;
  }
  else
  {
    if (found && ! result)
    {
      result= 1; /* At least one record found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }

    /* Handle columns table. */
    if (table_intact.check(tables[3].table, &mysql_columns_priv_table_def))
    {
      result= -1;
      goto end;
    }

    if ((found= handle_grant_table(tables, 3, drop, user_from, user_to)) < 0)
    {
      /* Handle of table failed, don't touch the in-memory array. */
      result= -1;
    }
    else
    {
      /* Handle columns hash. */
      if ((((ret= handle_grant_struct(COLUMN_PRIVILEGES_HASH, drop, user_from,
                                      user_to) > 0) && ! result) || found) &&
          ! result)
        result= 1; /* At least one record/element found. */
      else if (ret < 0)
        result= -1;
    }
  }

  /* Handle proxies_priv table. */
  if (tables[5].table)
  {
    if (table_intact.check(tables[5].table, &mysql_proxies_priv_table_def))
    {
      result= -1;
      goto end;
    }

    if ((found= handle_grant_table(tables, 5, drop, user_from, user_to)) < 0)
    {
      /* Handle of table failed, don't touch the in-memory array. */
      result= -1;
    }
    else
    {
      /* Handle proxies_priv array. */
      if (((ret= handle_grant_struct(PROXY_USERS_ACL, drop, user_from, user_to) > 0)
           && !result) || found)
        result= 1; /* At least one record/element found. */
      else if (ret < 0)
        result= -1;
    }
  }
 end:
  DBUG_RETURN(result);
}


static void append_user(String *str, LEX_USER *user)
{
  if (str->length())
    str->append(',');
  str->append('\'');
  str->append(user->user.str);
  str->append(STRING_WITH_LEN("'@'"));
  str->append(user->host.str);
  str->append('\'');
}


/*
  Create a list of users.

  SYNOPSIS
    mysql_create_user()
    thd                         The current thread.
    list                        The users to create.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_create_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_created= FALSE;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_create_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* CREATE USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_name= user_list++))
  {
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= TRUE;
      continue;
    }

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    if (handle_grant_data(tables, 0, user_name, NULL))
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
      continue;
    }

    some_users_created= TRUE;
    if (replace_user_table(thd, tables[0].table, *user_name, 0, 0, 1, 0))
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
    }
  }

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "CREATE USER", wrong_users.c_ptr_safe());

  if (some_users_created)
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  mysql_rwlock_unlock(&LOCK_grant);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


/*
  Drop a list of users and all their privileges.

  SYNOPSIS
    mysql_drop_user()
    thd                         The current thread.
    list                        The users to drop.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_drop_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_deleted= FALSE;
  ulong old_sql_mode= thd->variables.sql_mode;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_drop_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* DROP USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_name= user_list++))
  {
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= TRUE;
      continue;
    }  
    if (handle_grant_data(tables, 1, user_name, NULL) <= 0)
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
      continue;
    }
    some_users_deleted= TRUE;
  }

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "DROP USER", wrong_users.c_ptr_safe());

  if (some_users_deleted)
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  mysql_rwlock_unlock(&LOCK_grant);
  thd->variables.sql_mode= old_sql_mode;
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


/*
  Rename a user.

  SYNOPSIS
    mysql_rename_user()
    thd                         The current thread.
    list                        The user name pairs: (from, to).

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_rename_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_from, *tmp_user_from;
  LEX_USER *user_to, *tmp_user_to;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_renamed= FALSE;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_rename_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* RENAME USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  while ((tmp_user_from= user_list++))
  {
    if (!(user_from= get_current_user(thd, tmp_user_from)))
    {
      result= TRUE;
      continue;
    }  
    tmp_user_to= user_list++;
    if (!(user_to= get_current_user(thd, tmp_user_to)))
    {
      result= TRUE;
      continue;
    }  
    DBUG_ASSERT(user_to != 0); /* Syntax enforces pairs of users. */

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    if (handle_grant_data(tables, 0, user_to, NULL) ||
        handle_grant_data(tables, 0, user_from, user_to) <= 0)
    {
      append_user(&wrong_users, user_from);
      result= TRUE;
      continue;
    }
    some_users_renamed= TRUE;
  }
  
  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "RENAME USER", wrong_users.c_ptr_safe());
  
  if (some_users_renamed && mysql_bin_log.is_open())
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  mysql_rwlock_unlock(&LOCK_grant);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();
  DBUG_RETURN(result);
}


/*
  Revoke all privileges from a list of users.

  SYNOPSIS
    mysql_revoke_all()
    thd                         The current thread.
    list                        The users to revoke all privileges from.

  RETURN
    > 0         Error. Error message already sent.
    0           OK.
    < 0         Error. Error message not yet sent.
*/

bool mysql_revoke_all(THD *thd,  List <LEX_USER> &list)
{
  uint counter, revoked, is_proc;
  int result;
  ACL_DB *acl_db;
  TABLE_LIST tables[GRANT_TABLES];
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_revoke_all");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
    if (save_binlog_row_based)
      thd->set_current_stmt_binlog_format_row();
    DBUG_RETURN(result != 1);
  }

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  LEX_USER *lex_user, *tmp_lex_user;
  List_iterator <LEX_USER> user_list(list);
  while ((tmp_lex_user= user_list++))
  {
    if (!(lex_user= get_current_user(thd, tmp_lex_user)))
    {
      result= -1;
      continue;
    }  
    if (!find_acl_user(lex_user->host.str, lex_user->user.str, TRUE))
    {
      result= -1;
      continue;
    }

    if (replace_user_table(thd, tables[0].table,
			   *lex_user, ~(ulong)0, 1, 0, 0))
    {
      result= -1;
      continue;
    }

    /* Remove db access privileges */
    /*
      Because acl_dbs and column_priv_hash shrink and may re-order
      as privileges are removed, removal occurs in a repeated loop
      until no more privileges are revoked.
     */
    do
    {
      for (counter= 0, revoked= 0 ; counter < acl_dbs.elements ; )
      {
	const char *user,*host;

	acl_db=dynamic_element(&acl_dbs,counter,ACL_DB*);
	if (!(user=acl_db->user))
	  user= "";
	if (!(host=acl_db->host.hostname))
	  host= "";

	if (!strcmp(lex_user->user.str,user) &&
            !strcmp(lex_user->host.str, host))
	{
	  if (!replace_db_table(tables[1].table, acl_db->db, *lex_user,
                                ~(ulong)0, 1))
	  {
	    /*
	      Don't increment counter as replace_db_table deleted the
	      current element in acl_dbs.
	     */
	    revoked= 1;
	    continue;
	  }
	  result= -1; // Something went wrong
	}
	counter++;
      }
    } while (revoked);

    /* Remove column access */
    do
    {
      for (counter= 0, revoked= 0 ; counter < column_priv_hash.records ; )
      {
	const char *user,*host;
        GRANT_TABLE *grant_table=
          (GRANT_TABLE*) my_hash_element(&column_priv_hash, counter);
	if (!(user=grant_table->user))
	  user= "";
	if (!(host=grant_table->host.hostname))
	  host= "";

	if (!strcmp(lex_user->user.str,user) &&
            !strcmp(lex_user->host.str, host))
	{
	  if (replace_table_table(thd,grant_table,tables[2].table,*lex_user,
				  grant_table->db,
				  grant_table->tname,
				  ~(ulong)0, 0, 1))
	  {
	    result= -1;
	  }
	  else
	  {
	    if (!grant_table->cols)
	    {
	      revoked= 1;
	      continue;
	    }
	    List<LEX_COLUMN> columns;
	    if (!replace_column_table(grant_table,tables[3].table, *lex_user,
				      columns,
				      grant_table->db,
				      grant_table->tname,
				      ~(ulong)0, 1))
	    {
	      revoked= 1;
	      continue;
	    }
	    result= -1;
	  }
	}
	counter++;
      }
    } while (revoked);

    /* Remove procedure access */
    for (is_proc=0; is_proc<2; is_proc++) do {
      HASH *hash= is_proc ? &proc_priv_hash : &func_priv_hash;
      for (counter= 0, revoked= 0 ; counter < hash->records ; )
      {
	const char *user,*host;
        GRANT_NAME *grant_proc= (GRANT_NAME*) my_hash_element(hash, counter);
	if (!(user=grant_proc->user))
	  user= "";
	if (!(host=grant_proc->host.hostname))
	  host= "";

	if (!strcmp(lex_user->user.str,user) &&
            !strcmp(lex_user->host.str, host))
	{
	  if (replace_routine_table(thd,grant_proc,tables[4].table,*lex_user,
				  grant_proc->db,
				  grant_proc->tname,
                                  is_proc,
				  ~(ulong)0, 1) == 0)
	  {
	    revoked= 1;
	    continue;
	  }
	  result= -1;	// Something went wrong
	}
	counter++;
      }
    } while (revoked);
  }

  mysql_mutex_unlock(&acl_cache->lock);

  if (result)
    my_message(ER_REVOKE_GRANTS, ER(ER_REVOKE_GRANTS), MYF(0));

  result= result |
    write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  mysql_rwlock_unlock(&LOCK_grant);
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(result);
}




/**
  If the defining user for a routine does not exist, then the ACL lookup
  code should raise two errors which we should intercept.  We convert the more
  descriptive error into a warning, and consume the other.

  If any other errors are raised, then we set a flag that should indicate
  that there was some failure we should complain at a higher level.
*/
class Silence_routine_definer_errors : public Internal_error_handler
{
public:
  Silence_routine_definer_errors()
    : is_grave(FALSE)
  {}

  virtual ~Silence_routine_definer_errors()
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                MYSQL_ERROR::enum_warning_level level,
                                const char* msg,
                                MYSQL_ERROR ** cond_hdl);

  bool has_errors() { return is_grave; }

private:
  bool is_grave;
};

bool
Silence_routine_definer_errors::handle_condition(
  THD *thd,
  uint sql_errno,
  const char*,
  MYSQL_ERROR::enum_warning_level level,
  const char* msg,
  MYSQL_ERROR ** cond_hdl)
{
  *cond_hdl= NULL;
  if (level == MYSQL_ERROR::WARN_LEVEL_ERROR)
  {
    switch (sql_errno)
    {
      case ER_NONEXISTING_PROC_GRANT:
        /* Convert the error into a warning. */
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                     sql_errno, msg);
        return TRUE;
      default:
        is_grave= TRUE;
    }
  }

  return FALSE;
}


/**
  Revoke privileges for all users on a stored procedure.  Use an error handler
  that converts errors about missing grants into warnings.

  @param
    thd                         The current thread.
  @param
    db				DB of the stored procedure
  @param
    name			Name of the stored procedure

  @retval
    0           OK.
  @retval
    < 0         Error. Error message not yet sent.
*/

bool sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          bool is_proc)
{
  uint counter, revoked;
  int result;
  TABLE_LIST tables[GRANT_TABLES];
  HASH *hash= is_proc ? &proc_priv_hash : &func_priv_hash;
  Silence_routine_definer_errors error_handler;
  bool save_binlog_row_based;
  DBUG_ENTER("sp_revoke_privileges");

  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  /* Be sure to pop this before exiting this scope! */
  thd->push_internal_handler(&error_handler);

  mysql_rwlock_wrlock(&LOCK_grant);
  mysql_mutex_lock(&acl_cache->lock);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->is_current_stmt_binlog_format_row()))
    thd->clear_current_stmt_binlog_format_row();

  /* Remove procedure access */
  do
  {
    for (counter= 0, revoked= 0 ; counter < hash->records ; )
    {
      GRANT_NAME *grant_proc= (GRANT_NAME*) my_hash_element(hash, counter);
      if (!my_strcasecmp(&my_charset_utf8_bin, grant_proc->db, sp_db) &&
	  !my_strcasecmp(system_charset_info, grant_proc->tname, sp_name))
      {
        LEX_USER lex_user;
	lex_user.user.str= grant_proc->user;
	lex_user.user.length= strlen(grant_proc->user);
	lex_user.host.str= grant_proc->host.hostname ?
	  grant_proc->host.hostname : (char*)"";
	lex_user.host.length= grant_proc->host.hostname ?
	  strlen(grant_proc->host.hostname) : 0;

	if (replace_routine_table(thd,grant_proc,tables[4].table,lex_user,
				  grant_proc->db, grant_proc->tname,
                                  is_proc, ~(ulong)0, 1) == 0)
	{
	  revoked= 1;
	  continue;
	}
      }
      counter++;
    }
  } while (revoked);

  mysql_mutex_unlock(&acl_cache->lock);
  mysql_rwlock_unlock(&LOCK_grant);

  thd->pop_internal_handler();
  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->is_current_stmt_binlog_format_row());
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(error_handler.has_errors());
}


/**
  Grant EXECUTE,ALTER privilege for a stored procedure

  @param thd The current thread.
  @param sp_db
  @param sp_name
  @param is_proc

  @return
    @retval FALSE Success
    @retval TRUE An error occured. Error message not yet sent.
*/

bool sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         bool is_proc)
{
  Security_context *sctx= thd->security_ctx;
  LEX_USER *combo;
  TABLE_LIST tables[1];
  List<LEX_USER> user_list;
  bool result;
  ACL_USER *au;
  char passwd_buff[SCRAMBLED_PASSWORD_CHAR_LENGTH+1];
  Dummy_error_handler error_handler;
  DBUG_ENTER("sp_grant_privileges");

  if (!(combo=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
    DBUG_RETURN(TRUE);

  combo->user.str= (char *) sctx->priv_user;

  mysql_mutex_lock(&acl_cache->lock);

 if ((au= find_acl_user(combo->host.str= (char *) sctx->priv_host,
                        combo->user.str, FALSE)))
    goto found_acl;

  mysql_mutex_unlock(&acl_cache->lock);
  DBUG_RETURN(TRUE);

 found_acl:
  mysql_mutex_unlock(&acl_cache->lock);

  bzero((char*)tables, sizeof(TABLE_LIST));
  user_list.empty();

  tables->db= (char*)sp_db;
  tables->table_name= tables->alias= (char*)sp_name;

  thd->make_lex_string(&combo->user,
                       combo->user.str, strlen(combo->user.str), 0);
  thd->make_lex_string(&combo->host,
                       combo->host.str, strlen(combo->host.str), 0);

  combo->password= empty_lex_str;
  combo->plugin= empty_lex_str;
  combo->auth= empty_lex_str;

  if(au)
  {
    if (au->salt_len)
    {
      if (au->salt_len == SCRAMBLE_LENGTH)
      {
        make_password_from_salt(passwd_buff, au->salt);
        combo->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH;
      }
      else if (au->salt_len == SCRAMBLE_LENGTH_323)
      {
        make_password_from_salt_323(passwd_buff, (ulong *) au->salt);
        combo->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH_323;
      }
      else
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_PASSWD_LENGTH,
                            ER(ER_PASSWD_LENGTH), SCRAMBLED_PASSWORD_CHAR_LENGTH);
        return TRUE;
      }
      combo->password.str= passwd_buff;
    }

    if (au->plugin.str != native_password_plugin_name.str &&
        au->plugin.str != old_password_plugin_name.str)
    {
      combo->plugin= au->plugin;
      combo->auth= au->auth_string;
    }
  }

  if (user_list.push_back(combo))
    DBUG_RETURN(TRUE);

  thd->lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
  thd->lex->ssl_cipher= thd->lex->x509_subject= thd->lex->x509_issuer= 0;
  bzero((char*) &thd->lex->mqh, sizeof(thd->lex->mqh));

  /*
    Only care about whether the operation failed or succeeded
    as all errors will be handled later.
  */
  thd->push_internal_handler(&error_handler);
  result= mysql_routine_grant(thd, tables, is_proc, user_list,
                              DEFAULT_CREATE_PROC_ACLS, FALSE, FALSE);
  thd->pop_internal_handler();
  DBUG_RETURN(result);
}


/*****************************************************************************
  Instantiate used templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List_iterator<LEX_COLUMN>;
template class List_iterator<LEX_USER>;
template class List<LEX_COLUMN>;
template class List<LEX_USER>;
#endif

/**
  Validate if a user can proxy as another user

  @thd                     current thread
  @param user              the logged in user (proxy user)
  @param authenticated_as  the effective user a plugin is trying to 
                           impersonate as (proxied user)
  @return                  proxy user definition
    @retval NULL           proxy user definition not found or not applicable
    @retval non-null       the proxy user data
*/

static ACL_PROXY_USER *
acl_find_proxy_user(const char *user, const char *host, const char *ip, 
                    const char *authenticated_as, bool *proxy_used)
{
  uint i;
  /* if the proxied and proxy user are the same return OK */
  DBUG_ENTER("acl_find_proxy_user");
  DBUG_PRINT("info", ("user=%s host=%s ip=%s authenticated_as=%s",
                      user, host, ip, authenticated_as));

  if (!strcmp(authenticated_as, user))
  {
    DBUG_PRINT ("info", ("user is the same as authenticated_as"));
    DBUG_RETURN (NULL);
  }

  *proxy_used= TRUE; 
  for (i=0; i < acl_proxy_users.elements; i++)
  {
    ACL_PROXY_USER *proxy= dynamic_element(&acl_proxy_users, i, 
                                           ACL_PROXY_USER *);
    if (proxy->matches(host, user, ip, authenticated_as))
      DBUG_RETURN(proxy);
  }

  DBUG_RETURN(NULL);
}


bool
acl_check_proxy_grant_access(THD *thd, const char *host, const char *user,
                             bool with_grant)
{
  DBUG_ENTER("acl_check_proxy_grant_access");
  DBUG_PRINT("info", ("user=%s host=%s with_grant=%d", user, host, 
                      (int) with_grant));
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(1);
  }

  /* replication slave thread can do anything */
  if (thd->slave_thread)
  {
    DBUG_PRINT("info", ("replication slave"));
    DBUG_RETURN(FALSE);
  }

  /*
    one can grant proxy for self to others.
    Security context in THD contains two pairs of (user,host):
    1. (user,host) pair referring to inbound connection.
    2. (priv_user,priv_host) pair obtained from mysql.user table after doing
        authnetication of incoming connection.
    Privileges should be checked wrt (priv_user, priv_host) tuple, because
    (user,host) pair obtained from inbound connection may have different
    values than what is actually stored in mysql.user table and while granting
    or revoking proxy privilege, user is expected to provide entries mentioned
    in mysql.user table.
  */
  if (!strcmp(thd->security_ctx->priv_user, user) &&
      !my_strcasecmp(system_charset_info, host,
                     thd->security_ctx->priv_host))
  {
    DBUG_PRINT("info", ("strcmp (%s, %s) my_casestrcmp (%s, %s) equal", 
                        thd->security_ctx->priv_user, user,
                        host, thd->security_ctx->priv_host));
    DBUG_RETURN(FALSE);
  }

  mysql_mutex_lock(&acl_cache->lock);

  /* check for matching WITH PROXY rights */
  for (uint i=0; i < acl_proxy_users.elements; i++)
  {
    ACL_PROXY_USER *proxy= dynamic_element(&acl_proxy_users, i, 
                                           ACL_PROXY_USER *);
    DEBUG_SYNC(thd, "before_proxy_matches");
    if (proxy->matches(thd->security_ctx->get_host()->ptr(),
                       thd->security_ctx->user,
                       thd->security_ctx->get_ip()->ptr(),
                       user) &&
        proxy->get_with_grant())
    {
      DBUG_PRINT("info", ("found"));
      mysql_mutex_unlock(&acl_cache->lock);
      DBUG_RETURN(FALSE);
    }
  }

  mysql_mutex_unlock(&acl_cache->lock);
  my_error(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, MYF(0),
           thd->security_ctx->user,
           thd->security_ctx->host_or_ip);
  DBUG_RETURN(TRUE);
}


static bool
show_proxy_grants(THD *thd, LEX_USER *user, char *buff, size_t buffsize)
{
  Protocol *protocol= thd->protocol;
  int error= 0;

  for (uint i=0; i < acl_proxy_users.elements; i++)
  {
    ACL_PROXY_USER *proxy= dynamic_element(&acl_proxy_users, i,
                                           ACL_PROXY_USER *);
    if (proxy->granted_on(user->host.str, user->user.str))
    {
      String global(buff, buffsize, system_charset_info);
      global.length(0);
      proxy->print_grant(&global);
      protocol->prepare_for_resend();
      protocol->store(global.ptr(), global.length(), global.charset());
      if (protocol->write())
      {
        error= -1;
        break;
      }
    }
  }
  return error;
}


#endif /*NO_EMBEDDED_ACCESS_CHECKS */


int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr)
{
  reg3 int flag;
  DBUG_ENTER("wild_case_compare");
  DBUG_PRINT("enter",("str: '%s'  wildstr: '%s'",str,wildstr));
  while (*wildstr)
  {
    while (*wildstr && *wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == wild_prefix && wildstr[1])
	wildstr++;
      if (my_toupper(cs, *wildstr++) !=
          my_toupper(cs, *str++)) DBUG_RETURN(1);
    }
    if (! *wildstr ) DBUG_RETURN (*str != 0);
    if (*wildstr++ == wild_one)
    {
      if (! *str++) DBUG_RETURN (1);	/* One char; skip */
    }
    else
    {						/* Found '*' */
      if (!*wildstr) DBUG_RETURN(0);		/* '*' as last char: OK */
      flag=(*wildstr != wild_many && *wildstr != wild_one);
      do
      {
	if (flag)
	{
	  char cmp;
	  if ((cmp= *wildstr) == wild_prefix && wildstr[1])
	    cmp=wildstr[1];
	  cmp=my_toupper(cs, cmp);
	  while (*str && my_toupper(cs, *str) != cmp)
	    str++;
	  if (!*str) DBUG_RETURN (1);
	}
	if (wild_case_compare(cs, str,wildstr) == 0) DBUG_RETURN (0);
      } while (*str++);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN (*str != '\0');
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
static bool update_schema_privilege(THD *thd, TABLE *table, char *buff,
                                    const char* db, const char* t_name,
                                    const char* column, uint col_length,
                                    const char *priv, uint priv_length,
                                    const char* is_grantable)
{
  int i= 2;
  CHARSET_INFO *cs= system_charset_info;
  restore_record(table, s->default_values);
  table->field[0]->store(buff, (uint) strlen(buff), cs);
  table->field[1]->store(STRING_WITH_LEN("def"), cs);
  if (db)
    table->field[i++]->store(db, (uint) strlen(db), cs);
  if (t_name)
    table->field[i++]->store(t_name, (uint) strlen(t_name), cs);
  if (column)
    table->field[i++]->store(column, col_length, cs);
  table->field[i++]->store(priv, priv_length, cs);
  table->field[i]->store(is_grantable, strlen(is_grantable), cs);
  return schema_table_store_record(thd, table);
}
#endif


int fill_schema_user_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint counter;
  ACL_USER *acl_user;
  ulong want_access;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",
                                      NULL, NULL, 1, 1);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_user_privileges");

  if (!initialized)
    DBUG_RETURN(0);
  mysql_mutex_lock(&acl_cache->lock);

  for (counter=0 ; counter < acl_users.elements ; counter++)
  {
    const char *user,*host, *is_grantable="YES";
    acl_user=dynamic_element(&acl_users,counter,ACL_USER*);
    if (!(user=acl_user->user))
      user= "";
    if (!(host=acl_user->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;
      
    want_access= acl_user->access;
    if (!(want_access & GRANT_ACL))
      is_grantable= "NO";

    strxmov(buff,"'",user,"'@'",host,"'",NullS);
    if (!(want_access & ~GRANT_ACL))
    {
      if (update_schema_privilege(thd, table, buff, 0, 0, 0, 0,
                                  STRING_WITH_LEN("USAGE"), is_grantable))
      {
        error= 1;
        goto err;
      }
    }
    else
    {
      uint priv_id;
      ulong j,test_access= want_access & ~GRANT_ACL;
      for (priv_id=0, j = SELECT_ACL;j <= GLOBAL_ACLS; priv_id++,j <<= 1)
      {
	if (test_access & j)
        {
          if (update_schema_privilege(thd, table, buff, 0, 0, 0, 0, 
                                      command_array[priv_id],
                                      command_lengths[priv_id], is_grantable))
          {
            error= 1;
            goto err;
          }
        }
      }
    }
  }
err:
  mysql_mutex_unlock(&acl_cache->lock);

  DBUG_RETURN(error);
#else
  return(0);
#endif
}


int fill_schema_schema_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint counter;
  ACL_DB *acl_db;
  ulong want_access;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",
                                      NULL, NULL, 1, 1);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_schema_privileges");

  if (!initialized)
    DBUG_RETURN(0);
  mysql_mutex_lock(&acl_cache->lock);

  for (counter=0 ; counter < acl_dbs.elements ; counter++)
  {
    const char *user, *host, *is_grantable="YES";

    acl_db=dynamic_element(&acl_dbs,counter,ACL_DB*);
    if (!(user=acl_db->user))
      user= "";
    if (!(host=acl_db->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;

    want_access=acl_db->access;
    if (want_access)
    {
      if (!(want_access & GRANT_ACL))
      {
        is_grantable= "NO";
      }
      strxmov(buff,"'",user,"'@'",host,"'",NullS);
      if (!(want_access & ~GRANT_ACL))
      {
        if (update_schema_privilege(thd, table, buff, acl_db->db, 0, 0,
                                    0, STRING_WITH_LEN("USAGE"), is_grantable))
        {
          error= 1;
          goto err;
        }
      }
      else
      {
        int cnt;
        ulong j,test_access= want_access & ~GRANT_ACL;
        for (cnt=0, j = SELECT_ACL; j <= DB_ACLS; cnt++,j <<= 1)
          if (test_access & j)
          {
            if (update_schema_privilege(thd, table, buff, acl_db->db, 0, 0, 0,
                                        command_array[cnt], command_lengths[cnt],
                                        is_grantable))
            {
              error= 1;
              goto err;
            }
          }
      }
    }
  }
err:
  mysql_mutex_unlock(&acl_cache->lock);

  DBUG_RETURN(error);
#else
  return (0);
#endif
}


int fill_schema_table_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint index;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",
                                      NULL, NULL, 1, 1);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_table_privileges");

  mysql_rwlock_rdlock(&LOCK_grant);

  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *host, *is_grantable= "YES";
    GRANT_TABLE *grant_table= (GRANT_TABLE*) my_hash_element(&column_priv_hash,
							  index);
    if (!(user=grant_table->user))
      user= "";
    if (!(host= grant_table->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;

    ulong table_access= grant_table->privs;
    if (table_access)
    {
      ulong test_access= table_access & ~GRANT_ACL;
      /*
        We should skip 'usage' privilege on table if
        we have any privileges on column(s) of this table
      */
      if (!test_access && grant_table->cols)
        continue;
      if (!(table_access & GRANT_ACL))
        is_grantable= "NO";

      strxmov(buff, "'", user, "'@'", host, "'", NullS);
      if (!test_access)
      {
        if (update_schema_privilege(thd, table, buff, grant_table->db,
                                    grant_table->tname, 0, 0,
                                    STRING_WITH_LEN("USAGE"), is_grantable))
        {
          error= 1;
          goto err;
        }
      }
      else
      {
        ulong j;
        int cnt;
        for (cnt= 0, j= SELECT_ACL; j <= TABLE_ACLS; cnt++, j<<= 1)
        {
          if (test_access & j)
          {
            if (update_schema_privilege(thd, table, buff, grant_table->db,
                                        grant_table->tname, 0, 0,
                                        command_array[cnt],
                                        command_lengths[cnt], is_grantable))
            {
              error= 1;
              goto err;
            }
          }
        }
      }
    }   
  }
err:
  mysql_rwlock_unlock(&LOCK_grant);

  DBUG_RETURN(error);
#else
  return (0);
#endif
}


int fill_schema_column_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint index;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",
                                      NULL, NULL, 1, 1);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_table_privileges");

  mysql_rwlock_rdlock(&LOCK_grant);

  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *host, *is_grantable= "YES";
    GRANT_TABLE *grant_table= (GRANT_TABLE*) my_hash_element(&column_priv_hash,
							  index);
    if (!(user=grant_table->user))
      user= "";
    if (!(host= grant_table->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;

    ulong table_access= grant_table->cols;
    if (table_access != 0)
    {
      if (!(grant_table->privs & GRANT_ACL))
        is_grantable= "NO";

      ulong test_access= table_access & ~GRANT_ACL;
      strxmov(buff, "'", user, "'@'", host, "'", NullS);
      if (!test_access)
        continue;
      else
      {
        ulong j;
        int cnt;
        for (cnt= 0, j= SELECT_ACL; j <= TABLE_ACLS; cnt++, j<<= 1)
        {
          if (test_access & j)
          {
            for (uint col_index=0 ;
                 col_index < grant_table->hash_columns.records ;
                 col_index++)
            {
              GRANT_COLUMN *grant_column = (GRANT_COLUMN*)
                my_hash_element(&grant_table->hash_columns,col_index);
              if ((grant_column->rights & j) && (table_access & j))
              {
                if (update_schema_privilege(thd, table, buff, grant_table->db,
                                            grant_table->tname,
                                            grant_column->column,
                                            grant_column->key_length,
                                            command_array[cnt],
                                            command_lengths[cnt], is_grantable))
                {
                  error= 1;
                  goto err;
                }
              }
            }
          }
        }
      }
    }
  }
err:
  mysql_rwlock_unlock(&LOCK_grant);

  DBUG_RETURN(error);
#else
  return (0);
#endif
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
/*
  fill effective privileges for table

  SYNOPSIS
    fill_effective_table_privileges()
    thd     thread handler
    grant   grants table descriptor
    db      db name
    table   table name
*/

void fill_effective_table_privileges(THD *thd, GRANT_INFO *grant,
                                     const char *db, const char *table)
{
  Security_context *sctx= thd->security_ctx;
  DBUG_ENTER("fill_effective_table_privileges");
  DBUG_PRINT("enter", ("Host: '%s', Ip: '%s', User: '%s', table: `%s`.`%s`",
                       sctx->priv_host, (sctx->get_ip()->length() ?
                       sctx->get_ip()->ptr() : "(NULL)"),
                       (sctx->priv_user ? sctx->priv_user : "(NULL)"),
                       db, table));
  /* --skip-grants */
  if (!initialized)
  {
    DBUG_PRINT("info", ("skip grants"));
    grant->privilege= ~NO_ACCESS;             // everything is allowed
    DBUG_PRINT("info", ("privilege 0x%lx", grant->privilege));
    DBUG_VOID_RETURN;
  }

  /* global privileges */
  grant->privilege= sctx->master_access;

  if (!sctx->priv_user)
  {
    DBUG_PRINT("info", ("privilege 0x%lx", grant->privilege));
    DBUG_VOID_RETURN;                         // it is slave
  }

  /* db privileges */
  grant->privilege|= acl_get(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                             sctx->priv_user, db, 0);

  /* table privileges */
  mysql_rwlock_rdlock(&LOCK_grant);
  if (grant->version != grant_version)
  {
    grant->grant_table=
      table_hash_search(sctx->get_host()->ptr(), sctx->get_ip()->ptr(), db,
			sctx->priv_user,
			table, 0);              /* purecov: inspected */
    grant->version= grant_version;              /* purecov: inspected */
  }
  if (grant->grant_table != 0)
  {
    grant->privilege|= grant->grant_table->privs;
  }
  mysql_rwlock_unlock(&LOCK_grant);

  DBUG_PRINT("info", ("privilege 0x%lx", grant->privilege));
  DBUG_VOID_RETURN;
}

#else /* NO_EMBEDDED_ACCESS_CHECKS */

/****************************************************************************
 Dummy wrappers when we don't have any access checks
****************************************************************************/

bool check_routine_level_acl(THD *thd, const char *db, const char *name,
                             bool is_proc)
{
  return FALSE;
}

#endif

struct ACL_internal_schema_registry_entry
{
  const LEX_STRING *m_name;
  const ACL_internal_schema_access *m_access;
};

/**
  Internal schema registered.
  Currently, this is only:
  - performance_schema
  - information_schema,
  This can be reused later for:
  - mysql
*/
static ACL_internal_schema_registry_entry registry_array[2];
static uint m_registry_array_size= 0;

/**
  Add an internal schema to the registry.
  @param name the schema name
  @param access the schema ACL specific rules
*/
void ACL_internal_schema_registry::register_schema
  (const LEX_STRING *name, const ACL_internal_schema_access *access)
{
  DBUG_ASSERT(m_registry_array_size < array_elements(registry_array));

  /* Not thread safe, and does not need to be. */
  registry_array[m_registry_array_size].m_name= name;
  registry_array[m_registry_array_size].m_access= access;
  m_registry_array_size++;
}

/**
  Search per internal schema ACL by name.
  @param name a schema name
  @return per schema rules, or NULL
*/
const ACL_internal_schema_access *
ACL_internal_schema_registry::lookup(const char *name)
{
  DBUG_ASSERT(name != NULL);

  uint i;

  for (i= 0; i<m_registry_array_size; i++)
  {
    if (my_strcasecmp(system_charset_info, registry_array[i].m_name->str,
                      name) == 0)
      return registry_array[i].m_access;
  }
  return NULL;
}

/**
  Get a cached internal schema access.
  @param grant_internal_info the cache
  @param schema_name the name of the internal schema
*/
const ACL_internal_schema_access *
get_cached_schema_access(GRANT_INTERNAL_INFO *grant_internal_info,
                         const char *schema_name)
{
  if (grant_internal_info)
  {
    if (! grant_internal_info->m_schema_lookup_done)
    {
      grant_internal_info->m_schema_access=
        ACL_internal_schema_registry::lookup(schema_name);
      grant_internal_info->m_schema_lookup_done= TRUE;
    }
    return grant_internal_info->m_schema_access;
  }
  return ACL_internal_schema_registry::lookup(schema_name);
}

/**
  Get a cached internal table access.
  @param grant_internal_info the cache
  @param schema_name the name of the internal schema
  @param table_name the name of the internal table
*/
const ACL_internal_table_access *
get_cached_table_access(GRANT_INTERNAL_INFO *grant_internal_info,
                        const char *schema_name,
                        const char *table_name)
{
  DBUG_ASSERT(grant_internal_info);
  if (! grant_internal_info->m_table_lookup_done)
  {
    const ACL_internal_schema_access *schema_access;
    schema_access= get_cached_schema_access(grant_internal_info, schema_name);
    if (schema_access)
      grant_internal_info->m_table_access= schema_access->lookup(table_name);
    grant_internal_info->m_table_lookup_done= TRUE;
  }
  return grant_internal_info->m_table_access;
}


/****************************************************************************
   AUTHENTICATION CODE
   including initial connect handshake, invoking appropriate plugins,
   client-server plugin negotiation, COM_CHANGE_USER, and native
   MySQL authentication plugins.
****************************************************************************/

/* few defines to have less ifdef's in the code below */
#ifdef EMBEDDED_LIBRARY
#undef HAVE_OPENSSL
#ifdef NO_EMBEDDED_ACCESS_CHECKS
#define initialized 0
#endif
#endif
#ifndef HAVE_OPENSSL
#define ssl_acceptor_fd 0
#define sslaccept(A,B,C) 1
#endif


class Thd_charset_adapter
{
  THD *thd;
public:
  Thd_charset_adapter(THD *thd_arg) : thd (thd_arg) {} 
  bool init_client_charset(uint cs_number)
  {
    if (thd_init_client_charset(thd, cs_number))
      return true;
    thd->update_charset();
    return thd->is_error();
  }

  CHARSET_INFO *charset() { return thd->charset(); }
};


/**
  The internal version of what plugins know as MYSQL_PLUGIN_VIO,
  basically the context of the authentication session
*/
struct MPVIO_EXT :public MYSQL_PLUGIN_VIO
{
  MYSQL_SERVER_AUTH_INFO auth_info;
  const ACL_USER *acl_user;
  plugin_ref plugin;        ///< what plugin we're under
  LEX_STRING db;            ///< db name from the handshake packet
  /** when restarting a plugin this caches the last client reply */
  struct {
    char *plugin, *pkt;     ///< pointers into NET::buff
    uint pkt_len;
  } cached_client_reply;
  /** this caches the first plugin packet for restart request on the client */
  struct {
    char *pkt;
    uint pkt_len;
  } cached_server_packet;
  int packets_read, packets_written; ///< counters for send/received packets
  uint connect_errors;      ///< if there were connect errors for this host
  /** when plugin returns a failure this tells us what really happened */
  enum { SUCCESS, FAILURE, RESTART } status;

  /* encapsulation members */
  ulong client_capabilities;
  char *scramble;
  MEM_ROOT *mem_root;
  struct  rand_struct *rand;
  my_thread_id  thread_id;
  uint      *server_status;
  NET *net;
  ulong max_client_packet_length;
  char *ip;
  char *host;
  Thd_charset_adapter *charset_adapter;
  LEX_STRING acl_user_plugin;
  bool can_authenticate()
  {
    return (acl_user && acl_user->can_authenticate);
  }
};

/**
  a helper function to report an access denied error in all the proper places
*/
static void login_failed_error(MPVIO_EXT *mpvio, int passwd_used)
{
  THD *thd= current_thd;
  if (passwd_used == 2)
  {
    my_error(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, MYF(0),
             mpvio->auth_info.user_name,
             mpvio->auth_info.host_or_ip);
    general_log_print(thd, COM_CONNECT, ER(ER_ACCESS_DENIED_NO_PASSWORD_ERROR),
                      mpvio->auth_info.user_name,
                      mpvio->auth_info.host_or_ip);
    /* 
      Log access denied messages to the error log when log-warnings = 2
      so that the overhead of the general query log is not required to track 
      failed connections.
    */
    if (global_system_variables.log_warnings > 1)
    {
      sql_print_warning(ER(ER_ACCESS_DENIED_NO_PASSWORD_ERROR),
                        mpvio->auth_info.user_name,
                        mpvio->auth_info.host_or_ip);      
    }
  }
  else
  {
    my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
             mpvio->auth_info.user_name,
             mpvio->auth_info.host_or_ip,
             passwd_used ? ER(ER_YES) : ER(ER_NO));
    general_log_print(thd, COM_CONNECT, ER(ER_ACCESS_DENIED_ERROR),
                      mpvio->auth_info.user_name,
                      mpvio->auth_info.host_or_ip,
                      passwd_used ? ER(ER_YES) : ER(ER_NO));
    /* 
      Log access denied messages to the error log when log-warnings = 2
      so that the overhead of the general query log is not required to track 
      failed connections.
    */
    if (global_system_variables.log_warnings > 1)
    {
      sql_print_warning(ER(ER_ACCESS_DENIED_ERROR),
                        mpvio->auth_info.user_name,
                        mpvio->auth_info.host_or_ip,
                        passwd_used ? ER(ER_YES) : ER(ER_NO));      
    }
  }
}

/**
  sends a server handshake initialization packet, the very first packet
  after the connection was established

  Packet format:
   
    Bytes       Content
    -----       ----
    1           protocol version (always 10)
    n           server version string, \0-terminated
    4           thread id
    8           first 8 bytes of the plugin provided data (scramble)
    1           \0 byte, terminating the first part of a scramble
    2           server capabilities (two lower bytes)
    1           server character set
    2           server status
    2           server capabilities (two upper bytes)
    1           length of the scramble
    10          reserved, always 0
    n           rest of the plugin provided data (at least 12 bytes)
    1           \0 byte, terminating the second part of a scramble

  @retval 0 ok
  @retval 1 error
*/
static bool send_server_handshake_packet(MPVIO_EXT *mpvio,
                                         const char *data, uint data_len)
{
  DBUG_ASSERT(mpvio->status == MPVIO_EXT::FAILURE);
  DBUG_ASSERT(data_len <= 255);

  char *buff= (char *) my_alloca(1 + SERVER_VERSION_LENGTH + data_len + 64);
  char scramble_buf[SCRAMBLE_LENGTH];
  char *end= buff;

  DBUG_ENTER("send_server_handshake_packet");
  *end++= protocol_version;

  mpvio->client_capabilities= CLIENT_BASIC_FLAGS;

  if (opt_using_transactions)
    mpvio->client_capabilities|= CLIENT_TRANSACTIONS;

  mpvio->client_capabilities|= CAN_CLIENT_COMPRESS;

  if (ssl_acceptor_fd)
  {
    mpvio->client_capabilities|= CLIENT_SSL;
    mpvio->client_capabilities|= CLIENT_SSL_VERIFY_SERVER_CERT;
  }

  if (data_len)
  {
    mpvio->cached_server_packet.pkt= (char*) memdup_root(mpvio->mem_root, 
                                                         data, data_len);
    mpvio->cached_server_packet.pkt_len= data_len;
  }

  if (data_len < SCRAMBLE_LENGTH)
  {
    if (data_len)
    {
      /*
        the first packet *must* have at least 20 bytes of a scramble.
        if a plugin provided less, we pad it to 20 with zeros
      */
      memcpy(scramble_buf, data, data_len);
      bzero(scramble_buf + data_len, SCRAMBLE_LENGTH - data_len);
      data= scramble_buf;
    }
    else
    {
      /*
        if the default plugin does not provide the data for the scramble at
        all, we generate a scramble internally anyway, just in case the
        user account (that will be known only later) uses a
        native_password_plugin (which needs a scramble). If we don't send a
        scramble now - wasting 20 bytes in the packet -
        native_password_plugin will have to send it in a separate packet,
        adding one more round trip.
      */
      create_random_string(mpvio->scramble, SCRAMBLE_LENGTH, mpvio->rand);
      data= mpvio->scramble;
    }
    data_len= SCRAMBLE_LENGTH;
  }

  end= strnmov(end, server_version, SERVER_VERSION_LENGTH) + 1;
  int4store((uchar*) end, mpvio->thread_id);
  end+= 4;

  /*
    Old clients does not understand long scrambles, but can ignore packet
    tail: that's why first part of the scramble is placed here, and second
    part at the end of packet.
  */
  end= (char*) memcpy(end, data, SCRAMBLE_LENGTH_323);
  end+= SCRAMBLE_LENGTH_323;
  *end++= 0;
 
  int2store(end, mpvio->client_capabilities);
  /* write server characteristics: up to 16 bytes allowed */
  end[2]= (char) default_charset_info->number;
  int2store(end + 3, mpvio->server_status[0]);
  int2store(end + 5, mpvio->client_capabilities >> 16);
  end[7]= data_len;
  DBUG_EXECUTE_IF("poison_srv_handshake_scramble_len", end[7]= -100;);
  bzero(end + 8, 10);
  end+= 18;
  /* write scramble tail */
  end= (char*) memcpy(end, data + SCRAMBLE_LENGTH_323,
                      data_len - SCRAMBLE_LENGTH_323);
  end+= data_len - SCRAMBLE_LENGTH_323;
  end= strmake(end, plugin_name(mpvio->plugin)->str,
                    plugin_name(mpvio->plugin)->length);

  int res= my_net_write(mpvio->net, (uchar*) buff, (size_t) (end - buff + 1)) ||
           net_flush(mpvio->net);
  my_afree(buff);
  DBUG_RETURN (res);
}

static bool secure_auth(MPVIO_EXT *mpvio)
{
  THD *thd;
  if (!opt_secure_auth)
    return 0;
  /*
    If the server is running in secure auth mode, short scrambles are 
    forbidden. Extra juggling to report the same error as the old code.
  */

  thd= current_thd;
  if (mpvio->client_capabilities & CLIENT_PROTOCOL_41)
  {
    my_error(ER_SERVER_IS_IN_SECURE_AUTH_MODE, MYF(0),
             mpvio->auth_info.user_name,
             mpvio->auth_info.host_or_ip);
    general_log_print(thd, COM_CONNECT, ER(ER_SERVER_IS_IN_SECURE_AUTH_MODE),
                      mpvio->auth_info.user_name,
                      mpvio->auth_info.host_or_ip);
  }
  else
  {
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    general_log_print(thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
  }
  return 1;
}

/**
  sends a "change plugin" packet, requesting a client to restart authentication
  using a different authentication plugin

  Packet format:
   
    Bytes       Content
    -----       ----
    1           byte with the value 254
    n           client plugin to use, \0-terminated
    n           plugin provided data

  In a special case of switching from native_password_plugin to
  old_password_plugin, the packet contains only one - the first - byte,
  plugin name is omitted, plugin data aren't needed as the scramble was
  already sent. This one-byte packet is identical to the "use the short
  scramble" packet in the protocol before plugins were introduced.

  @retval 0 ok
  @retval 1 error
*/
static bool send_plugin_request_packet(MPVIO_EXT *mpvio,
                                       const uchar *data, uint data_len)
{
  DBUG_ASSERT(mpvio->packets_written == 1);
  DBUG_ASSERT(mpvio->packets_read == 1);
  NET *net= mpvio->net;
  static uchar switch_plugin_request_buf[]= { 254 };

  DBUG_ENTER("send_plugin_request_packet");
  mpvio->status= MPVIO_EXT::FAILURE; // the status is no longer RESTART

  const char *client_auth_plugin=
    ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin;

  DBUG_ASSERT(client_auth_plugin);

  /*
    we send an old "short 4.0 scramble request", if we need to request a
    client to use 4.0 auth plugin (short scramble) and the scramble was
    already sent to the client

    below, cached_client_reply.plugin is the plugin name that client has used,
    client_auth_plugin is derived from mysql.user table, for the given
    user account, it's the plugin that the client need to use to login.
  */
  bool switch_from_long_to_short_scramble=
    native_password_plugin_name.str == mpvio->cached_client_reply.plugin &&
    client_auth_plugin == old_password_plugin_name.str;

  if (switch_from_long_to_short_scramble)
    DBUG_RETURN (secure_auth(mpvio) ||
                 my_net_write(net, switch_plugin_request_buf, 1) ||
                 net_flush(net));

  /*
    We never request a client to switch from a short to long scramble.
    Plugin-aware clients can do that, but traditionally it meant to
    ask an old 4.0 client to use the new 4.1 authentication protocol.
  */
  bool switch_from_short_to_long_scramble=
    old_password_plugin_name.str == mpvio->cached_client_reply.plugin && 
    client_auth_plugin == native_password_plugin_name.str;

  if (switch_from_short_to_long_scramble)
  {
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    general_log_print(current_thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN (1);
  }

  /*
    If we're dealing with an older client we can't just send a change plugin
    packet to re-initiate the authentication handshake, because the client 
    won't understand it. The good thing is that we don't need to : the old client
    expects us to just check the user credentials here, which we can do by just reading
    the cached data that are placed there by parse_com_change_user_packet() 
    In this case we just do nothing and behave as if normal authentication
    should continue.
  */
  if (!(mpvio->client_capabilities & CLIENT_PLUGIN_AUTH))
  {
    DBUG_PRINT("info", ("old client sent a COM_CHANGE_USER"));
    DBUG_ASSERT(mpvio->cached_client_reply.pkt);
    /* get the status back so the read can process the cached result */
    mpvio->status= MPVIO_EXT::RESTART; 
    DBUG_RETURN(0);
  }

  DBUG_PRINT("info", ("requesting client to use the %s plugin", 
                      client_auth_plugin));
  DBUG_RETURN(net_write_command(net, switch_plugin_request_buf[0],
                                (uchar*) client_auth_plugin,
                                strlen(client_auth_plugin) + 1,
                                (uchar*) data, data_len));
}

#ifndef NO_EMBEDDED_ACCESS_CHECKS

/**
  When authentication is attempted using an unknown username a dummy user
  account with no authentication capabilites is assigned to the connection.
  This is done increase the cost of enumerating user accounts based on
  authentication protocol.
*/

ACL_USER *decoy_user(const LEX_STRING &username,
                      MEM_ROOT *mem)
{
  ACL_USER *user= (ACL_USER *) alloc_root(mem, sizeof(ACL_USER));
  user->can_authenticate= false;
  user->user= strmake_root(mem, username.str, username.length);
  user->auth_string= empty_lex_str;
  user->ssl_cipher= empty_c_string;
  user->x509_issuer= empty_c_string;
  user->x509_subject= empty_c_string;
  user->salt_len= 0;

  /*
    For now the common default account is used. Improvements might involve
    mapping a consistent hash of a username to a range of plugins.
  */
  user->plugin= *default_auth_plugin_name;
  return user;
}

/**
   Finds acl entry in user database for authentication purposes.
   
   Finds a user and copies it into mpvio. Reports an authentication
   failure if a user is not found.

   @note find_acl_user is not the same, because it doesn't take into
   account the case when user is not empty, but acl_user->user is empty

   @retval 0    found
   @retval 1    not found
*/
static bool find_mpvio_user(MPVIO_EXT *mpvio)
{
  DBUG_ENTER("find_mpvio_user");
  DBUG_PRINT("info", ("entry: %s", mpvio->auth_info.user_name));
  DBUG_ASSERT(mpvio->acl_user == 0);
  mysql_mutex_lock(&acl_cache->lock);
  for (uint i=0; i < acl_users.elements; i++)
  {
    ACL_USER *acl_user_tmp= dynamic_element(&acl_users, i, ACL_USER*);
    if ((!acl_user_tmp->user || 
         !strcmp(mpvio->auth_info.user_name, acl_user_tmp->user)) &&
        compare_hostname(&acl_user_tmp->host, mpvio->host, mpvio->ip))
    {
      mpvio->acl_user= acl_user_tmp->copy(mpvio->mem_root);
      if (acl_user_tmp->plugin.str == native_password_plugin_name.str ||
          acl_user_tmp->plugin.str == old_password_plugin_name.str)
        mpvio->acl_user_plugin= acl_user_tmp->plugin;
      else
        make_lex_string_root(mpvio->mem_root, 
                             &mpvio->acl_user_plugin, 
                             acl_user_tmp->plugin.str, 
                             acl_user_tmp->plugin.length, 0);
      break;
    }
  }
  mysql_mutex_unlock(&acl_cache->lock);

  if (!mpvio->acl_user)
  {
    /*
      Pretend the user exists; let the plugin decide how to handle
      bad credentials.
    */
    LEX_STRING usr= { mpvio->auth_info.user_name,
                      mpvio->auth_info.user_name_length };
    mpvio->acl_user= decoy_user(usr, mpvio->mem_root);
    mpvio->acl_user_plugin= mpvio->acl_user->plugin;
  }

  /* user account requires non-default plugin and the client is too old */
  if (mpvio->acl_user->plugin.str != native_password_plugin_name.str &&
      mpvio->acl_user->plugin.str != old_password_plugin_name.str &&
      !(mpvio->client_capabilities & CLIENT_PLUGIN_AUTH))
  {
    DBUG_ASSERT(my_strcasecmp(system_charset_info, mpvio->acl_user->plugin.str,
                              native_password_plugin_name.str));
    DBUG_ASSERT(my_strcasecmp(system_charset_info, mpvio->acl_user->plugin.str,
                              old_password_plugin_name.str));
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    general_log_print(current_thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN (1);
  }

  mpvio->auth_info.auth_string= mpvio->acl_user->auth_string.str;
  mpvio->auth_info.auth_string_length= 
    (unsigned long) mpvio->acl_user->auth_string.length;
  strmake(mpvio->auth_info.authenticated_as, mpvio->acl_user->user ?
          mpvio->acl_user->user : "", USERNAME_LENGTH);
  DBUG_PRINT("info", ("exit: user=%s, auth_string=%s, authenticated as=%s"
                      "plugin=%s",
                      mpvio->auth_info.user_name,
                      mpvio->auth_info.auth_string,
                      mpvio->auth_info.authenticated_as,
                      mpvio->acl_user->plugin.str));
  DBUG_RETURN(0);
}
#endif

/* the packet format is described in send_change_user_packet() */
static bool parse_com_change_user_packet(MPVIO_EXT *mpvio, uint packet_length)
{
  NET *net= mpvio->net;

  char *user= (char*) net->read_pos;
  char *end= user + packet_length;
  /* Safe because there is always a trailing \0 at the end of the packet */
  char *passwd= strend(user) + 1;
  uint user_len= passwd - user - 1;
  char *db= passwd;
  char db_buff[NAME_LEN + 1];                 // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];	      // buffer to store user in utf8
  uint dummy_errors;

  DBUG_ENTER ("parse_com_change_user_packet");
  if (passwd >= end)
  {
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    DBUG_RETURN (1);
  }

  /*
    Old clients send null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.

    This strlen() can't be easily deleted without changing protocol.

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  uint passwd_len= (mpvio->client_capabilities & CLIENT_SECURE_CONNECTION ?
                    (uchar) (*passwd++) : strlen(passwd));

  db+= passwd_len + 1;
  /*
    Database name is always NUL-terminated, so in case of empty database
    the packet must contain at least the trailing '\0'.
  */
  if (db >= end)
  {
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    DBUG_RETURN (1);
  }

  uint db_len= strlen(db);

  char *ptr= db + db_len + 1;

  if (ptr + 1 < end)
  {
    if (mpvio->charset_adapter->init_client_charset(uint2korr(ptr)))
      DBUG_RETURN(1);
  }


  /* Convert database and user names to utf8 */
  db_len= copy_and_convert(db_buff, sizeof(db_buff) - 1, system_charset_info,
                           db, db_len, mpvio->charset_adapter->charset(),
                           &dummy_errors);
  db_buff[db_len]= 0;

  user_len= copy_and_convert(user_buff, sizeof(user_buff) - 1,
                                  system_charset_info, user, user_len,
                                  mpvio->charset_adapter->charset(),
                                  &dummy_errors);
  user_buff[user_len]= 0;

  /* we should not free mpvio->user here: it's saved by dispatch_command() */
  if (!(mpvio->auth_info.user_name= my_strndup(user_buff, user_len, MYF(MY_WME))))
    return 1;
  mpvio->auth_info.user_name_length= user_len;

  if (make_lex_string_root(mpvio->mem_root, 
                           &mpvio->db, db_buff, db_len, 0) == 0)
    DBUG_RETURN(1); /* The error is set by make_lex_string(). */

  if (!initialized)
  {
    // if mysqld's been started with --skip-grant-tables option
    strmake(mpvio->auth_info.authenticated_as, 
            mpvio->auth_info.user_name, USERNAME_LENGTH);

    mpvio->status= MPVIO_EXT::SUCCESS;
    DBUG_RETURN(0);
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (find_mpvio_user(mpvio))
  {
    DBUG_RETURN(1);
  }

  char *client_plugin;
  if (mpvio->client_capabilities & CLIENT_PLUGIN_AUTH)
  {
    client_plugin= ptr + 2;
    if (client_plugin >= end)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      DBUG_RETURN(1);
    }
  }
  else
  {
    if (mpvio->client_capabilities & CLIENT_SECURE_CONNECTION)
      client_plugin= native_password_plugin_name.str;
    else
    {
      client_plugin=  old_password_plugin_name.str;
      /*
        For a passwordless accounts we use native_password_plugin.
        But when an old 4.0 client connects to it, we change it to
        old_password_plugin, otherwise MySQL will think that server 
        and client plugins don't match.
      */
      if (mpvio->acl_user->salt_len == 0)
        mpvio->acl_user_plugin= old_password_plugin_name;
    }
  }

  DBUG_PRINT("info", ("client_plugin=%s, restart", client_plugin));
  /* 
    Remember the data part of the packet, to present it to plugin in 
    read_packet() 
  */
  mpvio->cached_client_reply.pkt= passwd;
  mpvio->cached_client_reply.pkt_len= passwd_len;
  mpvio->cached_client_reply.plugin= client_plugin;
  mpvio->status= MPVIO_EXT::RESTART;
#endif

  DBUG_RETURN (0);
}

#ifndef EMBEDDED_LIBRARY

/** Get a string according to the protocol of the underlying buffer. */
typedef char * (*get_proto_string_func_t) (char **, size_t *, size_t *);

/**
  Get a string formatted according to the 4.1 version of the MySQL protocol.

  @param buffer[in, out]    Pointer to the user-supplied buffer to be scanned.
  @param max_bytes_available[in, out]  Limit the bytes to scan.
  @param string_length[out] The number of characters scanned not including
                            the null character.

  @remark Strings are always null character terminated in this version of the
          protocol.

  @remark The string_length does not include the terminating null character.
          However, after the call, the buffer is increased by string_length+1
          bytes, beyond the null character if there still available bytes to
          scan.

  @return pointer to beginning of the string scanned.
    @retval NULL The buffer content is malformed
*/

static
char *get_41_protocol_string(char **buffer,
                             size_t *max_bytes_available,
                             size_t *string_length)
{
  char *str= (char *)memchr(*buffer, '\0', *max_bytes_available);

  if (str == NULL)
    return NULL;

  *string_length= (size_t)(str - *buffer);
  *max_bytes_available-= *string_length + 1;
  str= *buffer;
  *buffer += *string_length + 1;

  return str;
}


/**
  Get a string formatted according to the 4.0 version of the MySQL protocol.

  @param buffer[in, out]    Pointer to the user-supplied buffer to be scanned.
  @param max_bytes_available[in, out]  Limit the bytes to scan.
  @param string_length[out] The number of characters scanned not including
                            the null character.

  @remark If there are not enough bytes left after the current position of
          the buffer to satisfy the current string, the string is considered
          to be empty and a pointer to empty_c_string is returned.

  @remark A string at the end of the packet is not null terminated.

  @return Pointer to beginning of the string scanned, or a pointer to a empty
          string.
*/
static
char *get_40_protocol_string(char **buffer,
                             size_t *max_bytes_available,
                             size_t *string_length)
{
  char *str;
  size_t len;

  /* No bytes to scan left, treat string as empty. */
  if ((*max_bytes_available) == 0)
  {
    *string_length= 0;
    return empty_c_string;
  }

  str= (char *) memchr(*buffer, '\0', *max_bytes_available);

  /*
    If the string was not null terminated by the client,
    the remainder of the packet is the string. Otherwise,
    advance the buffer past the end of the null terminated
    string.
  */
  if (str == NULL)
    len= *string_length= *max_bytes_available;
  else
    len= (*string_length= (size_t)(str - *buffer)) + 1;

  str= *buffer;
  *buffer+= len;
  *max_bytes_available-= len;

  return str;
}

/**
  Get a length encoded string from a user-supplied buffer.

  @param buffer[in, out] The buffer to scan; updates position after scan.
  @param max_bytes_available[in, out] Limit the number of bytes to scan
  @param string_length[out] Number of characters scanned

  @remark In case the length is zero, then the total size of the string is
    considered to be 1 byte; the size byte.

  @return pointer to first byte after the header in buffer.
    @retval NULL The buffer content is malformed
*/

static
char *get_length_encoded_string(char **buffer,
                                size_t *max_bytes_available,
                                size_t *string_length)
{
  if (*max_bytes_available == 0)
    return NULL;

  /* Do double cast to prevent overflow from signed / unsigned conversion */
  size_t str_len= (size_t)(unsigned char)**buffer;

  /*
    If the length encoded string has the length 0
    the total size of the string is only one byte long (the size byte)
  */
  if (str_len == 0)
  {
    ++*buffer;
    *string_length= 0;
    /*
      Return a pointer to the 0 character so the return value will be
      an empty string.
    */
    return *buffer-1;
  }

  if (str_len >= *max_bytes_available)
    return NULL;

  char *str= *buffer+1;
  *string_length= str_len;
  *max_bytes_available-= *string_length + 1;
  *buffer+= *string_length + 1;
  return str;
}
#endif


/* the packet format is described in send_client_reply_packet() */
static ulong parse_client_handshake_packet(MPVIO_EXT *mpvio,
                                           uchar **buff, ulong pkt_len)
{
#ifndef EMBEDDED_LIBRARY
  NET *net= mpvio->net;
  char *end;
  bool packet_has_required_size= false;
  DBUG_ASSERT(mpvio->status == MPVIO_EXT::FAILURE);


  uint charset_code= 0;
  end= (char *)net->read_pos;
  /*
    In order to safely scan a head for '\0' string terminators
    we must keep track of how many bytes remain in the allocated
    buffer or we might read past the end of the buffer.
  */
  size_t bytes_remaining_in_packet= pkt_len;
  
  DBUG_EXECUTE_IF("host_error_packet_length",
                  {
                    bytes_remaining_in_packet= 0;
                  };);

  /*
    Peek ahead on the client capability packet and determine which version of
    the protocol should be used.
  */
  if (bytes_remaining_in_packet < 2)
    return packet_error;
    
  mpvio->client_capabilities= uint2korr(end);

  /*
    JConnector only sends server capabilities before starting SSL
    negotiation.  The below code is patch for this.
  */
  if (bytes_remaining_in_packet == 4 &&
      mpvio->client_capabilities & CLIENT_SSL)
  {
    mpvio->client_capabilities= uint4korr(end);
    mpvio->max_client_packet_length= 0xfffff;
    charset_code= global_system_variables.character_set_client->number;
    sql_print_warning("Client failed to provide its character set. "
                      "'%s' will be used as client character set.",
                      global_system_variables.character_set_client->csname);
    if (mpvio->charset_adapter->init_client_charset(charset_code))
      return packet_error;
    goto skip_to_ssl;
  }
  
  if (mpvio->client_capabilities & CLIENT_PROTOCOL_41)
    packet_has_required_size= bytes_remaining_in_packet >= 
      AUTH_PACKET_HEADER_SIZE_PROTO_41;
  else
    packet_has_required_size= bytes_remaining_in_packet >=
      AUTH_PACKET_HEADER_SIZE_PROTO_40;
  
  if (!packet_has_required_size)
    return packet_error;
  
  if (mpvio->client_capabilities & CLIENT_PROTOCOL_41)
  {
    mpvio->client_capabilities= uint4korr(end);
    mpvio->max_client_packet_length= uint4korr(end + 4);
    charset_code= (uint)(uchar)*(end + 8);
    /*
      Skip 23 remaining filler bytes which have no particular meaning.
    */
    end+= AUTH_PACKET_HEADER_SIZE_PROTO_41;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_PROTO_41;
  }
  else
  {
    mpvio->client_capabilities= uint2korr(end);
    mpvio->max_client_packet_length= uint3korr(end + 2);
    end+= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    /**
      Old clients didn't have their own charset. Instead the assumption
      was that they used what ever the server used.
    */
    charset_code= global_system_variables.character_set_client->number;
    sql_print_warning("Client failed to provide its character set. "
                      "'%s' will be used as client character set.",
                      global_system_variables.character_set_client->csname);
  }
  DBUG_EXECUTE_IF("host_error_charset",
                  {
                    return packet_error;
                  };);


  DBUG_PRINT("info", ("client_character_set: %u", charset_code));
  if (mpvio->charset_adapter->init_client_charset(charset_code))
    return packet_error;

skip_to_ssl:
#if defined(HAVE_OPENSSL)
  DBUG_PRINT("info", ("client capabilities: %lu", mpvio->client_capabilities));
  
  /*
    If client requested SSL then we must stop parsing, try to switch to SSL,
    and wait for the client to send a new handshake packet.
    The client isn't expected to send any more bytes until SSL is initialized.
  */
  if (mpvio->client_capabilities & CLIENT_SSL)
  {
    unsigned long errptr;

    /* Do the SSL layering. */
    if (!ssl_acceptor_fd)
      return packet_error;

    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslaccept(ssl_acceptor_fd, net->vio, net->read_timeout, &errptr))
    {
      DBUG_PRINT("error", ("Failed to accept new SSL connection"));
      return packet_error;
    }

    DBUG_PRINT("info", ("Reading user information over SSL layer"));
    if ((pkt_len= my_net_read(net)) == packet_error)
    {
      DBUG_PRINT("error", ("Failed to read user information (pkt_len= %lu)",
			   pkt_len));
      return packet_error;
    }
    /*
      A new packet was read and the statistics reflecting the remaining bytes
      in the packet must be updated.
    */
    bytes_remaining_in_packet= pkt_len;

    /*
      After the SSL handshake is performed the client resends the handshake
      packet but because of legacy reasons we chose not to parse the packet
      fields a second time and instead only assert the length of the packet.
    */
    if (mpvio->client_capabilities & CLIENT_PROTOCOL_41)
    {
      packet_has_required_size= bytes_remaining_in_packet >= 
        AUTH_PACKET_HEADER_SIZE_PROTO_41;
      end= (char *)net->read_pos + AUTH_PACKET_HEADER_SIZE_PROTO_41;
      bytes_remaining_in_packet -= AUTH_PACKET_HEADER_SIZE_PROTO_41;
    }
    else
    {
      packet_has_required_size= bytes_remaining_in_packet >= 
        AUTH_PACKET_HEADER_SIZE_PROTO_40;
      end= (char *)net->read_pos + AUTH_PACKET_HEADER_SIZE_PROTO_40;
      bytes_remaining_in_packet -= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    }
    
    DBUG_EXECUTE_IF("host_error_SSL_layering",
                    {
                      packet_has_required_size= 0;
                    };);

    if (!packet_has_required_size)
      return packet_error;
  }
#endif /* HAVE_OPENSSL */

  if ((mpvio->client_capabilities & CLIENT_TRANSACTIONS) &&
      opt_using_transactions)
    net->return_status= mpvio->server_status;

  /*
    The 4.0 and 4.1 versions of the protocol differ on how strings
    are terminated. In the 4.0 version, if a string is at the end
    of the packet, the string is not null terminated. Do not assume
    that the returned string is always null terminated.
  */
  get_proto_string_func_t get_string;

  if (mpvio->client_capabilities & CLIENT_PROTOCOL_41)
    get_string= get_41_protocol_string;
  else
    get_string= get_40_protocol_string;

  /*
    In order to safely scan a head for '\0' string terminators
    we must keep track of how many bytes remain in the allocated
    buffer or we might read past the end of the buffer.
  */
  bytes_remaining_in_packet= pkt_len - (end - (char *)net->read_pos);

  size_t user_len;
  char *user= get_string(&end, &bytes_remaining_in_packet, &user_len);
  DBUG_EXECUTE_IF("host_error_user",
                  {
                    user= NULL;
                  };);

  if (user == NULL)
    return packet_error;

  /*
    Old clients send a null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.
  */
  size_t passwd_len= 0;
  char *passwd= NULL;

  if (mpvio->client_capabilities & CLIENT_SECURE_CONNECTION)
  {
    /*
      4.1+ password. First byte is password length.
    */
    passwd= get_length_encoded_string(&end, &bytes_remaining_in_packet,
                                      &passwd_len);
  }
  else
  {
    /*
      Old passwords are zero terminated strings.
    */
    passwd= get_string(&end, &bytes_remaining_in_packet, &passwd_len);
  }

  DBUG_EXECUTE_IF("host_error_password",
                  {
                    passwd= NULL;
                  };);

  if (passwd == NULL)
    return packet_error;

  size_t db_len= 0;
  char *db= NULL;

  if (mpvio->client_capabilities & CLIENT_CONNECT_WITH_DB)
  {
    db= get_string(&end, &bytes_remaining_in_packet, &db_len);
    if (db == NULL)
      return packet_error;
  }

  /*
    Set the default for the password supplied flag for non-existing users
    as the default plugin (native passsword authentication) would do it
    for compatibility reasons.
  */
  if (passwd_len)
    mpvio->auth_info.password_used= PASSWORD_USED_YES;

  size_t client_plugin_len= 0;
  char *client_plugin= get_string(&end, &bytes_remaining_in_packet,
                                  &client_plugin_len);
  if (client_plugin == NULL)
    client_plugin= &empty_c_string[0];

  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];	// buffer to store user in utf8
  uint dummy_errors;


  /*
    Copy and convert the user and database names to the character set used
    by the server. Since 4.1 all database names are stored in UTF-8. Also,
    ensure that the names are properly null-terminated as this is relied
    upon later.
  */
  if (db)
  {
    db_len= copy_and_convert(db_buff, sizeof(db_buff) - 1, system_charset_info,
                             db, db_len, mpvio->charset_adapter->charset(),
                             &dummy_errors);
    db_buff[db_len]= '\0';
    db= db_buff;
  }

  user_len= copy_and_convert(user_buff, sizeof(user_buff) - 1,
                             system_charset_info, user, user_len,
                             mpvio->charset_adapter->charset(),
                             &dummy_errors);
  user_buff[user_len]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len - 1]= 0;
    user++;
    user_len-= 2;
  }

  if (make_lex_string_root(mpvio->mem_root, 
                           &mpvio->db, db, db_len, 0) == 0)
    return packet_error; /* The error is set by make_lex_string(). */
  if (mpvio->auth_info.user_name)
    my_free(mpvio->auth_info.user_name);
  if (!(mpvio->auth_info.user_name= my_strndup(user, user_len, MYF(MY_WME))))
    return packet_error; /* The error is set by my_strdup(). */
  mpvio->auth_info.user_name_length= user_len;

  if (!initialized)
  {
    // if mysqld's been started with --skip-grant-tables option
    mpvio->status= MPVIO_EXT::SUCCESS;
    return packet_error;
  }

  if (find_mpvio_user(mpvio))
    return packet_error;

  if (!(mpvio->client_capabilities & CLIENT_PLUGIN_AUTH))
  {
    /*
      An old client is connecting
    */
    if (mpvio->client_capabilities & CLIENT_SECURE_CONNECTION)
      client_plugin= native_password_plugin_name.str;
    else
    {
      /*
        A really old client is connecting
      */
      client_plugin= old_password_plugin_name.str;
      /*
        For a passwordless accounts we use native_password_plugin.
        But when an old 4.0 client connects to it, we change it to
        old_password_plugin, otherwise MySQL will think that server 
        and client plugins don't match.
      */
      if (mpvio->acl_user->salt_len == 0)
        mpvio->acl_user_plugin= old_password_plugin_name;
    }
  }
  
  /*
    if the acl_user needs a different plugin to authenticate
    (specified in GRANT ... AUTHENTICATED VIA plugin_name ..)
    we need to restart the authentication in the server.
    But perhaps the client has already used the correct plugin -
    in that case the authentication on the client may not need to be
    restarted and a server auth plugin will read the data that the client
    has just send. Cache them to return in the next server_mpvio_read_packet().
  */
  if (my_strcasecmp(system_charset_info, mpvio->acl_user_plugin.str,
                    plugin_name(mpvio->plugin)->str) != 0)
  {
    mpvio->cached_client_reply.pkt= passwd;
    mpvio->cached_client_reply.pkt_len= passwd_len;
    mpvio->cached_client_reply.plugin= client_plugin;
    mpvio->status= MPVIO_EXT::RESTART;
    return packet_error;
  }

  /*
    ok, we don't need to restart the authentication on the server.
    but if the client used the wrong plugin, we need to restart
    the authentication on the client. Do it here, the server plugin
    doesn't need to know.
  */
  const char *client_auth_plugin=
    ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin;

  if (client_auth_plugin &&
      my_strcasecmp(system_charset_info, client_plugin, client_auth_plugin))
  {
    mpvio->cached_client_reply.plugin= client_plugin;
    if (send_plugin_request_packet(mpvio,
                                   (uchar*) mpvio->cached_server_packet.pkt,
                                   mpvio->cached_server_packet.pkt_len))
      return packet_error;

    passwd_len= my_net_read(mpvio->net);
    passwd = (char*) mpvio->net->read_pos;
  }

  *buff= (uchar*) passwd;
  return passwd_len;
#else
  return 0;
#endif
}


/**
  Make sure that when sending plugin supplied data to the client they
  are not considered a special out-of-band command, like e.g. 
  \255 (error) or \254 (change user request packet) or \0 (OK).
  To avoid this the server will send all plugin data packets "wrapped" 
  in a command \1.
  Note that the client will continue sending its replies unrwapped.
*/

static inline int 
wrap_plguin_data_into_proper_command(NET *net, 
                                     const uchar *packet, int packet_len)
{
  return net_write_command(net, 1, (uchar *) "", 0, packet, packet_len);
}


/**
  vio->write_packet() callback method for server authentication plugins

  This function is called by a server authentication plugin, when it wants
  to send data to the client.

  It transparently wraps the data into a handshake packet,
  and handles plugin negotiation with the client. If necessary,
  it escapes the plugin data, if it starts with a mysql protocol packet byte.
*/
static int server_mpvio_write_packet(MYSQL_PLUGIN_VIO *param,
                                   const uchar *packet, int packet_len)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) param;
  int res;

  DBUG_ENTER("server_mpvio_write_packet");
  /* 
    Reset cached_client_reply if not an old client doing mysql_change_user, 
    as this is where the password from COM_CHANGE_USER is stored.
  */
  if (!((!(mpvio->client_capabilities & CLIENT_PLUGIN_AUTH)) && 
        mpvio->status == MPVIO_EXT::RESTART &&
        mpvio->cached_client_reply.plugin == 
        ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin
        ))
    mpvio->cached_client_reply.pkt= 0;
  /* for the 1st packet we wrap plugin data into the handshake packet */
  if (mpvio->packets_written == 0)
    res= send_server_handshake_packet(mpvio, (char*) packet, packet_len);
  else if (mpvio->status == MPVIO_EXT::RESTART)
    res= send_plugin_request_packet(mpvio, packet, packet_len);
  else
    res= wrap_plguin_data_into_proper_command(mpvio->net, packet, packet_len);
  mpvio->packets_written++;
  DBUG_RETURN(res);
}

/**
  vio->read_packet() callback method for server authentication plugins

  This function is called by a server authentication plugin, when it wants
  to read data from the client.

  It transparently extracts the client plugin data, if embedded into
  a client authentication handshake packet, and handles plugin negotiation
  with the client, if necessary.
*/
static int server_mpvio_read_packet(MYSQL_PLUGIN_VIO *param, uchar **buf)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) param;
  ulong pkt_len;

  DBUG_ENTER("server_mpvio_read_packet");
  if (mpvio->packets_written == 0)
  {
    /*
      plugin wants to read the data without sending anything first.
      send an empty packet to force a server handshake packet to be sent
    */
    if (mpvio->write_packet(mpvio, 0, 0))
      pkt_len= packet_error;
    else
      pkt_len= my_net_read(mpvio->net);
  }
  else if (mpvio->cached_client_reply.pkt)
  {
    DBUG_ASSERT(mpvio->status == MPVIO_EXT::RESTART);
    DBUG_ASSERT(mpvio->packets_read > 0);
    /*
      if the have the data cached from the last server_mpvio_read_packet
      (which can be the case if it's a restarted authentication)
      and a client has used the correct plugin, then we can return the
      cached data straight away and avoid one round trip.
    */
    const char *client_auth_plugin=
      ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin;
    if (client_auth_plugin == 0 ||
        my_strcasecmp(system_charset_info, mpvio->cached_client_reply.plugin,
                      client_auth_plugin) == 0)
    {
      mpvio->status= MPVIO_EXT::FAILURE;
      *buf= (uchar*) mpvio->cached_client_reply.pkt;
      mpvio->cached_client_reply.pkt= 0;
      mpvio->packets_read++;
      DBUG_RETURN ((int) mpvio->cached_client_reply.pkt_len);
    }

    /* older clients don't support change of client plugin request */
    if (!(mpvio->client_capabilities & CLIENT_PLUGIN_AUTH))
    {
      mpvio->status= MPVIO_EXT::FAILURE;
      pkt_len= packet_error;
      goto err;
    }

    /*
      But if the client has used the wrong plugin, the cached data are
      useless. Furthermore, we have to send a "change plugin" request
      to the client.
    */
    if (mpvio->write_packet(mpvio, 0, 0))
      pkt_len= packet_error;
    else
      pkt_len= my_net_read(mpvio->net);
  }
  else
    pkt_len= my_net_read(mpvio->net);

  if (pkt_len == packet_error)
    goto err;

  mpvio->packets_read++;

  /*
    the 1st packet has the plugin data wrapped into the client authentication
    handshake packet
  */
  if (mpvio->packets_read == 1)
  {
    pkt_len= parse_client_handshake_packet(mpvio, buf, pkt_len);
    if (pkt_len == packet_error)
      goto err;
  }
  else
    *buf= mpvio->net->read_pos;

  DBUG_RETURN((int)pkt_len);

err:
  if (mpvio->status == MPVIO_EXT::FAILURE)
  {
    inc_host_errors(mpvio->ip);
    my_error(ER_HANDSHAKE_ERROR, MYF(0));
  }
  DBUG_RETURN(-1);
}

/**
  fills MYSQL_PLUGIN_VIO_INFO structure with the information about the
  connection
*/
static void server_mpvio_info(MYSQL_PLUGIN_VIO *vio,
                              MYSQL_PLUGIN_VIO_INFO *info)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;
  mpvio_info(mpvio->net->vio, info);
}

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static bool acl_check_ssl(THD *thd, const ACL_USER *acl_user)
{
#if defined(HAVE_OPENSSL)
  Vio *vio= thd->net.vio;
  SSL *ssl= (SSL *) vio->ssl_arg;
  X509 *cert;
#endif

  /*
    At this point we know that user is allowed to connect
    from given host by given username/password pair. Now
    we check if SSL is required, if user is using SSL and
    if X509 certificate attributes are OK
  */
  switch (acl_user->ssl_type) {
  case SSL_TYPE_NOT_SPECIFIED:                  // Impossible
  case SSL_TYPE_NONE:                           // SSL is not required
    return 0;
#if defined(HAVE_OPENSSL)
  case SSL_TYPE_ANY:                            // Any kind of SSL is ok
    return vio_type(vio) != VIO_TYPE_SSL;
  case SSL_TYPE_X509: /* Client should have any valid certificate. */
    /*
      Connections with non-valid certificates are dropped already
      in sslaccept() anyway, so we do not check validity here.

      We need to check for absence of SSL because without SSL
      we should reject connection.
    */
    if (vio_type(vio) == VIO_TYPE_SSL &&
        SSL_get_verify_result(ssl) == X509_V_OK &&
        (cert= SSL_get_peer_certificate(ssl)))
    {
      X509_free(cert);
      return 0;
    }
    return 1;
  case SSL_TYPE_SPECIFIED: /* Client should have specified attrib */
    /* If a cipher name is specified, we compare it to actual cipher in use. */
    if (vio_type(vio) != VIO_TYPE_SSL ||
        SSL_get_verify_result(ssl) != X509_V_OK)
      return 1;
    if (acl_user->ssl_cipher)
    {
      DBUG_PRINT("info", ("comparing ciphers: '%s' and '%s'",
                         acl_user->ssl_cipher, SSL_get_cipher(ssl)));
      if (strcmp(acl_user->ssl_cipher, SSL_get_cipher(ssl)))
      {
        if (global_system_variables.log_warnings)
          sql_print_information("X509 ciphers mismatch: should be '%s' but is '%s'",
                            acl_user->ssl_cipher, SSL_get_cipher(ssl));
        return 1;
      }
    }
    /* Prepare certificate (if exists) */
    if (!(cert= SSL_get_peer_certificate(ssl)))
      return 1;
    /* If X509 issuer is specified, we check it... */
    if (acl_user->x509_issuer)
    {
      char *ptr= X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
      DBUG_PRINT("info", ("comparing issuers: '%s' and '%s'",
                         acl_user->x509_issuer, ptr));
      if (strcmp(acl_user->x509_issuer, ptr))
      {
        if (global_system_variables.log_warnings)
          sql_print_information("X509 issuer mismatch: should be '%s' "
                            "but is '%s'", acl_user->x509_issuer, ptr);
        free(ptr);
        X509_free(cert);
        return 1;
      }
      free(ptr);
    }
    /* X509 subject is specified, we check it .. */
    if (acl_user->x509_subject)
    {
      char *ptr= X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
      DBUG_PRINT("info", ("comparing subjects: '%s' and '%s'",
                         acl_user->x509_subject, ptr));
      if (strcmp(acl_user->x509_subject, ptr))
      {
        if (global_system_variables.log_warnings)
          sql_print_information("X509 subject mismatch: should be '%s' but is '%s'",
                          acl_user->x509_subject, ptr);
        free(ptr);
        X509_free(cert);
        return 1;
      }
      free(ptr);
    }
    X509_free(cert);
    return 0;
#else  /* HAVE_OPENSSL */
  default:
    /*
      If we don't have SSL but SSL is required for this user the 
      authentication should fail.
    */
    return 1;
#endif /* HAVE_OPENSSL */
  }
  return 1;
}
#endif


static int do_auth_once(THD *thd, const LEX_STRING *auth_plugin_name,
                        MPVIO_EXT *mpvio)
{
  int res= CR_OK, old_status= MPVIO_EXT::FAILURE;
  bool unlock_plugin= false;
  plugin_ref plugin;

  if (auth_plugin_name->str == native_password_plugin_name.str)
    plugin= native_password_plugin;
  else
#ifndef EMBEDDED_LIBRARY
  if (auth_plugin_name->str == old_password_plugin_name.str)
    plugin= old_password_plugin;
  else if ((plugin= my_plugin_lock_by_name(thd, auth_plugin_name,
                                           MYSQL_AUTHENTICATION_PLUGIN)))
    unlock_plugin= true;
  else
#endif
    plugin= NULL;
    
  mpvio->plugin= plugin;
  old_status= mpvio->status;
  
  if (plugin)
  {
    st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
    res= auth->authenticate_user(mpvio, &mpvio->auth_info);

    if (unlock_plugin)
      plugin_unlock(thd, plugin);
  }
  else
  {
    /* Server cannot load the required plugin. */
    my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), auth_plugin_name->str);
    res= CR_ERROR;
  }

  /*
    If the status was MPVIO_EXT::RESTART before the authenticate_user() call
    it can never be MPVIO_EXT::RESTART after the call, because any call
    to write_packet() or read_packet() will reset the status.

    But (!) if a plugin never called a read_packet() or write_packet(), the
    status will stay unchanged. We'll fix it, by resetting the status here.
  */
  if (old_status == MPVIO_EXT::RESTART && mpvio->status == MPVIO_EXT::RESTART)
    mpvio->status= MPVIO_EXT::FAILURE; // reset to the default

  return res;
}


static void
server_mpvio_initialize(THD *thd, MPVIO_EXT *mpvio, uint connect_errors,
                        Thd_charset_adapter *charset_adapter)
{
  memset(mpvio, 0, sizeof(MPVIO_EXT));
  mpvio->read_packet= server_mpvio_read_packet;
  mpvio->write_packet= server_mpvio_write_packet;
  mpvio->info= server_mpvio_info;
  mpvio->auth_info.host_or_ip= thd->security_ctx->host_or_ip;
  mpvio->auth_info.host_or_ip_length= 
    (unsigned int) strlen(thd->security_ctx->host_or_ip);
  mpvio->auth_info.user_name= NULL;
  mpvio->auth_info.user_name_length= 0;
  mpvio->connect_errors= connect_errors;
  mpvio->status= MPVIO_EXT::FAILURE;

  mpvio->client_capabilities= thd->client_capabilities;
  mpvio->mem_root= thd->mem_root;
  mpvio->scramble= thd->scramble;
  mpvio->rand= &thd->rand;
  mpvio->thread_id= thd->thread_id;
  mpvio->server_status= &thd->server_status;
  mpvio->net= &thd->net;
  mpvio->ip= (char *) thd->security_ctx->get_ip()->ptr();
  mpvio->host= (char *) thd->security_ctx->get_host()->ptr();
  mpvio->charset_adapter= charset_adapter;
}


static void
server_mpvio_update_thd(THD *thd, MPVIO_EXT *mpvio)
{
  thd->client_capabilities= mpvio->client_capabilities;
  thd->max_client_packet_length= mpvio->max_client_packet_length;
  if (mpvio->client_capabilities & CLIENT_INTERACTIVE)
    thd->variables.net_wait_timeout= thd->variables.net_interactive_timeout;
  thd->security_ctx->user= mpvio->auth_info.user_name;
  if (thd->client_capabilities & CLIENT_IGNORE_SPACE)
    thd->variables.sql_mode|= MODE_IGNORE_SPACE;
}

/**
  Perform the handshake, authorize the client and update thd sctx variables.

  @param thd                     thread handle
  @param connect_errors          number of previous failed connect attemps
                                 from this host
  @param com_change_user_pkt_len size of the COM_CHANGE_USER packet
                                 (without the first, command, byte) or 0
                                 if it's not a COM_CHANGE_USER (that is, if
                                 it's a new connection)

  @retval 0  success, thd is updated.
  @retval 1  error
*/
bool 
acl_authenticate(THD *thd, uint connect_errors, uint com_change_user_pkt_len)
{
  int res= CR_OK;
  MPVIO_EXT mpvio;
  Thd_charset_adapter charset_adapter(thd);

  const LEX_STRING *auth_plugin_name= default_auth_plugin_name;
  enum  enum_server_command command= com_change_user_pkt_len ? COM_CHANGE_USER
                                                             : COM_CONNECT;

  DBUG_ENTER("acl_authenticate");
  compile_time_assert(MYSQL_USERNAME_LENGTH == USERNAME_LENGTH);

  server_mpvio_initialize(thd, &mpvio, connect_errors, &charset_adapter);

  DBUG_PRINT("info", ("com_change_user_pkt_len=%u", com_change_user_pkt_len));

  /*
    Clear thd->db as it points to something, that will be freed when
    connection is closed. We don't want to accidentally free a wrong
    pointer if connect failed.
  */
  thd->reset_db(NULL, 0);

  if (command == COM_CHANGE_USER)
  {
    mpvio.packets_written++; // pretend that a server handshake packet was sent
    mpvio.packets_read++;    // take COM_CHANGE_USER packet into account

    /* Clear variables that are allocated */
    thd->set_user_connect(NULL);

    if (parse_com_change_user_packet(&mpvio, com_change_user_pkt_len))
    {
      if (!thd->is_error())
        login_failed_error(&mpvio, mpvio.auth_info.password_used);
      server_mpvio_update_thd(thd, &mpvio);
      DBUG_RETURN(1);
    }

    DBUG_ASSERT(mpvio.status == MPVIO_EXT::RESTART ||
                mpvio.status == MPVIO_EXT::SUCCESS);
  }
  else
  {
    /* mark the thd as having no scramble yet */
    mpvio.scramble[SCRAMBLE_LENGTH]= 1;
    
    /*
     perform the first authentication attempt, with the default plugin.
     This sends the server handshake packet, reads the client reply
     with a user name, and performs the authentication if everyone has used
     the correct plugin.
    */

    res= do_auth_once(thd, auth_plugin_name, &mpvio);  
  }

  /*
   retry the authentication, if - after receiving the user name -
   we found that we need to switch to a non-default plugin
  */
  if (mpvio.status == MPVIO_EXT::RESTART)
  {
    DBUG_ASSERT(mpvio.acl_user);
    DBUG_ASSERT(command == COM_CHANGE_USER ||
                my_strcasecmp(system_charset_info, auth_plugin_name->str,
                              mpvio.acl_user->plugin.str));
    auth_plugin_name= &mpvio.acl_user->plugin;
    res= do_auth_once(thd, auth_plugin_name, &mpvio);
  }

  server_mpvio_update_thd(thd, &mpvio);

  Security_context *sctx= thd->security_ctx;
  const ACL_USER *acl_user= mpvio.acl_user;

  thd->password= mpvio.auth_info.password_used;  // remember for error messages 

  /*
    Log the command here so that the user can check the log
    for the tried logins and also to detect break-in attempts.

    if sctx->user is unset it's protocol failure, bad packet.
  */
  if (mpvio.auth_info.user_name)
  {
    if (strcmp(mpvio.auth_info.authenticated_as, mpvio.auth_info.user_name))
    {
      general_log_print(thd, command, "%s@%s as %s on %s",
                        mpvio.auth_info.user_name, mpvio.auth_info.host_or_ip,
                        mpvio.auth_info.authenticated_as ? 
                          mpvio.auth_info.authenticated_as : "anonymous",
                        mpvio.db.str ? mpvio.db.str : (char*) "");
    }
    else
      general_log_print(thd, command, (char*) "%s@%s on %s",
                        mpvio.auth_info.user_name, mpvio.auth_info.host_or_ip,
                        mpvio.db.str ? mpvio.db.str : (char*) "");
  }

  if (res == CR_OK && !mpvio.can_authenticate())
  {
    res= CR_ERROR;
  }

  if (res > CR_OK && mpvio.status != MPVIO_EXT::SUCCESS)
  {
    DBUG_ASSERT(mpvio.status == MPVIO_EXT::FAILURE);

    if (!thd->is_error())
      login_failed_error(&mpvio, mpvio.auth_info.password_used);
    DBUG_RETURN (1);
  }

  sctx->proxy_user[0]= 0;

  if (initialized) // if not --skip-grant-tables
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    bool is_proxy_user= FALSE;
    const char *auth_user = acl_user->user ? acl_user->user : "";
    ACL_PROXY_USER *proxy_user;
    /* check if the user is allowed to proxy as another user */
    proxy_user= acl_find_proxy_user(auth_user, sctx->get_host()->ptr(),
                                    sctx->get_ip()->ptr(),
                                    mpvio.auth_info.authenticated_as,
                                    &is_proxy_user);
    if (is_proxy_user)
    {
      ACL_USER *acl_proxy_user;

      /* we need to find the proxy user, but there was none */
      if (!proxy_user)
      {
        if (!thd->is_error())
          login_failed_error(&mpvio, mpvio.auth_info.password_used);
        DBUG_RETURN(1);
      }

      my_snprintf(sctx->proxy_user, sizeof(sctx->proxy_user) - 1,
                  "'%s'@'%s'", auth_user,
                  acl_user->host.hostname ? acl_user->host.hostname : "");

      /* we're proxying : find the proxy user definition */
      mysql_mutex_lock(&acl_cache->lock);
      acl_proxy_user= find_acl_user(proxy_user->get_proxied_host() ? 
                                    proxy_user->get_proxied_host() : "",
                                    mpvio.auth_info.authenticated_as, TRUE);
      if (!acl_proxy_user)
      {
        if (!thd->is_error())
          login_failed_error(&mpvio, mpvio.auth_info.password_used);
        mysql_mutex_unlock(&acl_cache->lock);
        DBUG_RETURN(1);
      }
      acl_user= acl_proxy_user->copy(thd->mem_root);
      mysql_mutex_unlock(&acl_cache->lock);
    }
#endif

    sctx->master_access= acl_user->access;
    if (acl_user->user)
      strmake(sctx->priv_user, acl_user->user, USERNAME_LENGTH - 1);
    else
      *sctx->priv_user= 0;

    if (acl_user->host.hostname)
      strmake(sctx->priv_host, acl_user->host.hostname, MAX_HOSTNAME - 1);
    else
      *sctx->priv_host= 0;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /*
      OK. Let's check the SSL. Historically it was checked after the password,
      as an additional layer, not instead of the password
      (in which case it would've been a plugin too).
    */
    if (acl_check_ssl(thd, acl_user))
    {
      if (!thd->is_error())
        login_failed_error(&mpvio, thd->password);
      DBUG_RETURN(1);
    }

    /* Don't allow the user to connect if he has done too many queries */
    if ((acl_user->user_resource.questions || acl_user->user_resource.updates ||
         acl_user->user_resource.conn_per_hour ||
         acl_user->user_resource.user_conn || 
         global_system_variables.max_user_connections) &&
        get_or_create_user_conn(thd,
          (opt_old_style_user_limits ? sctx->user : sctx->priv_user),
          (opt_old_style_user_limits ? sctx->host_or_ip : sctx->priv_host),
          &acl_user->user_resource))
      DBUG_RETURN(1); // The error is set by get_or_create_user_conn()

#endif
  }
  else
    sctx->skip_grants();

  const USER_CONN *uc;
  if ((uc= thd->get_user_connect()) &&
      (uc->user_resources.conn_per_hour || uc->user_resources.user_conn ||
       global_system_variables.max_user_connections) &&
       check_for_max_user_connections(thd, uc))
  {
    DBUG_RETURN(1); // The error is set in check_for_max_user_connections()
  }

  DBUG_PRINT("info",
             ("Capabilities: %lu  packet_length: %ld  Host: '%s'  "
              "Login user: '%s' Priv_user: '%s'  Using password: %s "
              "Access: %lu  db: '%s'",
              thd->client_capabilities, thd->max_client_packet_length,
              sctx->host_or_ip, sctx->user, sctx->priv_user,
              thd->password ? "yes": "no",
              sctx->master_access, mpvio.db.str));

  if (command == COM_CONNECT &&
      !(thd->main_security_ctx.master_access & SUPER_ACL))
  {
    mysql_mutex_lock(&LOCK_connection_count);
    bool count_ok= (connection_count <= max_connections);
    mysql_mutex_unlock(&LOCK_connection_count);
    if (!count_ok)
    {                                         // too many connections
      release_user_connection(thd);
      my_error(ER_CON_COUNT_ERROR, MYF(0));
      DBUG_RETURN(1);
    }
  }

  /*
    This is the default access rights for the current database.  It's
    set to 0 here because we don't have an active database yet (and we
    may not have an active database to set.
  */
  sctx->db_access=0;

  /* Change a database if necessary */
  if (mpvio.db.length)
  {
    if (mysql_change_db(thd, &mpvio.db, FALSE))
    {
      /* mysql_change_db() has pushed the error message. */
      release_user_connection(thd);
      DBUG_RETURN(1);
    }
  }

  if (mpvio.auth_info.external_user[0])
    sctx->set_external_user(my_strdup(mpvio.auth_info.external_user, MYF(0)));

  if (res == CR_OK_HANDSHAKE_COMPLETE)
    thd->stmt_da->disable_status();
  else
    my_ok(thd);

#if defined(MYSQL_SERVER) && !defined(EMBEDDED_LIBRARY)
  /*
    Allow the network layer to skip big packets. Although a malicious
    authenticated session might use this to trick the server to read
    big packets indefinitely, this is a previously established behavior
    that needs to be preserved as to not break backwards compatibility.
  */
  thd->net.skip_big_packet= TRUE;
#endif

  /*
     Reset previous connection failures if any.
  */
  if (mpvio.connect_errors)
    reset_host_errors(mpvio.ip);

  /* Ready to handle queries */
  DBUG_RETURN(0);
}

/**
  MySQL Server Password Authentication Plugin

  In the MySQL authentication protocol:
  1. the server sends the random scramble to the client
  2. client sends the encrypted password back to the server
  3. the server checks the password.
*/
static int native_password_authenticate(MYSQL_PLUGIN_VIO *vio,
                                        MYSQL_SERVER_AUTH_INFO *info)
{
  uchar *pkt;
  int pkt_len;
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;

  DBUG_ENTER("native_password_authenticate");

  /* generate the scramble, or reuse the old one */
  if (mpvio->scramble[SCRAMBLE_LENGTH])
    create_random_string(mpvio->scramble, SCRAMBLE_LENGTH, mpvio->rand);

  /* send it to the client */
  if (mpvio->write_packet(mpvio, (uchar*) mpvio->scramble, SCRAMBLE_LENGTH + 1))
    DBUG_RETURN(CR_ERROR);

  /* reply and authenticate */

  /*
    <digression>
      This is more complex than it looks.

      The plugin (we) may be called right after the client was connected -
      and will need to send a scramble, read reply, authenticate.

      Or the plugin may be called after another plugin has sent a scramble,
      and read the reply. If the client has used the correct client-plugin,
      we won't need to read anything here from the client, the client
      has already sent a reply with everything we need for authentication.

      Or the plugin may be called after another plugin has sent a scramble,
      and read the reply, but the client has used the wrong client-plugin.
      We'll need to sent a "switch to another plugin" packet to the
      client and read the reply. "Use the short scramble" packet is a special
      case of "switch to another plugin" packet.

      Or, perhaps, the plugin may be called after another plugin has
      done the handshake but did not send a useful scramble. We'll need
      to send a scramble (and perhaps a "switch to another plugin" packet)
      and read the reply.

      Besides, a client may be an old one, that doesn't understand plugins.
      Or doesn't even understand 4.0 scramble.

      And we want to keep the same protocol on the wire  unless non-native
      plugins are involved.

      Anyway, it still looks simple from a plugin point of view:
      "send the scramble, read the reply and authenticate"
      All the magic is transparently handled by the server.
    </digression>
  */

  /* read the reply with the encrypted password */
  if ((pkt_len= mpvio->read_packet(mpvio, &pkt)) < 0)
    DBUG_RETURN(CR_ERROR);
  DBUG_PRINT("info", ("reply read : pkt_len=%d", pkt_len));

#ifdef NO_EMBEDDED_ACCESS_CHECKS
  DBUG_RETURN(CR_OK);
#endif

  if (pkt_len == 0) /* no password */
    DBUG_RETURN(mpvio->acl_user->salt_len != 0 ? CR_ERROR : CR_OK);

  info->password_used= PASSWORD_USED_YES;
  if (pkt_len == SCRAMBLE_LENGTH)
  {
    if (!mpvio->acl_user->salt_len)
      DBUG_RETURN(CR_ERROR);

    DBUG_RETURN(check_scramble(pkt, mpvio->scramble, mpvio->acl_user->salt) ?
                CR_ERROR : CR_OK);
  }

  inc_host_errors(mpvio->ip);
  my_error(ER_HANDSHAKE_ERROR, MYF(0));
  DBUG_RETURN(CR_ERROR);
}

static int old_password_authenticate(MYSQL_PLUGIN_VIO *vio, 
                                     MYSQL_SERVER_AUTH_INFO *info)
{
  uchar *pkt;
  int pkt_len;
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;

  /* generate the scramble, or reuse the old one */
  if (mpvio->scramble[SCRAMBLE_LENGTH])
    create_random_string(mpvio->scramble, SCRAMBLE_LENGTH, mpvio->rand);

  /* send it to the client */
  if (mpvio->write_packet(mpvio, (uchar*) mpvio->scramble, SCRAMBLE_LENGTH + 1))
    return CR_ERROR;

  /* read the reply and authenticate */
  if ((pkt_len= mpvio->read_packet(mpvio, &pkt)) < 0)
    return CR_ERROR;

#ifdef NO_EMBEDDED_ACCESS_CHECKS
  return CR_OK;
#endif

  /*
    legacy: if switch_from_long_to_short_scramble,
    the password is sent \0-terminated, the pkt_len is always 9 bytes.
    We need to figure out the correct scramble length here.
  */
  if (pkt_len == SCRAMBLE_LENGTH_323 + 1)
    pkt_len= strnlen((char*)pkt, pkt_len);

  if (pkt_len == 0) /* no password */
    return mpvio->acl_user->salt_len != 0 ? CR_ERROR : CR_OK;

  if (secure_auth(mpvio))
    return CR_ERROR;

  info->password_used= PASSWORD_USED_YES;

  if (pkt_len == SCRAMBLE_LENGTH_323)
  {
    if (!mpvio->acl_user->salt_len)
      return CR_ERROR;

    return check_scramble_323(pkt, mpvio->scramble,
                             (ulong *) mpvio->acl_user->salt) ? 
                             CR_ERROR : CR_OK;
  }

  inc_host_errors(mpvio->ip);
  my_error(ER_HANDSHAKE_ERROR, MYF(0));
  return CR_ERROR;
}

static struct st_mysql_auth native_password_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  native_password_plugin_name.str,
  native_password_authenticate
};

static struct st_mysql_auth old_password_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  old_password_plugin_name.str,
  old_password_authenticate
};

mysql_declare_plugin(mysql_password)
{
  MYSQL_AUTHENTICATION_PLUGIN,                  /* type constant    */
  &native_password_handler,                     /* type descriptor  */
  native_password_plugin_name.str,              /* Name             */
  "R.J.Silk, Sergei Golubchik",                 /* Author           */
  "Native MySQL authentication",                /* Description      */
  PLUGIN_LICENSE_GPL,                           /* License          */
  NULL,                                         /* Init function    */
  NULL,                                         /* Deinit function  */
  0x0100,                                       /* Version (1.0)    */
  NULL,                                         /* status variables */
  NULL,                                         /* system variables */
  NULL,                                         /* config options   */
  0,                                            /* flags            */
},
{
  MYSQL_AUTHENTICATION_PLUGIN,                  /* type constant    */
  &old_password_handler,                        /* type descriptor  */
  old_password_plugin_name.str,                 /* Name             */
  "R.J.Silk, Sergei Golubchik",                 /* Author           */
  "Old MySQL-4.0 authentication",               /* Description      */
  PLUGIN_LICENSE_GPL,                           /* License          */
  NULL,                                         /* Init function    */
  NULL,                                         /* Deinit function  */
  0x0100,                                       /* Version (1.0)    */
  NULL,                                         /* status variables */
  NULL,                                         /* system variables */
  NULL,                                         /* config options   */
  0,                                            /* flags            */
}
mysql_declare_plugin_end;

