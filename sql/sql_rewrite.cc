/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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


#include "sql_acl.h"    // append_user
#include "sql_parse.h"  // get_current_user
#include "sql_show.h"   // append_identifier
#include "sp_head.h"    // struct set_var_base
#include "rpl_slave.h"  // SLAVE_SQL, SLAVE_IO


static void mysql_rewrite_grant(THD *thd, String *rlb)
{
  LEX        *lex= thd->lex;
  TABLE_LIST *first_table= (TABLE_LIST*) lex->select_lex.table_list.first;
  bool        comma= FALSE, comma_inner;
  String      cols(1024);
  int         c;

  rlb->append(STRING_WITH_LEN("GRANT "));

  if (lex->all_privileges)
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
            cols.append(column->column.ptr(),column->column.length());
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

  if (first_table)
  {
    append_identifier(thd, rlb, first_table->db, strlen(first_table->db));
    rlb->append(STRING_WITH_LEN("."));
    append_identifier(thd, rlb, first_table->table_name,
                      strlen(first_table->table_name));
  }
  else
  {
    if (lex->current_select->db)
      append_identifier(thd, rlb, lex->current_select->db,
                        strlen(lex->current_select->db));
    else
      rlb->append("*");
    rlb->append(STRING_WITH_LEN(".*"));
  }

  rlb->append(STRING_WITH_LEN(" TO "));
  {
    LEX_USER *user_name, *tmp_user_name;
    List_iterator <LEX_USER> user_list(lex->users_list);
    bool comma= FALSE;

    while ((tmp_user_name= user_list++))
    {
      if ((user_name= get_current_user(thd, tmp_user_name)))
      {
        append_user(thd, rlb, user_name, comma, true);
        comma= TRUE;
      }
    }
  }

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

  if (lex->mqh.specified_limits || (lex->grant & GRANT_ACL))
  {
    rlb->append(STRING_WITH_LEN(" WITH"));
    if (lex->grant & GRANT_ACL)
      rlb->append(STRING_WITH_LEN(" GRANT OPTION"));

    append_int(rlb, STRING_WITH_LEN(" MAX_QUERIES_PER_HOUR "),
               lex->mqh.questions,
               lex->mqh.specified_limits & USER_RESOURCES::QUERIES_PER_HOUR);

    append_int(rlb, STRING_WITH_LEN(" MAX_UPDATES_PER_HOUR "),
               lex->mqh.updates,
               lex->mqh.specified_limits & USER_RESOURCES::UPDATES_PER_HOUR);

    append_int(rlb, STRING_WITH_LEN(" MAX_CONNECTIONS_PER_HOUR "),
               lex->mqh.conn_per_hour,
               lex->mqh.specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR);

    append_int(rlb, STRING_WITH_LEN(" MAX_USER_CONNECTIONS "),
               lex->mqh.user_conn,
               lex->mqh.specified_limits & USER_RESOURCES::USER_CONNECTIONS);
  }
}



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


static void mysql_rewrite_create_user(THD *thd, String *rlb)
{
  LEX                      *lex= thd->lex;
  LEX_USER                 *user_name, *tmp_user_name;
  List_iterator <LEX_USER>  user_list(lex->users_list);
  bool                      comma= FALSE;

  rlb->append(STRING_WITH_LEN("CREATE USER "));
  while ((tmp_user_name= user_list++))
  {
    if ((user_name= get_current_user(thd, tmp_user_name)))
    {
      append_user(thd, rlb, user_name, comma, TRUE);
      comma= TRUE;
    }
  }
}


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
    for (uint i= 0; i < lex->mi.repl_ignore_server_ids.elements; i++)
    {
      ulong s_id;
      get_dynamic(&lex->mi.repl_ignore_server_ids, (uchar*) &s_id, i);
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
      snprintf(buf, 64, "%f", lex->mi.heartbeat_period);
      rlb->append(buf);
    }
  }
}

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
Rewrite a query (to obfuscate passwords etc.)
@param thd Current thread
*/

void mysql_rewrite_query(THD *thd)
{
  String *rlb= &thd->rewritten_query;

  rlb->free();

  switch(thd->lex->sql_command)
  {
  case SQLCOM_GRANT:         mysql_rewrite_grant(thd, rlb);         break;
  case SQLCOM_SET_OPTION:    mysql_rewrite_set(thd, rlb);           break;
  case SQLCOM_CREATE_USER:   mysql_rewrite_create_user(thd, rlb);   break;
  case SQLCOM_CHANGE_MASTER: mysql_rewrite_change_master(thd, rlb); break;
  case SQLCOM_SLAVE_START:   mysql_rewrite_start_slave(thd, rlb);   break;
  default:                   /* unhandled query types are legal. */ break;
  }
}
