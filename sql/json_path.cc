/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/*
  @file json_path.cc

  This file contains implementation support for the JSON path abstraction.
  The path abstraction is described by the functional spec
  attached to WL#7909.
 */

#include "sql/json_path.h"

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/encodings.h>
#include <rapidjson/memorystream.h>  // rapidjson::MemoryStream
#include <stddef.h>
#include <algorithm>  // any_of
#include <memory>     // unique_ptr
#include <string>

#include "m_ctype.h"
#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/json_dom.h"
#include "sql/psi_memory_key.h"  // key_memory_JSON
#include "sql/sql_const.h"       // STRING_BUFFER_USUAL_SIZE
#include "sql_string.h"          // String
#include "template_utils.h"      // down_cast

// For use in Json_path::parse_path
#define PARSER_RETURN(retval) \
  {                           \
    *status = retval;         \
    return charptr;           \
  }

namespace {

constexpr char SCOPE = '$';
constexpr char BEGIN_MEMBER = '.';
constexpr char BEGIN_ARRAY = '[';
constexpr char END_ARRAY = ']';
constexpr char DOUBLE_QUOTE = '"';
constexpr char WILDCARD = '*';
constexpr char MINUS = '-';
constexpr char LAST[] = "last";

}  // namespace

static bool is_ecmascript_identifier(const std::string &name);
static bool is_digit(unsigned codepoint);

static bool append_array_index(String *buf, size_t index, bool from_end) {
  if (!from_end) return buf->append_ulonglong(index);

  bool ret = buf->append(STRING_WITH_LEN(LAST));
  if (index > 0) ret |= buf->append(MINUS) || buf->append_ulonglong(index);
  return ret;
}

// Json_path_leg

bool Json_path_leg::to_string(String *buf) const {
  switch (m_leg_type) {
    case jpl_member:
      return buf->append(BEGIN_MEMBER) ||
             (is_ecmascript_identifier(m_member_name)
                  ? buf->append(m_member_name.data(), m_member_name.length())
                  : double_quote(m_member_name.data(), m_member_name.length(),
                                 buf));
    case jpl_array_cell:
      return buf->append(BEGIN_ARRAY) ||
             append_array_index(buf, m_first_array_index,
                                m_first_array_index_from_end) ||
             buf->append(END_ARRAY);
    case jpl_array_range:
      return buf->append(BEGIN_ARRAY) ||
             append_array_index(buf, m_first_array_index,
                                m_first_array_index_from_end) ||
             buf->append(STRING_WITH_LEN(" to ")) ||
             append_array_index(buf, m_last_array_index,
                                m_last_array_index_from_end) ||
             buf->append(END_ARRAY);
    case jpl_member_wildcard:
      return buf->append(BEGIN_MEMBER) || buf->append(WILDCARD);
    case jpl_array_cell_wildcard:
      return buf->append(BEGIN_ARRAY) || buf->append(WILDCARD) ||
             buf->append(END_ARRAY);
    case jpl_ellipsis:
      return buf->append(WILDCARD) || buf->append(WILDCARD);
  }

  // Unknown leg type.
  DBUG_ASSERT(false); /* purecov: inspected */
  return true;        /* purecov: inspected */
}

bool Json_path_leg::is_autowrap() const {
  switch (m_leg_type) {
    case jpl_array_cell:
      /*
        If the array cell index matches an element in a single-element
        array (`0` or `last`), it will also match a non-array value
        which is auto-wrapped in a single-element array.
      */
      return first_array_index(1).within_bounds();
    case jpl_array_range: {
      /*
        If the range matches an element in a single-element array, it
        will also match a non-array which is auto-wrapped in a
        single-element array.
      */
      Array_range range = get_array_range(1);
      return range.m_begin < range.m_end;
    }
    default:
      return false;
  }
}

Json_path_leg::Array_range Json_path_leg::get_array_range(
    size_t array_length) const {
  if (m_leg_type == jpl_array_cell_wildcard) return {0, array_length};

  DBUG_ASSERT(m_leg_type == jpl_array_range);

  // Get the beginning of the range.
  size_t begin = first_array_index(array_length).position();

  // Get the (exclusive) end of the range.
  Json_array_index last = last_array_index(array_length);
  size_t end = last.within_bounds() ? last.position() + 1 : last.position();

  return {begin, end};
}

