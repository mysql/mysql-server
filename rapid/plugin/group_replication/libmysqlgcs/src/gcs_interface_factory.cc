/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string>
#include <algorithm>
#ifdef _WIN32
#include<iterator>
#endif

#include "bindings/xcom/gcs_xcom_interface.h"

#include "gcs_interface.h"

Gcs_interface *
Gcs_interface_factory::
get_interface_implementation(enum_available_interfaces binding)
{
  Gcs_interface *retval= NULL;
  switch (binding)
  {
  case XCOM:
    retval= Gcs_xcom_interface::get_interface();
    break;
  default:
    break;
  }

  return retval;
}


Gcs_interface *
Gcs_interface_factory::get_interface_implementation(const std::string &binding)
{
  enum_available_interfaces binding_translation=
    Gcs_interface_factory::from_string(binding);

  return
    Gcs_interface_factory::get_interface_implementation(binding_translation);
}


void Gcs_interface_factory::cleanup(const std::string& binding)
{
  enum_available_interfaces binding_translation=
    Gcs_interface_factory::from_string(binding);

  Gcs_interface_factory::cleanup(binding_translation);
}


void Gcs_interface_factory::cleanup(enum_available_interfaces binding)
{
  switch(binding)
  {
  case XCOM:
    Gcs_xcom_interface::cleanup();
    break;
  default:
    break;
  }
}


enum_available_interfaces
Gcs_interface_factory::from_string(const std::string &binding)
{
  enum_available_interfaces retval= NONE;

  std::string binding_to_lower;
  binding_to_lower.clear();
  std::transform(binding.begin(), binding.end(),
                 std::back_inserter(binding_to_lower), ::tolower);

  if (binding_to_lower.compare("xcom") == 0)
    retval= XCOM;

  return retval;
}
