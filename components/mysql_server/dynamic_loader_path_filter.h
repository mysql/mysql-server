/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVER_DYNAMIC_LOADER_PATH_FILTER_H
#define MYSQL_SERVER_DYNAMIC_LOADER_PATH_FILTER_H

#include <mysql/components/service_implementation.h>
#include "../../../sql/sql_plugin.h" // opt_plugin_dir
#include "server_component.h"
#include <string>

/**
  Checks if path specified to load is contained in plug-in directory and
  change it to absolute one using plug-in directory. Calls wrapped file scheme
  service implementation on calculated absolute URN. Effectively it act as a
  filtering and mapping service.
*/
class mysql_dynamic_loader_scheme_file_path_filter_imp
{
public:
  /**
    Checks if path specified to load is contained in plug-in directory and
    change it to absolute one using plug-in directory. Calls wrapped file scheme
    service implementation on calculated absolute URN.

    @param urn URN to file to load components from.
    @param [out] out_data Pointer to pointer to MySQL component data structures
      to set result components data retrieved from specified file.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(load,
    (const char *urn, mysql_component_t** out_data));

  /**
    Checks if path specified to load is contained in plug-in directory and
    change it to absolute one using plug-in directory. Calls wrapped file scheme
    service implementation on calculated absolute URN.

    @param urn URN to file to unload all components from.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(unload,
    (const char *urn));

private:
  /**
    Ensure that the dynamic library doesn't have a path.
    This is done to ensure that only approved libraries from the
    plug-in directory are used (to make this even remotely secure).
    Extracts real absolute path to file in plug-in directory.

    @param input_urn URN with path to validate and make absolute.
    @param [out] out_path String to put result URN to.
  */
  static bool check_and_make_absolute_urn(
    const char* input_urn, std::string& out_path);
};

#endif /* MYSQL_SERVER_DYNAMIC_LOADER_PATH_FILTER_H */