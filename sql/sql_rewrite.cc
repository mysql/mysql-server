/* Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @brief  In here, we rewrite queries.

  For now, this is only used to obfuscate passwords before we log a
  statement.  If we ever get other clients for rewriting, we should
  introduce a rewrite_flags to determine what kind of rewriting
  (password obfuscation etc.) is desired by the client.

  Some items in the server can self-print anyway, but many can't.

  For instance, you'll see a re-synthesized SELECT in EXPLAIN EXTENDED,
  but you won't get a resynthized quer in EXPLAIN EXTENDED if you
  were explaining an UPDATE.

  The following does not claim to be able to re-synthesize every
  statement, but attempts to ultimately be able to resynthesize
  all statements that have need of rewriting.

  Stored procedures may also rewrite their statements (to show the actual
  values of their variables etc.). There is currently no scenario where
  a statement can be eligible for both rewrites (see sp_instr.cc).
  Special consideration will need to be taken if this is intenionally
  changed at a later date.  (There is an ASSERT() in place that will
  hopefully catch unintentional changes.)

  Finally, sp_* have code to print a stored program for use by
  SHOW PROCEDURE CODE / SHOW FUNCTION CODE.

  Thus, regular query parsing comes through here for logging.
  So does prepared statement logging.
  Stored instructions of the sp_instr_stmt type (which should
  be the only ones to contain passwords, and therefore at this
  time be eligible for rewriting) go through the regular parsing
  facilities and therefore also come through here for logging
  (other sp_instr_* types don't).

  Finally, as rewriting goes, we replace the password with its
  hash where we have the latter (so they could be replayed,
  IDENTIFIED BY vs IDENTIFIED BY PASSWORD etc.); if we don't
  have the hash, we replace the password by a literal "<secret>",
  with *no* quotation marks so the statement would fail if the
  user where to cut & paste it without filling in the real password.
*/

#include "sql/sql_rewrite.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <set>
#include <string>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "prealloced_array.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // GRANT_ACL
#include "sql/auth/auth_internal.h"
#include "sql/handler.h"
#include "sql/log_event.h"  // append_query_string
#include "sql/rpl_slave.h"  // SLAVE_SQL, SLAVE_IO
#include "sql/set_var.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_connect.h"
#include "sql/sql_lex.h"  // LEX
#include "sql/sql_list.h"
#include "sql/sql_parse.h"  // get_current_user
#include "sql/sql_servers.h"
#include "sql/sql_show.h"  // append_identifier
#include "sql/table.h"
#include "sql_string.h"  // String
#include "violite.h"

#ifndef DBUG_OFF
#define HASH_STRING_WITH_QUOTE \
  "$5$BVZy9O>'a+2MH]_?$fpWyabcdiHjfCVqId/quykZzjaA7adpkcen/uiQrtmOK4p4"
#endif

/**
  Append a comma to given string if item wasn't the first to be added.

  @param[in,out]  str    The string to (maybe) append to.
  @param[in,out]  comma  If true, there are already items in the list.
                         Always true afterwards.
*/
static inline void comma_maybe(String *str, bool *comma) {
  if (*comma)
    str->append(STRING_WITH_LEN(", "));
  else
    *comma = true;
}

/**
  Append a key/value pair to a string, with an optional preceeding comma.
  For numeric values.

  @param[in,out]   str                  The string to append to
  @param           comma                Prepend a comma?
  @param           txt                  C-string, must end in a space
  @param           len                  strlen(txt)
  @param           val                  numeric value
  @param           cond                 only append if this evaluates to true

  @retval          false if any subsequent key/value pair would be the first
*/

static bool append_int(String *str, bool comma, const char *txt, size_t len,
                       long val, int cond) {
  if (cond) {
    String numbuf(42);
    comma_maybe(str, &comma);
    str->append(txt, len);
    str->append(STRING_WITH_LEN(" "));
    numbuf.set((longlong)val, &my_charset_bin);
    str->append(numbuf);
    return true;
  }
  return comma;
}

