/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "sql/regexp/regexp_facade.h"

#include <optional>
#include <string>
#include <tuple>

#include "my_pointer_arithmetic.h"
#include "mysql/strings/m_ctype.h"  // CHARSET_INFO.
#include "sql/item_func.h"
#include "sql/mysqld.h"  // make_unique_destroy_only
#include "sql/regexp/regexp_engine.h"
#include "sql_string.h"
#include "template_utils.h"

namespace regexp {

CHARSET_INFO *regexp_lib_charset = nullptr;

/**
  When dealing with the binary character set, we tell ICU that we're using
  CP-1252. This way, comparisons will happen as the user expects; each octet
  value is equal to itself, and only to itself. And of course, CP-1252 is
  known as "latin1" in MySQL.
*/
static const CHARSET_INFO *faux_binary_charset = &my_charset_latin1;

/**
  Evaluates an expression to an output buffer, performing character set
  conversion to regexp_lib_charset if necessary.

  The procedure supports the optimization that code points before the start
  position are not converted to UTF-16.

  @param expr The expression to be printed.

  @param[out] out Will be cleared, and the result stored.

  @param skip If present, this number of code points will be exempted from
  conversion.  If the string is smaller than that, an out of bounds error is
  raised.

  @return false on success, true on error.
*/
static bool EvalExprToCharset(Item *expr, std::u16string *out, int skip = 0) {
  alignas(sizeof(UChar)) char aligned_buff[MAX_FIELD_WIDTH];
  String aligned_str(aligned_buff, sizeof(aligned_buff), &my_charset_bin);
  String *s = expr->val_str(&aligned_str);
  if (s == nullptr) return true;

  if (s->length() == 0) {
    out->clear();
    return false;
  }
  const CHARSET_INFO *cs = s->charset();
  size_t bytes_to_skip =
      cs->cset->charpos(cs, s->ptr(), s->ptr() + s->length(), skip);
  if (bytes_to_skip >= s->length()) {
    /*
      The following error would normally be raised by ICU, but since this is
      an alternative code path - i.e. we prune away from charset conversion
      the portion of the string that is skipped - we have to raise it
      ourselves.
    */
    my_error(ER_REGEXP_INDEX_OUTOFBOUNDS_ERROR, MYF(0));
    out->clear();
    return true;
  }
  if (expr->collation.collation != regexp_lib_charset) {
    // Character set conversion is called for.
    uint max_char_size = regexp_lib_charset->mbmaxlen;
    // cast to signed for subtraction
    auto length = static_cast<longlong>(s->length()) - bytes_to_skip;
    out->resize(length * max_char_size / sizeof(UChar));
    auto to = pointer_cast<char *>(&(*out)[0]);
    size_t to_size = out->size() * sizeof(UChar);
    const char *start = s->ptr() + bytes_to_skip;
    uint errors;
    const CHARSET_INFO *source_charset =
        s->charset() == &my_charset_bin ? faux_binary_charset : s->charset();
    size_t converted_size = my_convert(to, to_size, regexp_lib_charset, start,
                                       length, source_charset, &errors);

    if (errors > 0) {
      report_conversion_error(regexp_lib_charset, start, length,
                              source_charset);
      return true;
    }
    assert(converted_size % sizeof(UChar) == 0);
    out->resize(converted_size / sizeof(UChar));
    return false;
  }
  // No conversion needed; just copy into the u16string.
  // However: val_str() may ignore the input argument,
  // and return a pointer to some other buffer.
  if (!is_aligned_to(s->ptr(), alignof(UChar))) {
    assert(s != &aligned_str);
    aligned_str.copy(*s);
    s = &aligned_str;
  }
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

bool Regexp_facade::Reset(Item *subject_expr, int start) {
  DBUG_TRACE;

  if (m_engine == nullptr ||
      EvalExprToCharset(subject_expr, &m_current_subject, start - 1))
    return true;

  m_engine->Reset(m_current_subject);
  return false;
}

int Regexp_facade::ConvertCodePointToLibPosition(int position) const {
  auto start = reinterpret_cast<const char *>(m_current_subject.c_str());
  const char *end = start + m_current_subject.length() * sizeof(char16_t);
  MY_CHARSET_HANDLER *cset = regexp_lib_charset->cset;
  return cset->charpos(regexp_lib_charset, start, end, position - 1) /
         sizeof(char16_t);
}

int Regexp_facade::ConvertLibPositionToCodePoint(int position) const {
  auto start = reinterpret_cast<const char *>(m_current_subject.c_str());
  const char *end = start + position * sizeof(char16_t);
  MY_CHARSET_HANDLER *cset = regexp_lib_charset->cset;
  return cset->numchars(regexp_lib_charset, start, end);
}

std::optional<bool> Regexp_facade::Matches(Item *subject_expr, int start,
                                           int occurrence) {
  DBUG_TRACE;

  if (Reset(subject_expr, start)) return std::optional<bool>();

  /*
    As far as ICU is concerned, we always start on position 0, since we
    didn't convert the characters before 'start'.
  */
  return m_engine->Matches(0, occurrence);
}

std::optional<int> Regexp_facade::Find(Item *subject_expr, int start,
                                       int occurrence, bool after_match) {
  std::optional<bool> match_found = Matches(subject_expr, start, occurrence);
  if (!match_found.has_value()) return std::optional<int>();
  if (!match_found.value()) return 0;
  int native_start =
      after_match ? m_engine->EndOfMatch() : m_engine->StartOfMatch();
  return ConvertLibPositionToCodePoint(native_start) + start;
}

String *Regexp_facade::Replace(Item *subject_expr, Item *replacement_expr,
                               int start, int occurrence, String *result) {
  DBUG_TRACE;
  String replacement_buf;
  std::u16string replacement(MAX_FIELD_WIDTH, '\0');

  if (EvalExprToCharset(replacement_expr, &replacement)) return nullptr;

  if (Reset(subject_expr)) return nullptr;

  const std::u16string &result_buffer = m_engine->Replace(
      replacement, ConvertCodePointToLibPosition(start), occurrence);

  return AssignResult(pointer_cast<const char *>(result_buffer.data()),
                      result_buffer.size() * sizeof(UChar), result);
}

String *Regexp_facade::AssignResult(const char *str, size_t length,
                                    String *result) {
  size_t number_unaligned_characters;
  uint conversion_error;

  // We still pretend that binary charset is cp1252. Here it's necessary since
  // String::copy doesn't convert anything if the result charset is Binary, for
  // good reason.
  if (result->charset() == &my_charset_bin) {
    if (result->copy(str, length, regexp_lib_charset, faux_binary_charset,
                     &conversion_error))
      return nullptr;
  }

  if (result->needs_conversion(length, regexp_lib_charset, result->charset(),
                               &number_unaligned_characters)) {
    if (result->copy(str, length, regexp_lib_charset, result->charset(),
                     &conversion_error))
      return nullptr;
  } else
    result->set(str, length, regexp_lib_charset);
  return result;
}

String *Regexp_facade::Substr(Item *subject_expr, int start, int occurrence,
                              String *result) {
  if (Reset(subject_expr)) return nullptr;
  if (!m_engine->Matches(ConvertCodePointToLibPosition(start), occurrence)) {
    m_engine->CheckError();
    return nullptr;
  }
  int substart, sublength;
  std::tie(substart, sublength) = m_engine->MatchedSubstring();

  if (m_engine->CheckError()) return nullptr;

  auto substartptr =
      pointer_cast<const char *>(m_current_subject.c_str()) + substart;

  return AssignResult(substartptr, sublength, result);
}

bool Regexp_facade::SetupEngine(Item *pattern_expr, uint flags) {
  DBUG_TRACE;

  std::u16string pattern;
  if (EvalExprToCharset(pattern_expr, &pattern)) {
    m_engine = nullptr;
    return false;
  }

  // Actually compile the regular expression.
  m_engine = make_unique_destroy_only<Regexp_engine>(
      *THR_MALLOC, pattern, flags, opt_regexp_stack_limit,
      opt_regexp_time_limit);

  // If something went wrong, an error was raised.
  return m_engine->IsError();
}

}  // namespace regexp
