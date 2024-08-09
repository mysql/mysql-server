/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql_thd_attributes_imp.h"

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/bits/mysql_thd_attributes_bits.h>
#include <mysql/components/services/defs/event_tracking_common_defs.h>
#include <mysql/components/services/mysql_string.h>
#include <sql_string.h>
#include "sql/command_mapping.h"
#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "sql/sql_digest.h"
#include "sql/sql_lex.h"
#include "sql/sql_rewrite.h"
#include "sql/tztime.h"

DEFINE_BOOL_METHOD(mysql_thd_attributes_imp::get,
                   (MYSQL_THD thd, const char *name, void *inout_pvalue)) {
  try {
    if (inout_pvalue) {
      THD *t = static_cast<THD *>(thd);
      if (t == nullptr) t = current_thd;
      if (t == nullptr) return true;

      if (!strcmp(name, "thd_status")) {
        *((uint16_t *)inout_pvalue) = [](auto state) {
          switch (state) {
            case THD::killed_state::NOT_KILLED:
              return STATUS_SESSION_OK;
            case THD::killed_state::KILL_CONNECTION:
              return STATUS_SESSION_KILLED;
            case THD::killed_state::KILL_QUERY:
              return STATUS_QUERY_KILLED;
            case THD::killed_state::KILL_TIMEOUT:
              return STATUS_QUERY_TIMEOUT;
            case THD::killed_state::KILLED_NO_VALUE:
              return STATUS_SESSION_OK;
            default:
              return STATUS_SESSION_OK;
          }
        }(t->is_killed());
      } else if (!strcmp(name, "query_digest")) {
        if (t->m_digest == nullptr) return true;

        String *res = new String[1];

        compute_digest_text(&t->m_digest->m_digest_storage, res);

        /* compute_digest_text returns string as to utf8. */
        res->set_charset(&my_charset_utf8mb3_bin);

        *((my_h_string *)inout_pvalue) = (my_h_string)res;
      } else if (!strcmp(name, "is_upgrade_thread")) {
        *((bool *)inout_pvalue) = t->is_server_upgrade_thread();
      } else if (!strcmp(name, "is_init_file_thread")) {
        *((bool *)inout_pvalue) = t->is_init_file_system_thread();
      } else if (!strcmp(name, "sql_text")) {
        /*
          If we haven't tried to rewrite the query
          to obfuscate passwords etc. yet, do so now.
        */

        if (t->rewritten_query().length() == 0) mysql_rewrite_query(t);

        String *res = new String[1];
        /*
          If there was something to rewrite, use the rewritten query;
          otherwise, just use the original as submitted by the client.
        */

        if (t->rewritten_query().length() > 0) {
          res->append(t->rewritten_query().ptr(), t->rewritten_query().length(),
                      t->rewritten_query().charset());
        } else if (t->query().length > 0) {
          res->append(t->query().str, t->query().length, t->charset());
        }
        *((my_h_string *)inout_pvalue) = (my_h_string)res;
      } else if (!strcmp(name, "host_or_ip")) {
        Security_context *ctx = t->security_context();
        const char *host = (ctx != nullptr && ctx->host_or_ip().length)
                               ? ctx->host_or_ip().str
                               : "";
        String *res = new String[1];
        res->append(host, strlen(host));
        *((my_h_string *)inout_pvalue) = (my_h_string)res;
      } else if (!strcmp(name, "schema")) {
        String *res = new String[1];
        res->append(t->db().str, t->db().length);
        *((my_h_string *)inout_pvalue) = (my_h_string)res;
      } else if (!strcmp(name, "query_charset")) {
        mysql_cstring_with_length val;
        if (t->rewritten_query().length()) {
          val.str = t->rewritten_query().charset()->csname;
          val.length = strlen(t->rewritten_query().charset()->csname);
        } else {
          val.str = t->charset()->csname;
          val.length = strlen(t->charset()->csname);
        }
        *((mysql_cstring_with_length *)inout_pvalue) = val;
      } else if (!strcmp(name, "collation_connection_charset")) {
        auto collation_charset = t->variables.collation_connection->csname;
        auto val = mysql_cstring_with_length{collation_charset,
                                             strlen(collation_charset)};
        *((mysql_cstring_with_length *)inout_pvalue) = val;
      } else if (!strcmp(name, "sql_command")) {
        const char *sql_command = get_sql_command_string(t->lex->sql_command);
        if (t->lex->sql_command == SQLCOM_END &&
            t->get_command() != COM_QUERY) {
          *((mysql_cstring_with_length *)inout_pvalue) = {"", strlen("")};
        } else {
          *((mysql_cstring_with_length *)inout_pvalue) = {sql_command,
                                                          strlen(sql_command)};
        }
      } else if (!strcmp(name, "command")) {
        const char *command = get_server_command_string(t->get_command());
        *((mysql_cstring_with_length *)inout_pvalue) = {command,
                                                        strlen(command)};
      } else if (!strcmp(name, "time_zone_name")) {
        *reinterpret_cast<MYSQL_LEX_CSTRING *>(inout_pvalue) =
            t->time_zone()->get_name()->lex_cstring();
      } else if (!strcmp(name, "da_status")) {
        *((uint16_t *)inout_pvalue) = [&t](auto status) {
          switch (status) {
            case Diagnostics_area::DA_EMPTY:
              return STATUS_DA_EMPTY;
            case Diagnostics_area::DA_OK:
              return STATUS_DA_OK;
            case Diagnostics_area::DA_EOF:
              return STATUS_DA_EOF;
            case Diagnostics_area::DA_ERROR:
              if (t->is_fatal_error()) {
                return STATUS_DA_FATAL_ERROR;
              }
              return STATUS_DA_ERROR;
            case Diagnostics_area::DA_DISABLED:
              return STATUS_DA_DISABLED;
            default:
              return STATUS_DA_OK;
          }
        }(t->get_stmt_da()->status());
      } else
        return true; /* invalid option */
    }
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(mysql_thd_attributes_imp::set,
                   (MYSQL_THD thd [[maybe_unused]],
                    const char *name [[maybe_unused]],
                    void *inout_pvalue [[maybe_unused]])) {
  return true;
}

/*
DEFINE_BOOL_METHOD(mysql_thd_const_attributes_imp::get,
                   (MYSQL_THD o_thd, const char *name,
                    const void *inout_pvalue)) {
  try {
    if (name && inout_pvalue) {
      THD *thd = static_cast<THD *>(o_thd);
      if (!thd) thd = current_thd;
      if (thd == nullptr) return true;

      if (!strcmp(name, "query_charset")) {
        *((char **)inout_pvalue) =
            thd->rewritten_query().length()
                ? thd->rewritten_query().charset()->csname
                : thd->charset()->csname;
      } else if (!strcmp(name, "sql_command")) {
        *((const char **)inout_pvalue) =
            get_sql_command_string(thd->lex->sql_command);
      } else if (!strcmp(name, "command")) {
        *((const char **)inout_pvalue) =
            get_server_command_string(thd->get_command());
      } else
        return true;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
*/