/**
  Append a key/value pair to a string if the value is non-NULL,
  with an optional preceding comma.

  @param[in,out]   str                  The string to append to
  @param           comma                Prepend a comma?
  @param           key                  C-string: the key, must be non-NULL
  @param           val                  C-string: the value

  @retval          false if any subsequent key/value pair would be the first
*/

static bool append_str(String *str, bool comma, const char *key,
                       const char *val) {
  if (val) {
    comma_maybe(str, &comma);
    str->append(key);
    str->append(STRING_WITH_LEN(" '"));
    str->append(val);
    str->append(STRING_WITH_LEN("'"));
    return true;
  }
  return comma;
}

/**
  Used with List<>::sort for alphabetic sorting of LEX_USER records
  using user,host as keys.

  @param l1 A LEX_USER element
  @param l2 A LEX_USER element

  @return
    @retval 1 if n1 &gt; n2
    @retval 0 if n1 &lt;= n2
*/
static int lex_user_comp(LEX_USER *l1, LEX_USER *l2) {
  size_t length = std::min(l1->user.length, l2->user.length);
  int key = memcmp(l1->user.str, l2->user.str, length);
  if (key == 0 && l1->user.length == l2->user.length) {
    length = std::min(l1->host.length, l2->host.length);
    key = memcmp(l1->host.str, l2->host.str, length);
    if (key == 0 && l1->host.length == l2->host.length) return 0;
  }
  if (key == 0)
    return (l1->user.length > l2->user.length ? 1 : 0);
  else
    return (key > 0 ? 1 : 0);
}

static void rewrite_default_roles(LEX *lex, String *rlb) {
  bool comma = false;
  if (lex->default_roles && lex->default_roles->elements > 0) {
    rlb->append(" DEFAULT ROLE ");
    lex->default_roles->sort(&lex_user_comp);
    List_iterator<LEX_USER> role_it(*(lex->default_roles));
    LEX_USER *role;
    while ((role = role_it++)) {
      if (comma) rlb->append(',');
      rlb->append(create_authid_str_from(role).c_str());
      comma = true;
    }
  }
}

