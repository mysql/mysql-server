#ifndef SQL_REGEXP_REGEXP_ENGINE_H_
#define SQL_REGEXP_REGEXP_ENGINE_H_

/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <unicode/uregex.h>

#include <stdint.h>
#include <vector>

#include "m_ctype.h"    // CHARSET_INFO.
#include "my_config.h"  // WORDS_BIGENDIAN
#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/regexp/errors.h"
#include "sql/sql_class.h"  // THD
#include "sql_string.h"

extern CHARSET_INFO my_charset_utf16le_general_ci;
extern CHARSET_INFO my_charset_utf16_general_ci;

namespace regexp {

static constexpr CHARSET_INFO *regexp_lib_charset =
#ifdef WORDS_BIGENDIAN
    &::my_charset_utf16_general_ci;
#else
    &::my_charset_utf16le_general_ci;
#endif

const char *icu_version_string();

/**
  We are using an std::vector to communicate with ICU's C API, which writes
  through a UChar pointer, after which we call vector::resize(). The latter
  normally overwrites with zeroes and we don't want that. That's why we wrap
  it in this class which has a non-initializing constructor.
*/
struct Uninitialized_uchar {
  /// We have to explicitly define an empty constructor.
  Uninitialized_uchar() {}
  operator UChar() { return the_char; }
  UChar the_char;
};

using ReplacementBufferType = std::vector<Uninitialized_uchar>;

/**
  Implements a match callback function for icu that aborts execution if the
  query was killed.

  @param context The session to check for killed query.
  @param steps Not used.

  @retval false Query was killed in the session and the match should abort.
  @retval true Query was not killed, matching should continue.
*/
UBool QueryNotKilled(const void *context, int32_t steps);

/**
  This class exposes high-level regular expression operations to the
  facade. It implements the algorithm for search-and-replace and the various
  matching options.

  A buffer is used for search-and-replace, whose initial size is that of the
  subject string. The buffer uses ICU preflight features to probe the required
  buffer size within each append operation, and the buffer can grow up until
  max_allowed_packet, at which case and error will be thrown.
*/
class Regexp_engine {
 public:
  /**
    Compiles the URegularExpression object. If compilation fails, my_error()
    is called and the IsError() returns true. In this case, all subsequent
    operations will be no-ops, reporting failure. This follows ICU's chaining
    conventions, see http://icu-project.org/apiref/icu4c/utypes_8h.html.

    @param pattern The pattern string in ICU's character set.

    @param flags ICU flags.

    @param stack_limit Sets the amount of heap storage, in bytes, that the
    match backtracking stack is allowed to allocate.

    @param time_limit Gets set on the URegularExpression. Please refer to the
    ICU API docs for the definition of time limit.
  */
  Regexp_engine(String *pattern, uint flags, int stack_limit,
                int time_limit) {
    DBUG_ASSERT(pattern->charset() == regexp_lib_charset);
    UParseError error;
    auto upattern = pointer_cast<const UChar *>(pattern->ptr());
    int length = pattern->length() / sizeof(UChar);
    m_re = uregex_open(upattern, length, flags, &error, &m_error_code);
    uregex_setStackLimit(m_re, stack_limit, &m_error_code);
    uregex_setTimeLimit(m_re, time_limit, &m_error_code);
    uregex_setMatchCallback(m_re, QueryNotKilled, current_thd, &m_error_code);
    check_icu_status(m_error_code, &error);
  }

  /**
    Resets the engine with a new subject string.
    @param subject The new string to match the regular expression against.
  */
  bool Reset(String *subject);

  /**
    Tries to find match number `occurrence` in the string, starting on
    `start`.

    @param start Start position, 0-based.
    @param occurrence Which occurrence to replace. If zero, replace all
    occurrences.
  */
  bool Matches(int start, int occurrence);

