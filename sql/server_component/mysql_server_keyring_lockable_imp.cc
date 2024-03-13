/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <memory>
#include <sstream>

#include "my_dbug.h"
#include "mysql_server_keyring_lockable_imp.h"

#include <include/mysql/components/my_service.h>
#include <include/mysql/components/services/registry.h>
#include <include/mysqld_error.h>
#include <include/rwlock_scoped_lock.h>
#include <include/scope_guard.h>
#include <mysql/components/services/log_builtins.h>

#include "sql/mysqld.h"        /* srv_registry */
#include "sql/set_var.h"       /* keyring_access_test */
#include "sql/sql_component.h" /* init and deinit function for RWLock */
#include "sql/sql_plugin.h"    /* opt_plugin_dir */

/* clang-format off */
/**
  @page PAGE_COMPONENT_KEYRING_MYSQL_SERVER component_keyring_mysql_server

  MySQL server component provides a wrapper implementation of all
  component services related to keyring. Following is the list
  of services:

  - keyring_aes
  - keyring_generate
  - keyring_keys_metadata_iterator
  - keyring_component_status
  - keyring_metadata_query
  - keyring_reader
  - keyring_load
  - keyring_writer

  The implementation of above services in turn calls actual keyring component
  implementation (such as component_keyring_file). However, this component
  provides concurrency control for read/write operations affecting keyring.

  In addition, this implementation also checks if KEYRING_OPERATIONS system
  variable permits given operation or not.

  This implementation is set as default implementation for MySQL server.

  Handles for underlying keyring component service implementations are
  obtained at two locations:
  1. After processing server's manifest file
  2. After installing proxy keyring component for keyring plugin

  Once handles are set, they are not set again.

  Thus, if a keyring component is installed through manifest file, keyring plugin
  will not considered even if it is installed through --early-plugin-load.

  These handles ares freed at the time of server shutdown.
*/
/* clang-format on */

/** Server handle for keyring AES encryption service */
SERVICE_TYPE(keyring_aes) *srv_keyring_aes = nullptr;

/** Server handle for keyring generator service */
SERVICE_TYPE(keyring_generator) *srv_keyring_generator = nullptr;

/** Server handle for keyring keys metadata service */
SERVICE_TYPE(keyring_keys_metadata_iterator)
*srv_keyring_keys_metadata_iterator = nullptr;

/** Server handle for keyring status service */
SERVICE_TYPE(keyring_component_status) *srv_keyring_component_status = nullptr;

/** Server handle for keyring component metadata service */
SERVICE_TYPE(keyring_component_metadata_query)
*srv_keyring_component_metadata_query = nullptr;

/** Server handle for keyring reader service */
SERVICE_TYPE(keyring_reader_with_status) *srv_keyring_reader = nullptr;

/** Server handle for keyring load service */
SERVICE_TYPE(keyring_load) *srv_keyring_load = nullptr;

/** Server handle for keyring writer service */
SERVICE_TYPE(keyring_writer) *srv_keyring_writer = nullptr;

enum lockable_service_enum {
  AES_ENCRYPTION = 0,
  GENERATOR,
  KEYS_METADATA_ITERATOR,
  COMPONENT_STATUS,
  COMPONENT_METADATA_QUERY,
  READER_WITH_STATUS,
  LOAD,
  WRITER
};
static const char *lockable_service_names[] = {
    "keyring_aes",
    "keyring_generator",
    "keyring_keys_metadata_iterator",
    "keyring_component_status",
    "keyring_component_metadata_query",
    "keyring_reader_with_status",
    "keyring_load",
    "keyring_writer"};

