/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "xpl_regex.h"

#include "my_dbug.h"
#include <cstring>


namespace
{

inline void check_result(const int err)
{
  DBUG_ASSERT( err == 0 );
}


const struct Regex_finalizer
{
  Regex_finalizer() {}
  ~Regex_finalizer() { my_regex_end(); }
} regex_finalizer;

} // namespace


xpl::Regex::Regex(const char* const pattern)
{
  memset(&m_re, 0, sizeof(m_re));
  int err = my_regcomp(&m_re, pattern,
                       (MY_REG_EXTENDED | MY_REG_ICASE | MY_REG_NOSUB),
                       &my_charset_latin1);

  // Workaround for unused variable
  check_result(err);
}


xpl::Regex::~Regex()
{
  my_regfree(&m_re);
}


bool xpl::Regex::match(const char *value) const
{
  return my_regexec(&m_re, value, (size_t)0, NULL, 0) == 0;
}