static void rewrite_ssl_properties(LEX *lex, String *rlb) {
  if (lex->ssl_type != SSL_TYPE_NOT_SPECIFIED) {
    rlb->append(STRING_WITH_LEN(" REQUIRE"));
    switch (lex->ssl_type) {
      case SSL_TYPE_SPECIFIED:
        if (lex->x509_subject) {
          rlb->append(STRING_WITH_LEN(" SUBJECT '"));
          rlb->append(lex->x509_subject);
          rlb->append(STRING_WITH_LEN("'"));
        }
        if (lex->x509_issuer) {
          rlb->append(STRING_WITH_LEN(" ISSUER '"));
          rlb->append(lex->x509_issuer);
          rlb->append(STRING_WITH_LEN("'"));
        }
        if (lex->ssl_cipher) {
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

static void rewrite_user_resources(LEX *lex, String *rlb) {
  if (lex->mqh.specified_limits || (lex->grant & GRANT_ACL)) {
    rlb->append(STRING_WITH_LEN(" WITH"));
    if (lex->grant & GRANT_ACL) rlb->append(STRING_WITH_LEN(" GRANT OPTION"));

    append_int(rlb, false, STRING_WITH_LEN(" MAX_QUERIES_PER_HOUR"),
               lex->mqh.questions,
               lex->mqh.specified_limits & USER_RESOURCES::QUERIES_PER_HOUR);

    append_int(rlb, false, STRING_WITH_LEN(" MAX_UPDATES_PER_HOUR"),
               lex->mqh.updates,
               lex->mqh.specified_limits & USER_RESOURCES::UPDATES_PER_HOUR);

    append_int(
        rlb, false, STRING_WITH_LEN(" MAX_CONNECTIONS_PER_HOUR"),
        lex->mqh.conn_per_hour,
        lex->mqh.specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR);

    append_int(rlb, false, STRING_WITH_LEN(" MAX_USER_CONNECTIONS"),
               lex->mqh.user_conn,
               lex->mqh.specified_limits & USER_RESOURCES::USER_CONNECTIONS);
  }
}

static void rewrite_account_lock(LEX *lex, String *rlb) {
  if (lex->alter_password.account_locked) {
    rlb->append(STRING_WITH_LEN(" ACCOUNT LOCK"));
  } else {
    rlb->append(STRING_WITH_LEN(" ACCOUNT UNLOCK"));
  }
}

/**
  Rewrite a GRANT statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

void mysql_rewrite_grant(THD *thd, String *rlb) {
  LEX *lex = thd->lex;
  TABLE_LIST *first_table = lex->select_lex->table_list.first;
  bool comma = false, comma_inner;
  bool proxy_grant = lex->type == TYPE_ENUM_PROXY;
  String cols(1024);
  int c;

  rlb->append(STRING_WITH_LEN("GRANT "));

  if (proxy_grant)
    rlb->append(STRING_WITH_LEN("PROXY"));
  else if (lex->all_privileges)
    rlb->append(STRING_WITH_LEN("ALL PRIVILEGES"));
  else if (lex->grant_privilege)
    rlb->append(STRING_WITH_LEN("GRANT OPTION"));
  else {
    ulong priv;

    for (c = 0, priv = SELECT_ACL; priv <= GLOBAL_ACLS; c++, priv <<= 1) {
      if (priv == GRANT_ACL) continue;

      comma_inner = false;

      if (lex->columns.elements)  // show columns, if any
      {
        class LEX_COLUMN *column;
        List_iterator<LEX_COLUMN> column_iter(lex->columns);

        cols.length(0);
        cols.append(STRING_WITH_LEN(" ("));

        /*
          If the statement was GRANT SELECT(f2), INSERT(f3), UPDATE(f1,f3, f2),
          our list cols will contain the order f2, f3, f1, and thus that's
          the order we'll recreate the privilege: UPDATE (f2, f3, f1)
        */

        while ((column = column_iter++)) {
          if (column->rights & priv) {
            comma_maybe(&cols, &comma_inner);
            cols.append(column->column.ptr(), column->column.length());
          }
        }
        cols.append(STRING_WITH_LEN(")"));
      }

      if (comma_inner || (lex->grant & priv))  // show privilege name
      {
        comma_maybe(rlb, &comma);
        rlb->append(command_array[c], command_lengths[c]);
        if (!(lex->grant & priv))  // general outranks specific
          rlb->append(cols);
      }
    }
    /* List extended global privilege IDs */
    if (!first_table && !lex->current_select()->db) {
      List_iterator<LEX_CSTRING> it(lex->dynamic_privileges);
      LEX_CSTRING *priv;
      while ((priv = it++)) {
        comma_maybe(rlb, &comma);
        rlb->append(priv->str, priv->length);
      }
    }
    if (!comma)  // no privs, default to USAGE
      rlb->append(STRING_WITH_LEN("USAGE"));
  }

  rlb->append(STRING_WITH_LEN(" ON "));
  switch (lex->type) {
    case TYPE_ENUM_PROCEDURE:
      rlb->append(STRING_WITH_LEN("PROCEDURE "));
      break;
    case TYPE_ENUM_FUNCTION:
      rlb->append(STRING_WITH_LEN("FUNCTION "));
      break;
    default:
      break;
  }

  LEX_USER *user_name, *tmp_user_name;
  List_iterator<LEX_USER> user_list(lex->users_list);
  comma = false;

  if (proxy_grant) {
    tmp_user_name = user_list++;
    user_name = get_current_user(thd, tmp_user_name);
    if (user_name) append_user_new(thd, rlb, user_name, comma);
  } else if (first_table) {
    if (first_table->is_view()) {
      append_identifier(thd, rlb, first_table->view_db.str,
                        first_table->view_db.length);
      rlb->append(STRING_WITH_LEN("."));
      append_identifier(thd, rlb, first_table->view_name.str,
                        first_table->view_name.length);
    } else {
      append_identifier(thd, rlb, first_table->db, strlen(first_table->db));
      rlb->append(STRING_WITH_LEN("."));
      append_identifier(thd, rlb, first_table->table_name,
                        strlen(first_table->table_name));
    }
  } else {
    if (lex->current_select()->db)
      append_identifier(thd, rlb, lex->current_select()->db,
                        strlen(lex->current_select()->db));
    else
      rlb->append("*");
    rlb->append(STRING_WITH_LEN(".*"));
  }

  rlb->append(STRING_WITH_LEN(" TO "));

  while ((tmp_user_name = user_list++)) {
    if ((user_name = get_current_user(thd, tmp_user_name))) {
      append_user_new(thd, rlb, user_name, comma);
      comma = true;
    }
  }
  rewrite_ssl_properties(lex, rlb);
  rewrite_user_resources(lex, rlb);
}

