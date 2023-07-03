/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_REGISTRY_H
#define MYSQL_SERVER_REGISTRY_H

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/registry.h>
#include <map>
#include <memory>

#include "c_string_less.h"
#include "mysql_service_implementation.h"
#include "rwlock_scoped_lock.h"

typedef std::map<const char *, mysql_service_implementation *, c_string_less>
    my_service_registry;

class mysql_registry_imp {
  typedef std::map<my_h_service, mysql_service_implementation *>
      my_interface_mapping;

  /* contain the actual fields definitions */
  static my_service_registry service_registry;
  static my_interface_mapping interface_mapping;
  static mysql_rwlock_t LOCK_registry;

 public:
  /**
    Initializes registry for usage. Initializes RW lock, all other structures
    should be empty. Shouldn't be called multiple times.
  */
  static void init();
  /**
    De-initializes registry. De-initializes RW lock, all other structures
    are cleaned up.
  */
  static void deinit();

  /** De-initializes RW lock */
  static void rw_lock_deinit();

  /**
    Locks whole registry for write. For internal use only.

    @return A lock acquired wrapped into RAII object.
  */
  static minimal_chassis::rwlock_scoped_lock lock_registry_for_write();

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

 public: /* Service Implementations */
  /**
    Finds and acquires a Service by name. A name of the Service or the Service
    Implementation can be specified. In case of the Service name, the default
    Service Implementation for Service specified will be returned.

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

    @param service Service Implementation handle of already acquired Service.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(release, (my_h_service service));

  /**
    Registers a new Service Implementation. If it is the first Service
    Implementation for the specified Service then it is made a default one.

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

    @param service_implementation_name Name of the Service Implementation to
      set as default one.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(set_default,
                            (const char *service_implementation_name));

  /**
    Creates iterator that iterates through all registered Service
    Implementations. If successful it leaves read lock on the Registry until
    iterator is released. The starting point of iteration may be specified
    to be on one particular Service Implementation. The iterator will move
    through all Service Implementations and additionally through all default
    Service Implementation additionally, i.e. the default Service Implementation
    will be returned twice. If no name is specified for search, iterator will be
    positioned on the first Service Implementation.

    @param service_name_pattern Name of Service or Service Implementation to
      start iteration from. May be empty string or NULL pointer, in which case
      iteration starts from the first Service Implementation.
    @param [out] out_iterator Pointer to the Service Implementation iterator
      handle.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(iterator_create,
                            (const char *service_name_pattern,
                             my_h_service_iterator *out_iterator));

  /**
    Releases Service implementations iterator. Releases read lock on registry.

    @param iterator Service Implementation iterator handle.
  */
  static DEFINE_METHOD(void, iterator_release,
                       (my_h_service_iterator iterator));

  /**
    Gets name of Service pointed to by iterator. The pointer returned will last
    at least up to the moment of call to the release() method on the iterator.

    @param iterator Service Implementation iterator handle.
    @param [out] out_name Pointer to string with name to set result pointer to.
    @return Status of performed operation
    @retval false success
    @retval true Failure, may be caused when called on iterator that went
      through all values already.
  */
  static DEFINE_BOOL_METHOD(iterator_get, (my_h_service_iterator iterator,
                                           const char **out_name));

  /**
    Advances specified iterator to next element. Will succeed but return true if
    it reaches one-past-last element.

    @param iterator Service Implementation iterator handle.
    @return Status of performed operation and validity of iterator after
      operation.
    @retval false success
    @retval true Failure or called on iterator that was on last element.
  */
  static DEFINE_BOOL_METHOD(iterator_next, (my_h_service_iterator iterator));

  /**
    Checks if specified iterator is valid, i.e. have not reached one-past-last
    element.

    @param iterator Service Implementation iterator handle.
    @return Validity of iterator
    @retval false Valid
    @retval true Invalid or reached one-past-last element.
  */
  static DEFINE_BOOL_METHOD(iterator_is_valid,
                            (my_h_service_iterator iterator));

  /* This includes metadata-related method implementations that are shared
    by registry and dynamic_loader, so we don't duplicate the code. Following
    defines set up all required symbols. Unfortunately they are not only the
    types, but also static members with different name, so usage of templates
    is not enough to reuse that part of code. */
#define OBJECT_ITERATOR my_h_service_iterator
#define METADATA_ITERATOR my_h_service_metadata_iterator

#include "registry_metadata.h.inc"

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

#endif /* MYSQL_SERVER_REGISTRY_H */
