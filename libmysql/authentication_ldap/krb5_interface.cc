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

#include "krb5_interface.h"

#include <krb5/krb5.h>
#include <my_sharedlib.h>
#include <profile.h>

#include "auth_ldap_sasl_client.h"
#include "log_client.h"
#include "my_config.h"

#define KRB5_INVALID_HANDLE 0x7FFFFFFF

namespace auth_ldap_sasl_client {

Krb5_interface::Krb5_interface()
    : krb5_lib_handle(nullptr),
      profile_lib_handle(nullptr),
      krb5_build_principal_ptr(nullptr),
      krb5_cc_close_ptr(nullptr),
      krb5_cc_default_ptr(nullptr),
      krb5_cc_get_principal_ptr(nullptr),
      krb5_cc_initialize_ptr(nullptr),
      krb5_cc_remove_cred_ptr(nullptr),
      krb5_cc_retrieve_cred_ptr(nullptr),
      krb5_cc_store_cred_ptr(nullptr),
      krb5_free_context_ptr(nullptr),
      krb5_free_cred_contents_ptr(nullptr),
      krb5_free_default_realm_ptr(nullptr),
      krb5_free_error_message_ptr(nullptr),
      krb5_free_principal_ptr(nullptr),
      krb5_free_unparsed_name_ptr(nullptr),
      krb5_get_default_realm_ptr(nullptr),
      krb5_get_error_message_ptr(nullptr),
      krb5_get_init_creds_opt_alloc_ptr(nullptr),
      krb5_get_init_creds_opt_free_ptr(nullptr),
      krb5_get_init_creds_password_ptr(nullptr),
      krb5_get_profile_ptr(nullptr),
      krb5_init_context_ptr(nullptr),
      krb5_parse_name_ptr(nullptr),
      krb5_timeofday_ptr(nullptr),
      krb5_unparse_name_ptr(nullptr),
      krb5_verify_init_creds_ptr(nullptr),
      profile_get_boolean_ptr(nullptr),
      profile_get_string_ptr(nullptr),
      profile_release_ptr(nullptr),
      profile_release_string_ptr(nullptr) {}

Krb5_interface ::~Krb5_interface() { close_libs(); }

bool Krb5_interface::initialize() {
  if (krb5_lib_handle != nullptr && profile_lib_handle != nullptr) return true;

/* Windows case:
the functions are provided by 2 MIT Kerberos libraries krb5_64.dll and
xpprof64.dll. Normally they are shipped by MySQL and located in the same
directory as all 3. party libs like libsasl.dll.
*/
#if defined(WIN32)
  int ret_executable_path = 0;
  if (!load_lib("krb5_64.dll", krb5_lib_handle) ||
      !load_lib("xpprof64.dll", profile_lib_handle))
    return false;

/* Linux case:
The functions are provided by a single Kerberos library libkrb5.so. It is
expected to be installed in the system, so it can be found without providing the
path.
*/
#else
  const char *krb5_path = "libkrb5.so";
  if (!load_lib(krb5_path, krb5_lib_handle)) return false;
  profile_lib_handle = krb5_lib_handle;
#endif

  log_dbg("Kerberos libraries successfuly loaded.");

  if (!get_function<krb5_build_principal_type>(
          krb5_lib_handle, "krb5_build_principal", krb5_build_principal_ptr) ||
      !get_function<krb5_cc_close_type>(krb5_lib_handle, "krb5_cc_close",
                                        krb5_cc_close_ptr) ||
      !get_function<krb5_cc_default_type>(krb5_lib_handle, "krb5_cc_default",
                                          krb5_cc_default_ptr) ||
      !get_function<krb5_cc_get_principal_type>(krb5_lib_handle,
                                                "krb5_cc_get_principal",
                                                krb5_cc_get_principal_ptr) ||
      !get_function<krb5_cc_initialize_type>(
          krb5_lib_handle, "krb5_cc_initialize", krb5_cc_initialize_ptr) ||
      !get_function<krb5_cc_remove_cred_type>(
          krb5_lib_handle, "krb5_cc_remove_cred", krb5_cc_remove_cred_ptr) ||
      !get_function<krb5_cc_retrieve_cred_type>(krb5_lib_handle,
                                                "krb5_cc_retrieve_cred",
                                                krb5_cc_retrieve_cred_ptr) ||
      !get_function<krb5_cc_store_cred_type>(
          krb5_lib_handle, "krb5_cc_store_cred", krb5_cc_store_cred_ptr) ||
      !get_function<krb5_free_context_type>(
          krb5_lib_handle, "krb5_free_context", krb5_free_context_ptr) ||
      !get_function<krb5_free_cred_contents_type>(
          krb5_lib_handle, "krb5_free_cred_contents",
          krb5_free_cred_contents_ptr) ||
      !get_function<krb5_free_default_realm_type>(
          krb5_lib_handle, "krb5_free_default_realm",
          krb5_free_default_realm_ptr) ||
      !get_function<krb5_free_error_message_type>(
          krb5_lib_handle, "krb5_free_error_message",
          krb5_free_error_message_ptr) ||
      !get_function<krb5_free_principal_type>(
          krb5_lib_handle, "krb5_free_principal", krb5_free_principal_ptr) ||
      !get_function<krb5_free_unparsed_name_type>(
          krb5_lib_handle, "krb5_free_unparsed_name",
          krb5_free_unparsed_name_ptr) ||
      !get_function<krb5_get_default_realm_type>(krb5_lib_handle,
                                                 "krb5_get_default_realm",
                                                 krb5_get_default_realm_ptr) ||
      !get_function<krb5_get_error_message_type>(krb5_lib_handle,
                                                 "krb5_get_error_message",
                                                 krb5_get_error_message_ptr) ||
      !get_function<krb5_get_init_creds_opt_alloc_type>(
          krb5_lib_handle, "krb5_get_init_creds_opt_alloc",
          krb5_get_init_creds_opt_alloc_ptr) ||
      !get_function<krb5_get_init_creds_opt_free_type>(
          krb5_lib_handle, "krb5_get_init_creds_opt_free",
          krb5_get_init_creds_opt_free_ptr) ||
      !get_function<krb5_get_init_creds_password_type>(
          krb5_lib_handle, "krb5_get_init_creds_password",
          krb5_get_init_creds_password_ptr) ||
      !get_function<krb5_get_profile_type>(krb5_lib_handle, "krb5_get_profile",
                                           krb5_get_profile_ptr) ||
      !get_function<krb5_init_context_type>(
          krb5_lib_handle, "krb5_init_context", krb5_init_context_ptr) ||
      !get_function<krb5_parse_name_type>(krb5_lib_handle, "krb5_parse_name",
                                          krb5_parse_name_ptr) ||
      !get_function<krb5_timeofday_type>(krb5_lib_handle, "krb5_timeofday",
                                         krb5_timeofday_ptr) ||
      !get_function<krb5_unparse_name_type>(
          krb5_lib_handle, "krb5_unparse_name", krb5_unparse_name_ptr) ||
      !get_function<krb5_verify_init_creds_type>(krb5_lib_handle,
                                                 "krb5_verify_init_creds",
                                                 krb5_verify_init_creds_ptr) ||
      !get_function<profile_get_boolean_type>(
          profile_lib_handle, "profile_get_boolean", profile_get_boolean_ptr) ||
      !get_function<profile_get_string_type>(
          profile_lib_handle, "profile_get_string", profile_get_string_ptr) ||
      !get_function<profile_release_type>(profile_lib_handle, "profile_release",
                                          profile_release_ptr) ||
      !get_function<profile_release_string_type>(profile_lib_handle,
                                                 "profile_release_string",
                                                 profile_release_string_ptr)) {
    log_error("Failed to load all required Kerberos functions ");
    return false;
  }

  log_info("All required Kerberos functions succesfully loaded.");
  return true;
}

bool Krb5_interface::load_lib(const char *name, void *&handle) {
#ifdef _WIN32
  handle = LoadLibraryEx(
      name, nullptr,
      LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
#elif defined(HAVE_ASAN) || defined(HAVE_LSAN)
  // Do not unload the shared object during dlclose().
  // LeakSanitizer needs this in order to provide call stacks,
  // and to match entries in lsan.supp.
  handle = dlopen(name, RTLD_LAZY | RTLD_NODELETE);
#else
  handle = dlopen(name, RTLD_LAZY);
#endif
  if (handle == nullptr) {
    const char *errmsg;
    DLERROR_GENERATE(errmsg, dlopen_errno);
    log_error("Failed to open ", name, ".");
    log_error(errmsg);
    return false;
  }
  log_dbg("Loaded ", name);
  return true;
}

void Krb5_interface::close_libs() {
  if (krb5_lib_handle != nullptr) {
    dlclose(krb5_lib_handle);
    krb5_lib_handle = nullptr;
  }
#if defined(WIN32)
  if (profile_lib_handle != nullptr) {
    dlclose(profile_lib_handle);
    profile_lib_handle = nullptr;
  }
#else
  profile_lib_handle = nullptr;
#endif
  krb5_build_principal_ptr = nullptr;
  krb5_cc_close_ptr = nullptr;
  krb5_cc_default_ptr = nullptr;
  krb5_cc_get_principal_ptr = nullptr;
  krb5_cc_initialize_ptr = nullptr;
  krb5_cc_remove_cred_ptr = nullptr;
  krb5_cc_retrieve_cred_ptr = nullptr;
  krb5_cc_store_cred_ptr = nullptr;
  krb5_free_context_ptr = nullptr;
  krb5_free_cred_contents_ptr = nullptr;
  krb5_free_default_realm_ptr = nullptr;
  krb5_free_error_message_ptr = nullptr;
  krb5_free_principal_ptr = nullptr;
  krb5_free_unparsed_name_ptr = nullptr;
  krb5_get_default_realm_ptr = nullptr;
  krb5_get_error_message_ptr = nullptr;
  krb5_get_init_creds_opt_alloc_ptr = nullptr;
  krb5_get_init_creds_opt_free_ptr = nullptr;
  krb5_get_init_creds_password_ptr = nullptr;
  krb5_get_profile_ptr = nullptr;
  krb5_init_context_ptr = nullptr;
  krb5_parse_name_ptr = nullptr;
  krb5_timeofday_ptr = nullptr;
  krb5_unparse_name_ptr = nullptr;
  krb5_verify_init_creds_ptr = nullptr;
  profile_get_boolean_ptr = nullptr;
  profile_get_string_ptr = nullptr;
  profile_release_ptr = nullptr;
  profile_release_string_ptr = nullptr;
}

}  // namespace auth_ldap_sasl_client