/**
  Rewrite a SET statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_set(THD *thd, String *rlb) {
  LEX *lex = thd->lex;
  List_iterator_fast<set_var_base> it(lex->var_list);
  set_var_base *var;
  bool comma = false;

  rlb->append(STRING_WITH_LEN("SET "));

  while ((var = it++)) {
    comma_maybe(rlb, &comma);
    var->print(thd, rlb);
  }
}

/**
  Rewrite SET PASSWORD for binary log

  @param thd         The THD to rewrite for.
  @param rlb         An empty string object to put rewritten query in.
  @param users       List of users
  @param for_binlog  Whether rewrite is for binlog or not
*/
void mysql_rewrite_set_password(THD *thd, String *rlb,
                                std::set<LEX_USER *> *users,
                                bool for_binlog) /* = false */
{
  bool set_temp_string = false;
  /*
    Setting this flag will generate the password hash string which
    contains a single quote.
  */
  DBUG_EXECUTE_IF("force_hash_string_with_quote", set_temp_string = true;);
  if (!for_binlog)
    mysql_rewrite_set(thd, rlb);
  else {
    if (users->size()) {
      /* SET PASSWORD should always have one user */
      DBUG_ASSERT(users->size() == 1);
      LEX_USER *user = *(users->begin());
      String current_user(user->user.str, user->user.length,
                          system_charset_info);
      String current_host(user->host.str, user->host.length,
                          system_charset_info);
      String auth_str;
      if (set_temp_string) {
#ifndef DBUG_OFF
        auth_str = String(HASH_STRING_WITH_QUOTE,
                          strlen(HASH_STRING_WITH_QUOTE), system_charset_info);
#endif
      } else {
        auth_str =
            String(user->auth.str, user->auth.length, system_charset_info);
      }
      /*
        Construct :
        ALTER USER '<user>'@'<host>' IDENTIFIED WITH '<plugin>' AS '<HASH>'
      */
      rlb->append(STRING_WITH_LEN("ALTER USER "));
      append_query_string(thd, system_charset_info, &current_user, rlb);
      rlb->append(STRING_WITH_LEN("@"));
      append_query_string(thd, system_charset_info, &current_host, rlb);
      rlb->append(STRING_WITH_LEN(" IDENTIFIED WITH '"));
      rlb->append(user->plugin.str);
      rlb->append(STRING_WITH_LEN("' AS "));
      append_query_string(thd, system_charset_info, &auth_str, rlb);
    }
  }
}

/**
  Rewrite CREATE/ALTER USER statement.

  @param thd      The THD to rewrite for.
  @param rlb      An empty String object to put the rewritten query in.
  @param users_not_to_log Members of this list are not added to the generated
                           statement.
  @param for_binlog We don't skip any user while writing to binlog
*/