namespace keyring_lockable {

/** Server component internal handle for keyring AES encryption service */
SERVICE_TYPE(keyring_aes) *internal_keyring_aes = nullptr;

/** Server component internal handle for keyring generator service */
SERVICE_TYPE(keyring_generator) *internal_keyring_generator = nullptr;

/** Server component internal handle for keyring keys metadata service */
SERVICE_TYPE(keyring_keys_metadata_iterator)
*internal_keyring_keys_metadata_iterator = nullptr;

/** Server component internal handle for keyring component status service */
SERVICE_TYPE(keyring_component_status) *internal_keyring_component_status =
    nullptr;

/** Server component internal handle for keyring component metadata service */
SERVICE_TYPE(keyring_component_metadata_query)
*internal_keyring_component_metadata_query = nullptr;

/** Server component internal handle for keyring reader service */
SERVICE_TYPE(keyring_reader_with_status) *internal_keyring_reader = nullptr;

/** Server component internal handle for keyring load service */
SERVICE_TYPE(keyring_load) *internal_keyring_load = nullptr;

/** Server component internal handle for keyring writer service */
SERVICE_TYPE(keyring_writer) *internal_keyring_writer = nullptr;

/** Access control for keyring */
mysql_rwlock_t LOCK_keyring_component;
/** PSI key for @sa LOCK_keyring_component */
static PSI_rwlock_key key_LOCK_keyring_component;

/** Error messages */

/**
  Wrapper over my_h_keyring_reader_object
  to associate RWlock.

  Read lock is taken when reader is initialized
  and freed when reader is deinitialized.

  This makes sure that fetch_length() and fetch()
  APIs are protected by the read lock.
*/
struct my_h_keyring_reader_object_server {
  my_h_keyring_reader_object object_;
  rwlock_scoped_lock lock_;
};

/**
  Wrapper over my_h_keyring_keys_metadata_iterator
  to associate RWlock.

  Read lock is taken when iterator is initialized
  and freed when iterator is deinitialized.

  This makes sure that get_length(), get(), next()
  and is_valid() APIs are protected by the read lock.
*/
struct my_h_keyring_keys_metadata_iterator_server {
  my_h_keyring_keys_metadata_iterator iterator_;
  rwlock_scoped_lock lock_;
};

namespace keyring_common::service_definition {

/**
  Helper to check service validity

  @param [in] service Service handle
  @param [in] name    Service name enum

  @returns validity state of esrvice
    @retval false Valid service handle
    @retval true  Invalid service handle. Error raised.
*/
static bool check_service(const void *service,
                          enum lockable_service_enum name) {
  if (service == nullptr) {
    LogErr(WARNING_LEVEL, ER_WARN_NO_KEYRING_COMPONENT_SERVICE_FOUND,
           lockable_service_names[name]);
    return true;
  }
  return false;
}

/* AES Encryption Service */

DEFINE_BOOL_METHOD(Keyring_aes_service_impl::get_size,
                   (size_t input_length, const char *mode, size_t block_size,
                    size_t *out_size)) {
  if (check_service(internal_keyring_aes, AES_ENCRYPTION)) return true;

  /* No need to take a lock as this one does not access keyring */
  return internal_keyring_aes->get_size(input_length, mode, block_size,
                                        out_size);
}

DEFINE_BOOL_METHOD(Keyring_aes_service_impl::encrypt,
                   (const char *data_id, const char *auth_id, const char *mode,
                    size_t block_size, const unsigned char *iv, int padding,
                    const unsigned char *data_buffer, size_t data_buffer_length,
                    unsigned char *out_buffer, size_t out_buffer_length,
                    size_t *out_length)) {
  if (check_service(internal_keyring_aes, AES_ENCRYPTION)) return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_aes->encrypt(
      data_id, auth_id, mode, block_size, iv, padding, data_buffer,
      data_buffer_length, out_buffer, out_buffer_length, out_length);
}

DEFINE_BOOL_METHOD(Keyring_aes_service_impl::decrypt,
                   (const char *data_id, const char *auth_id, const char *mode,
                    size_t block_size, const unsigned char *iv, int padding,
                    const unsigned char *data_buffer, size_t data_buffer_length,
                    unsigned char *out_buffer, size_t out_buffer_length,
                    size_t *out_length)) {
  if (check_service(internal_keyring_aes, AES_ENCRYPTION)) return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_aes->decrypt(
      data_id, auth_id, mode, block_size, iv, padding, data_buffer,
      data_buffer_length, out_buffer, out_buffer_length, out_length);
}

/* keyring_generator */

DEFINE_BOOL_METHOD(Keyring_generator_service_impl::generate,
                   (const char *data_id, const char *auth_id,
                    const char *data_type, size_t data_size)) {
  if (keyring_access_test()) return true;

  if (check_service(internal_keyring_generator, GENERATOR)) return true;

  DBUG_EXECUTE_IF("keyring_generate_fail", DBUG_SUICIDE(););
  const rwlock_scoped_lock lock(&LOCK_keyring_component, true, __FILE__,
                                __LINE__);
  return internal_keyring_generator->generate(data_id, auth_id, data_type,
                                              data_size);
}

/* keyring_keys_metdata_iterator */

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::init,
                   (my_h_keyring_keys_metadata_iterator * forward_iterator)) {
  if (check_service(internal_keyring_keys_metadata_iterator,
                    KEYS_METADATA_ITERATOR))
    return true;

  rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__, __LINE__);
  auto *local_object =
      new my_h_keyring_keys_metadata_iterator_server{nullptr, std::move(lock)};
  if (local_object == nullptr) return true;
  const bool retval =
      internal_keyring_keys_metadata_iterator->init(&local_object->iterator_);
  if (retval) {
    delete local_object;
    return retval;
  }

