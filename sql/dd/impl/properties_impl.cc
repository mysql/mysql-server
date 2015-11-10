/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/properties_impl.h"

#include "dd/impl/utils.h"     // eat_pairs

namespace dd {

///////////////////////////////////////////////////////////////////////////

const std::string Properties_impl::EMPTY_STR= "";

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

Properties *Properties_impl::parse_properties(const std::string &raw_properties)
{
  Properties *tmp= new (std::nothrow) Properties_impl();
  std::string::const_iterator it= raw_properties.begin();

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

const std::string Properties_impl::raw_string() const
{
  std::string str("");

  // Iterate over all map entries
  for (Iterator it= m_map->begin(); it != m_map->end(); it++)
    str+= escape(it->first) + "=" + escape(it->second) + ";";

  return str;
}

///////////////////////////////////////////////////////////////////////////

}
