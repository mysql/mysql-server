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