void mysql_rewrite_create_alter_user(THD *thd, String *rlb,
                                     std::set<LEX_USER *> *users_not_to_log,
                                     bool for_binlog, bool hide_password_hash) {
  LEX *lex = thd->lex;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator<LEX_USER> user_list(lex->users_list);
  bool comma = false;

  if (thd->lex->sql_command == SQLCOM_CREATE_USER ||
      thd->lex->sql_command == SQLCOM_SHOW_CREATE_USER)
    rlb->append(STRING_WITH_LEN("CREATE USER "));
  else
    rlb->append(STRING_WITH_LEN("ALTER USER "));

  if (thd->lex->sql_command == SQLCOM_CREATE_USER &&
      thd->lex->create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    rlb->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (thd->lex->sql_command == SQLCOM_ALTER_USER && thd->lex->drop_if_exists)
    rlb->append(STRING_WITH_LEN("IF EXISTS "));

  while ((tmp_user_name = user_list++)) {
    if (!for_binlog && users_not_to_log &&
        users_not_to_log->find(tmp_user_name) != users_not_to_log->end())
      continue;
    if ((user_name = get_current_user(thd, tmp_user_name))) {
      append_user_new(thd, rlb, user_name, comma, hide_password_hash);
      comma = true;
    }
  }

  if (thd->lex->sql_command == SQLCOM_SHOW_CREATE_USER)
    rewrite_default_roles(lex, rlb);
  rewrite_ssl_properties(lex, rlb);
  rewrite_user_resources(lex, rlb);

  /* rewrite password expired */
  if (lex->alter_password.update_password_expired_fields) {
    if (lex->alter_password.update_password_expired_column) {
      rlb->append(STRING_WITH_LEN(" PASSWORD EXPIRE"));
    } else if (lex->alter_password.expire_after_days) {
      append_int(rlb, false, STRING_WITH_LEN(" PASSWORD EXPIRE INTERVAL"),
                 lex->alter_password.expire_after_days, true);
      rlb->append(STRING_WITH_LEN(" DAY"));
    } else if (lex->alter_password.use_default_password_lifetime) {
      rlb->append(STRING_WITH_LEN(" PASSWORD EXPIRE DEFAULT"));
    } else {
      rlb->append(STRING_WITH_LEN(" PASSWORD EXPIRE NEVER"));
    }
  }

  if (lex->alter_password.update_account_locked_column) {
    rewrite_account_lock(lex, rlb);
  }

  if (!for_binlog || lex->alter_password.update_password_history) {
    if (lex->alter_password.use_default_password_history) {
      rlb->append(STRING_WITH_LEN(" PASSWORD HISTORY DEFAULT"));
    } else {
      append_int(rlb, false, STRING_WITH_LEN(" PASSWORD HISTORY"),
                 lex->alter_password.password_history_length, true);
    }
  }

  if (!for_binlog || lex->alter_password.update_password_reuse_interval) {
    if (lex->alter_password.use_default_password_reuse_interval) {
      rlb->append(STRING_WITH_LEN(" PASSWORD REUSE INTERVAL DEFAULT"));
    } else {
      append_int(rlb, false, STRING_WITH_LEN(" PASSWORD REUSE INTERVAL"),
                 lex->alter_password.password_reuse_interval, true);
      rlb->append(STRING_WITH_LEN(" DAY"));
    }
  }
}

/**
  Rewrite a CHANGE MASTER statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_change_master(THD *thd, String *rlb) {
  LEX *lex = thd->lex;
  bool comma = false;

  rlb->append(STRING_WITH_LEN("CHANGE MASTER TO "));

  comma = append_str(rlb, comma, "MASTER_BIND =", lex->mi.bind_addr);
  comma = append_str(rlb, comma, "MASTER_HOST =", lex->mi.host);
  comma = append_str(rlb, comma, "MASTER_USER =", lex->mi.user);

  if (lex->mi.password) {
    comma_maybe(rlb, &comma);
    rlb->append(STRING_WITH_LEN("MASTER_PASSWORD = <secret>"));
  }
  comma = append_int(rlb, comma, STRING_WITH_LEN("MASTER_PORT ="), lex->mi.port,
                     lex->mi.port > 0);
  // condition as per rpl_slave.cc
  comma = append_int(rlb, comma, STRING_WITH_LEN("MASTER_CONNECT_RETRY ="),
                     lex->mi.connect_retry, lex->mi.connect_retry > 0);
  comma = append_int(
      rlb, comma, STRING_WITH_LEN("MASTER_RETRY_COUNT ="), lex->mi.retry_count,
      lex->mi.retry_count_opt != LEX_MASTER_INFO::LEX_MI_UNCHANGED);
  // MASTER_DELAY 0..MASTER_DELAY_MAX; -1 == unspecified
  comma = append_int(rlb, comma, STRING_WITH_LEN("MASTER_DELAY ="),
                     lex->mi.sql_delay, lex->mi.sql_delay >= 0);

  if (lex->mi.heartbeat_opt != LEX_MASTER_INFO::LEX_MI_UNCHANGED) {
    comma_maybe(rlb, &comma);
    rlb->append(STRING_WITH_LEN("MASTER_HEARTBEAT_PERIOD = "));
    if (lex->mi.heartbeat_opt == LEX_MASTER_INFO::LEX_MI_DISABLE)
      rlb->append(STRING_WITH_LEN("0"));
    else {
      char buf[64];
      snprintf(buf, 64, "%f", lex->mi.heartbeat_period);
      rlb->append(buf);
    }
  }

  // log file (slave I/O thread)
  comma = append_str(rlb, comma, "MASTER_LOG_FILE =", lex->mi.log_file_name);
  // MASTER_LOG_POS is >= BIN_LOG_HEADER_SIZE; 0 == unspecified in stmt.
  comma = append_int(rlb, comma, STRING_WITH_LEN("MASTER_LOG_POS ="),
                     lex->mi.pos, lex->mi.pos != 0);
  comma = append_int(
      rlb, comma, STRING_WITH_LEN("MASTER_AUTO_POSITION ="),
      (lex->mi.auto_position == LEX_MASTER_INFO::LEX_MI_ENABLE) ? 1 : 0,
      lex->mi.auto_position != LEX_MASTER_INFO::LEX_MI_UNCHANGED);

  // log file (slave SQL thread)
  comma = append_str(rlb, comma, "RELAY_LOG_FILE =", lex->mi.relay_log_name);
  // RELAY_LOG_POS is >= BIN_LOG_HEADER_SIZE; 0 == unspecified in stmt.
  comma = append_int(rlb, comma, STRING_WITH_LEN("RELAY_LOG_POS ="),
                     lex->mi.relay_log_pos, lex->mi.relay_log_pos != 0);

  // SSL
  comma = append_int(rlb, comma, STRING_WITH_LEN("MASTER_SSL ="),
                     lex->mi.ssl == LEX_MASTER_INFO::LEX_MI_ENABLE ? 1 : 0,
                     lex->mi.ssl != LEX_MASTER_INFO::LEX_MI_UNCHANGED);
  comma = append_str(rlb, comma, "MASTER_SSL_CA =", lex->mi.ssl_ca);
  comma = append_str(rlb, comma, "MASTER_SSL_CAPATH =", lex->mi.ssl_capath);
  comma = append_str(rlb, comma, "MASTER_SSL_CERT =", lex->mi.ssl_cert);
  comma = append_str(rlb, comma, "MASTER_SSL_CRL =", lex->mi.ssl_crl);
  comma = append_str(rlb, comma, "MASTER_SSL_CRLPATH =", lex->mi.ssl_crlpath);
  comma = append_str(rlb, comma, "MASTER_SSL_KEY =", lex->mi.ssl_key);
  comma = append_str(rlb, comma, "MASTER_SSL_CIPHER =", lex->mi.ssl_cipher);
  comma = append_int(
      rlb, comma, STRING_WITH_LEN("MASTER_SSL_VERIFY_SERVER_CERT ="),
      (lex->mi.ssl_verify_server_cert == LEX_MASTER_INFO::LEX_MI_ENABLE) ? 1
                                                                         : 0,
      lex->mi.ssl_verify_server_cert != LEX_MASTER_INFO::LEX_MI_UNCHANGED);

  comma = append_str(rlb, comma, "MASTER_TLS_VERSION =", lex->mi.tls_version);

  // Public key
  comma = append_str(rlb, comma,
                     "MASTER_PUBLIC_KEY_PATH =", lex->mi.public_key_path);
  comma = append_int(
      rlb, comma, STRING_WITH_LEN("GET_MASTER_PUBLIC_KEY ="),
      (lex->mi.get_public_key == LEX_MASTER_INFO::LEX_MI_ENABLE) ? 1 : 0,
      lex->mi.get_public_key != LEX_MASTER_INFO::LEX_MI_UNCHANGED);

  // IGNORE_SERVER_IDS
  if (lex->mi.repl_ignore_server_ids_opt != LEX_MASTER_INFO::LEX_MI_UNCHANGED) {
    bool comma_list = false;

    comma_maybe(rlb, &comma);
    rlb->append(STRING_WITH_LEN("IGNORE_SERVER_IDS = ( "));

    for (size_t i = 0; i < lex->mi.repl_ignore_server_ids.size(); i++) {
      ulong s_id = lex->mi.repl_ignore_server_ids[i];
      comma_maybe(rlb, &comma_list);
      rlb->append_ulonglong(s_id);
    }
    rlb->append(STRING_WITH_LEN(" )"));
  }

  /* channel options -- no preceding comma here! */
  if (lex->mi.for_channel)
    append_str(rlb, false, " FOR CHANNEL", lex->mi.channel);
}

