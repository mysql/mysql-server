/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include "components/mysql_server/mysql_component.h"

#include <stddef.h>
#include <vector>

mysql_component::mysql_component(
  mysql_component_t* component_data, my_string urn)
  : m_component_data(component_data),
  m_urn(urn)
{
  for (const mysql_metadata_ref_t* metadata_iterator= component_data->metadata;
    metadata_iterator->key != NULL;
    ++metadata_iterator)
  {
    this->set_value(metadata_iterator->key, metadata_iterator->value);
  }
}

/**
  Gets name of this component.

  @return Pointer to component name. Pointer is valid till component is not
    unloaded.
*/
const char* mysql_component::name_c_str() const
{
  return m_component_data->name;
}

/**
  Gets original URN used to load this component.

  @return Pointer to URN. Pointer is valid till component is not unloaded.
*/
const char* mysql_component::urn_c_str() const
{
  return m_urn.c_str();
}

/**
  Gets original URN used to load this component.

  @return Reference to string object with URN. Pointer is valid till component
    is not unloaded.
*/
const my_string& mysql_component::get_urn() const
{
  return m_urn;
}

/**
  Gets list of all service implementations provided by this component.

  @return List of service implementations.
*/
std::vector<const mysql_service_ref_t*>
  mysql_component::get_provided_services() const
{
  std::vector<const mysql_service_ref_t*> res;
  for (mysql_service_ref_t* implementation_it= m_component_data->provides;
    implementation_it->implementation != NULL;
    ++implementation_it)
  {
    res.push_back(implementation_it);
  }
  return res;
}

/**
  Gets list of services required by this component to work.

  @return List of service names.
*/
std::vector<mysql_service_placeholder_ref_t*>
  mysql_component::get_required_services() const
{
  std::vector<mysql_service_placeholder_ref_t*> res;
  for (mysql_service_placeholder_ref_t* implementation_it=
    m_component_data->requires;
    implementation_it->name != NULL;
    ++implementation_it)
  {
    res.push_back(implementation_it);
  }
  return res;
}

/**
  Gets underlaying component data structure.

  @return Component data structure.
*/
const mysql_component_t* mysql_component::get_data() const
{
  return m_component_data;
}
