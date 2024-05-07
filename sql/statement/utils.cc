/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "sql/statement/utils.h"

#include "sql/protocol.h"
#include "sql/sp.h"
#include "sql/sp_head.h"
#include "sql/sp_rcontext.h"
#include "sql/sql_class.h"

bool set_sp_multi_result_state(THD *thd, LEX *lex) {
  assert(thd->sp_runtime_ctx != nullptr);

  sp_head *sp = thd->sp_runtime_ctx->sp;
  if (sp->m_flags & sp_head::MULTI_RESULTS) {
    assert(thd->server_status & SERVER_MORE_RESULTS_EXISTS);
    if (thd->server_status & SERVER_MORE_RESULTS_EXISTS) return false;
  }

  // Set SP flags depending on current statement.
  sp->m_flags |= sp_get_flags_for_command(lex);

  /*
    Ideally, SERVER_MORE_RESULTS_EXISTS should be set only when sp_head::
    MULTI_RESULTS flag is set. However, for SPs parsing statements at
    execution phase, deciding server state without prior knowledge of
    all the statements within the stored procedure is not possible.
    Hence SERVER_MORE_RESULTS_EXISTS is set regardless of sp_head::
    MULTI_RESULTS flag.
  */
  if (!thd->get_protocol()->has_client_capability(CLIENT_MULTI_RESULTS)) {
    // Client does not support multiple result sets.
    my_error(ER_SP_BADSELECT, MYF(0), sp->m_qname.str);
    return true;
  }

  thd->server_status |= SERVER_MORE_RESULTS_EXISTS;

  return false;
}

void set_query_for_display(THD *thd) {
  if (thd->rewritten_query().length() > 0) {
    thd->set_query_for_display(thd->rewritten_query().ptr(),
                               thd->rewritten_query().length());
  } else {
    thd->set_query_for_display(thd->query().str, thd->query().length);
  }
}

LEX_CSTRING convert_and_store(MEM_ROOT *mem_root, const char *str,
                              size_t length, const CHARSET_INFO *src_cs,
                              const CHARSET_INFO *dst_cs) {
  // Conversion happens only if there is no dst_cs, or a different charset or a
  // non-binary charset
  // TODO HCS-9585: show warnings when errors != 0
  if (dst_cs != nullptr && !my_charset_same(src_cs, dst_cs) &&
      src_cs != &my_charset_bin && dst_cs != &my_charset_bin) {
    const auto new_length = size_t{dst_cs->mbmaxlen * length};
    auto *converted_str = static_cast<char *>(mem_root->Alloc(new_length + 1));
    if (converted_str == nullptr) return {};  // OOM

    auto errors = uint{0};
    auto converted_length = copy_and_convert(converted_str, new_length, dst_cs,
                                             str, length, src_cs, &errors);
    converted_str[converted_length] = 0;
    return {converted_str, converted_length};
  }
  str = strmake_root(mem_root, str, length);
  return {str, length};
}
