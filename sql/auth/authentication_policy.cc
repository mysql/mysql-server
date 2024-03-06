/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "authentication_policy.h"

#include <ctype.h>
#include <algorithm>
#include <sstream>

#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/plugin_auth.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/auth/sql_authentication.h"
#include "sql/mysqld.h"
#include "sql/sql_plugin.h"
#include "sql/strfunc.h"
#include "sql_string.h"

namespace authentication_policy {

Policy *Policy::policy(nullptr);

static inline void trim(std::string &str) {
  str.erase(0, str.find_first_not_of(" \n\r\t"));
  str.erase(str.find_last_not_of(" \n\r\t") + 1);
}

Factor::Factor(const std::string &mandatory_plugin,
               const std::string &default_plugin)
    : mandatory_plugin(mandatory_plugin), default_plugin(default_plugin) {}

st_mysql_auth *Policy::get_mysql_auth(const std::string &plugin_name) {
  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, to_lex_cstring(plugin_name.c_str()),
                             MYSQL_AUTHENTICATION_PLUGIN);
  if (!plugin) return nullptr;

  plugin_refs.push_back(plugin);
  return (st_mysql_auth *)plugin_decl(plugin)->info;
}

bool Policy::parse(const std::string &new_policy_value,
                   Factors &parsed_factors) {
  parsed_factors.clear();

  std::stringstream policy_strm(new_policy_value);
  bool plugin_must_be_optional(false);
  bool is_first_factor(true);
  std::string factor_spec;
  std::string mandatory_plugin;
  std::string default_plugin;

  while (std::getline(policy_strm, factor_spec, ',')) {
    auto colon_pos = factor_spec.find(':');
    mandatory_plugin = factor_spec.substr(0, colon_pos);
    default_plugin =
        colon_pos == std::string::npos ? "" : factor_spec.substr(colon_pos + 1);

    trim(mandatory_plugin);
    trim(default_plugin);

    if (mandatory_plugin.empty()) /* plugin is optional */ {
      /* do not allow optional plugin for first factor */
      if (is_first_factor) goto error;
      /* There must be no ':' */
      if (colon_pos != std::string::npos) goto error;
      /* all following plugins must be optional */
      plugin_must_be_optional = true;
    } else /* plugin is not optional */ {
      /* do not allow optional plugin followed by non-optional one */
      if (plugin_must_be_optional) goto error;
      if (mandatory_plugin == "*") /* any plugin is accepted */ {
        if (colon_pos != std::string::npos) /* default specified */ {
          /* the default plugin must not be empty and
             cannot contain '*' or ':' */
          if (default_plugin.empty() ||
              default_plugin.find_first_of("*:") != std::string::npos)
            goto error;
        }
      } else /* mandatory plugin specified */ {
        /* There must be no ':' in the specification and
           the mandatory plugin cannot contain '*' */
        if (colon_pos != std::string::npos ||
            mandatory_plugin.find('*') != std::string::npos)
          goto error;
      }
    }

    parsed_factors.push_back(Factor(mandatory_plugin, default_plugin));

    if (policy_strm.eof()) break;

    is_first_factor = false;
  }

  /* if the new_policy_opt ends with "," */
  if (!is_first_factor && policy_strm.eof() && policy_strm.fail())
    parsed_factors.push_back(Factor("", ""));

  if (parsed_factors.size() > MAX_AUTH_FACTORS || parsed_factors.size() == 0)
    goto error;

  return false;
error:
  parsed_factors.clear();
  return true;
}

bool Policy::validate(const char *new_policy_value) {
  std::string new_policy_str(new_policy_value ? new_policy_value : "");
  st_mysql_auth *auth(nullptr);
  bool ret(true);
  Factors parsed_factors;

  /* do parse before critical section */
  if (parse(new_policy_str, parsed_factors)) return true;

  mysql_mutex_lock(&LOCK_authentication_policy);

  new_factors = std::move(parsed_factors);

  auto factor = new_factors.begin();

  /* -- check conditions for 1. factor --------------------------- */

  /* it must not be optional */
  if (factor->is_optional()) return true;

  /* it must specify either mandatory or default plugin */
  if (!factor->is_mandatory_specified() && !factor->is_default_specified())
    /* for backward compatibility, instead of raising error,
       the default is set to system defined */
    factor->set_default();

  /* either mandatory or default plugin name must denote a valid plugin that
     doesn't require registration step */
  auth = get_mysql_auth(factor->get_mandatory_or_default_plugin());
  if (auth == nullptr ||
      auth->authentication_flags & AUTH_FLAG_REQUIRES_REGISTRATION)
    goto end;

  /* -- check conditions for following factors ------------------- */

  for (++factor; factor != new_factors.end(); ++factor) {
    if (factor->is_mandatory_specified()) {
      /* mandatory or default plugin name must denote a valid plugin
       that doesn't store password in mysql database */
      auth = get_mysql_auth(factor->get_mandatory_plugin());
      if (auth == nullptr ||
          auth->authentication_flags & AUTH_FLAG_USES_INTERNAL_STORAGE)
        goto end;
    } else if (factor->is_default_specified()) {
      /* default plugin name must denote a valid plugin
       that doesn't store password in mysql database */
      auth = get_mysql_auth(factor->get_default_plugin());
      if (auth == nullptr ||
          auth->authentication_flags & AUTH_FLAG_USES_INTERNAL_STORAGE)
        goto end;
    }
  }
  verified_policy_value = new_policy_str;
  ret = false;
end:
  if (ret) {
    release_plugin_refs();
  }
  mysql_mutex_unlock(&LOCK_authentication_policy);
  return ret;
}

bool Policy::update(const char *new_policy_value) {
  std::string new_policy_str(new_policy_value ? new_policy_value : "");
  bool ret(true);

  /* Ensure the new policy was already verified */
  if (new_policy_str != verified_policy_value) goto end;

  /* update the actual factors and clear the verified_policy_value*/
  assert(new_factors.size() >= 1);
  factors = std::move(new_factors);
  verified_policy_value.clear();

  ret = false;

end:
  release_plugin_refs();
  return ret;
}

void Policy::get_factors(Factors &factors) const {
  mysql_mutex_lock(&LOCK_authentication_policy);
  factors = this->factors;
  mysql_mutex_unlock(&LOCK_authentication_policy);
}

void Policy::get_default_plugin(size_t factor, std::string &name) const {
  mysql_mutex_lock(&LOCK_authentication_policy);
  name = this->factors[factor].get_mandatory_or_default_plugin();
  mysql_mutex_unlock(&LOCK_authentication_policy);
}

void Policy::get_default_plugin(size_t factor, MEM_ROOT *mem_root,
                                LEX_CSTRING *name) const {
  mysql_mutex_lock(&LOCK_authentication_policy);
  lex_string_strmake(
      mem_root, name,
      this->factors[factor].get_mandatory_or_default_plugin().c_str(),
      this->factors[factor].get_mandatory_or_default_plugin().length());
  mysql_mutex_unlock(&LOCK_authentication_policy);
}

int init(const char *opt_authentication_policy) {
  assert(!Policy::policy);

  Policy::policy = new Policy;
  if (opt_authentication_policy &&
      Policy::policy->validate(opt_authentication_policy)) {
    /* --authentication_policy is set to invalid value */
    delete Policy::policy;
    Policy::policy = nullptr;
    return 1;
  }

  /* update the value */
  Policy::policy->update(opt_authentication_policy);

  return 0;
}

void deinit() {
  if (Policy::policy) {
    delete Policy::policy;
    Policy::policy = nullptr;
  }
}

}  // namespace authentication_policy