/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include "component_common.h"
#include "mysql_service_implementation.h"
#include "registry_imp.h"

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/registry.h>
#include <map>
#include <memory>

// pfs instrumentation headers
#include <mysql/components/services/mysql_rwlock.h>

/**
  @page PAGE_COMPONENTS_REGISTRY The Service Registry Service
  The Service Registry is a central part of Components subsystem. It maintains
  a list of the Service Implementations and allow acquiring them by any actor
  that has a reference to main Registry Service Implementation. For each Service
  a reference counter is maintained to make sure no one can remove any Service
  Implementation being actively used.
  The Service Implementations can be acquired by the Service name or by the full
  Service Implementation name.
  For each Service a default Service Implementation is distinguished, the first
  one to be added, but it is possible to change it at will at any point.

  A convenient RAII-style wrapper on Service acquisition and automatic release
  is provided, the my_service<T> class.
*/

static PSI_rwlock_key key_rwlock_LOCK_registry;

typedef std::map<const char *, mysql_service_implementation *, c_string_less>
    my_service_registry;

struct my_h_service_iterator_imp {
  my_service_registry::const_iterator m_it;
  minimal_chassis::rwlock_scoped_lock m_lock;
};

struct my_h_service_metadata_iterator_imp {
  my_metadata::const_iterator m_it;
  minimal_chassis::rwlock_scoped_lock m_lock;
};