/**
  Rewrite a START SLAVE statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_start_slave(THD *thd, String *rlb) {
  LEX *lex = thd->lex;

  rlb->append(STRING_WITH_LEN("START SLAVE"));

  /* thread_types */

  if (lex->slave_thd_opt & SLAVE_IO) rlb->append(STRING_WITH_LEN(" IO_THREAD"));

  if (lex->slave_thd_opt & SLAVE_IO && lex->slave_thd_opt & SLAVE_SQL)
    rlb->append(STRING_WITH_LEN(","));

  if (lex->slave_thd_opt & SLAVE_SQL)
    rlb->append(STRING_WITH_LEN(" SQL_THREAD"));

  /* UNTIL options */

  // GTID
  if (lex->mi.gtid) {
    rlb->append((lex->mi.gtid_until_condition ==
                 LEX_MASTER_INFO::UNTIL_SQL_BEFORE_GTIDS)
                    ? " UNTIL SQL_BEFORE_GTIDS"
                    : " UNTIL SQL_AFTER_GTIDS");
    append_str(rlb, false, " =", lex->mi.gtid);
  }

  // SQL_AFTER_MTS_GAPS
  else if (lex->mi.until_after_gaps) {
    rlb->append(STRING_WITH_LEN(" UNTIL SQL_AFTER_MTS_GAPS"));
  }

  // MASTER_LOG_FILE/POS
  else if (lex->mi.log_file_name) {
    append_str(rlb, false, " UNTIL MASTER_LOG_FILE =", lex->mi.log_file_name);
    append_int(rlb, true, STRING_WITH_LEN("MASTER_LOG_POS ="), lex->mi.pos,
               lex->mi.pos > 0);
  }

  // RELAY_LOG_FILE/POS
  else if (lex->mi.relay_log_name) {
    append_str(rlb, false, " UNTIL RELAY_LOG_FILE =", lex->mi.relay_log_name);
    append_int(rlb, true, STRING_WITH_LEN("RELAY_LOG_POS ="),
               lex->mi.relay_log_pos, lex->mi.relay_log_pos > 0);
  }

  /* connection options */
  append_str(rlb, false, " USER =", lex->slave_connection.user);

  if (lex->slave_connection.password)
    rlb->append(STRING_WITH_LEN(" PASSWORD = <secret>"));

  append_str(rlb, false, " DEFAULT_AUTH =", lex->slave_connection.plugin_auth);
  append_str(rlb, false, " PLUGIN_DIR =", lex->slave_connection.plugin_dir);

  /* channel options */
  if (lex->mi.for_channel)
    append_str(rlb, false, " FOR CHANNEL", lex->mi.channel);
}