  *forward_iterator =
      reinterpret_cast<my_h_keyring_keys_metadata_iterator>(local_object);
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::deinit,
                   (my_h_keyring_keys_metadata_iterator forward_iterator)) {
  if (check_service(internal_keyring_keys_metadata_iterator,
                    KEYS_METADATA_ITERATOR))
    return true;

  auto *local_object =
      reinterpret_cast<my_h_keyring_keys_metadata_iterator_server *>(
          forward_iterator);
  if (!local_object) return true;

  /*
    Even if underlying keyring fails to deinitialize
    actual iterator, we will release the lock.
    Otherwise, keyring will remain in locked state
  */
  const bool retval =
      internal_keyring_keys_metadata_iterator->deinit(local_object->iterator_);
  local_object->iterator_ = nullptr;

  /* This shall release the lock too */
  delete local_object;
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::is_valid,
                   (my_h_keyring_keys_metadata_iterator forward_iterator)) {
  if (check_service(internal_keyring_keys_metadata_iterator,
                    KEYS_METADATA_ITERATOR))
    return false;

  const auto *local_object =
      reinterpret_cast<my_h_keyring_keys_metadata_iterator_server *>(
          forward_iterator);
  if (!local_object) return false;

  return internal_keyring_keys_metadata_iterator->is_valid(
      local_object->iterator_);
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::get_length,
                   (my_h_keyring_keys_metadata_iterator forward_iterator,
                    size_t *data_id_length, size_t *auth_id_length)) {
  if (check_service(internal_keyring_keys_metadata_iterator,
                    KEYS_METADATA_ITERATOR))
    return true;

  const auto *local_object =
      reinterpret_cast<my_h_keyring_keys_metadata_iterator_server *>(
          forward_iterator);
  if (!local_object) return true;

  return internal_keyring_keys_metadata_iterator->get_length(
      local_object->iterator_, data_id_length, auth_id_length);
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::next,
                   (my_h_keyring_keys_metadata_iterator forward_iterator)) {
  if (check_service(internal_keyring_keys_metadata_iterator,
                    KEYS_METADATA_ITERATOR))
    return true;

  const auto *local_object =
      reinterpret_cast<my_h_keyring_keys_metadata_iterator_server *>(
          forward_iterator);
  if (!local_object) return true;

  return internal_keyring_keys_metadata_iterator->next(local_object->iterator_);
}

