/*
   Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include <stddef.h>
#include <vector>

#define RAPIDJSON_HAS_STDSTRING 1

#include "manifest.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/my_service.h"
#include "mysql/components/service.h"
#include "mysql/components/services/persistent_dynamic_loader.h"
#include "mysql/mysql_lex_string.h"
#include "mysqld_error.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/mysqld.h"                      // srv_registry
#include "sql/resourcegroups/resource_group_mgr.h"  // Resource_group_mgr
#include "sql/server_component/persistent_dynamic_loader_imp.h"
#include "sql/server_component/set_variables_helper.h"
#include "sql/sql_backup_lock.h"  // acquire_shared_backup_lock
#include "sql/sql_class.h"        // THD
#include "sql/sql_plugin.h"       // end_transaction
#include "sql/sql_plugin_var.h"   // convert_underscore_to_dash
#include "sql/thd_raii.h"

#include "sql/sql_component.h"

using manifest::Manifest_reader;

bool Sql_cmd_install_component::execute(THD *thd) {
  my_service<SERVICE_TYPE(persistent_dynamic_loader)> persisted_loader(
      "persistent_dynamic_loader", srv_registry);
  if (persisted_loader) {
    my_error(ER_COMPONENTS_CANT_ACQUIRE_SERVICE_IMPLEMENTATION, MYF(0),
             "persistent_dynamic_loader");
    return true;
  }

  if (acquire_shared_backup_lock(thd, thd->variables.lock_wait_timeout))
    return true;

  Disable_autocommit_guard autocommit_guard(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  DBUG_EXECUTE_IF("disable_rg_pfs_notifications", {
    auto name = "file://component_test_pfs_notification";
    if (m_urns.size() == 1 && strcmp(name, m_urns[0].str) == 0)
      resourcegroups::Resource_group_mgr::instance()
          ->disable_pfs_notification();
  });

  m_arg_list_size = m_set_exprs->size() * 2 + 1;
  m_arg_list = thd->mem_root->ArrayAlloc<char *>(m_arg_list_size + 1);
  m_arg_list[m_arg_list_size] = nullptr;
  char **arg = m_arg_list;

  *arg++ = nullptr;  // no program name

  for (auto &set : *m_set_exprs) {
    if (!set.expr->fixed && set.expr->fix_fields(thd, &set.expr)) return true;

    char buff[STRING_BUFFER_USUAL_SIZE];
    String value(buff, sizeof(buff), system_charset_info), *val;
    val = set.expr->val_str(&value);

    if (!val || set.expr->is_null()) {
      String x;
      if (set.name.prefix.length > 0) {
        x.append(set.name.prefix);
        x.append('.');
      }
      x.append(set.name.name);
      my_error(ER_INSTALL_COMPONENT_SET_NULL_VALUE, MYF(0), x.c_ptr());
      return true;
    }

    String argument(STRING_WITH_LEN("--"), system_charset_info);
    if (set.name.prefix.length > 0) {
      argument.append(set.name.prefix);
      argument.append('.');
    }
    argument.append(set.name.name);
    char *arg_name = thd->strmake(argument.c_ptr(), argument.length());
    convert_underscore_to_dash(arg_name, argument.length());

    *arg++ = arg_name;
    *arg++ = thd->strmake(val->c_ptr_safe(), val->length());
  }

  std::vector<const char *> urns(m_urns.size());
  for (size_t i = 0; i < m_urns.size(); ++i) {
    urns[i] = m_urns[i].str;
  }
  if (persisted_loader->load(thd, urns.data(), m_urns.size())) {
    return (end_transaction(thd, true));
  }

  bool set_var_failed = false;

  if (m_arg_list_size > 1) {
    std::stringstream str;
    str << m_arg_list[1] + 2;
    for (int idx = 3; idx < m_arg_list_size; idx += 2)
      str << ((idx + 2 < m_arg_list_size) ? ", " : " and ")
          << (m_arg_list[idx] + 2);
    my_error(ER_INSTALL_COMPONENT_SET_UNUSED_VALUE, MYF(0), str.str().c_str());
    set_var_failed = true;
  }
  /* persist the values that need persisting */
  if (!set_var_failed && m_set_exprs->elements > 0) {
    Set_variables_helper hlp(thd);
    for (auto &set_var : *m_set_exprs) {
      enum_var_type type = set_var.type;
      /* we already did the SET GLOBAL part, convert to PERSIST */
      if (type == enum_var_type::OPT_PERSIST)
        type = enum_var_type::OPT_PERSIST_ONLY;

      if (type == enum_var_type::OPT_PERSIST ||
          type == enum_var_type::OPT_PERSIST_ONLY) {
        if (hlp.add_variable(set_var.name.prefix.str,
                             set_var.name.prefix.length, set_var.name.name.str,
                             set_var.name.name.length, set_var.expr, type)) {
          set_var_failed = true;
          break;
        }
      } else if (hlp.check_variable_update_type(
                     set_var.name.prefix.str, set_var.name.prefix.length,
                     set_var.name.name.str, set_var.name.name.length,
                     set_var.expr)) {
        set_var_failed = true;
        break;
      }
    }
    if (set_var_failed || hlp.execute()) {
      set_var_failed = true;
    }
  }
  if (set_var_failed) {
    if (acquire_shared_backup_lock(thd, thd->variables.lock_wait_timeout) ||
        acquire_shared_global_read_lock(thd, thd->variables.lock_wait_timeout))
      return true;
    if (dynamic_loader_srv->unload(urns.data(), m_urns.size()) ||
        mysql_persistent_dynamic_loader_imp::remove_from_cache(
            urns.data(), urns.size()) != (int)urns.size()) {
      assert(0);
    }
    return (end_transaction(thd, true));
  }

  my_ok(thd);
  return (end_transaction(thd, false));
}