static PSI_rwlock_info all_registry_rwlocks[] = {
    {&::key_rwlock_LOCK_registry, "LOCK_registry", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static void init_registry_psi_keys(void) {
  const char *category = "components";
  int count;

  count = static_cast<int>(array_elements(all_registry_rwlocks));
  mysql_rwlock_register(category, all_registry_rwlocks, count);
}

/**
  Initializes registry for usage. Initializes RW lock, all other structures
  should be empty. Shouldn't be called multiple times.
*/
void mysql_registry_imp::init() {
  init_registry_psi_keys();
  mysql_rwlock_init(key_rwlock_LOCK_registry,
                    &mysql_registry_imp::LOCK_registry);
}
/**
  De-initializes registry. De-initializes RW lock, all other structures
  are cleaned up.
*/
void mysql_registry_imp::deinit() {
  mysql_registry_imp::service_registry.clear();
  mysql_registry_imp::interface_mapping.clear();

  mysql_rwlock_destroy(&mysql_registry_imp::LOCK_registry);
}

/**
  De-initializes RW lock
*/
void mysql_registry_imp::rw_lock_deinit() {
  mysql_rwlock_destroy(&mysql_registry_imp::LOCK_registry);
}

/**
  Locks whole registry for write. For internal use only.

  @return A lock acquired wrapped into RAII object.
*/
minimal_chassis::rwlock_scoped_lock
mysql_registry_imp::lock_registry_for_write() {
  return minimal_chassis::rwlock_scoped_lock(&LOCK_registry, true, __FILE__,
                                             __LINE__);
}

/**
  Finds a Service Implementation data structure based on the pointer to
  interface struct supplied. Assumes caller has at least a read lock on the
  Registry.

  @param interface A pointer to the interface structure of the Service
    Implementation to look for.
  @return A pointer to respective Service Implementation data structure, or
    NULL if no such interface pointer is registered within the Registry.
*/
mysql_service_implementation *
mysql_registry_imp::get_service_implementation_by_interface(
    my_h_service interface) {
  my_interface_mapping::const_iterator iter =
      mysql_registry_imp::interface_mapping.find(interface);
  if (iter == mysql_registry_imp::interface_mapping.cend()) {
    return nullptr;
  }

  return iter->second;
}

/**
  Gets current reference count for a Service Implementation related to the
  specified pointer to the interface structure. Assumes caller has at least
  a read lock on the Registry.

  @param interface A pointer to the interface structure of the Service
    Implementation to get reference count of.
  @return A current reference count for specified Service Implementation.
    Returns 0 in case there is no such interface or it is not referenced.
*/
uint64_t mysql_registry_imp::get_service_implementation_reference_count(
    my_h_service interface) {
  my_interface_mapping::const_iterator iter =
      mysql_registry_imp::interface_mapping.find(interface);
  if (iter == mysql_registry_imp::interface_mapping.cend()) {
    return -1;
  }

  return iter->second->get_reference_count();
}

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
bool mysql_registry_imp::acquire_nolock(const char *service_name,
                                        my_h_service *out_service) {
  try {
    if (out_service == nullptr) {
      return true;
    }
    my_service_registry::const_iterator iter;

    iter = mysql_registry_imp::service_registry.find(service_name);

    if (iter == mysql_registry_imp::service_registry.cend()) return true;

    mysql_service_implementation *imp = iter->second;
    imp->add_reference();
    *out_service = imp->interface();
    return false;
  } catch (...) {
  }
  return true;
}

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
bool mysql_registry_imp::release_nolock(my_h_service service) {
  try {
    if (service == nullptr) {
      return true;
    }

    mysql_service_implementation *service_implementation =
        mysql_registry_imp::get_service_implementation_by_interface(service);
    if (service_implementation == nullptr) {
      return true;
    }
    return service_implementation->release_reference();
  } catch (...) {
  }
  return true;
}

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
bool mysql_registry_imp::register_service_nolock(
    const char *service_implementation_name, my_h_service ptr) {
  try {
    std::unique_ptr<mysql_service_implementation> imp =
        std::unique_ptr<mysql_service_implementation>(
            new mysql_service_implementation(ptr, service_implementation_name));

    if (imp->interface() == nullptr) {
      return true;
    }

    /* Register the implementation name. */
    std::pair<my_service_registry::iterator, bool> addition_result =
        mysql_registry_imp::service_registry.emplace(imp->name_c_str(),
                                                     imp.get());

    /* Fail if it was present already. */
    if (!addition_result.second) {
      return true;
    } else {
      try {
        /* Register interface in mapping */
        mysql_registry_imp::interface_mapping.emplace(imp->interface(),
                                                      imp.get());

        /* Register the Service Implementation as default for Service name in
          case none were registered before. */
        mysql_registry_imp::service_registry.emplace_hint(
            addition_result.first, imp->service_name_c_str(), imp.get());
      } catch (...) {
        mysql_registry_imp::service_registry.erase(addition_result.first);
        /* unique_ptr still has ownership over implementation object, we
          don't have to delete it explicitly. */
        return true;
      }
    }

    /* Pointer is stored in registry, thous we release ownership. */
    imp.release();

    return false;
  } catch (...) {
  }
  return true;
}

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
bool mysql_registry_imp::unregister_nolock(
    const char *service_implementation_name) {
  try {
    std::unique_ptr<mysql_service_implementation> imp;

    {
      /* Find the implementation and check if it is not being referenced. */
      my_service_registry::iterator imp_iter =
          mysql_registry_imp::service_registry.find(
              service_implementation_name);
      if (imp_iter == mysql_registry_imp::service_registry.end() ||
          imp_iter->second->get_reference_count() > 0) {
        return true;
      }

      /* First remove specified implementation, to not include it in search
        for new default one. Take ownership on implementation object. */
      imp.reset(imp_iter->second);
      mysql_registry_imp::service_registry.erase(imp_iter);
      /* After deletion, implementation iterator is not valid, we go out of
        scope to prevent it from being reused. */
    }

    /* Remove interface mapping. */
    mysql_registry_imp::interface_mapping.erase(
        mysql_registry_imp::interface_mapping.find(imp->interface()));

    /* Look if it is the default implementation. */
    my_service_registry::iterator default_iter =
        mysql_registry_imp::service_registry.find(imp->service_name_c_str());
    if (default_iter == mysql_registry_imp::service_registry.end()) {
      /* A Service Implementation and no default present. The state is not
        consistent. */
      return true;
    }

    if (default_iter->second == imp.get()) {
      /* Remove the default implementation too. */
      my_service_registry::iterator new_default_iter =
          mysql_registry_imp::service_registry.erase(default_iter);

      /* Search for a new default implementation. */
      if (new_default_iter != mysql_registry_imp::service_registry.end() &&
          !strcmp(imp->service_name_c_str(),
                  new_default_iter->second->service_name_c_str())) {
        /* Set as default implementation. */
        mysql_service_implementation *new_default = new_default_iter->second;
        mysql_registry_imp::service_registry.emplace_hint(
            new_default_iter, new_default->service_name_c_str(), new_default);
      }
    }

    return false;
  } catch (...) {
  }
  return true;
}

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
DEFINE_BOOL_METHOD(mysql_registry_imp::acquire,
                   (const char *service_name, my_h_service *out_service)) {
  minimal_chassis::rwlock_scoped_lock lock(&LOCK_registry, false, __FILE__,
                                           __LINE__);

  return mysql_registry_imp::acquire_nolock(service_name, out_service);
}

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
DEFINE_BOOL_METHOD(mysql_registry_imp::acquire_related,
                   (const char *service_name, my_h_service service,
                    my_h_service *out_service)) {
  try {
    minimal_chassis::rwlock_scoped_lock lock(&mysql_registry_imp::LOCK_registry,
                                             false, __FILE__, __LINE__);

    mysql_service_implementation *service_implementation =
        mysql_registry_imp::get_service_implementation_by_interface(service);
    if (service_implementation == nullptr) {
      return true;
    }
    /* Find dot, the component name is right after the dot. */
    const char *component_part =
        strchr(service_implementation->name_c_str(), '.');
    if (component_part == nullptr) {
      return true;
    }
    /* Assure given service_name is not fully qualified. */
    if (strchr(service_name, '.') != nullptr) {
      return true;
    }
    my_string service_implementation_name =
        my_string(service_name) + component_part;
    /* Try to acquire such Service. */
    if (mysql_registry_imp::acquire_nolock(service_implementation_name.c_str(),
                                           out_service)) {
      /* service is not found */
      return true;
    }
    return false;
  } catch (...) {
  }
  return true;
}

/**
  Releases the Service Implementation previously acquired. After the call to
  this method the usage of the Service Implementation handle will lead to
  unpredicted results.

  @param service Service Implementation handle of already acquired Service.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(mysql_registry_imp::release, (my_h_service service)) {
  minimal_chassis::rwlock_scoped_lock lock(&mysql_registry_imp::LOCK_registry,
                                           false, __FILE__, __LINE__);

  return mysql_registry_imp::release_nolock(service);
}

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
DEFINE_BOOL_METHOD(mysql_registry_imp::register_service,
                   (const char *service_implementation_name,
                    my_h_service ptr)) {
  minimal_chassis::rwlock_scoped_lock lock(&mysql_registry_imp::LOCK_registry,
                                           true, __FILE__, __LINE__);

  return mysql_registry_imp::register_service_nolock(
      service_implementation_name, ptr);
}

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
DEFINE_BOOL_METHOD(mysql_registry_imp::unregister,
                   (const char *service_implementation_name)) {
  minimal_chassis::rwlock_scoped_lock lock(&mysql_registry_imp::LOCK_registry,
                                           true, __FILE__, __LINE__);

  return mysql_registry_imp::unregister_nolock(service_implementation_name);
}

/**
  Sets new default Service Implementation for corresponding Service name.

  @param service_implementation_name Name of the Service Implementation to
    set as default one.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(mysql_registry_imp::set_default,
                   (const char *service_implementation_name)) {
  try {
    my_service_registry::const_iterator iter;

    minimal_chassis::rwlock_scoped_lock lock(&mysql_registry_imp::LOCK_registry,
                                             true, __FILE__, __LINE__);

    /* register the implementation name */
    iter =
        mysql_registry_imp::service_registry.find(service_implementation_name);

    if (iter == mysql_registry_imp::service_registry.cend()) {
      return true;
    }
    mysql_service_implementation *imp = iter->second;
    /* We have to remove and reinsert value as key, the string pointer will
      not be valid if we unregister previous default implementation. */
    iter = mysql_registry_imp::service_registry.erase(
        mysql_registry_imp::service_registry.find(imp->service_name_c_str()));
    mysql_registry_imp::service_registry.emplace_hint(
        iter, imp->service_name_c_str(), imp);

    return false;
  } catch (...) {
  }
  return true;
}

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
DEFINE_BOOL_METHOD(mysql_registry_imp::iterator_create,
                   (const char *service_name_pattern,
                    my_h_service_iterator *out_iterator)) {
  try {
    *out_iterator = {};

    /* This read lock on whole registry will be held, until the iterator is
      released. */
    minimal_chassis::rwlock_scoped_lock lock(&mysql_registry_imp::LOCK_registry,
                                             false, __FILE__, __LINE__);

    my_service_registry::const_iterator r =
        (!service_name_pattern || !*service_name_pattern)
            ? mysql_registry_imp::service_registry.cbegin()
            : mysql_registry_imp::service_registry.find(service_name_pattern);
    if (r == mysql_registry_imp::service_registry.cend()) {
      return true;
    }

    *out_iterator = new my_h_service_iterator_imp{r, std::move(lock)};
    return false;
  } catch (...) {
  }
  return true;
}

