/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_types.h"
#include <iostream>
#include <utility>
#include <map>

void Gcs_interface_parameters::add_parameter(const std::string &name,
                                             const std::string &value)
{
  std::pair<const std::string, const std::string> to_add(name, value);

  parameters.erase(name);
  parameters.insert(to_add);
}

/* purecov: begin deadcode */
bool Gcs_interface_parameters::check_parameters(const char* params[], int size) const
{
  for (int index=0; index < size; index++)
  {
    std::string param(params[index]);
    if (get_parameter(param))
      return true;
  }
  return false;
}

bool Gcs_interface_parameters::check_parameters(const std::vector<std::string> &params) const
{
  for (std::vector<std::string>::const_iterator it = params.begin() ;
       it != params.end(); ++it)
  {
    if (get_parameter(*it))
      return true;
  }
  return false;
}
/* purecov: end */

const std::string *
Gcs_interface_parameters::get_parameter(const std::string &name) const
{
  const std::string *retval= NULL;

  std::map<std::string, std::string>::const_iterator to_find;

  if ((to_find= parameters.find(name)) != parameters.end())
  {
    retval= &((*to_find).second);
  }

  return retval;
}
