/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _KRB5_INTERFACE_H_
#define _KRB5_INTERFACE_H_

#include <string>

#include <assert.h>
#include <krb5/krb5.h>
#include <my_sharedlib.h>
#include <profile.h>

#include "log_client.h"

namespace auth_ldap_sasl_client {

/**
  \defgroup Krb5FunctionTypes Types of Krb5 interface functions.
  @{
*/
using krb5_build_principal_type = krb5_error_code (*)(krb5_context context,
                                                      krb5_principal *princ,
                                                      unsigned int rlen,
                                                      const char *realm, ...);
using krb5_cc_close_type = krb5_error_code (*)(krb5_context context,
                                               krb5_ccache cache);
using krb5_cc_default_type = krb5_error_code (*)(krb5_context context,
                                                 krb5_ccache *ccache);
using krb5_cc_get_principal_type = krb5_error_code (*)(
    krb5_context context, krb5_ccache cache, krb5_principal *principal);
using krb5_cc_initialize_type = krb5_error_code (*)(krb5_context context,
                                                    krb5_ccache cache,
                                                    krb5_principal principal);
using krb5_cc_remove_cred_type = krb5_error_code (*)(krb5_context context,
                                                     krb5_ccache cache,
                                                     krb5_flags flags,
                                                     krb5_creds *creds);
using krb5_cc_retrieve_cred_type = krb5_error_code (*)(krb5_context context,
                                                       krb5_ccache cache,
                                                       krb5_flags flags,
                                                       krb5_creds *mcreds,
                                                       krb5_creds *creds);
using krb5_cc_store_cred_type = krb5_error_code (*)(krb5_context context,
                                                    krb5_ccache cache,
                                                    krb5_creds *creds);
using krb5_free_context_type = void (*)(krb5_context context);
using krb5_free_cred_contents_type = void (*)(krb5_context context,
                                              krb5_creds *val);
using krb5_free_default_realm_type = void (*)(krb5_context context,
                                              char *lrealm);
using krb5_free_error_message_type = void (*)(krb5_context ctx,
                                              const char *msg);
using krb5_free_principal_type = void (*)(krb5_context context,
                                          krb5_principal val);
using krb5_free_unparsed_name_type = void (*)(krb5_context context, char *val);
using krb5_get_default_realm_type = krb5_error_code (*)(krb5_context context,
                                                        char **lrealm);
using krb5_get_error_message_type = const char *(*)(krb5_context ctx,
                                                    krb5_error_code code);
using krb5_get_init_creds_opt_alloc_type =
    krb5_error_code (*)(krb5_context context, krb5_get_init_creds_opt **opt);
using krb5_get_init_creds_opt_free_type =
    void (*)(krb5_context context, krb5_get_init_creds_opt *opt);
using krb5_get_init_creds_password_type = krb5_error_code (*)(
    krb5_context context, krb5_creds *creds, krb5_principal client,
    const char *password, krb5_prompter_fct prompter, void *data,
    krb5_deltat start_time, const char *in_tkt_service,
    krb5_get_init_creds_opt *k5_gic_options);
using krb5_get_profile_type = krb5_error_code (*)(krb5_context context,
                                                  struct _profile_t **profile);

using krb5_init_context_type = krb5_error_code (*)(krb5_context *context);
using krb5_parse_name_type = krb5_error_code (*)(krb5_context context,
                                                 const char *name,
                                                 krb5_principal *principal_out);
using krb5_timeofday_type = krb5_error_code (*)(krb5_context context,
                                                krb5_timestamp *timeret);
using krb5_unparse_name_type = krb5_error_code (*)(
    krb5_context context, krb5_const_principal principal, char **name);
using krb5_verify_init_creds_type = krb5_error_code (*)(
    krb5_context context, krb5_creds *creds, krb5_principal server,
    krb5_keytab keytab, krb5_ccache *ccache,
    krb5_verify_init_creds_opt *options);
using profile_get_boolean_type = long (*)(profile_t profile, const char *name,
                                          const char *subname,
                                          const char *subsubname, int def_val,
                                          int *ret_default);
using profile_get_string_type = long (*)(profile_t profile, const char *name,
                                         const char *subname,
                                         const char *subsubname,
                                         const char *def_val,
                                         char **ret_string);
using profile_release_type = void (*)(profile_t profile);
using profile_release_string_type = void (*)(char *str);
/**@}*/

/**
 Shortcut macro defining getter of the interfacee function
*/
#define KRB5_INTERFACE_DECLARE_FUNCTION(FUNCTION) \
  auto FUNCTION() {                               \
    assert(FUNCTION##_ptr);                       \
    return FUNCTION##_ptr;                        \
  }

/**
 Shortcut macro defining pointer to the interfacee function
*/
#define KRB5_INTERFACE_DECLARE_FUNCTION_PTR(FUNCTION) \
  FUNCTION##_type FUNCTION##_ptr;

/**
  Class representing interface to KRB5 functions.
  The functions are located in a library or libraries that are loaded in
  runtime. The class provides easy and safe access to them.
*/
class Krb5_interface {
 public:
  /**
   Constructor.
   The constructor is trivial and the libraries are not loaded by it. This is
   done in initialize() to give chance the caller to check if succeeded.
  */
  Krb5_interface();

  /**
   Destructor.
   Closes the libraries.
  */
  ~Krb5_interface();

  /**
   Initialize the object by loading the libraries and setting pointers to the
   interface functions. It must be called before any interface functions are
   called.

   @retval true success
   @retval false failure
  */
  bool initialize();

  /**
   \defgroup Krb5Functions Getters of pointers to the interface functions.
   @{
  */
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_build_principal)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_close)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_default)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_get_principal)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_initialize)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_remove_cred)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_retrieve_cred)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_cc_store_cred)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_free_context)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_free_cred_contents)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_free_default_realm)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_free_error_message)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_free_principal)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_free_unparsed_name)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_get_default_realm)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_get_error_message)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_get_init_creds_opt_alloc)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_get_init_creds_opt_free)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_get_init_creds_password)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_get_profile)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_init_context)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_parse_name)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_timeofday)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_unparse_name)
  KRB5_INTERFACE_DECLARE_FUNCTION(krb5_verify_init_creds)
  KRB5_INTERFACE_DECLARE_FUNCTION(profile_get_boolean)
  KRB5_INTERFACE_DECLARE_FUNCTION(profile_get_string)
  KRB5_INTERFACE_DECLARE_FUNCTION(profile_release)
  KRB5_INTERFACE_DECLARE_FUNCTION(profile_release_string)
  /**@}*/

 private:
  /**
   Handle to the library providing krb5_* functions
  */
  void *krb5_lib_handle;

  /**
   Handle to the library providing profile_* functions
  */
  void *profile_lib_handle;

  /**
   \defgroup Krb5FunctionPointers Pointers to the interface functions.
   @{
  */
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_build_principal)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_close)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_default)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_get_principal)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_initialize)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_remove_cred)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_retrieve_cred)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_cc_store_cred)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_free_context)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_free_cred_contents)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_free_default_realm)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_free_error_message)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_free_principal)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_free_unparsed_name)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_get_default_realm)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_get_error_message)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_get_init_creds_opt_alloc)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_get_init_creds_opt_free)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_get_init_creds_password)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_get_profile)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_init_context)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_parse_name)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_timeofday)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_unparse_name)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(krb5_verify_init_creds)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(profile_get_boolean)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(profile_get_string)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(profile_release)
  KRB5_INTERFACE_DECLARE_FUNCTION_PTR(profile_release_string)
  /**@}*/

  /**
   Loads single library.

   @param name [in] path or name of the library
   @param handle [out] handle to the library

   @retval true success
   @retval false failure
  */
  bool load_lib(const char *name, void *&handle);

  /**
   Closes the libraries.
  */
  void close_libs();

  /**
   Get pointer to the interface function by its name.

   @tparam T type of the function
   @param lib_handle [in] handle to the library providing the function
   @param name [in] name of the function
   @param function [out] pointer to the function

   @retval true success
   @retval false failure
  */
  template <class T>
  bool get_function(void *lib_handle, const char *name, T &function) {
    function = reinterpret_cast<T>(dlsym(lib_handle, name));
    if (function == nullptr) {
      log_error("Failed to load function ", name, ".");
      return false;
    }
    log_dbg("Successfuly loaded function ", name, ".");
    return true;
  }
};
}  // namespace auth_ldap_sasl_client
#endif  //_KRB5_INTERFACE_H_
