/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/properties_impl.h"

#include <stddef.h>
#include <new>

#include "sql/dd/impl/utils.h" // eat_pairs

namespace dd {

///////////////////////////////////////////////////////////////////////////

const String_type Properties_impl::EMPTY_STR= "";

///////////////////////////////////////////////////////////////////////////

Properties_impl::Properties_impl()
 :m_map(new Properties::Map())
{ } /* purecov: tested */

///////////////////////////////////////////////////////////////////////////


/**
  Parse the submitted string for properties on the format
  "key=value;key=value;...". Create new property object and add
  the properties to the map in the object.

  @param raw_properties  string containing list of key=value pairs
  @return                pointer to new Property_impl object
    @retval NULL         if an error occurred
*/
Properties *Properties::parse_properties(const String_type &raw_properties)
{ return Properties_impl::parse_properties(raw_properties); }

Properties *Properties_impl::parse_properties(const String_type &raw_properties)
{
  Properties *tmp= new (std::nothrow) Properties_impl();
  String_type::const_iterator it= raw_properties.begin();

  if (eat_pairs(it, raw_properties.end(), tmp))
  {
    delete tmp;
    tmp= NULL;
  }

  return tmp;
}

///////////////////////////////////////////////////////////////////////////


/**
  Iterate over all entries in the private hash table. For each
  key value pair, escape both key and value, and append the strings
  to the result. Use '=' to separate key and value, and use ';'
  to separate pairs.

  @return string containing all escaped key value pairs
*/

const String_type Properties_impl::raw_string() const
{
  String_type str("");
  str.reserve(16*m_map->size());

  // Iterate over all map entries
  const Const_iterator map_end= m_map->end();
  for (Const_iterator it= m_map->begin(); it != map_end; ++it)
  {
    escape(&str, it->first);
    str.append("=");
    escape(&str, it->second);
    str.append(";");
  }
  return str;
}

///////////////////////////////////////////////////////////////////////////

Properties *parse_properties(const String_type &str)
{
  return Properties_impl::parse_properties(str);
}

}
