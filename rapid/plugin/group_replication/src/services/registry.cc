/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "services/registry.h"

const std::string Registry_module_interface::SVC_NAME_MEMBERSHIP=
  "group_membership_listener";

const std::string Registry_module_interface::SVC_NAME_STATUS=
  "group_member_status_listener";

const std::string Registry_module_interface::SVC_NAME_REGISTRY_QUERY=
  "registry_query";

bool
Registry_module::initialize()
{
  bool res= false;
  my_h_service h= NULL;
  m_registry= mysql_plugin_registry_acquire();
  if(!m_registry)
  {
    /* purecov: begin inspected */
    res= true;
    goto end;
    /* purecov: end */
  }

  if (m_registry->acquire(SVC_NAME_REGISTRY_QUERY.c_str(), &h) || !h)
  {
    /* purecov: begin inspected */
    res= true;
    goto end;
    /* purecov: end */
  }
  m_registry_query= reinterpret_cast<SERVICE_TYPE(registry_query) *>(h);

end:
  if (res)
  {
    /* On error, cleanup. */
    finalize(); /* purecov: inspected */
  }

  return res;
}

bool
Registry_module::finalize()
{
  bool res= false;
  my_h_service h;

  /* release the registry query service */
  if (m_registry_query)
  {
    h= const_cast<my_h_service>(
        reinterpret_cast<const my_h_service_imp*>(
          m_registry_query));
    if (m_registry->release(h))
      res= true; /* purecov: inspected */
    else
      m_registry_query= NULL;
  }

  /* release the registry handle */
  if (m_registry && mysql_plugin_registry_release(m_registry))
    res= true; /* purecov: inspected */
  else
    m_registry= NULL;

  return res;
}

SERVICE_TYPE(registry) *Registry_module::get_registry_handle()
{
  return m_registry;
}

SERVICE_TYPE(registry_query) *Registry_module::get_registry_query_handle()
{
  return m_registry_query;
}