/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "sql/sql_cmd_ddl.h"

#include <string>
#include <utility>

#include "lex_string.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/my_service.h"
#include "mysql/components/service.h"
#include "mysql/components/services/defs/mysql_string_defs.h"
#include "mysql/components/services/language_service.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/derror.h"
#include "sql/enum_query_type.h"
#include "sql/item_cmpfunc.h"
#include "sql/mysqld.h"
#include "sql/parser_yystype.h"
#include "sql/psi_memory_key.h"
#include "sql/sp.h"
#include "sql/sp_head.h"  // close_thread_tables
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_masking_policy.h"
#include "sql/sql_parse.h"
#include "sql/sql_table.h"  // write_bin_log
#include "sql_string.h"
#include "string_with_len.h"

namespace {
bool check_supported_languages(
    my_service<SERVICE_TYPE(external_library)> *library_service,
    LEX_CSTRING &language) {
  assert(library_service->is_valid());
  bool supported = false;
  if ((*library_service)
          ->is_language_supported({language.str, language.length}, &supported))
    return true;
  if (!supported) {
    my_error(ER_LIBRARIES_NOT_SUPPORTED, MYF(0), language.str);
    return true;
  }
  return false;
}
}  // namespace

Sql_cmd_create_library::Sql_cmd_create_library(
    THD *thd, bool if_not_exists, sp_name *name, LEX_CSTRING comment,
    LEX_CSTRING language, LEX_STRING source_code, bool is_binary)
    : m_if_not_exists{if_not_exists},
      m_name{name},
      m_language{language},
      m_source{thd->strmake(source_code.str, source_code.length),
               source_code.length},
      m_comment{thd->strmake(comment.str, comment.length), comment.length},
      m_is_binary{is_binary} {}

bool Sql_cmd_create_library::execute(THD *thd) {
  // check DB access
  if (check_access(thd, CREATE_PROC_ACL, m_name->m_db.str, nullptr, nullptr,
                   false, false)) {
    return true;
  }

  LEX *lex = thd->lex;
  // Unconditionally creates default definer, since CREATE LIBRARY does not
  // specify an explicit definer and lex->definer may be uninitialized (unlike
  // for SPs).
  {
    lex->definer = create_default_definer(thd);
    /* Error has been already reported. */
    if (lex->definer == nullptr) return true;
  }

  if (srv_registry == nullptr) {
    my_error(ER_LANGUAGE_COMPONENT_NOT_AVAILABLE, MYF(0));
    return true;
  }

  my_service<SERVICE_TYPE(external_library)> library_service("external_library",
                                                             srv_registry);
  my_service<SERVICE_TYPE(external_library_ext)> library_service_ext(
      "external_library_ext", srv_registry);
  if (!library_service.is_valid() || !library_service_ext.is_valid()) {
    push_warning(thd, ER_LANGUAGE_COMPONENT_NOT_AVAILABLE);
  } else if (::check_supported_languages(&library_service, m_language)) {
    return true;
  }

  st_sp_chistics sp_chistics;
  sp_chistics.language = m_language;
  sp_chistics.comment = m_comment;
  sp_chistics.is_binary = m_is_binary;

  // A new MEM_ROOT is needed and consumed by the sp_head constructor.
  MEM_ROOT own_root(key_memory_sp_head_main_root, MEM_ROOT_BLOCK_SIZE);
  sp_head sp(std::move(own_root), enum_sp_type::LIBRARY);
  sp.init_sp_name(thd, m_name);
  sp.m_chistics = &sp_chistics;

  mysql_cstring_with_length src_to_parse;

  if (m_is_binary) {
    // Store the binary literal as provided.
    sp.m_body = m_source;
    sp.m_body_utf8 = EMPTY_CSTR;
    src_to_parse = mysql_cstring_with_length{sp.m_body.str, sp.m_body.length};
  } else {
    LEX_STRING body;
    thd->convert_string(&body, &my_charset_utf8mb4_general_ci, m_source.str,
                        m_source.length, thd->charset());
    sp.m_body = to_lex_cstring(body);
    LEX_STRING body_utf8;
    thd->convert_string(&body_utf8, &my_charset_utf8mb3_general_ci, body.str,
                        body.length, &my_charset_utf8mb4_general_ci);
    sp.m_body_utf8 = to_lex_cstring(body_utf8);

    // parsing has to be on null-terminated sp body, not the source coming
    // from the parser
    src_to_parse =
        mysql_cstring_with_length{sp.m_body_utf8.str, sp.m_body_utf8.length};
  }

  if (library_service_ext.is_valid()) {
    auto correct_syntax = false;
    if (library_service_ext->parse({m_name->m_name.str, m_name->m_name.length},
                                   {m_language.str, m_language.length},
                                   src_to_parse, m_is_binary,
                                   &correct_syntax)) {
      // parsing failed
      assert(!correct_syntax);
      return true;
    }
    assert(correct_syntax);
  }

  /*
    Record the CURRENT_USER in binlog. The CURRENT_USER is used on slave to
    grant default privileges when sp_automatic_privileges variable is set.
  */
  thd->binlog_invoker();

  bool sp_already_exists = false;
  if (sp_create_routine(thd, &sp, thd->lex->definer, m_if_not_exists,
                        sp_already_exists)) {
    return true;
  }

  if (!sp_already_exists) {
    add_automatic_sp_privileges(thd, enum_sp_type::LIBRARY, m_name->m_db.str,
                                m_name->m_name.str);
  }

  my_ok(thd);
  return false;
}