DEFINE_BOOL_METHOD(Keyring_keys_metadata_iterator_service_impl::get,
                   (my_h_keyring_keys_metadata_iterator forward_iterator,
                    char *data_id, size_t data_id_length, char *auth_id,
                    size_t auth_id_length)) {
  if (check_service(internal_keyring_keys_metadata_iterator,
                    KEYS_METADATA_ITERATOR))
    return true;

  const auto *local_object =
      reinterpret_cast<my_h_keyring_keys_metadata_iterator_server *>(
          forward_iterator);
  if (!local_object) return true;

  return internal_keyring_keys_metadata_iterator->get(local_object->iterator_,
                                                      data_id, data_id_length,
                                                      auth_id, auth_id_length);
}

/* keyring_component_metadata_query */

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::is_initialized, ()) {
  if (check_service(internal_keyring_component_status, COMPONENT_STATUS))
    return false;
  return internal_keyring_component_status->is_initialized();
}

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::init,
                   (my_h_keyring_component_metadata_iterator *
                    metadata_iterator)) {
  if (check_service(internal_keyring_component_metadata_query,
                    COMPONENT_METADATA_QUERY))
    return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_component_metadata_query->init(metadata_iterator);
}

DEFINE_BOOL_METHOD(
    Keyring_metadata_query_service_impl::deinit,
    (my_h_keyring_component_metadata_iterator metadata_iterator)) {
  if (check_service(internal_keyring_component_metadata_query,
                    COMPONENT_METADATA_QUERY))
    return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_component_metadata_query->deinit(metadata_iterator);
}

DEFINE_BOOL_METHOD(
    Keyring_metadata_query_service_impl::is_valid,
    (my_h_keyring_component_metadata_iterator metadata_iterator)) {
  if (check_service(internal_keyring_component_metadata_query,
                    COMPONENT_METADATA_QUERY))
    return false;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_component_metadata_query->is_valid(metadata_iterator);
}

DEFINE_BOOL_METHOD(
    Keyring_metadata_query_service_impl::next,
    (my_h_keyring_component_metadata_iterator metadata_iterator)) {
  if (check_service(internal_keyring_component_metadata_query,
                    COMPONENT_METADATA_QUERY))
    return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_component_metadata_query->next(metadata_iterator);
}

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::get_length,
                   (my_h_keyring_component_metadata_iterator metadata_iterator,
                    size_t *key_buffer_length, size_t *value_buffer_length)) {
  if (check_service(internal_keyring_component_metadata_query,
                    COMPONENT_METADATA_QUERY))
    return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_component_metadata_query->get_length(
      metadata_iterator, key_buffer_length, value_buffer_length);
}

DEFINE_BOOL_METHOD(Keyring_metadata_query_service_impl::get,
                   (my_h_keyring_component_metadata_iterator metadata_iterator,
                    char *key_buffer, size_t key_buffer_length,
                    char *value_buffer, size_t value_buffer_length)) {
  if (check_service(internal_keyring_component_metadata_query,
                    COMPONENT_METADATA_QUERY))
    return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__,
                                __LINE__);
  return internal_keyring_component_metadata_query->get(
      metadata_iterator, key_buffer, key_buffer_length, value_buffer,
      value_buffer_length);
}