/**
  Rewrite a SERVER OPTIONS clause (for CREATE SERVER and ALTER SERVER).

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_server_options(THD *thd, String *rlb) {
  LEX *lex = thd->lex;

  rlb->append(STRING_WITH_LEN(" OPTIONS ( "));

  rlb->append(STRING_WITH_LEN("PASSWORD <secret>"));
  append_str(rlb, true, "USER", lex->server_options.get_username());
  append_str(rlb, true, "HOST", lex->server_options.get_host());
  append_str(rlb, true, "DATABASE", lex->server_options.get_db());
  append_str(rlb, true, "OWNER", lex->server_options.get_owner());
  append_str(rlb, true, "SOCKET", lex->server_options.get_socket());
  append_int(rlb, true, STRING_WITH_LEN("PORT"), lex->server_options.get_port(),
             lex->server_options.get_port() != Server_options::PORT_NOT_SET);

  rlb->append(STRING_WITH_LEN(" )"));
}

/**
  Rewrite a CREATE SERVER statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_create_server(THD *thd, String *rlb) {
  LEX *lex = thd->lex;

  if (!lex->server_options.get_password()) return;

  rlb->append(STRING_WITH_LEN("CREATE SERVER "));

  rlb->append(lex->server_options.m_server_name.str
                  ? lex->server_options.m_server_name.str
                  : "");

  rlb->append(STRING_WITH_LEN(" FOREIGN DATA WRAPPER '"));
  rlb->append(
      lex->server_options.get_scheme() ? lex->server_options.get_scheme() : "");
  rlb->append(STRING_WITH_LEN("'"));

  mysql_rewrite_server_options(thd, rlb);
}

/**
  Rewrite a ALTER SERVER statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_alter_server(THD *thd, String *rlb) {
  LEX *lex = thd->lex;

  if (!lex->server_options.get_password()) return;

  rlb->append(STRING_WITH_LEN("ALTER SERVER "));

  rlb->append(lex->server_options.m_server_name.str
                  ? lex->server_options.m_server_name.str
                  : "");

  mysql_rewrite_server_options(thd, rlb);
}

/**
  Rewrite a PREPARE statement.

  @param thd          The THD to rewrite for.
  @param [in,out] rlb An empty String object to put the rewritten query in.
*/