Json_seekable_path::Json_seekable_path() : m_path_legs(key_memory_JSON) {}

// Json_path

Json_path::Json_path() : m_mem_root(key_memory_JSON, 256) {}

bool Json_path::to_string(String *buf) const {
  /*
    3-part scope prefixes are not needed by wl7909.
    There is no way to test them at the SQL level right now
    since they would raise errors in all possible use-cases.
    Support for them can be added in some follow-on worklog
    which actually needs them.

    This is where we would put pretty-printing support
    for 3-part scope prefixes.
  */

  if (buf->append(SCOPE)) return true;

  for (const Json_path_leg *leg : *this) {
    if (leg->to_string(buf)) return true;
  }

  return false;
}

bool Json_path::can_match_many() const {
  return std::any_of(begin(), end(), [](const Json_path_leg *leg) -> bool {
    switch (leg->get_type()) {
      case jpl_member_wildcard:
      case jpl_array_cell_wildcard:
      case jpl_ellipsis:
      case jpl_array_range:
        return true;
      default:
        return false;
    }
  });
}

// Json_path parsing

/** Top level parsing factory method */
bool parse_path(const bool begins_with_column_id, const size_t path_length,
                const char *path_expression, Json_path *path,
                size_t *bad_index) {
  bool status = false;

  const char *end_of_parsed_path = path->parse_path(
      begins_with_column_id, path_length, path_expression, &status);

  if (status) {
    *bad_index = 0;
    return false;
  }

  *bad_index = end_of_parsed_path - path_expression;
  return true;
}

/// Is this a whitespace character?
static inline bool is_whitespace(char ch) {
  return my_isspace(&my_charset_utf8mb4_bin, ch);
}

/**
  Purge leading whitespace in a string.
  @param[in] str  the string to purge whitespace from
  @param[in] end  the end of the input string
  @return pointer to the first non-whitespace character in str
*/
static inline const char *purge_whitespace(const char *str, const char *end) {
  return std::find_if_not(str, end, [](char c) { return is_whitespace(c); });
}

const char *Json_path::parse_path(const bool begins_with_column_id,
                                  const size_t path_length,
                                  const char *path_expression, bool *status) {
  clear();

  const char *charptr = path_expression;
  const char *endptr = path_expression + path_length;

  if (begins_with_column_id) {
    /*
      3-part scope prefixes are not needed by wl7909.
      There is no way to test them at the SQL level right now
      since they would raise errors in all possible use-cases.
      Support for them can be added in some follow-on worklog
      which actually needs them.

      This is where we would add parsing support
      for 3-part scope prefixes.
    */

    // not supported yet
    PARSER_RETURN(false);
  } else {
    // the first non-whitespace character must be $
    charptr = purge_whitespace(charptr, endptr);
    if ((charptr >= endptr) || (*charptr++ != SCOPE)) PARSER_RETURN(false);
  }

  // now add the legs
  *status = true;
  while (*status) {
    charptr = purge_whitespace(charptr, endptr);
    if (charptr >= endptr) break;  // input exhausted

    charptr = parse_path_leg(charptr, endptr, status);
  }

  // a path may not end with an ellipsis
  if (m_path_legs.size() > 0 &&
      m_path_legs.back()->get_type() == jpl_ellipsis) {
    *status = false;
  }

  return charptr;
}

const char *Json_path::parse_path_leg(const char *charptr, const char *endptr,
                                      bool *status) {
  switch (*charptr) {
    case BEGIN_ARRAY:
      return parse_array_leg(charptr, endptr, status);
    case BEGIN_MEMBER:
      return parse_member_leg(charptr, endptr, status);
    case WILDCARD:
      return parse_ellipsis_leg(charptr, endptr, status);
    default:
      PARSER_RETURN(false);
  }
}

