/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef SP_INSTR_INLINE_INCLUDED
#define SP_INSTR_INLINE_INCLUDED

#include <string>
#include <unordered_set>
#include "my_inttypes.h"

class THD;
class sp_head;
class Item;
struct Name_resolution_context;
class sp_instr;
class sp_pcontext;
template <class T>
class Mem_root_array;

namespace sp_inl {

class sp_inline_instr;

///////////////////////////////////////////////////////////////////////////

/**
    Checks if stored function inlining is required.
    Currently we attempt stored function inlining only for the secondary engine.
    In certain cases (such as PFS creation) m_sql_cmd could be nullptr.

    @param[in] thd Thread context
    @return "true" if inlining is required, "false" otherwise
*/
bool needs_stored_function_inlining(THD *thd);

/**
    Checks if general stored function properties are eligible for inlining.

    @param[in] thd Thread context
    @param[in] sp Stored program (function) instance
    @param[in] sp_arg_count Number of arguments of the stored function
    @return "true" if the function can potentially be inlined, "false"
    otherwise.
*/
bool can_inline_stored_function(THD *thd, sp_head *sp, uint sp_arg_count);

/**
    Creates the list of prepared instructions. A prepared instruction is an
    instruction eligible for inlining according to its method validate(). This
    function also computes and marks the redundant instructions so they can be
    later recognized using the method is_redundant_instr(). A redundant
    instruction is an instruction not needed for computing the returned result
    of the stored function.

    @param[in] thd Thread context
    @param[in] sp Stored program (stored function) instance
    @param[out] used_sp_functions Set of stored function instances called from
   sp

    @return nullptr if an error occurs. List of prepared instructions otherwise.
    The list may contain both redundant and not redundant instruction.
*/
Mem_root_array<sp_inline_instr *> *prepare(
    THD *thd, sp_head *sp, std::unordered_set<sp_head *> &used_sp_functions);

/**
  Inlines the given stored function instructions into a single Item

  @param[in] thd Thread context
  @param[in] prepared_instructions Instructions to inline
  @param[in] sp_args Input arguments of the stored function
  @param[in] sp_arg_count Number of arguments of the stored function
  @param[in] sp_head Stored function instance
  @param[in] sp_name_resolution_ctx Name resolution context of the stored
  function

  @return "nullptr" if an error occurs, inlined Item otherwise
*/
Item *inline_stored_function(
    THD *thd, Mem_root_array<sp_inline_instr *> *prepared_instructions,
    Item **sp_args, uint sp_arg_count, sp_head *sp_head,
    Name_resolution_context *sp_name_resolution_ctx);

/**
  Finalizes the error message for stored function inlining and reports the
  error.

  @param[in] thd Thread context
  @param[in] func_name Stored function name
  @param[in] err_reason Failure reason
*/
void report_stored_function_inlining_error(THD *thd, const char *func_name,
                                           std::string &err_reason);

}  // namespace sp_inl

#endif /* SP_INSTR_INLINE_INCLUDED */
