/* Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301  USA */


/*
  In here, we rewrite queries (to obfuscate passwords etc.) that need it
  before we log them.

  Stored procedures may also rewrite their statements (to show the actual
  values of their variables etc.). There is currently no scenario where
  a statement can be eligible for both rewrites. (see sp_instr.cc)
  Special consideration will need to be taken if this assertion is changed.

  We also do not intersect with query cache at this time, as QC only
  caches SELECTs (which we don't rewrite). If and when QC becomes more
  general, it should probably cache the rewritten query along with the
  user-submitted one. (see sql_parse.cc)
*/


#include "auth_common.h"    // append_user
#include "sql_parse.h"  // get_current_user
#include "sql_show.h"   // append_identifier
#include "sp_head.h"    // struct set_var_base
#include "rpl_slave.h"  // SLAVE_SQL, SLAVE_IO
#include "mysqld.h"     // opt_log_builtin_as_identified_by_password
#include "log.h"

/**
  Append a key/value pair to a string, with an optional preceding comma.
  For numeric values.

  @param           str                  The string to append to
  @param           comma                Prepend a comma?
  @param           txt                  C-string, must end in a space
  @param           len                  strlen(txt)
  @param           val                  numeric value
  @param           cond                 only append if this evaluates to true

  @retval          false if any subsequent key/value pair would be the first
*/

bool append_int(String *str, bool comma, const char *txt, size_t len,
                long val, int cond)
{
  if (cond)
  {
    String numbuf(42);
    if (comma)
      str->append(STRING_WITH_LEN(", "));
    str->append(txt,len);
    numbuf.set((longlong)val,&my_charset_bin);
    str->append(numbuf);
    return true;
  }
  return comma;
}


/**
  Append a key/value pair to a string if the value is non-NULL,
  with an optional preceding comma.

  @param           str                  The string to append to
  @param           comma                Prepend a comma?
  @param           key                  C-string: the key, must be non-NULL
  @param           val                  C-string: the value

  @retval          false if any subsequent key/value pair would be the first
*/

bool append_str(String *str, bool comma, const char *key, const char *val)
{
  if (val)
  {
    if (comma)
      str->append(STRING_WITH_LEN(", "));
    str->append(key);
    str->append(STRING_WITH_LEN(" '"));
    str->append(val);
    str->append(STRING_WITH_LEN("'"));
    return true;
  }
  return comma;
}

void rewrite_ssl_properties(LEX *lex, String *rlb)
{
  if (lex->ssl_type != SSL_TYPE_NOT_SPECIFIED)
  {
    rlb->append(STRING_WITH_LEN(" REQUIRE"));
    switch (lex->ssl_type)
    {
    case SSL_TYPE_SPECIFIED:
      if (lex->x509_subject)
      {
        rlb->append(STRING_WITH_LEN(" SUBJECT '"));
        rlb->append(lex->x509_subject);
        rlb->append(STRING_WITH_LEN("'"));
      }
      if (lex->x509_issuer)
      {
        rlb->append(STRING_WITH_LEN(" ISSUER '"));
        rlb->append(lex->x509_issuer);
        rlb->append(STRING_WITH_LEN("'"));
      }
      if (lex->ssl_cipher)
      {
        rlb->append(STRING_WITH_LEN(" CIPHER '"));
        rlb->append(lex->ssl_cipher);
        rlb->append(STRING_WITH_LEN("'"));
      }
      break;
    case SSL_TYPE_X509:
      rlb->append(STRING_WITH_LEN(" X509"));
      break;
    case SSL_TYPE_ANY:
      rlb->append(STRING_WITH_LEN(" SSL"));
      break;
    case SSL_TYPE_NOT_SPECIFIED:
      /* fall-thru */
    case SSL_TYPE_NONE:
      rlb->append(STRING_WITH_LEN(" NONE"));
      break;
    }
  }
}