const char *Json_path::parse_ellipsis_leg(const char *charptr,
                                          const char *endptr, bool *status) {
  // assume the worst
  *status = false;

  // advance past the first *
  charptr++;

  // must be followed by a second *
  if ((charptr >= endptr) || (*charptr++ != WILDCARD)) {
    PARSER_RETURN(false);
  }

  // may not be the last leg
  if (charptr >= endptr) {
    PARSER_RETURN(false);
  }

  // forbid the hard-to-read *** combination
  if (*charptr == WILDCARD) {
    PARSER_RETURN(false);
  }

  PARSER_RETURN(!append(Json_path_leg(jpl_ellipsis)));
}

/**
  Parse an array index in an array cell index or array range path leg.

  An array index is either a non-negative integer (a 0-based index relative to
  the beginning of the array), or the keyword "last" (which means the last
  element in the array), or the keyword "last" followed by a minus ("-") and a
  non-negative integer (which is the 0-based index relative to the end of the
  array).

  @param charptr   the current position in the path being parsed
  @param endptr    the end of the JSON path being parsed
  @param[out] error  gets set to true if there is a syntax error,
                     false on success
  @param[out] array_index  gets set to the parsed array index
  @param[out] from_end     gets set to true if the array index is
                           relative to the end of the array

  @return pointer to the first character after the parsed array index
*/
static const char *parse_array_index(const char *charptr, const char *endptr,
                                     bool *error, uint32 *array_index,
                                     bool *from_end) {
  *from_end = false;

  // Do we have the "last" token?
  if (charptr + 4 <= endptr && std::equal(charptr, charptr + 4, LAST)) {
    charptr += 4;
    *from_end = true;

    const char *next_token = purge_whitespace(charptr, endptr);
    if (next_token < endptr && next_token[0] == MINUS) {
      // Found a minus sign, go on parsing to find the array index.
      charptr = purge_whitespace(next_token + 1, endptr);
    } else {
      // Didn't find any minus sign after "last", so we're done.
      *array_index = 0;
      *error = false;
      return charptr;
    }
  }

  if (charptr >= endptr || !is_digit(*charptr)) {
    *error = true;
    return charptr;
  }

  char *endp;
  int err;
  ulonglong idx = my_strntoull(&my_charset_utf8mb4_bin, charptr,
                               endptr - charptr, 10, &endp, &err);
  if (err != 0 || idx > UINT_MAX32) {
    *error = true;
    return charptr;
  }

  *array_index = static_cast<uint32>(idx);
  *error = false;
  return endp;
}

const char *Json_path::parse_array_leg(const char *charptr, const char *endptr,
                                       bool *status) {
  // assume the worst
  *status = false;

  // advance past the [
  charptr++;

  charptr = purge_whitespace(charptr, endptr);
  if (charptr >= endptr) PARSER_RETURN(false);  // input exhausted

  if (*charptr == WILDCARD) {
    charptr++;

    if (append(Json_path_leg(jpl_array_cell_wildcard)))
      PARSER_RETURN(false); /* purecov: inspected */
  } else {
    /*
      Not a WILDCARD. The next token must be an array index (either
      the single index of a jpl_array_cell path leg, or the start
      index of a jpl_array_range path leg.
    */
    bool error;
    uint32 cell_index1;
    bool from_end1;
    charptr =
        parse_array_index(charptr, endptr, &error, &cell_index1, &from_end1);
    if (error) PARSER_RETURN(false);

    const char *number_end = charptr;
    charptr = purge_whitespace(charptr, endptr);
    if (charptr >= endptr) PARSER_RETURN(false);

    // Is this a range, <arrayIndex> to <arrayIndex>?
    if (charptr > number_end && endptr - charptr > 3 && charptr[0] == 't' &&
        charptr[1] == 'o' && is_whitespace(charptr[2])) {
      // A range. Skip over the "to" token and any whitespace.
      charptr = purge_whitespace(charptr + 3, endptr);

      uint32 cell_index2;
      bool from_end2;
      charptr =
          parse_array_index(charptr, endptr, &error, &cell_index2, &from_end2);
      if (error) PARSER_RETURN(false);

      /*
        Reject pointless paths that can never return any matches, regardless of
        which array they are evaluated against. We know this if both indexes
        count from the same side of the array, and the start index is after the
        end index.
      */
      if (from_end1 == from_end2 && ((from_end1 && cell_index1 < cell_index2) ||
                                     (!from_end1 && cell_index2 < cell_index1)))
        PARSER_RETURN(false);

      if (append(Json_path_leg(cell_index1, from_end1, cell_index2, from_end2)))
        PARSER_RETURN(false); /* purecov: inspected */
    } else {
      // A single array cell.
      if (append(Json_path_leg(cell_index1, from_end1)))
        PARSER_RETURN(false); /* purecov: inspected */
    }
  }

  // the next non-whitespace should be the closing ]
  charptr = purge_whitespace(charptr, endptr);
  if ((charptr < endptr) && (*charptr++ == END_ARRAY)) {
    // all is well
    PARSER_RETURN(true);
  }

  // An error has occurred.
  PARSER_RETURN(false);
}

