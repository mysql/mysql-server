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
#ifndef _AUTHENTICATION_POLICY_H_
#define _AUTHENTICATION_POLICY_H_

#include <string>
#include <vector>

#include "mysql/plugin_auth.h"
#include "sql/sql_plugin_ref.h"

/** namespace for authentication policy */
namespace authentication_policy {

/**
   Class representing authenticalion policy factor.
*/
class Factor {
 public:
  /**
   Constructor.

   @param [in] mandatory_plugin mandatory plugin name
   @param [in] default_plugin   default plugin name
  */
  Factor(const std::string &mandatory_plugin,
         const std::string &default_plugin);

  /**
   Is the factor optional (may be omitted)?

   @retval true   the factor is optional
   @retval false  the factor is not optional
  */
  inline bool is_optional() const { return mandatory_plugin.empty(); }

  /**
   Is the factor whichever (any auth plugin may be used for it)?

   @retval true   the factor is whichever
   @retval false  the factor is not whichever
  */
  inline bool is_whichever() const { return mandatory_plugin == "*"; }

  /**
   Has the factor a concrete mandatory auth plugin specified?

   @retval true   the factor has a mandatory plugin
   @retval false  the factor doesn't have a mandatory plugin
  */
  inline bool is_mandatory_specified() const {
    return !is_optional() && !is_whichever();
  }

  /**
   Has the factor a default plugin specified?

   @retval true   the factor has a default plugin
   @retval false  the factor doesn't have a default plugin
  */
  inline bool is_default_specified() const { return !default_plugin.empty(); }

  /**
   Get mandatory plugin name.

   @return reference to the plugin name.
  */
  const std::string &get_mandatory_plugin() const { return mandatory_plugin; }

  /**
   Get default plugin name.

   @return reference to the plugin name.
  */
  const std::string &get_default_plugin() const { return default_plugin; }

  /**
   Get mandatory plugin name (if defined) else the default plugin name.
   This is used e.g. while creating user when the statement doesn't provide
   plugin name for nth factor.

   @return reference to the plugin name.
  */
  const std::string &get_mandatory_or_default_plugin() const {
    return is_mandatory_specified() ? mandatory_plugin : default_plugin;
  }

 protected:
  /**
    Set default to system defined. It is used for 1. factor to avoid undefined
    default authentication.
  */
  void set_default() { default_plugin = "caching_sha2_password"; }

 private:
  /**
   If empty: the factor is optional
   If "*"  : the factor may be whichever plugin
   Else    : mandatory plugin name
  */
  std::string mandatory_plugin;
  /**
   Default plugin name
  */
  std::string default_plugin;

  friend class Policy;
};

/**
 Type of container with authentication policy factors.
*/
using Factors = std::vector<Factor>;

/**
  Class representing authentication policy.
*/
class Policy {
 protected:
  /** Pointer to the authentication policy object */
  static Policy *policy;

  /** Destructor */
  ~Policy() { release_plugin_refs(); }

  /**
    Validate @@authentication_policy variable value.

    @param [in]  new_policy  the new value of the variable.

    @retval  false    success
    @retval  true     failure
  */
  bool validate(const char *new_policy);

  /**
    Update @@authentication_policy variable value.

    @param [in]  new_policy  the new value of the variable.

    @retval  false    success
    @retval  true     failure
  */
  bool update(const char *new_policy);

  /**
    Get copy of the authentication policy factors.
    The aim is to work is with consistent snapshot of the factor avoiding long
    time locking.

    @param factors [out] authentication policy factors
  */
  void get_factors(Factors &factors) const;

  /**
    Get copy of default plugin name.

    @param factor [in] no of the factor
    @param name [out] copy of the name
  */
  void get_default_plugin(size_t factor, std::string &name) const;

  /**
    Get copy of default plugin name.

    @param factor [in] no of the factor
    @param mem_root [in] place to allocate the name
    @param name [out] copy of the name
  */
  void get_default_plugin(size_t factor, MEM_ROOT *mem_root,
                          LEX_CSTRING *name) const;

