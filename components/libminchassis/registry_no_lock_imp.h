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

#ifndef MYSQL_SERVER_REGISTRY_NO_LOCK_H
#define MYSQL_SERVER_REGISTRY_NO_LOCK_H

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/registry.h>
#include <map>
#include <memory>

#include "c_string_less.h"
#include "mysql_service_implementation.h"

typedef std::map<const char *, mysql_service_implementation *, c_string_less>
    my_service_registry;

class mysql_registry_no_lock_imp {
 protected:
  typedef std::map<my_h_service, mysql_service_implementation *>
      my_interface_mapping;

  /* contain the actual fields definitions */
  static my_service_registry service_registry;
  static my_interface_mapping interface_mapping;

 public:
  /**
    De-initializes registry, other structures.
  */
  static void deinit();

  /**
    Gets current reference count for a Service Implementation related to the
    specified pointer to the interface structure. Assumes caller has at least
    a read lock on the Registry.

    @param interface A pointer to the interface structure of the Service
      Implementation to get reference count of.
    @return A current reference count for specified Service Implementation.
      Returns 0 in case there is no such interface or it is not referenced.
  */
  static uint64_t get_service_implementation_reference_count(
      my_h_service interface);

  /**
    Finds and acquires a Service by name. A name of the Service or the Service
    Implementation can be specified. In case of the Service name, the default
    Service Implementation for Service specified will be returned. Assumes
    caller has at least a read lock on the Registry.

    @param service_name Name of Service or Service Implementation to acquire.
    @param [out] out_service Pointer to Service handle to set acquired Service.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool acquire_nolock(const char *service_name,
                             my_h_service *out_service);

  /**
    Releases the Service Implementation previously acquired. After the call to
    this method the usage of the Service Implementation handle will lead to
    unpredicted results. Assumes caller has at least a read lock on the
    Registry.

    @param service Service Implementation handle of already acquired Service.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool release_nolock(my_h_service service);

  /**
    Registers a new Service Implementation. If it is the first Service
    Implementation for the specified Service then it is made a default one.
    Assumes caller has a write lock on the Registry.

    @param service_implementation_name Name of the Service Implementation to
      register.
    @param ptr Pointer to the Service Implementation structure.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool register_service_nolock(const char *service_implementation_name,
                                      my_h_service ptr);

  /**
    Removes previously registered Service Implementation from registry. If it is
    the default one for specified Service then any one still registered is made
    default. If there is no other, the default entry is removed from the
    Registry too. Assumes caller has a write lock on the Registry.

    @param service_implementation_name Name of the Service Implementation to
      unregister.
    @return Status of performed operation
    @retval false success
    @retval true Failure. May happen when Service is still being referenced.
  */
  static bool unregister_nolock(const char *service_implementation_name);

  /* Service Implementations */
  /**
    Finds and acquires a Service by name. A name of the Service or the Service
    Implementation can be specified. In case of the Service name, the default
    Service Implementation for Service specified will be returned.

    This does not take any lock on the registry. It must not be used unless
    absolutely necessary. Use the mysql_registry_imp version instead.

    @param service_name Name of Service or Service Implementation to acquire.
    @param [out] out_service Pointer to Service handle to set acquired Service.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(acquire, (const char *service_name,
                                      my_h_service *out_service));

  /**
    Finds a Service by name. If there is a Service Implementation with the same
    Component part of name as the input Service then the found Service is
    returned.

    This does not take any lock on the registry. It must not be used unless
    absolutely necessary. Use the mysql_registry_imp version instead.

    @param service_name Name of Service or Service Implementation to acquire.
    @param service Service handle already acquired Service Implementation.
    @param [out] out_service Pointer to Service Implementation handle to set
      acquired Service Implementation.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(acquire_related,
                            (const char *service_name, my_h_service service,
                             my_h_service *out_service));

  /**
    Releases the Service Implementation previously acquired. After the call to
    this method the usage of the Service Implementation handle will lead to
    unpredicted results.

    This does not take any lock on the registry. It must not be used unless
    absolutely necessary. Use the mysql_registry_imp version instead.

    @param service Service Implementation handle of already acquired Service.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(release, (my_h_service service));

  /**
    Registers a new Service Implementation. If it is the first Service
    Implementation for the specified Service then it is made a default one.

    This does not take any lock on the registry. It must not be used unless
    absolutely necessary. Use the mysql_registry_imp version instead.

    @param service_implementation_name Name of the Service Implementation to
      register.
    @param ptr Pointer to the Service Implementation structure.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(register_service,
                            (const char *service_implementation_name,
                             my_h_service ptr));

  /**
    Removes previously registered Service Implementation from registry. If it is
    the default one for specified Service then any one still registered is made
    default. If there is no other, the default entry is removed from the
    Registry too.

    This does not take any lock on the registry. It must not be used unless
    absolutely necessary. Use the mysql_registry_imp version instead.

    @param service_implementation_name Name of the Service Implementation to
      unregister.
    @return Status of performed operation
    @retval false success
    @retval true Failure. May happen when Service is still being referenced.
  */
  static DEFINE_BOOL_METHOD(unregister,
                            (const char *service_implementation_name));

  /**
    Sets new default Service Implementation for corresponding Service name.

    This does not take any lock on the registry. It must not be used unless
    absolutely necessary. Use the mysql_registry_imp version instead.

    @param service_implementation_name Name of the Service Implementation to
      set as default one.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(set_default,
                            (const char *service_implementation_name));

 private:
  /**
    Finds a Service Implementation data structure based on the pointer to
    interface struct supplied. Assumes caller has at least a read lock on the
    Registry.

    @param interface A pointer to the interface structure of the Service
      Implementation to look for.
    @return A pointer to respective Service Implementation data structure, or
      NULL if no such interface pointer is registered within the Registry.
  */
  static mysql_service_implementation *get_service_implementation_by_interface(
      my_h_service interface);
};
#endif /* MYSQL_SERVER_REGISTRY_NO_LOCK_H */