Sql_cmd_alter_library::Sql_cmd_alter_library(THD *thd, sp_name *name,
                                             LEX_STRING comment)
    : m_name{name},
      m_comment{thd->strmake(comment.str, comment.length), comment.length} {}

bool Sql_cmd_alter_library::execute(THD *thd) {
  if (check_routine_access(thd, ALTER_PROC_ACL, m_name->m_db.str,
                           m_name->m_name.str, Acl_type::LIBRARY, false))
    return true;

  st_sp_chistics chistics;
  chistics.comment = to_lex_cstring(m_comment);

  /* Conditionally writes to binlog */
  if (sp_update_routine(thd, enum_sp_type::LIBRARY, m_name, &chistics))
    return true;

  my_ok(thd);
  return false;
}

bool Sql_cmd_drop_library::execute(THD *thd) {
  if (check_routine_access(thd, ALTER_PROC_ACL, m_name->m_db.str,
                           m_name->m_name.str, Acl_type::LIBRARY, false)) {
    return true;
  }

  const enum_sp_return_code sp_result =
      sp_drop_routine(thd, enum_sp_type::LIBRARY, m_name);

  if (remove_automatic_sp_privileges(thd, enum_sp_type::LIBRARY,
                                     sp_result == SP_DOES_NOT_EXISTS,
                                     m_name->m_db.str, m_name->m_name.str)) {
    return true;
  }

  switch (sp_result) {
    case SP_OK:
      my_ok(thd);
      return false;
    case SP_DOES_NOT_EXISTS:
      if (m_if_exists) {
        // With IF EXISTS clause for DROP statement, statement is written to
        // binlog even if object does not exists. sp_drop_routine() does not
        // write to binlog in this case, so the statement is written to binlog
        // here.
        if (write_bin_log(thd, true, thd->query().str, thd->query().length)) {
          return true;
        }
        push_warning_printf(thd, Sql_condition::SL_NOTE, ER_SP_DOES_NOT_EXIST,
                            ER_THD(thd, ER_SP_DOES_NOT_EXIST), "LIBRARY",
                            m_name->m_qname.str);
        my_ok(thd);
        return false;
      }
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "LIBRARY", m_name->m_qname.str);
      return true;
    default:
      my_error(ER_SP_DROP_FAILED, MYF(0), "LIBRARY", m_name->m_qname.str);
      return true;
  }
}

bool Sql_cmd_create_masking_policy::execute(THD *thd) {
  if (check_masking_policy_manage_privilege(thd)) return true;

  Sql_masking_policy_spec masking_policy_spec;
  masking_policy_spec.policy_name = m_policy_name;
  masking_policy_spec.argument_name = m_argument_name;

  StringBuffer<1024> str;
  m_masking_expr->print(
      thd, &str,
      enum_query_type(QT_NO_DB | QT_NO_TABLE | QT_FORCE_INTRODUCERS));
  masking_policy_spec.masking_expression = str.lex_cstring();

  if (create_masking_policy(thd, m_if_not_exists, masking_policy_spec)) {
    return true;
  }

  my_ok(thd);
  return false;
}

bool Sql_cmd_drop_masking_policy::execute(THD *thd) {
  if (check_masking_policy_manage_privilege(thd)) return true;

  if (drop_masking_policy(thd, m_policy_name, m_if_exists)) {
    return true;
  }

  my_ok(thd);
  return false;
}
