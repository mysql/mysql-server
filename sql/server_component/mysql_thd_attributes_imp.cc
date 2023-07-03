/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mysql_thd_attributes_imp.h"

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/mysql_string.h>
#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "sql/sql_digest.h"

DEFINE_BOOL_METHOD(mysql_thd_attributes_imp::get,
                   (MYSQL_THD thd, const char *name, void *inout_pvalue)) {
  try {
    if (inout_pvalue) {
      THD *t = static_cast<THD *>(thd);
      if (t == nullptr) return true;

      if (!strcmp(name, "query_digest")) {
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
        String *res = new String[1];
        res->append(t->query().str, t->query().length);
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
