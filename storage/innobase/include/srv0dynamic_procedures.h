/*****************************************************************************

Copyright (c) 2020, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/srv0dynamic_procedures.h
 Helper for managing dynamic SQL procedures.

 *******************************************************/

#ifndef srv0dynamic_procedures_h
#define srv0dynamic_procedures_h

#include <string>
#include <vector>

#include "mysql/components/my_service.h"
#include "mysql/components/services/udf_registration.h"
#include "mysql/plugin.h"
#include "mysql/service_plugin_registry.h"
#include "sql/auth/auth_acls.h"
#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "univ.i" /* LogErr */

namespace srv {
#ifndef UNIV_HOTBACKUP
/**
  Type and data for tracking registered UDFs.
*/
struct dynamic_procedure_data_t {
  const std::string m_name;
  const Item_result m_return_type;
  const Udf_func_any m_func;
  const Udf_func_init m_init_func;
  const Udf_func_deinit m_deinit_func;

  dynamic_procedure_data_t(const std::string &name, const Udf_func_string func,
                           const Udf_func_init init_func,
                           const Udf_func_deinit deinit_func,
                           const Item_result return_type = STRING_RESULT)
      : m_name(name),
        m_return_type(return_type),
        m_func(reinterpret_cast<Udf_func_any>(func)),
        m_init_func(init_func),
        m_deinit_func(deinit_func) {}

  dynamic_procedure_data_t(const std::string &name,
                           const Udf_func_longlong func,
                           const Udf_func_init init_func,
                           const Udf_func_deinit deinit_func)
      : m_name(name),
        m_return_type(INT_RESULT),
        m_func(reinterpret_cast<Udf_func_any>(func)),
        m_init_func(init_func),
        m_deinit_func(deinit_func) {}

  dynamic_procedure_data_t(const std::string &name, const Udf_func_double func,
                           const Udf_func_init init_func,
                           const Udf_func_deinit deinit_func)
      : m_name(name),
        m_return_type(REAL_RESULT),
        m_func(reinterpret_cast<Udf_func_any>(func)),
        m_init_func(init_func),
        m_deinit_func(deinit_func) {}
};

class Dynamic_procedures {
 public:
  virtual ~Dynamic_procedures() = default;

  /** Register dynamic SQL procedure.
  This does first try to unregister any functions, that might be left over
  from an earlier use of the component.

  @return status, true on success */
  bool register_procedures() {
    /* Try to unregister potentially left over functions from last run. */
    unregister();

    auto plugin_registry = get_mysql_registry();

    bool success = true;
    /* Open a new block so that udf_registrar is automatically destroyed
    before we release the plugin_registry. */
    {
      my_service<SERVICE_TYPE(udf_registration)> registrar =
          get_procedure_registar(plugin_registry);
      if (registrar.is_valid()) {
        for (auto &procedure : get_procedures()) {
          const char *name = procedure.m_name.c_str();
          if (registrar->udf_register(name, procedure.m_return_type,
                                      procedure.m_func, procedure.m_init_func,
                                      procedure.m_deinit_func)) {
            /* purecov: begin inspected */
            /* Only needed if register fails. */
            std::string msg = get_module_name() +
                              ": Cannot register dynamic SQL procedure '" +
                              name + "'";
            LogErr(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG, msg.c_str());
            success = false;
            /* purecov: end */
          }
        }
      }
    }
    /* end of udf_registrar block */
    mysql_plugin_registry_release(plugin_registry);
    if (!success) {
      unregister();
    }
    return success;
  }

  /** Unregister dynamic SQL procedure */
  void unregister() {
    auto plugin_registry = get_mysql_registry();

    /* Open a new block so that udf_registrar is automatically destroyed
    before we release the plugin_registry. */
    {
      my_service<SERVICE_TYPE(udf_registration)> registrar =
          get_procedure_registar(plugin_registry);
      if (registrar.is_valid()) {
        for (auto &procedure : get_procedures()) {
          const char *name = procedure.m_name.c_str();
          int was_present = 0;
          if (registrar->udf_unregister(name, &was_present) && was_present) {
            /* purecov: begin inspected */
            /* Only needed if unregister fails. */
            std::string msg = get_module_name() +
                              ": Cannot unregister dynamic SQL procedure '" +
                              name + "'";
            LogErr(WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG, msg.c_str());
            /* purecov: end */
          }
        }
      }
    } /* end of udf_registrar block */
    mysql_plugin_registry_release(plugin_registry);
  }

 protected:
  virtual std::vector<dynamic_procedure_data_t> get_procedures() const = 0;
  virtual std::string get_module_name() const = 0;

 private:
  SERVICE_TYPE(registry) * get_mysql_registry() {
    SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
    if (plugin_registry == nullptr) {
      /* purecov: begin inspected */
      LogErr(
          WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
          (get_module_name() + ": mysql_plugin_registry_acquire() returns NULL")
              .c_str());
      /* purecov: end */
    }
    return plugin_registry;
  }

  my_service<SERVICE_TYPE(udf_registration)> get_procedure_registar(
      SERVICE_TYPE(registry) * plugin_registry) {
    my_service<SERVICE_TYPE(udf_registration)> registrar("udf_registration",
                                                         plugin_registry);
    if (!registrar.is_valid()) {
      LogErr(WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
             (get_module_name() + ": Cannot get valid udf_registration service")
                 .c_str());
    }
    return registrar;
  }
};

#endif

}  // namespace srv

#endif
