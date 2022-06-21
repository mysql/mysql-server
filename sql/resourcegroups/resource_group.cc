/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "sql/resourcegroups/resource_group.h"

#include "sql/resourcegroups/resource_group_mgr.h"  // Resource_group_mgr

namespace resourcegroups {
/**
  Default resource group switch handler instance.
*/
Resource_group_switch_handler default_rg_switch_handler;

bool Resource_group_switch_handler::apply(Resource_group *new_rg,
                                          my_thread_os_id_t thread_os_id,
                                          bool *is_rg_applied_to_thread) {
  bool ret_val = (thread_os_id != 0)
                     ? new_rg->controller()->apply_control(thread_os_id)
                     : new_rg->controller()->apply_control();

  assert(is_rg_applied_to_thread != nullptr);
  if (is_rg_applied_to_thread != nullptr) *is_rg_applied_to_thread = !ret_val;

  return ret_val;
}
}  // namespace resourcegroups