/* keyring_reader_with_status */
DEFINE_BOOL_METHOD(Keyring_reader_service_impl::init,
                   (const char *data_id, const char *auth_id,
                    my_h_keyring_reader_object *reader_object)) {
  if (check_service(internal_keyring_reader, READER_WITH_STATUS)) return true;

  rwlock_scoped_lock lock(&LOCK_keyring_component, false, __FILE__, __LINE__);
  auto *local_object =
      new my_h_keyring_reader_object_server{nullptr, std::move(lock)};
  if (local_object == nullptr) return true;

  const bool retval =
      internal_keyring_reader->init(data_id, auth_id, &(local_object->object_));
  if (retval || local_object->object_ == nullptr) {
    delete local_object;
    *reader_object = nullptr;
    return retval;
  }

  *reader_object = reinterpret_cast<my_h_keyring_reader_object>(local_object);
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::deinit,
                   (my_h_keyring_reader_object reader_object)) {
  if (check_service(internal_keyring_reader, READER_WITH_STATUS)) return true;

  auto *local_object =
      reinterpret_cast<my_h_keyring_reader_object_server *>(reader_object);
  if (!local_object) return true;

  /*
    Even if underlying keyring fails to deinitialize
    actual iterator, we will release the lock.
    Otherwise, keyring will remain in locked state
  */
  const bool retval = internal_keyring_reader->deinit(local_object->object_);
  local_object->object_ = nullptr;

  /* This shall release the lock too */
  delete local_object;
  return retval;
}

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::fetch_length,
                   (my_h_keyring_reader_object reader_object, size_t *data_size,
                    size_t *data_type_size)) {
  if (check_service(internal_keyring_reader, READER_WITH_STATUS)) return true;

  const auto *local_object =
      reinterpret_cast<my_h_keyring_reader_object_server *>(reader_object);
  if (!local_object) return true;

  return internal_keyring_reader->fetch_length(local_object->object_, data_size,
                                               data_type_size);
}

DEFINE_BOOL_METHOD(Keyring_reader_service_impl::fetch,
                   (my_h_keyring_reader_object reader_object,
                    unsigned char *data_buffer, size_t data_buffer_length,
                    size_t *data_size, char *data_type,
                    size_t data_type_buffer_length, size_t *data_type_size)) {
  if (check_service(internal_keyring_reader, READER_WITH_STATUS)) return true;

  const auto *local_object =
      reinterpret_cast<my_h_keyring_reader_object_server *>(reader_object);
  if (!local_object) return true;

  return internal_keyring_reader->fetch(
      local_object->object_, data_buffer, data_buffer_length, data_size,
      data_type, data_type_buffer_length, data_type_size);
}

/* keyring_load */

DEFINE_BOOL_METHOD(Keyring_load_service_impl::load,
                   (const char *component_path, const char *instance_path)) {
  if (keyring_access_test()) return true;

  if (check_service(internal_keyring_load, LOAD)) return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, true, __FILE__,
                                __LINE__);
  return internal_keyring_load->load(component_path, instance_path);
}

/* keyring_writer */

DEFINE_BOOL_METHOD(Keyring_writer_service_impl::store,
                   (const char *data_id, const char *auth_id,
                    const unsigned char *data, size_t data_size,
                    const char *data_type)) {
  if (keyring_access_test()) return true;

  if (check_service(internal_keyring_writer, WRITER)) return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, true, __FILE__,
                                __LINE__);
  return internal_keyring_writer->store(data_id, auth_id, data, data_size,
                                        data_type);
}

DEFINE_BOOL_METHOD(Keyring_writer_service_impl::remove,
                   (const char *data_id, const char *auth_id)) {
  if (keyring_access_test()) return true;

  if (check_service(internal_keyring_writer, WRITER)) return true;

  const rwlock_scoped_lock lock(&LOCK_keyring_component, true, __FILE__,
                                __LINE__);
  return internal_keyring_writer->remove(data_id, auth_id);
}

}  // namespace keyring_common::service_definition
}  // namespace keyring_lockable

using keyring_aes_t = SERVICE_TYPE_NO_CONST(keyring_aes);
using keyring_generator_t = SERVICE_TYPE_NO_CONST(keyring_generator);
using keyring_keys_metadata_iterator_t =
    SERVICE_TYPE_NO_CONST(keyring_keys_metadata_iterator);
using keyring_component_status_t =
    SERVICE_TYPE_NO_CONST(keyring_component_status);
using keyring_component_metadata_query_t =
    SERVICE_TYPE_NO_CONST(keyring_component_metadata_query);
using keyring_reader_with_status_t =
    SERVICE_TYPE_NO_CONST(keyring_reader_with_status);
