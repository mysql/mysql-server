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

bool EvalExprToCharset(Item *expr, std::u16string *out) {
  StringBuffer<MAX_FIELD_WIDTH> pre_conversion_buffer;
  String *s = expr->val_str(&pre_conversion_buffer);
  if (s == nullptr) return true;

  if (s->length() == 0) {
    out->clear();
    return false;
  }
  if (expr->collation.collation != regexp_lib_charset) {
    // Character set conversion is called for.
    out->resize(s->length() * regexp_lib_charset->mbmaxlen / sizeof(UChar));
    uint errors;
    size_t converted_size = my_convert(
        pointer_cast<char *>(&(*out)[0]), out->size() * sizeof(UChar),
        regexp_lib_charset, s->ptr(), s->length(), s->charset(), &errors);

    if (errors > 0) return true;
    DBUG_ASSERT(converted_size % sizeof(UChar) == 0);
    out->resize(converted_size / sizeof(UChar));
    return false;
  }
  // No conversion needed; just copy into the u16string.
  out->clear();
  out->insert(out->end(), pointer_cast<const UChar *>(s->ptr()),
              pointer_cast<const UChar *>(s->ptr() + s->length()));

  return false;
}

bool Regexp_facade::SetPattern(Item *pattern_expr, uint32_t flags) {
  if (pattern_expr == nullptr) {
    m_engine = nullptr;
    return true;
  }
  if (m_engine == nullptr)
    // Called for the first time.
    return SetupEngine(pattern_expr, flags);

  /*
    We don't need to recompile the regular expression if the pattern is
    a constant in the query and the flags are the same.
  */
  if (pattern_expr->const_item() && flags == m_engine->flags()) return false;
  return SetupEngine(pattern_expr, flags);
}

bool Regexp_facade::Reset(Item *subject_expr) {
  DBUG_ENTER("Regexp_facade::Reset");

  if (m_engine == nullptr ||
      EvalExprToCharset(subject_expr, &m_current_subject))
    DBUG_RETURN(true);

  m_engine->Reset(m_current_subject);
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
  std::u16string replacement(MAX_FIELD_WIDTH, '\0');

  if (EvalExprToCharset(replacement_expr, &replacement)) DBUG_RETURN(nullptr);

  if (Reset(subject_expr)) DBUG_RETURN(nullptr);

  const std::u16string &result_buffer =
      m_engine->Replace(replacement, start - 1, occurrence);
  result->set(pointer_cast<const char *>(result_buffer.data()),
              result_buffer.size() * sizeof(UChar), regexp_lib_charset);
  DBUG_RETURN(result);
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

  std::u16string pattern;
  if (EvalExprToCharset(pattern_expr, &pattern)) {
    m_engine = nullptr;
    DBUG_RETURN(false);
  }

  // Actually compile the regular expression.
  m_engine = make_unique_destroy_only<Regexp_engine>(
      *THR_MALLOC, pattern, flags, opt_regexp_stack_limit,
      opt_regexp_time_limit);

  // If something went wrong, an error was raised.
  DBUG_RETURN(m_engine->IsError());
}

}  // namespace regexp