/**
  Releases Service implementations iterator. Releases read lock on registry.

  @param iterator Service Implementation iterator handle.
*/
DEFINE_METHOD(void, mysql_registry_imp::iterator_release,
              (my_h_service_iterator iterator)) {
  try {
    my_h_service_iterator_imp *iter =
        reinterpret_cast<my_h_service_iterator_imp *>(iterator);

    if (!iter) return;

    delete iter;
  } catch (...) {
  }
}

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
DEFINE_BOOL_METHOD(mysql_registry_imp::iterator_get,
                   (my_h_service_iterator iterator, const char **out_name)) {
  try {
    *out_name = nullptr;

    if (!iterator) return true;

    my_service_registry::const_iterator &iter =
        reinterpret_cast<my_h_service_iterator_imp *>(iterator)->m_it;

    if (iter != mysql_registry_imp::service_registry.cend()) {
      *out_name = iter->second->name_c_str();

      return false;
    }
  } catch (...) {
  }
  return true;
}

/**
  Advances specified iterator to next element. Will succeed but return true if
  it reaches one-past-last element.

  @param iterator Service Implementation iterator handle.
  @return Status of performed operation and validity of iterator after
    operation.
  @retval false success
  @retval true Failure or called on iterator that was on last element.
*/
DEFINE_BOOL_METHOD(mysql_registry_imp::iterator_next,
                   (my_h_service_iterator iterator)) {
  try {
    if (!iterator) return true;

    my_service_registry::const_iterator &iter =
        reinterpret_cast<my_h_service_iterator_imp *>(iterator)->m_it;

    if (iter != mysql_registry_imp::service_registry.cend()) {
      ++iter;
      return iter == mysql_registry_imp::service_registry.cend();
    }
  } catch (...) {
  }
  return true;
}

