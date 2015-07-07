/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "pattern_matcher.h"

using namespace Mysql::Tools::Dump::Detail;


Pattern_matcher::Pattern_matcher()
{}

bool Pattern_matcher::is_pattern_matched(
  const std::string& to_match, const std::string& pattern,
  size_t i/*= 0*/, size_t j/*= 0*/)
{
  while (i < to_match.size() && j < pattern.size())
  {
    if (pattern[j] == '%')
    {
      /*
      Check two possibilities: either we stop consuming to_match with
      this instance or we consume one more character.
      */
      if (is_pattern_matched(to_match, pattern, i + 1, j))
        return true;
      j++;
    }
    else if (pattern[j] == '_' || pattern[j] == to_match[i])
    {
      i++;
      j++;
    }
    else
      return false;
  }
  /*
  There might be % pattern matching characters on the end of pattern, we
  can omit them.
  */
  while (j < pattern.size() && pattern[j] == '%')
    j++;
  return i == to_match.size() && j == pattern.size();
}