/**
  Find the end of a member name in a JSON path. The name could be
  either a quoted or an unquoted identifier.

  @param start the start of the member name
  @param end the end of the JSON path expression
  @return pointer to the position right after the end of the name, or
  to the position right after the end of the string if the input
  string is an unterminated quoted identifier
*/
static const char *find_end_of_member_name(const char *start, const char *end) {
  const char *str = start;

  /*
    If we have a double-quoted name, the end of the name is the next
    unescaped double quote.
  */
  if (*str == DOUBLE_QUOTE) {
    str++;  // Advance past the opening double quote.
    while (str < end) {
      switch (*str++) {
        case '\\':
          /*
            Skip the next character after a backslash. It cannot mark
            the end of the quoted string.
          */
          str++;
          break;
        case DOUBLE_QUOTE:
          // An unescaped double quote marks the end of the quoted string.
          return str;
      }
    }

    /*
      Whoops. No terminating quote was found. Just return the end of
      the string. When we send the unterminated string through the
      JSON parser, it will detect and report the syntax error, so
      there is no need to handle the syntax error here.
    */
    return end;
  }

  /*
    If we have an unquoted name, the name is terminated by whitespace
    or [ or . or * or end-of-string.
  */
  const auto is_terminator = [](const char c) {
    return is_whitespace(c) || c == BEGIN_ARRAY || c == BEGIN_MEMBER ||
           c == WILDCARD;
  };
  return std::find_if(str, end, is_terminator);
}

/**
  Parse a quoted member name using the rapidjson parser, so that we
  get the name without the enclosing quotes and with any escape
  sequences replaced with the actual characters.

  It is the caller's responsibility to destroy the returned
  Json_string when it's done with it.

  @param str the input string
  @param len the length of the input string
  @return a Json_string that represents the member name, or NULL if
  the input string is not a valid name
*/
static std::unique_ptr<Json_string> parse_name_with_rapidjson(const char *str,
                                                              size_t len) {
  Json_dom_ptr dom = Json_dom::parse(str, len, nullptr, nullptr);

  if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
    return nullptr;

  return std::unique_ptr<Json_string>(down_cast<Json_string *>(dom.release()));
}

const char *Json_path::parse_member_leg(const char *charptr, const char *endptr,
                                        bool *status) {
  // advance past the .
  charptr++;

  charptr = purge_whitespace(charptr, endptr);
  if (charptr >= endptr) PARSER_RETURN(false);  // input exhausted

  if (*charptr == WILDCARD) {
    charptr++;

    if (append(Json_path_leg(jpl_member_wildcard)))
      PARSER_RETURN(false); /* purecov: inspected */
  } else {
    const char *key_start = charptr;
    const char *key_end = find_end_of_member_name(key_start, endptr);
    const bool was_quoted = (*key_start == DOUBLE_QUOTE);

    charptr = key_end;

    std::unique_ptr<Json_string> jstr;

    if (was_quoted) {
      /*
        Send the quoted name through the parser to unquote and
        unescape it.
      */
      jstr = parse_name_with_rapidjson(key_start, key_end - key_start);
    } else {
      /*
        An unquoted name may contain escape sequences. Wrap it in
        double quotes and send it through the JSON parser to unescape
        it.
      */
      char buff[STRING_BUFFER_USUAL_SIZE];
      String strbuff(buff, sizeof(buff), &my_charset_utf8mb4_bin);
      strbuff.length(0);
      if (strbuff.append(DOUBLE_QUOTE) ||
          strbuff.append(key_start, key_end - key_start) ||
          strbuff.append(DOUBLE_QUOTE))
        PARSER_RETURN(false); /* purecov: inspected */
      jstr = parse_name_with_rapidjson(strbuff.ptr(), strbuff.length());
    }

    if (jstr == nullptr) PARSER_RETURN(false);

    // unquoted names must be valid ECMAScript identifiers
    if (!was_quoted && !is_ecmascript_identifier(jstr->value()))
      PARSER_RETURN(false);

    // Looking good.
    if (append(Json_path_leg(jstr->value())))
      PARSER_RETURN(false); /* purecov: inspected */
  }

  PARSER_RETURN(true);
}