  /**
    Parse @@authentication_policy variable value.

    Format of the variable:
     authentication_policy = factor_spec[, factor_spec] ...
     factor_spec = [ * | \<empty\> | mandatory_plugin |
                     *:default_plugin ]

    Additional rules:
    The first plugin cannot be empty (optional)
    An empty (optional) plugin can be followed only by empty (optional) plugins.
    The number of factors is limited to 3.

    Below are some invalid values:
     ',,'
     ',authentication_fido,'
     ',:caching_sha2_password'
     'caching_sha2_password,,authentication_fido'
     'caching_sha2_password,:authentication_ldap_simple,authentication_fido'
     ',authentication_fido,authentication_ldap_simple'
     ',*:authentication_fido,'
     'caching_sha2_password:authentication_ldap_simple'

    @param new_policy_value [in] new value of the variable
    @param parsed_factors [out]  parsed factors

    @retval  false    OK
    @retval  true     Error
  */
  static bool parse(const std::string &new_policy_value,
                    Factors &parsed_factors);

 private:
  /** Actual authentication policy factors. */
  Factors factors;
  /**
    Verified, but not yet set authentication policy factors.
    Set in validate(), replace actual factors in update().
    Used to avoid second validation in update().
   */
  Factors new_factors;
  /**
    The verified policy value. Used to ensure the following validate() and
    update() work with the same value.
  */
  std::string verified_policy_value;
  /*
    Container with server authentication plugin descriptors. Each descriptor is
    locked and stored in plugin_refs while validating a new policy and unlocked
    after update. This ensures that no plugin unloaded in between check()
    and update() of authentication_policy variable.
  */
  std::vector<plugin_ref> plugin_refs;

  /**
    Release all plugin references and clear plugin_refs container.
  */
  inline void release_plugin_refs() {
    for (auto plugin : plugin_refs) plugin_unlock(nullptr, plugin);
    plugin_refs.clear();
  }

  /**
   Get server authentication plugin descriptor by plugin name.
   Store the descriptor in plugin_refs.

   @param plugin_name [in] name of the plugin
   @return server authentication plugin descriptor
  */
  st_mysql_auth *get_mysql_auth(const std::string &plugin_name);

  friend bool policy_validate(const char *new_policy);
  friend inline bool policy_update(const char *new_policy);
  friend void get_policy_factors(Factors &factors);
  friend void get_first_factor_default_plugin(std::string &name);
  friend void get_first_factor_default_plugin(MEM_ROOT *mem_root,
                                              LEX_CSTRING *name);
  friend int init(const char *opt_authentication_policy);
  friend void deinit();
};

/**
   Initialize authentication policy

  @param opt_authentication_policy [in] value of authentication_policy sysvar

  @retval 0 success
  @retval non 0 failure;
*/
int init(const char *opt_authentication_policy);

/**
 Deinitialize authentication policy
*/
void deinit();

/**
  Validate @@authentication_policy variable value.

  @param [in]  new_policy  the new value of the variable.

  @retval  false    success
  @retval  true     failure
*/
inline bool policy_validate(const char *new_policy) {
  assert(Policy::policy);
  return Policy::policy->validate(new_policy);
}

/**
  Validate @@authentication_policy variable value.

  @param [in]  new_policy  the new value of the variable.

  @retval  false    success
  @retval  true     failure
*/
inline bool policy_update(const char *new_policy) {
  assert(Policy::policy);
  return Policy::policy->update(new_policy);
}

/**
  Get copy of the authentication policy factors.
  The aim is to work is with consistent snapshot of the factor avoiding long
  time locking.

  @param factors [out] authentication policy factors
*/
inline void get_policy_factors(Factors &factors) {
  assert(Policy::policy);
  Policy::policy->get_factors(factors);
}

/**
  Get copy of first factor default plugin name.

  @param name [out] copy of the name
*/
inline void get_first_factor_default_plugin(std::string &name) {
  assert(Policy::policy);
  Policy::policy->get_default_plugin(0, name);
}

/**
  Get copy of default plugin name.

  @param mem_root [in] place to allocate the name
  @param name [out] copy of the name
*/
inline void get_first_factor_default_plugin(MEM_ROOT *mem_root,
                                            LEX_CSTRING *name) {
  assert(Policy::policy);
  Policy::policy->get_default_plugin(0, mem_root, name);
}

}  // namespace authentication_policy

#endif  //_AUTHENTICATION_POLICY_H_