using keyring_load_t = SERVICE_TYPE_NO_CONST(keyring_load);
using keyring_writer_t = SERVICE_TYPE_NO_CONST(keyring_writer);

/** Initialize lockable keyring component */
void keyring_lockable_init() {
  mysql_rwlock_init(keyring_lockable::key_LOCK_keyring_component,
                    &keyring_lockable::LOCK_keyring_component);
}

/** Deinitialize lockable keyring component */
void keyring_lockable_deinit() {
  mysql_rwlock_destroy(&keyring_lockable::LOCK_keyring_component);
}

/**
  Set server's implementation of keyring as default

  server component provides implementation of all keyring
  related services to provide concurrency control.

  In turn it uses either one of the following as actual
  implementation(in order of priority):
  A> A keyring component loaded through manifest file
  B> Proxy keyring component over keyring plugin

  There are two places where this function is called.

  Path 1
  ======

  At the time of server startup, server_component's services
  are registered when minimal chassis is initialized. However,
  without actual keyring implementation, they are not really
  of any use.

  After minimal chassis initialization, server read manifest
  file. If file is present and contains keyring component details,
  the component is loaded and services are registered.

  At this point we have 2 implementations of each keyring
  services: One provided by server and another provided by
  keyring component.

  One component is loaded set_srv_keyring_implementation_as_default()
  is called. At this stage function will:
  1. Set server's implementation of lockable keyring as default
  2. Acquire handles to all services provided by keyring component

  After this point, keyring functionality can be used.

  Path 2
  ======

  If manifest file is not provided or does not contain details
  of keyring plugin, call to set_srv_keyring_implementation_as_default()
  will still be made and set server's implementation as default.

  However, no internal handles are set. Thus, keyring functionality
  can not be used still.

  Startup sequence will then process --early-plugin-load if provided.

  Afterwards, daemon_proxy_keyring_implementation plugin is loaded
  which registers a subset of keyring component services. These services
  will use keyring plugin if installed.

  At this point set_srv_keyring_implementation_as_default() is called
  once again. If internal handles were not set with the first call,
  they are set now and will use daemon_proxy_keyring_implementation.

  This means, if keyring plugin is available (either through
  --early-plugin-load OR loaded later through INSTALL PLUGIN),
  it will be used.

*/
void set_srv_keyring_implementation_as_default() {
  my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", srv_registry);

  auto obtain_service_handles =
      [](const char *component_part,
         SERVICE_TYPE(keyring_aes) * *aes_encryption,
         SERVICE_TYPE(keyring_generator) * *generator,
         SERVICE_TYPE(keyring_keys_metadata_iterator) * *keys_metadata_iterator,
         SERVICE_TYPE(keyring_component_status) * *component_status,
         SERVICE_TYPE(keyring_component_metadata_query) *
             *component_metadata_query,
         SERVICE_TYPE(keyring_reader_with_status) * *reader,
         SERVICE_TYPE(keyring_load) * *load,
         SERVICE_TYPE(keyring_writer) * *writer) {
        my_service<SERVICE_TYPE(registry_registration)> lamda_registrator(
            "registry_registration", srv_registry);

        std::string service_name;

        /* Reader service */
        service_name.assign(lockable_service_names[READER_WITH_STATUS]);
        service_name.append(component_part);
        srv_registry->acquire(
            service_name.c_str(),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_reader_with_status_t **>(reader)));

        /* AES encryption service */
        service_name.assign(lockable_service_names[AES_ENCRYPTION]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_aes_t **>(aes_encryption)));

        /* Generator service */
        service_name.assign(lockable_service_names[GENERATOR]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_generator_t **>(generator)));

        /* Keys metadata iterator service */
        service_name.assign(lockable_service_names[KEYS_METADATA_ITERATOR]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_keys_metadata_iterator_t **>(
                    keys_metadata_iterator)));

        /* Component status service */
        service_name.assign(lockable_service_names[COMPONENT_STATUS]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_component_status_t **>(component_status)));

        /* Component metadata query service */
        service_name.assign(lockable_service_names[COMPONENT_METADATA_QUERY]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_component_metadata_query_t **>(
                    component_metadata_query)));

        /* Reload service */
        service_name.assign(lockable_service_names[LOAD]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_load_t **>(load)));

        /* Writer service */
        service_name.assign(lockable_service_names[WRITER]);
        srv_registry->acquire_related(
            service_name.c_str(),
            reinterpret_cast<my_h_service>(
                const_cast<keyring_reader_with_status_t *>(*reader)),
            reinterpret_cast<my_h_service *>(
                const_cast<keyring_writer_t **>(writer)));
      };

  /* Part 1: Set lockable keyring as default */

  /*
    The service infrastructure's current behavior is that for any given service,
    the very first implementation that's registered with service registry
    becomes the default implementation for that service.

    Since server component's services are registered right after initializing
    minimal chassis, we don't have to set server component's implementation
    for keyring services as default explicitly.

    If this changes in future, something similar to following will be needed.

    for (auto service : lockable_service_names) {
      std::string name(service);
      name.append(".mysql_server");
      registrator->set_default(name.c_str());
    }
  */

  /* Part 2: Set internal handles that point to actual implementation */
  if (keyring_lockable::internal_keyring_aes == nullptr &&
      keyring_lockable::internal_keyring_generator == nullptr &&
      keyring_lockable::internal_keyring_component_status == nullptr &&
      keyring_lockable::internal_keyring_keys_metadata_iterator == nullptr &&
      keyring_lockable::internal_keyring_component_metadata_query == nullptr &&
      keyring_lockable::internal_keyring_reader == nullptr &&
      keyring_lockable::internal_keyring_load == nullptr &&
      keyring_lockable::internal_keyring_writer == nullptr) {
    /*
      1. Acquire iterator for keyring_reader
      2. Move forward if current handle is pointing to
      keyring_reader.mysql_server
      3. If found, acquire all related services (writer, generator, forward
      iterator, status)
      4. Set global handles to point to them
    */
    my_h_service_iterator iterator;
    my_service<SERVICE_TYPE(registry_query)> reg_query("registry_query",
                                                       srv_registry);
    if (reg_query->create(lockable_service_names[READER_WITH_STATUS],
                          &iterator))
      return;
    std::string service_name;
    {
      auto guard = create_scope_guard(
          [&reg_query, &iterator] { reg_query->release(iterator); });
      for (; !reg_query->is_valid(iterator); reg_query->next(iterator)) {
        const char *name = nullptr;
        if (reg_query->get(iterator, &name)) return;
        service_name.assign(name);
        /*
          Registry implementation does not necessarily return services
          with name "keyring_error_state". Current implementation returns an
          iterator over std::map. Hence, if following entries are present in the
          service registry:
          "keyring_reader.mysql_server"
          "keyring_writer.mysql_server"
          "keyring_generator.mysql_server"

          iterator will not only return "keyring_error_state.mysql_server", but
          also "keyring_reader.mysql_server" and "keyring_writer.mysql_server".

          Thus, we can not assume that only services matching with
          "keyring_error_state" will be returned through iterator.

          Hence, we need to match service name and component name part.

          This will fall apart if a clever implementation decides to register
          keyring_error_state service with name other than
          "keyring_error_state". To resolve that, we need iterator based on
          service type and not service name.
        */
        if (service_name.find(lockable_service_names[READER_WITH_STATUS]) !=
            std::string::npos) {
          if (service_name.find("mysql_server") == std::string::npos) break;
        }
        service_name.clear();
      }
    }

    if (service_name.length()) {
      obtain_service_handles(
          /* Find dot, the component name is right after the dot. */
          strchr(service_name.c_str(), '.'),
          &keyring_lockable::internal_keyring_aes,
          &keyring_lockable::internal_keyring_generator,
          &keyring_lockable::internal_keyring_keys_metadata_iterator,
          &keyring_lockable::internal_keyring_component_status,
          &keyring_lockable::internal_keyring_component_metadata_query,
          &keyring_lockable::internal_keyring_reader,
          &keyring_lockable::internal_keyring_load,
          &keyring_lockable::internal_keyring_writer);
    }

    /* Initialize keyring */
    if (keyring_lockable::internal_keyring_load != nullptr) {
      (void)keyring_lockable::internal_keyring_load->load(opt_plugin_dir,
                                                          mysql_real_data_home);
    }
  }

  /* Part 3: Set server wide handles */
  if (srv_keyring_aes == nullptr &&
      srv_keyring_component_metadata_query == nullptr &&
      srv_keyring_generator == nullptr &&
      srv_keyring_keys_metadata_iterator == nullptr &&
      srv_keyring_reader == nullptr && srv_keyring_load == nullptr &&
      srv_keyring_writer == nullptr) {
    obtain_service_handles(
        ".mysql_server", &srv_keyring_aes, &srv_keyring_generator,
        &srv_keyring_keys_metadata_iterator, &srv_keyring_component_status,
        &srv_keyring_component_metadata_query, &srv_keyring_reader,
        &srv_keyring_load, &srv_keyring_writer);
  }
}

