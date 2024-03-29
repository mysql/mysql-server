/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/udf/udf_registration.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/udf_registration.h>
#include <array>
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_communication_protocol.h"
#include "plugin/group_replication/include/udf/udf_descriptor.h"
#include "plugin/group_replication/include/udf/udf_member_actions.h"
#include "plugin/group_replication/include/udf/udf_multi_primary.h"
#include "plugin/group_replication/include/udf/udf_single_primary.h"
#include "plugin/group_replication/include/udf/udf_write_concurrency.h"

/* The UDFs we will register. */
static std::array<udf_descriptor, 10> udfs = {
    {/* single primary */
     set_as_primary_udf(), switch_to_single_primary_udf(),
     /* multi primary */
     switch_to_multi_primary_udf(),
     /* write concurrency */
     get_write_concurrency_udf(), set_write_concurrency_udf(),
     /* group communication protocol */
     get_communication_protocol_udf(), set_communication_protocol_udf(),
     /* member actions */
     enable_member_action_udf(), disable_member_action_udf(),
     reset_member_actions_udf()}};

bool register_udfs() {
  bool error = false;
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();

  if (plugin_registry == nullptr) {
    /* purecov: begin inspected */
    error = true;
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UDF_REGISTER_SERVICE_ERROR);
    goto end;
    /* purecov: end */
  }

  {
    /* We open a new scope so that udf_registrar is (automatically) destroyed
       before plugin_registry. */
    my_service<SERVICE_TYPE(udf_registration)> udf_registrar("udf_registration",
                                                             plugin_registry);
    if (udf_registrar.is_valid()) {
      for (udf_descriptor const &udf : udfs) {
        error = udf_registrar->udf_register(
            udf.name, udf.result_type, udf.main_function, udf.init_function,
            udf.deinit_function);
        if (error) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UDF_REGISTER_ERROR, udf.name);
          break;
          /* purecov: end */
        }
      }

      if (error) {
        /* purecov: begin inspected */
        int was_present;
        for (udf_descriptor const &udf : udfs) {
          // Don't care about errors since we are already erroring out.
          udf_registrar->udf_unregister(udf.name, &was_present);
        }
        /* purecov: end */
      }
    } else {
      /* purecov: begin inspected */
      error = true;
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UDF_REGISTER_SERVICE_ERROR);
      /* purecov: end */
    }
  }
  mysql_plugin_registry_release(plugin_registry);
end:
  return error;
}

bool unregister_udfs() {
  bool error = false;

  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();

  if (plugin_registry == nullptr) {
    /* purecov: begin inspected */
    error = true;
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UDF_UNREGISTER_ERROR);
    goto end;
    /* purecov: end */
  }

  {
    /* We open a new scope so that udf_registrar is (automatically) destroyed
       before plugin_registry. */
    my_service<SERVICE_TYPE(udf_registration)> udf_registrar("udf_registration",
                                                             plugin_registry);
    if (udf_registrar.is_valid()) {
      int was_present;
      for (udf_descriptor const &udf : udfs) {
        // Don't care about the functions not being there.
        error = error || udf_registrar->udf_unregister(udf.name, &was_present);
      }
    } else {
      error = true;
    }

    if (error) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UDF_UNREGISTER_ERROR);
    }
  }
  mysql_plugin_registry_release(plugin_registry);
end:
  return error;
}