/**
   Return true if the character is a unicode combining mark.

   @param codepoint A unicode codepoint.

   @return True if the codepoint is a unicode combining mark.
*/
static inline bool unicode_combining_mark(unsigned codepoint) {
  return ((0x300 <= codepoint) && (codepoint <= 0x36F));
}

/**
   Return true if the codepoint is a Unicode letter.

   This was the best
   recommendation from the old-timers about how to answer this question.
   But as you can see from the need to call unicode_combining_mark(),
   my_isalpha() isn't good enough. It probably has many other defects.

   FIXME
*/
static bool is_letter(unsigned codepoint) {
  /*
    The Unicode combining mark \u036F passes the my_isalpha() test.
    That doesn't inspire much confidence in the correctness
    of my_isalpha().
   */
  if (unicode_combining_mark(codepoint)) {
    return false;
  }
  return my_isalpha(&my_charset_utf8mb4_bin, codepoint);
}

/**
   Return true if the codepoint is a Unicode digit.

   This was the best
   recommendation from the old-times about how to answer this question.
*/
static bool is_digit(unsigned codepoint) {
  return my_isdigit(&my_charset_utf8mb4_bin, codepoint);
}

/**
   Return true if the codepoint is Unicode connector punctuation.
*/
static bool is_connector_punctuation(unsigned codepoint) {
  switch (codepoint) {
    case 0x5F:    // low line
    case 0x203F:  // undertie
    case 0x2040:  // character tie
    case 0x2054:  // inverted undertie
    case 0xFE33:  // presentation form for vertical low line
    case 0xFE34:  // presentation form for vertical wavy low line
    case 0xFE4D:  // dashed low line
    case 0xFE4E:  // centerline low line
    case 0xFE4F:  // wavy low line
    case 0xFF3F:  // fullwidth low line
    {
      return true;
    }
    default: { return false; }
  }
}

/**
   Returns true if the name is a valid ECMAScript identifier.

   The name
   must be a sequence of UTF8-encoded bytes. All escape sequences
   have been replaced with UTF8-encoded bytes.

   @param[in] name        name to check

   @return True if the name is a valid ECMAScript identifier. False otherwise.
*/
static bool is_ecmascript_identifier(const std::string &name) {
  // An empty string is not a valid identifier.
  if (name.empty()) return false;

  /*
    At this point, The unicode escape sequences have already
    been replaced with the corresponding UTF-8 bytes. Now we apply
    the rules here: https://es5.github.io/x7.html#x7.6
  */
  rapidjson::MemoryStream input_stream(name.data(), name.length());
  unsigned codepoint;

  while (input_stream.Tell() < name.length()) {
    bool first_codepoint = (input_stream.Tell() == 0);
    if (!rapidjson::UTF8<char>::Decode(input_stream, &codepoint)) return false;

    // a unicode letter
    if (is_letter(codepoint)) continue;
    // $ is ok
    if (codepoint == 0x24) continue;
    // _ is ok
    if (codepoint == 0x5F) continue;

    /*
      the first character must be one of the above.
      more possibilities are available for subsequent characters.
    */

    if (first_codepoint) {
      return false;
    } else {
      // unicode combining mark
      if (unicode_combining_mark(codepoint)) continue;

      // a unicode digit
      if (is_digit(codepoint)) continue;
      if (is_connector_punctuation(codepoint)) continue;
      // <ZWNJ>
      if (codepoint == 0x200C) continue;
      // <ZWJ>
      if (codepoint == 0x200D) continue;
    }

    // nope
    return false;
  }

  return true;
}