void rewrite_user_resources(LEX *lex, String *rlb)
{
  if (lex->mqh.specified_limits || (lex->grant & GRANT_ACL))
  {
    rlb->append(STRING_WITH_LEN(" WITH"));
    if (lex->grant & GRANT_ACL)
      rlb->append(STRING_WITH_LEN(" GRANT OPTION"));

    append_int(rlb, false, STRING_WITH_LEN(" MAX_QUERIES_PER_HOUR "),
               lex->mqh.questions,
               lex->mqh.specified_limits & USER_RESOURCES::QUERIES_PER_HOUR);

    append_int(rlb, false, STRING_WITH_LEN(" MAX_UPDATES_PER_HOUR "),
               lex->mqh.updates,
               lex->mqh.specified_limits & USER_RESOURCES::UPDATES_PER_HOUR);

    append_int(rlb, false, STRING_WITH_LEN(" MAX_CONNECTIONS_PER_HOUR "),
               lex->mqh.conn_per_hour,
               lex->mqh.specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR);

    append_int(rlb, false, STRING_WITH_LEN(" MAX_USER_CONNECTIONS "),
               lex->mqh.user_conn,
               lex->mqh.specified_limits & USER_RESOURCES::USER_CONNECTIONS);
  }
}

void rewrite_account_lock(LEX *lex, String *rlb)
{
  if (lex->alter_password.account_locked)
  {
    rlb->append(STRING_WITH_LEN(" ACCOUNT LOCK"));
  }
  else
  {
    rlb->append(STRING_WITH_LEN(" ACCOUNT UNLOCK"));
  }
}

/**
  Rewrite a GRANT statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

void mysql_rewrite_grant(THD *thd, String *rlb)
{
  LEX        *lex= thd->lex;
  TABLE_LIST *first_table= lex->select_lex->table_list.first;
  bool        comma= FALSE, comma_inner;
  bool        proxy_grant= lex->type == TYPE_ENUM_PROXY;
  String      cols(1024);
  int         c;

  rlb->append(STRING_WITH_LEN("GRANT "));

  if (proxy_grant)
    rlb->append(STRING_WITH_LEN("PROXY"));
  else if (lex->all_privileges)
    rlb->append(STRING_WITH_LEN("ALL PRIVILEGES"));
  else
  {
    ulong priv;

    for (c= 0, priv= SELECT_ACL; priv <= GLOBAL_ACLS; c++, priv <<= 1)
    {
      if (priv == GRANT_ACL)
        continue;

      comma_inner= FALSE;

      if (lex->columns.elements)               // show columns, if any
      {
        class LEX_COLUMN *column;
        List_iterator <LEX_COLUMN> column_iter(lex->columns);

        cols.length(0);
        cols.append(STRING_WITH_LEN(" ("));

        /*
          If the statement was GRANT SELECT(f2), INSERT(f3), UPDATE(f1,f3, f2),
          our list cols will contain the order f2, f3, f1, and thus that's
          the order we'll recreate the privilege: UPDATE (f2, f3, f1)
        */

        while ((column= column_iter++))
        {
          if (column->rights & priv)
          {
            if (comma_inner)
              cols.append(STRING_WITH_LEN(", "));
            else
              comma_inner= TRUE;
            append_identifier(thd, &cols, column->column.ptr(), column->column.length());
          }
        }
        cols.append(STRING_WITH_LEN(")"));
      }

      if (comma_inner || (lex->grant & priv))  // show privilege name
      {
        if (comma)
          rlb->append(STRING_WITH_LEN(", "));
        else
          comma= TRUE;
        rlb->append(command_array[c],command_lengths[c]);
        if (!(lex->grant & priv))              // general outranks specific
          rlb->append(cols);
      }
    }
    if (!comma)                                // no privs, default to USAGE
      rlb->append(STRING_WITH_LEN("USAGE"));
  }

  rlb->append(STRING_WITH_LEN(" ON "));
  switch(lex->type)
  {
    case TYPE_ENUM_PROCEDURE: rlb->append(STRING_WITH_LEN("PROCEDURE ")); break;
    case TYPE_ENUM_FUNCTION:  rlb->append(STRING_WITH_LEN("FUNCTION "));  break;
    default:                                                              break;
  }

  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(lex->users_list);
  comma= FALSE;

  if (proxy_grant)
  {
    tmp_user_name= user_list++;
    user_name= get_current_user(thd, tmp_user_name);
    if (user_name)
      append_user_new(thd, rlb, user_name, comma);
  }
  else if (first_table)
  {
    if (first_table->is_view())
    {
      append_identifier(thd, rlb, first_table->view_db.str,
                        first_table->view_db.length);
      rlb->append(STRING_WITH_LEN("."));
      append_identifier(thd, rlb, first_table->view_name.str,
                        first_table->view_name.length);
    }
    else
    {
      append_identifier(thd, rlb, first_table->db, strlen(first_table->db));
      rlb->append(STRING_WITH_LEN("."));
      append_identifier(thd, rlb, first_table->table_name,
                        strlen(first_table->table_name));
    }
  }
  else
  {
    if (lex->current_select()->db)
      append_identifier(thd, rlb, lex->current_select()->db,
                        strlen(lex->current_select()->db));
    else
      rlb->append("*");
    rlb->append(STRING_WITH_LEN(".*"));
  }

  rlb->append(STRING_WITH_LEN(" TO "));
  {
    while ((tmp_user_name= user_list++))
    {
      if ((user_name= get_current_user(thd, tmp_user_name)))
      {
        if (opt_log_builtin_as_identified_by_password)
          append_user(thd, rlb, user_name, comma, true);
        else
          append_user_new(thd, rlb, user_name, comma);
        comma= TRUE;
      }
    }
  }
  rewrite_ssl_properties(lex, rlb);
  rewrite_user_resources(lex, rlb);
}