/**
  Checks if specified iterator is valid, i.e. have not reached one-past-last
  element.

  @param iterator Service Implementation iterator handle.
  @return Validity of iterator
  @retval false Valid
  @retval true Invalid or reached one-past-last element.
*/
DEFINE_BOOL_METHOD(mysql_registry_imp::iterator_is_valid,
                   (my_h_service_iterator iterator)) {
  try {
    if (!iterator) return true;

    my_service_registry::const_iterator &iter =
        reinterpret_cast<my_h_service_iterator_imp *>(iterator)->m_it;

    return iter == mysql_registry_imp::service_registry.cend();
  } catch (...) {
  }
  return true;
}

/* This includes metadata-related method implementations that are shared
  by registry and dynamic_loader, so we don't duplicate the code. Following
  defines set up all required symbols. Unfortunately they are not only the
  types, but also static members with different name, so usage of templates
  is not enough to reuse that part of code. */
#define REGISTRY_IMP mysql_registry_imp
#define REGISTRY mysql_registry_imp::service_registry
#define REGISTRY_TYPE my_service_registry
#define LOCK mysql_registry_imp::LOCK_registry
#define ITERATOR_TYPE my_h_service_iterator_imp
#define METADATA_ITERATOR_TYPE my_h_service_metadata_iterator_imp
#define OBJECT_ITERATOR my_h_service_iterator
#define METADATA_ITERATOR my_h_service_metadata_iterator

#include "registry_metadata.cc.inc"

/* static members for mysql_registry_imp */
my_service_registry mysql_registry_imp::service_registry;
mysql_registry_imp::my_interface_mapping mysql_registry_imp::interface_mapping;
mysql_rwlock_t mysql_registry_imp::LOCK_registry;
