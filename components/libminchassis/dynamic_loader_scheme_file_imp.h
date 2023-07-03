/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_DYNAMIC_LOADER_SCHEMA_FILE_H
#define MYSQL_SERVER_DYNAMIC_LOADER_SCHEMA_FILE_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/dynamic_loader.h>
#include <mysql/components/services/dynamic_loader_scheme_file.h>
#include <mysql/components/services/mysql_rwlock_service.h>
#include <map>
#include <unordered_set>

#if defined(_WIN32)
#define dlsym(lib, name) (void *)GetProcAddress((HMODULE)lib, name)
#define dlopen(libname, unused) LoadLibraryEx(libname, NULL, 0)
#define dlclose(lib) FreeLibrary((HMODULE)lib)
#define RTLD_NOW 0x00002
#define DLERROR_GENERATE(errmsg, error_number)                          \
  char win_errormsg[2048];                                              \
  if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, error_number, 0,     \
                    win_errormsg, 2048, NULL)) {                        \
    char *ptr;                                                          \
    for (ptr = &win_errormsg[0] + strlen(win_errormsg) - 1;             \
         ptr >= &win_errormsg[0] && strchr("\r\n\t\0x20", *ptr); ptr--) \
      *ptr = 0;                                                         \
    errmsg = win_errormsg;                                              \
  } else                                                                \
    errmsg = ""
#define dlerror() ""
#define dlopen_errno GetLastError()

#else /* _WIN32 */

#ifndef MYSQL_ABI_CHECK
#include <dlfcn.h>
#include <errno.h>
#endif

#define DLERROR_GENERATE(errmsg, error_number) errmsg = dlerror()
#define dlopen_errno errno
#endif

class mysql_dynamic_loader_scheme_file_imp {
  typedef std::map<std::string, void *> my_registry;
  typedef mysql_component_t *(*list_components_func)();

  static my_registry object_files_list;
  static std::unordered_set<list_components_func> library_entry_set;
  static mysql_rwlock_t LOCK_dynamic_loader_scheme_file;

 public:
  /**
    Initializes file:// scheme for dynamic loader for usage. Initializes
    RW lock, all other structures should be empty. Shouldn't be called multiple
    times.
  */
  static void init();
  /**
    De-initializes RW lock, all other structures doesn't require any action.
  */
  static void deinit();

 public:
  /**
    Loads components that are located in executable file specified by URN.
    We assume that URN starts with file://, but accept any. Will not success
    when called multiple times on the same file.

    @param urn URN to file to load components from.
    @param [out] out_data Pointer to pointer to MySQL component data structures
      to set result components data retrieved from specified file.
    @return Status of performed operation
    @retval false success
    @retval true Failure, may be caused when name does not contain ://, cannot
      be located, is not proper executable file or does not contain proper
      initialization function.
  */
  static DEFINE_BOOL_METHOD(load,
                            (const char *urn, mysql_component_t **out_data));

  /**
    Unloads file that was previously loaded. The URN string must be exactly
    the same as one used during call to load. Although you can call load() on
    specified URN multiple times, subsequent calls unload() will always fail,
    and all components from specified file will be invalid after first call to
    unload().

    @param urn URN to file to unload all components from.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(unload, (const char *urn));
};

#endif /* MYSQL_SERVER_DYNAMIC_LOADER_SCHEMA_FILE_H */