/**
  Rewrite a SET statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_set(THD *thd, String *rlb)
{
  LEX                              *lex= thd->lex;
  List_iterator_fast<set_var_base>  it(lex->var_list);
  set_var_base                     *var;
  bool                              comma= FALSE;

  rlb->append(STRING_WITH_LEN("SET "));

  while ((var= it++))
  {
    if (comma)
      rlb->append(STRING_WITH_LEN(","));
    else
      comma= TRUE;

    var->print(thd, rlb);
  }
}

/**
  Rewrite CREATE/ALTER USER statement.

  @param thd                     The THD to rewrite for.
  @param rlb                     An empty String object to put the rewritten query in.
  @param hide_password_hash      If password hash has to be shown as <secret> or not.
*/

void mysql_rewrite_create_alter_user(THD *thd, String *rlb,
                                     std::set<LEX_USER *> *extra_users,
                                     bool hide_password_hash)
{
  LEX                      *lex= thd->lex;
  LEX_USER                 *user_name, *tmp_user_name;
  List_iterator <LEX_USER>  user_list(lex->users_list);
  bool                      comma= FALSE;

  if (thd->lex->sql_command == SQLCOM_CREATE_USER ||
      thd->lex->sql_command == SQLCOM_SHOW_CREATE_USER)
    rlb->append(STRING_WITH_LEN("CREATE USER "));
  else
    rlb->append(STRING_WITH_LEN("ALTER USER "));

  if (thd->lex->sql_command == SQLCOM_CREATE_USER &&
      thd->lex->create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS)
    rlb->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (thd->lex->sql_command == SQLCOM_ALTER_USER &&
      thd->lex->drop_if_exists)
    rlb->append(STRING_WITH_LEN("IF EXISTS "));

  while ((tmp_user_name= user_list++))
  {
    if ((user_name= get_current_user(thd, tmp_user_name)))
    {
      if (opt_log_builtin_as_identified_by_password &&
          thd->lex->sql_command != SQLCOM_ALTER_USER)
        append_user(thd, rlb, user_name, comma, true);
      else
        append_user_new(thd, rlb, user_name, comma, hide_password_hash);
      comma= TRUE;
    }
  }

  rewrite_ssl_properties(lex, rlb);
  rewrite_user_resources(lex, rlb);

  /* rewrite password expired */
  if (lex->alter_password.update_password_expired_fields)
  {
    if (lex->alter_password.update_password_expired_column)
    {
      rlb->append(STRING_WITH_LEN(" PASSWORD EXPIRE"));
    }
    else if (lex->alter_password.expire_after_days)
    {
      append_int(rlb, false, STRING_WITH_LEN(" PASSWORD EXPIRE INTERVAL "),
                 lex->alter_password.expire_after_days, TRUE);
      rlb->append(STRING_WITH_LEN(" DAY"));
    }
    else if (lex->alter_password.use_default_password_lifetime)
    {
      rlb->append(STRING_WITH_LEN(" PASSWORD EXPIRE DEFAULT"));
    }
    else
    {
      rlb->append(STRING_WITH_LEN(" PASSWORD EXPIRE NEVER"));
    }
  }

  if (lex->alter_password.update_account_locked_column)
  {
    rewrite_account_lock(lex, rlb);
  }

  if ((lex->sql_command == SQLCOM_CREATE_USER ||
      lex->sql_command == SQLCOM_ALTER_USER) &&
      extra_users && extra_users->size())
  {
    String warn_user;
    bool comma= false;
    bool log_warning= false;
    std::set<LEX_USER *>::iterator it;
    for (it = extra_users->begin(); it != extra_users->end(); it++)
    {
      /*
        Consider for warning if one of the following is true:
        1. If SQLCOM_CREATE_USER and IF NOT EXISTS clause is used and
           IDENTIFIED WITH clause is not used
        2. If SQLCOM_ALTER_USER and IF EXISTS clause is used and
           IDENTIFIED WITH clause is not used
      */
      LEX_USER* extra_user= *it;
      if (!extra_user->uses_identified_with_clause &&
          (lex->sql_command == SQLCOM_CREATE_USER ||
          extra_user->uses_identified_by_clause))
      {
        append_user(thd, &warn_user, extra_user, comma, false);
        comma= true;
        log_warning= true;
      }
    }
    if (log_warning)
    {
      if (lex->sql_command == SQLCOM_CREATE_USER)
      {
        sql_print_warning("Following users were specified in CREATE USER "
                          "IF NOT EXISTS but they already exist. "
                          "Corresponding entry in binary log used default "
                          "authentication plugin '%s' to rewrite "
                          "authentication information(if any) for them: %s\n",
                          default_auth_plugin, warn_user.c_ptr_safe());
      }
      else if (lex->sql_command == SQLCOM_ALTER_USER)
      {
        sql_print_warning("Following users were specified in ALTER USER "
                          "IF EXISTS but they do not exist. "
                          "Corresponding entry in binary log used default "
                          "authentication plugin '%s' to rewrite "
                          "authentication information(if any) for them: %s\n",
                          default_auth_plugin, warn_user.c_ptr_safe());
      }
    }
    warn_user.mem_free();
  }
}

