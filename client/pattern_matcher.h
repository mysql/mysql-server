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

#include <m_ctype.h>
#include <unordered_set>
#include <string>

/**
  Enables comparison of strings against particular set of patterns. Patterns
  may contain wildcards (WILD_ONE/WILD_MANY/WILD_ESCAPE). Pattern strings may
  be added to the class through a special method. Matching method traverses all
  of the patterns within pattern matcher in search for a match.
*/
class Pattern_matcher
{

public:

  size_t add_patterns(const std::string& patterns, char delimiter= ':');
  bool is_matching(const std::string& text, const CHARSET_INFO* info) const;
  void clear();

private:

  /** any (single) character wild card */
  const static char WILD_ONE= '?';

  /** zero or many characters wild card */
  const static char WILD_MANY= '*';

  /** escape sequence character */
  const static char WILD_ESCAPE= '\\';

  /** used for storing matcher patterns */
  std::unordered_set<std::string> m_patterns;
};
