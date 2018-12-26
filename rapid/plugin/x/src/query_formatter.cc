/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <algorithm>

#include "xpl_log.h"
#include "query_formatter.h"
#include "my_sys.h" // escape_string_for_mysql
#include "xpl_error.h"
#include "ngs/error_code.h"

using namespace xpl;

enum Block {Block_none, Block_string_quoted, Block_string_double_quoted, Block_identifier, Block_comment, Block_line_comment};

class Sql_search_tags
{
public:
  Sql_search_tags()
  : m_state(Block_none),
    m_matching_chars_comment(0),
    m_matching_chars_line_comment1(0),
    m_matching_chars_line_comment2(0),
    m_escape_chars(0)
  {
  }

  bool should_ignore_block(const char character, const Block try_block, const char character_begin, const char character_end, bool escape = false)
  {
    if (m_state != try_block && m_state != Block_none)
      return false;

    if (m_state == Block_none)
    {
      if (character_begin == character)
      {
        m_escape_chars = 0;
        m_state = try_block;

        return true;
      }
    }
    else
    {
      if (escape)
      {
        if (0 != m_escape_chars)
        {
          --m_escape_chars;
          return true;
        }
        else if ('\\' == character)
        {
          ++m_escape_chars;
          return true;
        }
      }

      if (character_end == character)
      {
        m_state = Block_none;
      }

      return true;
    }

    return false;
  }

  bool if_matching_switch_state(const char character, const Block try_block, uint8_t &matching_chars, const char *match, const std::size_t match_length)
  {
    bool repeat = true;

    while (repeat)
    {
      if (character == match[matching_chars])
      {
        ++matching_chars;
        break;
      }

      repeat = matching_chars != 0;

      matching_chars = 0;
    }

    if (matching_chars == match_length - 1)
    {
      m_state = try_block;
      matching_chars = 0;

      return true;
    }

    return false;
  }

  template <std::size_t block_begin_length, std::size_t block_end_length>
  bool should_ignore_block_multichar(const char character, const Block try_block_state, uint8_t &matching_chars, const char (&block_begin)[block_begin_length], const char (&block_end)[block_end_length])
  {
    if (m_state != try_block_state && m_state != Block_none)
      return false;

    if (m_state == Block_none)
    {
      return if_matching_switch_state(character, try_block_state, matching_chars, block_begin, block_begin_length);
    }
    else
    {
      if_matching_switch_state(character, Block_none, matching_chars, block_end, block_end_length);

      return true;
    }
  }

  bool should_be_ignored(const char character)
  {
    const bool escape_sequence = true;

    if (should_ignore_block(character, Block_string_quoted, '\'', '\'', escape_sequence))
      return true;

    if (should_ignore_block(character, Block_string_double_quoted, '"', '"', escape_sequence))
      return true;

    if (should_ignore_block(character, Block_identifier, '`', '`'))
      return true;

    if (should_ignore_block_multichar(character, Block_comment, m_matching_chars_comment, "/*", "*/"))
      return true;

    if (should_ignore_block_multichar(character, Block_line_comment, m_matching_chars_line_comment1, "#", "\n"))
      return true;

    if (should_ignore_block_multichar(character, Block_line_comment, m_matching_chars_line_comment2, "-- ", "\n"))
      return true;

    return false;
  }

  bool operator() (const char query_character)
  {
    if (should_be_ignored(query_character))
      return false;

    return query_character == '?';
  }

private:
  Block  m_state;
  uint8_t m_matching_chars_comment;
  uint8_t m_matching_chars_line_comment1;
  uint8_t m_matching_chars_line_comment2;
  uint8_t m_escape_chars;
};


Query_formatter::Query_formatter(ngs::PFS_string &query, charset_info_st &charset)
: m_query(query), m_charset(charset), m_last_tag_position(0)
{
}

Query_formatter &Query_formatter::operator % (const char *value)
{
  validate_next_tag();

  put_value_and_escape(value, strlen(value));

  return *this;
}

Query_formatter &Query_formatter::operator % (const No_escape<const char *> &value)
{
  validate_next_tag();

  put_value(value.m_value, strlen(value.m_value));

  return *this;
}

Query_formatter &Query_formatter::operator % (const std::string &value)
{
  validate_next_tag();

  put_value_and_escape(value.c_str(), value.length());

  return *this;
}

Query_formatter &Query_formatter::operator % (const No_escape<std::string> &value)
{
  validate_next_tag();

  put_value(value.m_value.c_str(), value.m_value.length());

  return *this;
}

void Query_formatter::validate_next_tag()
{
  ngs::PFS_string::iterator i = std::find_if(m_query.begin() + m_last_tag_position, m_query.end(), Sql_search_tags());

  if (m_query.end() == i)
  {
    throw ngs::Error_code(ER_X_CMD_NUM_ARGUMENTS, "Too many arguments");
  }

  m_last_tag_position = std::distance(m_query.begin(), i);
}

void Query_formatter::put_value_and_escape(const char *value, const std::size_t length)
{
  const std::size_t length_maximum = 2 * length + 1 + 2;
  std::string       value_escaped(length_maximum, '\0');

  std::size_t length_escaped = escape_string_for_mysql(&m_charset, &value_escaped[1], length_maximum, value, length);
  value_escaped[0] = value_escaped[1 + length_escaped] = '\'';

  value_escaped.resize(length_escaped + 2);

  put_value(value_escaped.c_str(), value_escaped.length());
}

void Query_formatter::put_value(const char *value, const std::size_t length)
{
  const uint8_t     tag_size = 1;
  const std::size_t length_source = m_query.length();
  const std::size_t length_target = m_query.length() +  length - tag_size;

  if (length_source < length_target)
  {
    m_query.resize(length_target, '\0');
  }

  ngs::PFS_string::iterator tag_position = m_query.begin() + m_last_tag_position;
  ngs::PFS_string::iterator move_to = tag_position + length;
  ngs::PFS_string::iterator move_from = tag_position + tag_size;

  std::copy(move_from, m_query.begin() + length_source, move_to);
  std::copy(value, value + length,  tag_position);

  m_last_tag_position += length;

  if (m_query.length() != length_target)
  {
    m_query.resize(length_target);
  }
}
