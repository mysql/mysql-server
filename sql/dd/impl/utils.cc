/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/utils.h"

#include "dd/properties.h"          // dd::Properties

namespace dd {

///////////////////////////////////////////////////////////////////////////

const std::string escape(const std::string &src)
{
  std::string dest= src;
  for (std::string::iterator d= dest.begin(); d != dest.end(); d++)
    if (*d == '\\' || *d == '=' || *d == ';')
    {
      d= dest.insert(d, '\\');
      // Advance to get past the escaped character
      d++;
    }
  return dest;
}

///////////////////////////////////////////////////////////////////////////

bool unescape(std::string &dest)
{
  for (std::string::iterator d= dest.begin(); d != dest.end(); d++)
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

bool eat_to(std::string::const_iterator &it,
            std::string::const_iterator end,
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

bool eat_str(std::string &dest, std::string::const_iterator &it,
             std::string::const_iterator end, char c)
{
  // Save starting point for later copying
  std::string::const_iterator start= it;

  // Find the first unescaped occurrence of c, or the end
  if (eat_to(it, end, c))
    return true;

  // Create destination string up to, but not including c
  dest= std::string(start, it);

  // Remove escape characters
  if (unescape(dest))
    return true;

  // Make iterator point to character after c or at the end of the string
  if (it != end)
    it++;

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool eat_pairs(std::string::const_iterator &it,
               std::string::const_iterator end,
               dd::Properties *props)
{
  std::string key("");
  std::string val("");

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
