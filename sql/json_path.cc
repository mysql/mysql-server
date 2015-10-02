/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  @file json_path.cc

  This file contains implementation support for the JSON path abstraction.
  The path abstraction is described by the functional spec
  attached to WL#7909.
 */

#include "json_path.h"

#include "json_dom.h"
#include "mysqld.h"                             // key_memory_JSON
#include "rapidjson/rapidjson.h"                // rapidjson::UTF8<char>::Decode
#include "rapidjson/memorystream.h"             // rapidjson::MemoryStream
#include "sql_const.h"                          // STRING_BUFFER_USUAL_SIZE
#include "sql_string.h"                         // String
#include "template_utils.h"                     // down_cast

#include <m_ctype.h>

#include <cwctype>
#include <memory>                               // auto_ptr
#include <string>

// For use in Json_path::parse_path
#define PARSER_RETURN(retval) { *status= retval; return charptr; }
#define SCOPE '$'
#define BEGIN_MEMBER '.'
#define BEGIN_ARRAY '['
#define END_ARRAY ']'
#define DOUBLE_QUOTE '\"'
#define WILDCARD '*'
#define PRINTABLE_SPACE ' '

bool is_ecmascript_identifier(const char *name, size_t name_length);
bool is_digit(unsigned codepoint);

// Json_path_leg

enum_json_path_leg_type Json_path_leg::get_type() const
{
  return m_leg_type;
}

size_t Json_path_leg::get_member_name_length() const
{
  return m_member_name.size();
}

const char *Json_path_leg::get_member_name() const
{
  return m_member_name.data();
}

size_t Json_path_leg::get_array_cell_index() const
{
  return m_array_cell_index;
}

