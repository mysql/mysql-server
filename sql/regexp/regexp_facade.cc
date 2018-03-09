/* Copyright (c) 2017, 2018 Oracle and/or its affiliates. All rights reserved.

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

#include "sql/regexp/regexp_facade.h"

#include <string>

#include "sql/mysqld.h"  // make_unique_destroy_only
#include "sql/regexp/regexp_engine.h"
#include "sql_string.h"
#include "template_utils.h"

namespace regexp {

String *EvalExprToCharset(Item *expr, String *out) {
  uint dummy_errors;
  if (expr->collation.collation != regexp_lib_charset) {
    // Character set conversion is called for.
    StringBuffer<MAX_FIELD_WIDTH> pre_conversion_buffer;
    String *s = expr->val_str(&pre_conversion_buffer);
    if (s == nullptr) return nullptr;
    if (out->copy(s->ptr(), s->length(), s->charset(), regexp_lib_charset,
                  &dummy_errors))
      return nullptr;
    return out;
  }
  String *result = expr->val_str(out);
  if (result != nullptr && !is_aligned_to(result->ptr(), alignof(UChar))) {
    if (out->copy(result->ptr(), result->length(), result->charset(),
                  regexp_lib_charset, &dummy_errors))
      return nullptr;
    DBUG_ASSERT(is_aligned_to(out->ptr(), alignof(UChar)));
    return out;
  }
  return result;
}

bool Regexp_facade::SetPattern(Item *pattern_expr) {
  if (pattern_expr == nullptr) {
    m_engine = nullptr;
    return true;
  }
  if (!pattern_expr->const_item() ||  // Non-constant pattern, see above.
      m_engine == nullptr) {          // Called for the first time.
    if (SetupEngine(pattern_expr, m_flags)) return true;
  }
  return false;
}

bool Regexp_facade::Reset(Item *subject_expr) {
  DBUG_ENTER("Regexp_facade::Reset");

  if (m_engine == nullptr) DBUG_RETURN(true);
  String *subject = EvalExprToCharset(subject_expr, &m_current_subject);
  if (subject == nullptr) DBUG_RETURN(true);

  m_engine->Reset(subject);
  DBUG_RETURN(false);
}

Mysql::Nullable<bool> Regexp_facade::Matches(Item *subject_expr, int start,
                                             int occurrence) {
  DBUG_ENTER("Regexp_facade::Find");

  if (Reset(subject_expr)) DBUG_RETURN(Mysql::Nullable<bool>());

  DBUG_RETURN(m_engine->Matches(start - 1, occurrence));
}

Mysql::Nullable<int> Regexp_facade::Find(Item *subject_expr, int start,
                                         int occurrence, bool after_match) {
  Nullable<bool> match_found = Matches(subject_expr, start, occurrence);
  if (!match_found.has_value()) return Mysql::Nullable<int>();
  if (!match_found.value()) return 0;
  return (after_match ? m_engine->EndOfMatch() : m_engine->StartOfMatch()) + 1;
}

String *Regexp_facade::Replace(Item *subject_expr, Item *replacement_expr,
                               int64_t start, int occurrence, String *result) {
  DBUG_ENTER("Regexp_facade::Replace");
  String replacement_buf;

  String *replacement = EvalExprToCharset(replacement_expr, &replacement_buf);

  if (replacement == nullptr) DBUG_RETURN(nullptr);

  if (Reset(subject_expr)) DBUG_RETURN(nullptr);

  DBUG_RETURN(m_engine->Replace(replacement->ptr(), replacement->length(),
                                start - 1, occurrence, result));
}

String *Regexp_facade::Substr(Item *subject_expr, int start, int occurrence,
                              String *result) {
  if (Reset(subject_expr)) return nullptr;
  if (!m_engine->Matches(start - 1, occurrence)) {
    m_engine->CheckError();
    return nullptr;
  }
  String *res = m_engine->MatchedSubstring(result);
  if (m_engine->CheckError()) return nullptr;
  return res;
}

bool Regexp_facade::SetupEngine(Item *pattern_expr, uint flags) {
  DBUG_ENTER("Regexp_facade::SetupEngine");

  String pattern_buffer;
  String *pattern = EvalExprToCharset(pattern_expr, &pattern_buffer);

  if (pattern == nullptr) {
    m_engine = nullptr;
    DBUG_RETURN(false);
  }

  DBUG_ASSERT(is_aligned_to(pattern->ptr(), alignof(UChar)));

  // Actually compile the regular expression.
  m_engine = make_unique_destroy_only<Regexp_engine>(
      *THR_MALLOC, pattern, flags, opt_regexp_stack_limit,
      opt_regexp_time_limit);

  // If something went wrong, an error was raised.
  DBUG_RETURN(m_engine->IsError());
}

}  // namespace regexp
