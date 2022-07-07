/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PARSE_MASK_HPP
#define PARSE_MASK_HPP

#include <util/BaseString.hpp>
#include <util/SparseBitmask.hpp>
#include <ctype.h>

#define PARSE_END_ENTRIES 8192
#define MAX_STRING_SIZE 32

struct ParseEntries
{
  const char *m_name;
  unsigned int m_type;
};

struct ParseParams
{
  const char *name;
  enum { S_UNSIGNED, S_BITMASK, S_STRING } type;
};

struct ParamValue
{
  ParamValue() { found = false;}
  bool found;
  char buf[MAX_STRING_SIZE];
  char * string_val;
  unsigned unsigned_val;
  SparseBitmask mask_val;
};

class ParseThreadConfiguration
{
public:
  ParseThreadConfiguration(const char *str,
                           const struct ParseEntries *parse_entries,
                           const unsigned int num_parse_entries,
                           const struct ParseParams *parse_params,
                           const unsigned int num_parse_params,
                           BaseString &err_msg);
  ~ParseThreadConfiguration();

  int read_params(ParamValue values[],
                  unsigned int num_values,
                  unsigned int *type,
                  int *ret_code,
                  bool allow_empty);

private:
  unsigned int get_entry_type(const char *type);
  void skipblank();
  unsigned int get_param_len();
  int find_params(char **start, char **end);
  int parse_params(char *start, ParamValue values[]);
  unsigned int find_type();
  int find_next();
  int parse_string(char *dst);
  int parse_unsigned(unsigned *dst);
  int parse_bitmask(SparseBitmask& mask);

  char *m_curr_str;
  char *m_save_str;
  const struct ParseEntries *m_parse_entries;
  const unsigned int m_num_parse_entries;
  const struct ParseParams *m_parse_params;
  const unsigned int m_num_parse_params;
  BaseString & m_err_msg;
  bool m_first;
};

/**
 * Parse string with numbers format
 *   1,2,3-5
 * @return -1 if unparsable chars found,
 *         -2 str has number > bitmask size
 *            else returns number of bits set
 */
template <typename T>
inline int
parse_mask(const char *str, T& mask)
{
  int cnt = 0;
  BaseString tmp(str);
  Vector<BaseString> list;

  if (tmp.trim().empty())
  {
    return 0; /* Empty bitmask allowed */
  }
  tmp.split(list, ",");
  for (unsigned i = 0; i<list.size(); i++)
  {
    list[i].trim();
    if (list[i].empty())
    {
      return -3;
    }
    char * delim = const_cast<char*>(strchr(list[i].c_str(), '-'));
    unsigned first = 0;
    unsigned last = 0;
    if (delim == nullptr)
    {
      int res = sscanf(list[i].c_str(), "%u", &first);
      if (res != 1)
      {
        return -1;
      }
      last = first;
    }
    else
    {
      * delim = 0;
      delim++;
      int res0 = sscanf(list[i].c_str(), "%u", &first);
      if (res0 != 1)
      {
        return -1;
      }
      int res1 = sscanf(delim, "%u", &last);
      if (res1 != 1)
      {
        return -1;
      }
      if (first > last)
      {
        unsigned tmp = first;
        first = last;
        last = tmp;
      }
    }

    for (unsigned j = first; j<(last+1); j++)
    {
      if (j > mask.max_size())
      {
        return -2;
      }
      cnt++;
      mask.set(j);
    }
  }
  return cnt;
}

#endif