bool Json_path_leg::to_string(String *buf) const
{
  switch(m_leg_type)
  {
  case jpl_member:
    return buf->append(BEGIN_MEMBER) ||
      (is_ecmascript_identifier(get_member_name(),
                                get_member_name_length()) ?
       buf->append(get_member_name(), get_member_name_length()) :
       double_quote(get_member_name(), get_member_name_length(), buf));
  case jpl_array_cell:
    return buf->append(BEGIN_ARRAY) ||
      buf->append_ulonglong(m_array_cell_index) ||
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
  DBUG_ABORT();                                 /* purecov: inspected */
  return true;                                  /* purecov: inspected */
}

// Json_path_clone

Json_path_clone::Json_path_clone()
  : m_path_legs(key_memory_JSON)
{}


Json_path_clone::~Json_path_clone()
{
  clear();
}


size_t Json_path_clone::leg_count() const { return m_path_legs.size(); }


const Json_path_leg *Json_path_clone::get_leg_at(const size_t index) const
{
  if (index >= m_path_legs.size())
  {
    return NULL;
  }

  return m_path_legs.at(index);
}


bool Json_path_clone::append(const Json_path_leg *leg)
{
  return m_path_legs.push_back(leg);
}


bool Json_path_clone::set(Json_seekable_path *source)
{
  clear();

  size_t legcount= source->leg_count();
  for (size_t idx= 0; idx < legcount; idx++)
  {
    Json_path_leg *path_leg= (Json_path_leg *) source->get_leg_at(idx);
    if (append(path_leg))
    {
      return true;
    }
  }

  return false;
}


const Json_path_leg *Json_path_clone::pop()
{
  DBUG_ASSERT(m_path_legs.size() > 0);
  const Json_path_leg *p= m_path_legs.back();
  m_path_legs.pop_back();
  return p;
}


void Json_path_clone::clear()
{
  m_path_legs.clear();
}


bool Json_path_clone::contains_ellipsis() const
{
  for (Path_leg_pointers::const_iterator iter= m_path_legs.begin();
       iter != m_path_legs.end(); ++iter)
  {
    const Json_path_leg *path_leg= *iter;
    if (path_leg->get_type() == jpl_ellipsis)
      return true;
  }

  return false;
}


// Json_path

Json_path::Json_path()
  : m_path_legs(key_memory_JSON)
{}


Json_path::~Json_path()
{
  m_path_legs.clear();
}


size_t Json_path::leg_count() const { return m_path_legs.size(); }


const Json_path_leg *Json_path::get_leg_at(const size_t index) const
{
  if (index >= m_path_legs.size())
  {
    return NULL;
  }

  return &m_path_legs.at(index);
}


bool Json_path::append(const Json_path_leg &leg)
{
  return m_path_legs.push_back(leg);
}

Json_path_leg Json_path::pop()
{
  DBUG_ASSERT(m_path_legs.size() > 0);
  Json_path_leg p= m_path_legs.back();
  m_path_legs.pop_back();
  return p;
}

void Json_path::clear()
{
  m_path_legs.clear();
}

bool Json_path::to_string(String *buf) const
{
  /*
    3-part scope prefixes are not needed by wl7909.
    There is no way to test them at the SQL level right now
    since they would raise errors in all possible use-cases.
    Support for them can be added in some follow-on worklog
    which actually needs them.

    This is where we would put pretty-printing support
    for 3-part scope prefixes.
  */

  if (buf->append(SCOPE))
    return true;

  for (Path_leg_vector::const_iterator iter= m_path_legs.begin();
       iter != m_path_legs.end(); ++iter)
  {
    if (iter->to_string(buf))
      return true;
  }

  return false;
}


static inline bool is_wildcard_or_ellipsis(const Json_path_leg &leg)
{
  switch (leg.get_type())
  {
  case jpl_member_wildcard:
  case jpl_array_cell_wildcard:
  case jpl_ellipsis:
    return true;
  default:
    return false;
  }
}


bool Json_path::contains_wildcard_or_ellipsis() const
{
  return std::find_if(m_path_legs.begin(), m_path_legs.end(),
                      is_wildcard_or_ellipsis) != m_path_legs.end();
}


static inline bool is_ellipsis(const Json_path_leg &leg)
{
  return leg.get_type() == jpl_ellipsis;
}


bool Json_path::contains_ellipsis() const
{
  return std::find_if(m_path_legs.begin(), m_path_legs.end(),
                      is_ellipsis) != m_path_legs.end();
}


// Json_path parsing

void Json_path::initialize()
{
  m_path_legs.clear();
}

/** Top level parsing factory method */
bool parse_path(const bool begins_with_column_id, const size_t path_length,
                const char *path_expression, Json_path *path, size_t *bad_index)
{
  bool  status= false;

  const char *end_of_parsed_path=
    path->parse_path(begins_with_column_id, path_length, path_expression,
                     &status);

  if (status)
  {
    *bad_index= 0;
    return false;
  }

  *bad_index= end_of_parsed_path - path_expression;
  return true;
}


/**
  Purge leading whitespace in a string.
  @param[in] str  the string to purge whitespace from
  @param[in] end  the end of the input string
  @return pointer to the first non-whitespace character in str
*/
static inline const char *purge_whitespace(const char *str, const char *end)
{
  while (str < end && my_isspace(&my_charset_utf8mb4_bin, *str))
    ++str;
  return str;
}


const char *Json_path::parse_path(const bool begins_with_column_id,
                                  const size_t path_length,
                                  const char *path_expression,
                                  bool *status)
{
  initialize();

  const char *charptr= path_expression;
  const char *endptr= path_expression + path_length;

  if (begins_with_column_id)
  {
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
  }
  else
  {
    // the first non-whitespace character must be $
    charptr= purge_whitespace(charptr, endptr);
    if ((charptr >= endptr) || (*charptr++ != SCOPE))
      PARSER_RETURN(false);
  }

  // now add the legs
  *status= true;
  while (*status)
  {
    charptr= purge_whitespace(charptr, endptr);
    if (charptr >= endptr)
      break;                                    // input exhausted

    charptr= parse_path_leg(charptr, endptr, status);
  }

  // a path may not end with an ellipsis
  if (m_path_legs.size() > 0 && is_ellipsis(m_path_legs.back()))
  {
    *status= false;
  }

  return charptr;
}


const char *Json_path::parse_path_leg(const char *charptr,
                                      const char *endptr,
                                      bool *status)
{
  switch (*charptr)
  {
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
                                          const char *endptr,
                                          bool *status)
{
  // assume the worst
  *status= false;

  // advance past the first *
  charptr++;

  // must be followed by a second *
  if ((charptr >= endptr) || (*charptr++ != WILDCARD))
  {
    PARSER_RETURN(false);
  }

  // may not be the last leg
  if (charptr >= endptr)
  {
    PARSER_RETURN(false);
  }

  // forbid the hard-to-read *** combination
  if (*charptr == WILDCARD)
  {
    PARSER_RETURN(false);
  }

  PARSER_RETURN(!append(Json_path_leg(jpl_ellipsis)));
}


const char *Json_path::parse_array_leg(const char *charptr,
                                       const char *endptr,
                                       bool *status)
{
  // assume the worst
  *status= false;

  // advance past the [
  charptr++;

  charptr= purge_whitespace(charptr, endptr);
  if (charptr >= endptr)
    PARSER_RETURN(false);                       // input exhausted

  if (*charptr == WILDCARD)
  {
    charptr++;

    if (append(Json_path_leg(jpl_array_cell_wildcard)))
      PARSER_RETURN(false);                   /* purecov: inspected */
  }
  else
  {
    // Not a WILDCARD. Must be an array index.
    const char *number_start= charptr;

    while ((charptr < endptr) && is_digit(*charptr))
    {
      charptr++;
    }
    if (charptr == number_start)
    {
      PARSER_RETURN(false);
    }

    int dummy_err;
    longlong cell_index= my_strntoll(&my_charset_utf8mb4_bin, number_start,
                                     charptr - number_start, 10,
                                     (char**) 0, &dummy_err);

    if (dummy_err != 0)
    {
      PARSER_RETURN(false);
    }

    if (append(Json_path_leg(static_cast<size_t>(cell_index))))
      PARSER_RETURN(false);                   /* purecov: inspected */
  }

  // the next non-whitespace should be the closing ]
  charptr= purge_whitespace(charptr, endptr);
  if ((charptr < endptr) && (*charptr++ == END_ARRAY))
  {
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
static const char *find_end_of_member_name(const char *start, const char *end)
{
  const char *str= start;

  /*
    If we have a double-quoted name, the end of the name is the next
    unescaped double quote.
  */
  if (*str == DOUBLE_QUOTE)
  {
    str++;                   // Advance past the opening double quote.
    while (str < end)
    {
      switch (*str++)
      {
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
  while (str < end &&
         !my_isspace(&my_charset_utf8mb4_bin, *str) &&
         *str != BEGIN_ARRAY &&
         *str != BEGIN_MEMBER &&
         *str != WILDCARD)
  {
    str++;
  }

  return str;
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
static const Json_string *parse_name_with_rapidjson(const char *str, size_t len)
{
  const Json_dom *dom= Json_dom::parse(str, len, NULL, NULL);

  if (dom != NULL && dom->json_type() == Json_dom::J_STRING)
    return down_cast<const Json_string *>(dom);

  delete dom;
  return NULL;
}


const char *Json_path::parse_member_leg(const char *charptr,
                                        const char *endptr,
                                        bool *status)
{
  // advance past the .
  charptr++;

  charptr= purge_whitespace(charptr, endptr);
  if (charptr >= endptr)
    PARSER_RETURN(false);                       // input exhausted

  if (*charptr == WILDCARD)
  {
    charptr++;

    if (append(Json_path_leg(jpl_member_wildcard)))
      PARSER_RETURN(false);                   /* purecov: inspected */
  }
  else
  {
    const char *key_start= charptr;
    const char *key_end= find_end_of_member_name(key_start, endptr);
    const bool was_quoted= (*key_start == DOUBLE_QUOTE);

    charptr= key_end;

    std::auto_ptr<const Json_string> jstr;

    if (was_quoted)
    {
      /*
        Send the quoted name through the parser to unquote and
        unescape it.
      */
      jstr.reset(parse_name_with_rapidjson(key_start, key_end - key_start));
    }
    else
    {
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
        PARSER_RETURN(false);                 /* purecov: inspected */
      jstr.reset(parse_name_with_rapidjson(strbuff.ptr(), strbuff.length()));
    }

    if (jstr.get() == NULL)
      PARSER_RETURN(false);

    // empty key names are illegal
    if (jstr->size() == 0)
      PARSER_RETURN(false);

    // unquoted names must be valid ECMAScript identifiers
    if (!was_quoted &&
        !is_ecmascript_identifier(jstr->value().data(), jstr->size()))
      PARSER_RETURN(false);

    // Looking good.
    if (append(Json_path_leg(jstr->value())))
      PARSER_RETURN(false);                   /* purecov: inspected */
  }

  PARSER_RETURN(true);
}


/**
   Return true if the character is a unicode combining mark.

   @param codepoint [in] A unicode codepoint.

   @return True if the codepoint is a unicode combining mark.
*/
inline bool unicode_combining_mark(unsigned codepoint)
{
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
bool is_letter(unsigned codepoint)
{
  /*
    The Unicode combining mark \u036F passes the my_isalpha() test.
    That doesn't inspire much confidence in the correctness
    of my_isalpha().
   */
  if (unicode_combining_mark(codepoint))
  {
    return false;
  }
  return my_isalpha(&my_charset_utf8mb4_bin, codepoint);
}


/**
   Return true if the codepoint is a Unicode digit.

   This was the best
   recommendation from the old-times about how to answer this question.
*/
bool is_digit(unsigned codepoint)
{
  return my_isdigit(&my_charset_utf8mb4_bin, codepoint);
}


/**
   Return true if the codepoint is Unicode connector punctuation.
*/
bool is_connector_punctuation(unsigned codepoint)
{
  switch(codepoint)
  {
  case 0x5F:  // low line
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
  default:
    {
      return false;
    }
  }
}


/**
   Returns true if the name is a valid ECMAScript identifier.

   The name
   must be a sequence of UTF8-encoded bytes. All escape sequences
   have been replaced with UTF8-encoded bytes.

   @param[in] name        name to check
   @param[in] name_length its length

   @return True if the name is a valid ECMAScript identifier. False otherwise.
*/
bool is_ecmascript_identifier(const char *name, size_t name_length)
{
  /*
    At this point, The unicode escape sequences have already
    been replaced with the corresponding UTF-8 bytes. Now we apply
    the rules here: https://es5.github.io/x7.html#x7.6
  */
  rapidjson::MemoryStream input_stream(name, name_length);
  unsigned  codepoint;

  while (input_stream.Tell() < name_length)
  {
    bool  first_codepoint= (input_stream.Tell() == 0);
    if (!rapidjson::UTF8<char>::Decode(input_stream, &codepoint))
      return false;

    // a unicode letter
    if (is_letter(codepoint))
      continue;
    // $ is ok
    if (codepoint == 0x24)
      continue;
    // _ is ok
    if (codepoint == 0x5F)
      continue;

    /*
      the first character must be one of the above.
      more possibilities are available for subsequent characters.
    */

    if (first_codepoint)
    {
      return false;
    }
    else
    {
      // unicode combining mark
      if (unicode_combining_mark(codepoint))
        continue;

      // a unicode digit
      if (is_digit(codepoint))
        continue;
      if (is_connector_punctuation(codepoint))
        continue;
      // <ZWNJ>
      if (codepoint == 0x200C)
        continue;
      // <ZWJ>
      if (codepoint == 0x200D)
        continue;
    }

    // nope
    return false;
  }

  return true;
}