bool Sql_cmd_uninstall_component::execute(THD *thd) {
  my_service<SERVICE_TYPE(persistent_dynamic_loader)> service_dynamic_loader(
      "persistent_dynamic_loader", srv_registry);
  if (service_dynamic_loader) {
    my_error(ER_COMPONENTS_CANT_ACQUIRE_SERVICE_IMPLEMENTATION, MYF(0),
             "persistent_dynamic_loader");
    return true;
  }

  if (acquire_shared_backup_lock(thd, thd->variables.lock_wait_timeout))
    return true;

  Disable_autocommit_guard autocommit_guard(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  std::vector<const char *> urns(m_urns.size());
  for (size_t i = 0; i < m_urns.size(); ++i) {
    urns[i] = m_urns[i].str;
  }
  if (service_dynamic_loader->unload(thd, urns.data(), m_urns.size())) {
    return (end_transaction(thd, true));
  }
  my_ok(thd);
  return (end_transaction(thd, false));
}

Deployed_components::Deployed_components(const std::string program_name,
                                         const std::string instance_path)
    : program_name_(program_name),
      instance_path_(instance_path),
      components_(),
      last_error_(),
      valid_(false),
      loaded_(false) {
  valid_ = load();
  if (valid_ == false) {
    LogErr(ERROR_LEVEL, ER_COMPONENTS_INFRASTRUCTURE_MANIFEST_INIT,
           last_error_.c_str());
    last_error_.clear();
    components_.clear();
  }
}

Deployed_components::~Deployed_components() {
  if (unload() == false)
    LogErr(ERROR_LEVEL, ER_COMPONENTS_INFRASTRUCTURE_MANIFEST_DEINIT,
           last_error_.c_str());
}

void Deployed_components::get_next_component(std::string &components_list,
                                             std::string &one_component) {
  std::string component_separator(",");
  one_component.clear();
  if (components_list.find(component_separator) != std::string::npos) {
    one_component = component_separator;
    /* Skip ",," */
    while (one_component[0] == component_separator[0]) {
      one_component =
          components_list.substr(0, components_list.find(component_separator));
      components_list.erase(0, components_list.find(component_separator) + 1);
    }
  } else {
    one_component = components_list;
    components_list.clear();
  }
}

bool Deployed_components::make_urns(std::vector<const char *> &urns) {
  auto free_memory = [&urns]() {
    for (auto element : urns)
      if (element != nullptr) my_free(const_cast<char *>(element));
  };

  std::string one_component;
  std::string components = components_;
  while (components.length() > 0) {
    get_next_component(components, one_component);
    if (one_component.length()) {
      char *component =
          my_strdup(PSI_NOT_INSTRUMENTED, one_component.c_str(), MYF(MY_WME));
      if (component == nullptr) {
        free_memory();
        last_error_.assign(
            "Failed to allocated required memory for component name");
        return false;
      }
      urns.push_back(component);
    }
    one_component.clear();
  }
  return true;
}

bool Deployed_components::load() {
  if (program_name_.length() == 0) {
    last_error_.assign("Program name can not be empty.");
    return false;
  }

  /* Parse program name and load manifest file */

  std::unique_ptr<Manifest_reader> current_reader(
      new Manifest_reader(program_name_, {}));

  /* It's ok if manifest file is not present or is empty */
  if (current_reader->empty()) return true;

  if (current_reader->ro() == false)
    LogErr(WARNING_LEVEL, ER_WARN_COMPONENTS_INFRASTRUCTURE_MANIFEST_NOT_RO,
           current_reader->manifest_file().c_str());

  if (current_reader->read_local_manifest() == true) {
    current_reader.reset();
    /* Read manifest file locally */
    current_reader =
        std::make_unique<Manifest_reader>(program_name_, instance_path_);
    /* It is possible that current instance is not using keyring component */
    if (current_reader->empty()) return true;

    if (current_reader->ro() == false) {
      LogErr(WARNING_LEVEL, ER_WARN_COMPONENTS_INFRASTRUCTURE_MANIFEST_NOT_RO,
             current_reader->manifest_file().c_str());
    }
  }

  /* Get component details from manifest file */
  if (current_reader->components(components_) == false) {
    last_error_.assign(
        "Could not parse 'components' attribute from manifest file.");
    return false;
  }

  std::vector<const char *> urns;
  auto free_memory = [&urns]() {
    for (auto element : urns)
      if (element != nullptr) my_free(const_cast<char *>(element));
  };

  if (make_urns(urns) == false) {
    free_memory();
    return false;
  }
  if (urns.size() > 0) {
    /* Load components */
    bool load_status = dynamic_loader_srv->load(urns.data(), urns.size());
    if (load_status) {
      free_memory();
      last_error_.assign("Failed to load components from manifest file");
      return false;
    }
    loaded_ = true;
    free_memory();
  }
  if (components_.find("keyring") != components_.rfind("keyring")) {
    /* Multiple keyring component URNs present in manifest file */
    LogErr(WARNING_LEVEL,
           ER_WARN_COMPONENTS_INFRASTRUCTURE_MANIFEST_MULTIPLE_KEYRING,
           components_.c_str(), current_reader->manifest_file().c_str());
  }
  return true;
}

bool Deployed_components::unload() {
  if (components_.length() == 0) return true;
  std::vector<const char *> urns;
  auto free_memory = [&urns]() {
    for (auto element : urns)
      if (element != nullptr) my_free(const_cast<char *>(element));
  };

  if (make_urns(urns) == false) {
    free_memory();
    return false;
  }
  if (urns.size() > 0) {
    /* Unload components */
    bool unload_status = dynamic_loader_srv->unload(urns.data(), urns.size());
    if (unload_status) {
      free_memory();
      last_error_.assign("Failed to unload components read from manifest file");
      return false;
    }
    free_memory();
  }
  return true;
}