void release_keyring_handles() {
  auto release_service_handles =
      [](SERVICE_TYPE(keyring_aes) * *aes_encryption,
         SERVICE_TYPE(keyring_generator) * *generator,
         SERVICE_TYPE(keyring_keys_metadata_iterator) * *keys_metadata_iterator,
         SERVICE_TYPE(keyring_component_status) * *component_status,
         SERVICE_TYPE(keyring_component_metadata_query) *
             *component_metadata_query,
         SERVICE_TYPE(keyring_reader_with_status) * *reader,
         SERVICE_TYPE(keyring_load) * *load,
         SERVICE_TYPE(keyring_writer) * *writer) {
        my_service<SERVICE_TYPE(registry_registration)> registrator(
            "registry_registration", srv_registry);
        if (*aes_encryption)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_aes_t *>(*aes_encryption)));
        *aes_encryption = nullptr;

        if (*generator)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_generator_t *>(*generator)));
        *generator = nullptr;

        if (*keys_metadata_iterator)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_keys_metadata_iterator_t *>(
                  *keys_metadata_iterator)));
        *keys_metadata_iterator = nullptr;

        if (*component_status)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_component_status_t *>(*component_status)));
        *component_status = nullptr;

        if (*component_metadata_query)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_component_metadata_query_t *>(
                  *component_metadata_query)));
        *component_metadata_query = nullptr;

        if (*reader)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_reader_with_status_t *>(*reader)));
        *reader = nullptr;

        if (*load)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_load_t *>(*load)));
        *load = nullptr;

        if (*writer)
          srv_registry->release(reinterpret_cast<my_h_service>(
              const_cast<keyring_writer_t *>(*writer)));
        *writer = nullptr;
      };

  /* Part 1: Release server wide handles */
  release_service_handles(
      &srv_keyring_aes, &srv_keyring_generator,
      &srv_keyring_keys_metadata_iterator, &srv_keyring_component_status,
      &srv_keyring_component_metadata_query, &srv_keyring_reader,
      &srv_keyring_load, &srv_keyring_writer);

  /* Part 2: Release internal handles that point to actual implementation */
  release_service_handles(
      &keyring_lockable::internal_keyring_aes,
      &keyring_lockable::internal_keyring_generator,
      &keyring_lockable::internal_keyring_keys_metadata_iterator,
      &keyring_lockable::internal_keyring_component_status,
      &keyring_lockable::internal_keyring_component_metadata_query,
      &keyring_lockable::internal_keyring_reader,
      &keyring_lockable::internal_keyring_load,
      &keyring_lockable::internal_keyring_writer);
}

bool keyring_status_no_error() {
  return (
      keyring_lockable::internal_keyring_component_status != nullptr &&
      keyring_lockable::internal_keyring_component_status->is_initialized());
}