  /**
    Returns the start position in the input string of the string where
    Matches() found a match.
  */
  int StartOfMatch() {
    /*
      The 0 is for capture group number, but we don't deal with those
      here. Zero means the start of the whole match, which is what's needed.
    */
    return uregex_start(m_re, 0, &m_error_code);
  }

  /**
    Returns the position in the input string right after the end of the text
    where Matches() found a match.
  */
  int EndOfMatch() {
    // The 0 means start of capture group 0, ie., the whole match.
    return uregex_end(m_re, 0, &m_error_code);
  }

  /**
    Iterates over the subject string, replacing matches.

    @param replacement The string to replace matches with.
    @param length The length in bytes.
    @param start Start position, 0-based.
    @param occurrence Which occurrence to replace. If zero, replace all
    occurrences.
    @param result The result string.

    @return The same as `result`.
  */
  String *Replace(const char *replacement, int length, int start,
                  int occurrence, String *result);

  /**
    Copies the portion of the subject string between the start of the match
    and the end of the match into result.

    @param[out] result A string we can write to. The character set
    regexp_lib_charset is used.

    @return A pointer to @p result.
  */
  String *MatchedSubstring(String *result);

  bool IsError() const { return U_FAILURE(m_error_code); }
  bool CheckError() const { return check_icu_status(m_error_code); }

  ~Regexp_engine() { uregex_close(m_re); }

 private:
  /**
    The hard limit for growing the replace buffer. The buffer cannot grow
    beyond this size, and an error will be thrown if the limit is reached.
  */
  int HardLimit() {
    return current_thd->variables.max_allowed_packet / sizeof(UChar);
  }

  /**
    Fills in the prefix in case we are doing a replace operation starting on a
    non-first occurrence of the pattern or a non-first start
    position. AppendReplacement() will fill in the section starting after the
    previous match or start position, so a prefix must be appended first.
  */
  void AppendHead(int end_of_previous_match, int start_of_search);

  /**
    Preflight function: If the buffer capacity is adequate, the replacement is
    appended to the buffer, otherwise nothing is written. Either way, the
    replacement's full size is returned.
  */
  int TryToAppendReplacement(const UChar *repl, size_t length) {
    int capacity = SpareCapacity();
    auto replace_buffer_pos = &m_replace_buffer.end()->the_char;
    return uregex_appendReplacement(m_re, repl, length, &replace_buffer_pos,
                                    &capacity, &m_error_code);
  }

  /**
    Tries to write the replacement, growing the buffer if needed.

    @param replacement The replacement string.
    @param length The length in code points.
  */
  void AppendReplacement(const UChar *replacement, size_t length);

  /**
    Tries to append the part of the subject string after the last match to the
    buffer. This is a preflight function: If the buffer capacity is adequate,
    the tail is appended to the buffer, otherwise nothing is written. Either
    way, the tail's full size is returned.
  */
  int TryToAppendTail() {
    int capacity = SpareCapacity();
    auto replace_buffer_pos = &m_replace_buffer.end()->the_char;
    return uregex_appendTail(m_re, &replace_buffer_pos, &capacity,
                             &m_error_code);
  }

  /// Appends the trailing segment after the last match to the subject string,
  void AppendTail();

  /**
    The spare capacity in the replacement buffer, given in code points.

    ICU communicates via a `capacity` variable, but we like to use an absolute
    position instead, and we want to keep a single source of truth, so we
    calculate it when needed and assert that the number is correct.
  */
  int SpareCapacity() const {
    return m_replace_buffer.capacity() - m_replace_buffer.size();
  }

  /**
    Our handle to ICU's compiled regular expression, owned by instances of
    this class. URegularExpression is a C struct, but this class follows RAII
    and initializes this pointer in the constructor and cleans it up in the
    destructor.
  */
  URegularExpression *m_re;
  UErrorCode m_error_code = U_ZERO_ERROR;
  String *m_current_subject = nullptr;
  ReplacementBufferType m_replace_buffer;
};

}  // namespace regexp

#endif  // SQL_REGEXP_REGEXP_ENGINE_H_
