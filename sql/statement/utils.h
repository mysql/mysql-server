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

#ifndef RUN_COMMAND_UTILS_H
#define RUN_COMMAND_UTILS_H

#include "lex_string.h"
#include "my_alloc.h"
#include "mysql/strings/m_ctype.h"
class THD;
struct LEX;
struct MEM_ROOT;
struct CHARSET_INFO;

/**
  Sets multi-result state for SP being executed by the Statement_handle
  family.

  For SQL SP, flags required for multi-result are set while parsing SP
  statements. SPs for which parsing its statement is deferred to execution
  phase, multi-result state is set by this function.

  The function invoker need not have to reset this state. State is reset
  in the Sql_cmd_call::execute_inner.

  @param   thd     Thread Handle.
  @param   lex     Lex instance of SP statement.

  @returns false on Success and true if multi-result can not be used.
*/
bool set_sp_multi_result_state(THD *thd, LEX *lex);

/**
  Set query to be displayed in performance schema.

  @param   thd     Thread Handle.
*/
void set_query_for_display(THD *thd);

/**
 * @brief Potentially convert a string from src charset to destination charset
 * and store the returned string on the specified memroot
 *
 * @param mem_root The mem_root to store the result str
 * @param str The input string to be converted
 * @param length Length of the input string
 * @param src_cs Source charset
 * @param dst_cs Dest charset
 * @return LEX_CSTRING Pointer to the converted string on the memroot
 */
LEX_CSTRING convert_and_store(MEM_ROOT *mem_root, const char *str,
                              size_t length, const CHARSET_INFO *src_cs,
                              const CHARSET_INFO *dst_cs);

#endif