/**
  Rewrite a CHANGE MASTER statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_change_master(THD *thd, String *rlb)
{
  LEX *lex= thd->lex;

  rlb->append(STRING_WITH_LEN("CHANGE MASTER TO"));

  if (lex->mi.host)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_HOST = '"));
    rlb->append(lex->mi.host);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.user)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_USER = '"));
    rlb->append(lex->mi.user);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.password)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_PASSWORD = <secret>"));
  }
  if (lex->mi.port)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_PORT = "));
    rlb->append_ulonglong(lex->mi.port);
  }
  if (lex->mi.connect_retry)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_CONNECT_RETRY = "));
    rlb->append_ulonglong(lex->mi.connect_retry);
  }
  if (lex->mi.ssl)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL = "));
    rlb->append(lex->mi.ssl == LEX_MASTER_INFO::LEX_MI_ENABLE ? "1" : "0");
  }
  if (lex->mi.ssl_ca)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL_CA = '"));
    rlb->append(lex->mi.ssl_ca);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.ssl_capath)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL_CAPATH = '"));
    rlb->append(lex->mi.ssl_capath);
    rlb->append(STRING_WITH_LEN("'"));
  }

  if (lex->mi.tls_version)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_TLS_VERSION = '"));
    rlb->append(lex->mi.tls_version);
    rlb->append(STRING_WITH_LEN("'"));
  }

  if (lex->mi.ssl_cert)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL_CERT = '"));
    rlb->append(lex->mi.ssl_cert);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.ssl_cipher)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL_CIPHER = '"));
    rlb->append(lex->mi.ssl_cipher);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.ssl_key)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL_KEY = '"));
    rlb->append(lex->mi.ssl_key);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.log_file_name)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_LOG_FILE = '"));
    rlb->append(lex->mi.log_file_name);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.pos)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_LOG_POS = "));
    rlb->append_ulonglong(lex->mi.pos);
  }
  if (lex->mi.relay_log_name)
  {
    rlb->append(STRING_WITH_LEN(" RELAY_LOG_FILE = '"));
    rlb->append(lex->mi.relay_log_name);
    rlb->append(STRING_WITH_LEN("'"));
  }
  if (lex->mi.relay_log_pos)
  {
    rlb->append(STRING_WITH_LEN(" RELAY_LOG_POS = "));
    rlb->append_ulonglong(lex->mi.relay_log_pos);
  }

  if (lex->mi.ssl_verify_server_cert)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_SSL_VERIFY_SERVER_CERT = "));
    rlb->append(lex->mi.ssl_verify_server_cert == LEX_MASTER_INFO::LEX_MI_ENABLE ? "1" : "0");
  }
  if (lex->mi.repl_ignore_server_ids_opt)
  {
    bool first= TRUE;
    rlb->append(STRING_WITH_LEN(" IGNORE_SERVER_IDS = ( "));
    for (size_t i= 0; i < lex->mi.repl_ignore_server_ids.size(); i++)
    {
      ulong s_id= lex->mi.repl_ignore_server_ids[i];
      if (first)
        first= FALSE;
      else
        rlb->append(STRING_WITH_LEN(", "));
      rlb->append_ulonglong(s_id);
    }
    rlb->append(STRING_WITH_LEN(" )"));
  }
  if (lex->mi.heartbeat_opt != LEX_MASTER_INFO::LEX_MI_UNCHANGED)
  {
    rlb->append(STRING_WITH_LEN(" MASTER_HEARTBEAT_PERIOD = "));
    if (lex->mi.heartbeat_opt == LEX_MASTER_INFO::LEX_MI_DISABLE)
      rlb->append(STRING_WITH_LEN("0"));
    else
    {
      char buf[64];
      my_snprintf(buf, 64, "%f", lex->mi.heartbeat_period);
      rlb->append(buf);
    }
  }
}


/**
  Rewrite a START SLAVE statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_start_slave(THD *thd, String *rlb)
{
  LEX *lex= thd->lex;

  if (!lex->slave_connection.password)
    return;

  rlb->append(STRING_WITH_LEN("START SLAVE"));

  if (lex->slave_thd_opt & SLAVE_IO)
    rlb->append(STRING_WITH_LEN(" IO_THREAD"));

  /* we have printed the IO THREAD related options */
  if (lex->slave_thd_opt & SLAVE_IO && 
      lex->slave_thd_opt & SLAVE_SQL)
    rlb->append(STRING_WITH_LEN(","));

  if (lex->slave_thd_opt & SLAVE_SQL)
    rlb->append(STRING_WITH_LEN(" SQL_THREAD"));

  /* until options */
  if (lex->mi.log_file_name || lex->mi.relay_log_name)
  {
    rlb->append(STRING_WITH_LEN(" UNTIL"));
    if (lex->mi.log_file_name)
    {
      rlb->append(STRING_WITH_LEN(" MASTER_LOG_FILE = '"));
      rlb->append(lex->mi.log_file_name);
      rlb->append(STRING_WITH_LEN("', "));
      rlb->append(STRING_WITH_LEN("MASTER_LOG_POS = "));
      rlb->append_ulonglong(lex->mi.pos);
    }

    if (lex->mi.relay_log_name)
    {
      rlb->append(STRING_WITH_LEN(" RELAY_LOG_FILE = '"));
      rlb->append(lex->mi.relay_log_name);
      rlb->append(STRING_WITH_LEN("', "));
      rlb->append(STRING_WITH_LEN("RELAY_LOG_POS = "));
      rlb->append_ulonglong(lex->mi.relay_log_pos);
    }
  }

  /* connection options */
  if (lex->slave_connection.user)
  {
    rlb->append(STRING_WITH_LEN(" USER = '"));
    rlb->append(lex->slave_connection.user);
    rlb->append(STRING_WITH_LEN("'"));
  }

  if (lex->slave_connection.password)
    rlb->append(STRING_WITH_LEN(" PASSWORD = '<secret>'"));

  if (lex->slave_connection.plugin_auth)
  {
    rlb->append(STRING_WITH_LEN(" DEFAULT_AUTH = '"));
    rlb->append(lex->slave_connection.plugin_auth);
    rlb->append(STRING_WITH_LEN("'"));
  }

  if (lex->slave_connection.plugin_dir)
  {
    rlb->append(STRING_WITH_LEN(" PLUGIN_DIR = '"));
    rlb->append(lex->slave_connection.plugin_dir);
    rlb->append(STRING_WITH_LEN("'"));
  }
}


