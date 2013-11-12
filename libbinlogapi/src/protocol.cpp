/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#include "protocol.h"
#include "transitional_methods.h"
#include "binary_log.h"
#include <my_global.h>
#include <mysql_com.h>
#include <iostream>
#include <stdint.h>
#include <vector>

namespace binary_log { namespace system {

buffer_source &operator>>(buffer_source &src, Protocol &chunk)
{
  unsigned int ct= 0;
  char *ptr= (char*)chunk.data();

  while(ct < chunk.size() && src.m_ptr < src.m_size)
  {
    ptr[ct]= src.m_src[src.m_ptr];
    ++ct;
    ++src.m_ptr;
  }
  return src;
}

std::istream &operator>>(std::istream &is, Protocol &chunk)
{
 if (chunk.is_length_encoded_binary())
  {
    unsigned int ct= 0;
    is.read((char *)chunk.data(),1);
    unsigned char byte= *(unsigned char *)chunk.data();
    if (byte < 250)
    {
      chunk.collapse_size(1);
      return is;
    }
    else if (byte == 251)
    {
      // is this a row data packet? if so, then this column value is NULL
      chunk.collapse_size(1);
      ct= 1;
    }
    else if (byte == 252)
    {
      chunk.collapse_size(2);
      ct= 1;
    }
    else if(byte == 253)
    {
      chunk.collapse_size(3);
      ct= 1;
    }

    /* Read remaining bytes */
    //is.read((char *)chunk.data(), chunk.size()-1);
    char ch;
    char *ptr= (char*)chunk.data();
    while(ct < chunk.size())
    {
      is.get(ch);
      ptr[ct]= ch;
      ++ct;
    }
  }
  else
  {
    char ch;
    int ct= 0;
    char *ptr= (char*)chunk.data();
    int sz= chunk.size();
    while(ct < sz)
    {
      is.get(ch);
      ptr[ct]= ch;
      ++ct;
    }
  }

  return is;
}

std::istream &operator>>(std::istream &is, std::string &str)
{
  std::ostringstream out;
  char ch;
  int ct= 0;
  do
  {
    is.get(ch);
    out.put(ch);
    ++ct;
  } while (is.good() && ch != '\0');
  str.append(out.str());
  return is;
}

std::istream &operator>>(std::istream &is, Protocol_chunk_string &str)
{
  char ch;
  int ct= 0;
  int sz= str.m_str->size();
  for (ct=0; ct< sz && is.good(); ct++)
  {
    is.get(ch);
    str.m_str->at(ct)= ch;
  }

  return is;
}

std::istream &operator>>(std::istream &is, Protocol_chunk_string_len &lenstr)
{
  uint8_t len;
  std::string *str= lenstr.m_storage;
  Protocol_chunk<uint8_t> proto_str_len(len);
  is >> proto_str_len;
  Protocol_chunk_string   proto_str(*str, len);
  is >> proto_str;
  return is;
}

std::ostream &operator<<(std::ostream &os, Protocol &chunk)
{
  if (!os.bad())
    os.write((const char *) chunk.data(),chunk.size());
  return os;
}

/* Removes trailing whitspaces from the input string */
void trim2(std::string& str)
{
  std::string::size_type pos = str.find_last_not_of('\0');
  if (pos != std::string::npos)
    str.erase(pos + 1);
  else
    str.clear();
}

} } // end namespace binary_log::system
