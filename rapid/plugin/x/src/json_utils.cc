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

#include "json_utils.h"
#include "my_dbug.h"
#include <ctype.h>
#include "xpl_error.h"

#if 0
bool xpl::validate_json_string(const char *s, size_t length)
{
  const char *c = s;
  const char *end = s + length;

  if (*c == '"') // string literal
  {
    ++c;
    while (c < end)
    {
      if (iscntrl(*c) || *c == 0)
      {
        return false;
      }
      if (*c == '\\')
      {
        if (c >= end-1)
          return false;
        /* The allowed escape codes:
         \"
         \\
         \/
         \b
         \f
         \n
         \r
         \t
         \u four-hex-digits
         */
        ++c;
        switch (*c)
        {
          case '"':
          case '\\':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
            ++c;
            break;
          case 'u':
            if (c < end - 5 && isxdigit(c[1]) && isxdigit(c[2]) && isxdigit(c[3]) && isxdigit(c[4]))
              c += 5;
            else
              return false;
            break;
          default:
            return false;
        }
      }
      else if (*c == '"')
        break;
      else
        ++c;
    }
    if (*c != '"')
      return false;
    ++c;
  }
  else
    return false;


  return true;
}


bool xpl::validate_json_string(const std::string &s)
{
  return xpl::validate_json_string(s.data(), s.length());
}

ngs::Error_code xpl::validate_json_document_path(const std::string &s)
{
  if (s.empty())
    return ngs::Error(ER_X_BAD_DOC_PATH, "Empty document path");

  return ngs::Error_code();
}
#endif

std::string xpl::quote_json(const std::string &s)
{
  std::string out;
  size_t i, end = s.length();

  out.reserve(s.length() * 2 + 1);

  out.push_back('"');

  for (i = 0; i < end; ++i)
  {
    switch (s[i])
    {
      case '"':
        out.append("\\\"");
        break;

      case '\\':
        out.append("\\\\");
        break;

      case '/':
        out.append("\\/");
        break;

      case '\b':
        out.append("\\b");
        break;

      case'\f':
        out.append("\\f");
        break;

      case '\n':
        out.append("\\n");
        break;

      case '\r':
        out.append("\\r");
        break;

      case '\t':
        out.append("\\t");
        break;

      default:
        out.push_back(s[i]);
        break;
    }
  }
  out.push_back('"');
  return out;
}


std::string xpl::quote_json_if_needed(const std::string &s)
{
  size_t i, end = s.length();

  if (isalpha(s[0]) || s[0] == '_')
  {
    for (i = 1; i < end && (isdigit(s[i]) || isalpha(s[i]) || s[i] == '_'); i++)
    {}
    if (i == end)
      return s;
  }
  return quote_json(s);
}