/**
  Rewrite a SERVER OPTIONS clause (for CREATE SERVER and ALTER SERVER).

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_server_options(THD *thd, String *rlb)
{
  LEX *lex= thd->lex;

  rlb->append(STRING_WITH_LEN(" OPTIONS ( "));

  rlb->append(STRING_WITH_LEN("PASSWORD '<secret>'"));
  append_str(rlb, true, "USER", lex->server_options.get_username());
  append_str(rlb, true, "HOST", lex->server_options.get_host());
  append_str(rlb, true, "DATABASE", lex->server_options.get_db());
  append_str(rlb, true, "OWNER", lex->server_options.get_owner());
  append_str(rlb, true, "SOCKET", lex->server_options.get_socket());
  append_int(rlb, true, STRING_WITH_LEN("PORT "),
             lex->server_options.get_port(),
             lex->server_options.get_port() != Server_options::PORT_NOT_SET);

  rlb->append(STRING_WITH_LEN(" )"));
}


/**
  Rewrite a CREATE SERVER statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_create_server(THD *thd, String *rlb)
{
  LEX *lex= thd->lex;

  if (!lex->server_options.get_password())
    return;

  rlb->append(STRING_WITH_LEN("CREATE SERVER "));

  rlb->append(lex->server_options.m_server_name.str ?
              lex->server_options.m_server_name.str : "");

  rlb->append(STRING_WITH_LEN(" FOREIGN DATA WRAPPER '"));
  rlb->append(lex->server_options.get_scheme() ?
              lex->server_options.get_scheme() : "");
  rlb->append(STRING_WITH_LEN("'"));

  mysql_rewrite_server_options(thd, rlb);
}


/**
  Rewrite a ALTER SERVER statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_alter_server(THD *thd, String *rlb)
{
  LEX *lex= thd->lex;

  if (!lex->server_options.get_password())
    return;

  rlb->append(STRING_WITH_LEN("ALTER SERVER "));

  rlb->append(lex->server_options.m_server_name.str ?
              lex->server_options.m_server_name.str : "");

  mysql_rewrite_server_options(thd, rlb);
}




/**
  Rewrite a PREPARE statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_prepare(THD *thd, String *rlb)
{
  LEX *lex= thd->lex;

  if (lex->prepared_stmt_code_is_varref)
    return;

  rlb->append(STRING_WITH_LEN("PREPARE "));
  rlb->append(lex->prepared_stmt_name.str,
              lex->prepared_stmt_name.length);
  rlb->append(STRING_WITH_LEN(" FROM ..."));
}




/**
   Rewrite a query (to obfuscate passwords etc.)

   Side-effects: thd->rewritten_query will contain a rewritten query,
   or be cleared if no rewriting took place.

   @param thd     The THD to rewrite for.
*/

