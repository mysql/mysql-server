/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <my_sys.h>
#include "pattern_matcher.h"


/**
  @brief Parses concatenated patterns and adds them to internal pattern list

  @param[in] patterns     concatenated patterns
  @param[in] delimiter    delimiter for pattern concatenation

  @return                 number of added patterns
*/
size_t Pattern_matcher::add_patterns(const std::string& patterns, char delimiter)
{
  DBUG_ENTER("Pattern_matcher::add_patterns");
  size_t length= patterns.length();
  size_t pattern_count= 0;

  // we don't parse empty patterns
  if (length == 0)
    DBUG_RETURN(pattern_count);

  size_t first= 0;
  size_t last= 0;
  do
  {
    // find end of the token
    if ((last= patterns.find(delimiter, first)) == std::string::npos)
      last= length;

    // we store only tokens that are not empty
    if (last - first > 0)
    {
      m_patterns.emplace(patterns, first, last - first);
      ++pattern_count;
    }

    first= last + 1;
  } while (last != length);

  DBUG_RETURN(pattern_count);
}


/**
  @brief Verifies whether text matches any of the matcher internal patterns

  @param[in]  text      string to search for patterns
  @param[in]  info      charset information for comparison rules

  @return               result of matching the text to internal patterns
    @retval   true        at least one pattern matches provided string
    @retval   false       string does not match any of the patterns
*/
bool Pattern_matcher::is_matching(const std::string& text, const CHARSET_INFO* info) const
{
  DBUG_ENTER("Pattern_matcher::is_matching");

  // traverse all patterns, return true on first match
  for (auto& pattern : m_patterns)
    if (info->coll->wildcmp(info,
                           text.c_str(), text.c_str() + text.length(),
                           pattern.c_str(), pattern.c_str() + pattern.length(),
                           WILD_ESCAPE, WILD_ONE, WILD_MANY) == 0)
      DBUG_RETURN(true);

  // none of the patterns matched
  DBUG_RETURN(false);
}


/**
  @brief Removes all previously stored patterns from pattern matcher
*/
void Pattern_matcher::clear()
{
  DBUG_ENTER("Pattern_matcher::clear");
  m_patterns.clear();
}