static void mysql_rewrite_prepare(THD *thd, String *rlb) {
  LEX *lex = thd->lex;

  if (lex->prepared_stmt_code_is_varref) return;

  rlb->append(STRING_WITH_LEN("PREPARE "));
  rlb->append(lex->prepared_stmt_name.str, lex->prepared_stmt_name.length);
  rlb->append(STRING_WITH_LEN(" FROM ..."));
}

/**
   Rewrite a query (to obfuscate passwords etc.)

   Side-effects: thd->rewritten_query will contain a rewritten query,
   or be cleared if no rewriting took place.

   @param thd     The THD to rewrite for.
*/

void mysql_rewrite_query(THD *thd) {
  String *rlb = &thd->rewritten_query;

  rlb->mem_free();

  if (thd->lex->contains_plaintext_password) {
    switch (thd->lex->sql_command) {
      case SQLCOM_GRANT:
        mysql_rewrite_grant(thd, rlb);
        break;
      case SQLCOM_SET_PASSWORD:
      case SQLCOM_SET_OPTION:
        mysql_rewrite_set(thd, rlb);
        break;
      case SQLCOM_CREATE_USER:
      case SQLCOM_ALTER_USER:
        mysql_rewrite_create_alter_user(thd, rlb);
        break;
      case SQLCOM_CHANGE_MASTER:
        mysql_rewrite_change_master(thd, rlb);
        break;
      case SQLCOM_SLAVE_START:
        mysql_rewrite_start_slave(thd, rlb);
        break;
      case SQLCOM_CREATE_SERVER:
        mysql_rewrite_create_server(thd, rlb);
        break;
      case SQLCOM_ALTER_SERVER:
        mysql_rewrite_alter_server(thd, rlb);
        break;

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
      case SQLCOM_PREPARE:
        mysql_rewrite_prepare(thd, rlb);
        break;
      default: /* unhandled query types are legal. */
        break;
    }
  }
}