void mysql_rewrite_query(THD *thd)
{
  String *rlb= &thd->rewritten_query;

  rlb->mem_free();

  if (thd->lex->contains_plaintext_password)
  {
    switch(thd->lex->sql_command)
    {
    case SQLCOM_GRANT:         mysql_rewrite_grant(thd, rlb);         break;
    case SQLCOM_SET_OPTION:    mysql_rewrite_set(thd, rlb);           break;
    case SQLCOM_CREATE_USER:
    case SQLCOM_ALTER_USER:
                        mysql_rewrite_create_alter_user(thd, rlb);    break;
    case SQLCOM_CHANGE_MASTER: mysql_rewrite_change_master(thd, rlb); break;
    case SQLCOM_SLAVE_START:   mysql_rewrite_start_slave(thd, rlb);   break;
    case SQLCOM_CREATE_SERVER: mysql_rewrite_create_server(thd, rlb); break;
    case SQLCOM_ALTER_SERVER:  mysql_rewrite_alter_server(thd, rlb);  break;

    /*
      PREPARE stmt FROM <string> is rewritten so that <string> is
      not logged.  The statement in <string> will in turn be logged
      by the prepare and the execute functions in sql_prepare.cc.
      They do call rewrite so they can safely log the statement,
      but when they call us, it'll be with sql_command set to reflect
      the statement in question, not SQLCOM_PREPARE or SQLCOM_EXECUTE.
      Therefore, there is no SQLCOM_EXECUTE case here, and all
      SQLCOM_PREPARE does is remove <string>; the "other half",
      i.e. printing what string we prepare from happens when the
      prepare function calls the logger (and comes by here with
      sql_command set to the command being prepared).
    */
    case SQLCOM_PREPARE:       mysql_rewrite_prepare(thd, rlb);       break;
    default:                   /* unhandled query types are legal. */ break;
    }
  }
}
