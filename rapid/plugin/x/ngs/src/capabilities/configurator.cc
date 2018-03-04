/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/ngs/include/ngs/capabilities/configurator.h"

#include <algorithm>

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/ngs_error.h"


namespace ngs
{


using ::Mysqlx::Connection::Capabilities;

typedef std::vector<Capability_handler_ptr>::const_iterator Handler_ptrs_iterator;


bool operator==(const Capability_handler_ptr &handler, const std::string &name)
{
  return handler->name() == name;
}


Capabilities_configurator::Capabilities_configurator(const std::vector<Capability_handler_ptr> &capabilities)
: m_capabilities(capabilities)
{
}


void Capabilities_configurator::add_handler(Capability_handler_ptr handler)
{
  m_capabilities.push_back(handler);
}


Capabilities *Capabilities_configurator::get()
{
  Capabilities          *result = ngs::allocate_object<Capabilities>();
  Handler_ptrs_iterator  i = m_capabilities.begin();

  while (i !=m_capabilities.end())
  {
    const Capability_handler_ptr handler = *i;

    if (handler->is_supported())
    {
      ::Mysqlx::Connection::Capability *c = result->add_capabilities();

      c->set_name(handler->name());

      handler->get(*c->mutable_value());
    }

    ++i;
  }

  return result;
}


ngs::Error_code Capabilities_configurator::prepare_set(const ::Mysqlx::Connection::Capabilities &capabilities)
{
  std::size_t capabilities_size = capabilities.capabilities_size();

  m_capabilities_prepared.clear();

  for(std::size_t index = 0; index < capabilities_size; ++index)
  {
    const ::Mysqlx::Connection::Capability &c = capabilities.capabilities(static_cast<int>(index));
    Capability_handler_ptr handler = get_capabilitie_by_name(c.name());

    if (!handler)
    {
      m_capabilities_prepared.clear();

      return Error(ER_X_CAPABILITY_NOT_FOUND, "Capability '%s' doesn't exist", c.name().c_str());
    }

    if (!handler->set(c.value()))
    {
      m_capabilities_prepared.clear();
      return Error(ER_X_CAPABILITIES_PREPARE_FAILED, "Capability prepare failed for '%s'", c.name().c_str());
    }

    m_capabilities_prepared.push_back(handler);
  }

  return Error_code();
}


Capability_handler_ptr Capabilities_configurator::get_capabilitie_by_name(const std::string &name)
{
  Handler_ptrs_iterator result =  std::find(m_capabilities.begin(), m_capabilities.end(), name);

  if (m_capabilities.end() == result)
  {
    return Capability_handler_ptr();
  }

  return *result;
}


void Capabilities_configurator::commit()
{
  Handler_ptrs_iterator i = m_capabilities_prepared.begin();

  while (i !=m_capabilities_prepared.end())
  {
    (*i++)->commit();
  }

  m_capabilities_prepared.clear();
}


} // namespace ngs
