/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/utils.h"

#include <string>

#include "sql/dd/properties.h"      // dd::Properties
#include "sql/stateless_allocator.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

void escape(String_type *sp, const String_type &src)
{
  const String_type::const_iterator src_end= src.end();
  for (String_type::const_iterator s= src.begin(); s != src_end; ++s)
  {
    if (*s == '\\' || *s == '=' || *s == ';')
    {
      sp->append(1, '\\');
    }
    sp->append(1, *s);
  }
}

///////////////////////////////////////////////////////////////////////////

bool unescape(String_type &dest)
{
  for (String_type::iterator d= dest.begin(); d != dest.end(); d++)
    if (*d == '\\')
    {
      // An escape character preceeding end is an error, it must be succeeded
      // by an escapable character.
      if ((d + 1) != dest.end() &&
          (*(d + 1) == '\\' || *(d + 1) == '=' || *(d + 1) == ';'))
        d= dest.erase(d);
      else
        return true;
    }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool eat_to(String_type::const_iterator &it,
            String_type::const_iterator end,
            char c)
{
  // Verify valid stop characters
  if (c != '=' && c != ';')
    return true;

  // Loop until end of string or stop character
  while (it != end && *it != c)
  {
    // Unexpected unescaped stop character is an error
    if ((*it == '=' && c == ';') || (*it == ';' && c == '='))
      return true;

    // The escape character must be succeeded by an escapable character
    if (*it == '\\')
    {
      it++;
      // Hitting end here is an error, we must have an escapable character
      if (it == end || (*it != '\\' && *it != '=' && *it != ';'))
        return true;
    }

    // Advance iterator, also after finding an escapable character
    it++;
  }

  // Hitting end searching for ';' is ok; if searching for '=', it is not
  return (it == end && c == '=');
}

///////////////////////////////////////////////////////////////////////////

bool eat_str(String_type &dest, String_type::const_iterator &it,
             String_type::const_iterator end, char c)
{
  // Save starting point for later copying
  String_type::const_iterator start= it;

  // Find the first unescaped occurrence of c, or the end
  if (eat_to(it, end, c))
    return true;

  // Create destination string up to, but not including c
  dest= String_type(start, it);

  // Remove escape characters
  if (unescape(dest))
    return true;

  // Make iterator point to character after c or at the end of the string
  if (it != end)
    it++;

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool eat_pairs(String_type::const_iterator &it,
               String_type::const_iterator end,
               dd::Properties *props)
{
  String_type key("");
  String_type val("");

  if (it == end) return false;

  if (eat_str(key, it, end, '=') || eat_str(val, it, end, ';'))
    return true;

  // Empty keys are rejected, empty values are ok
  if (key == "")
    return true;

  props->set(key, val);

  return eat_pairs(it, end, props);
}

///////////////////////////////////////////////////////////////////////////

}
