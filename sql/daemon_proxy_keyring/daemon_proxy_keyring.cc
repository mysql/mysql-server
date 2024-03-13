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

#include <functional>
#include <string>

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>

#include <mysql/components/services/keyring_generator.h>
#include <mysql/components/services/keyring_keys_metadata_iterator.h>
#include <mysql/components/services/keyring_reader_with_status.h>
#include <mysql/components/services/keyring_writer.h>

#include <mysql/plugin.h>
#include <mysql/plugin_keyring.h>
#include <mysql_com.h>
#include "sql/current_thd.h"
#include "sql/mysqld.h"
#include "sql/server_component/mysql_server_keyring_lockable_imp.h"
#include "sql/sql_plugin.h"
#include "sql/sql_plugin_ref.h"

/* clang-format off */
/**
  @page PAGE_COMPONENT_DAEMON_KEYRING_PROXY component_daemon_keyring_proxy

  This plugin implementation provides wrappers over keyring plugin
  implementation and exposes functionality provided by plugin through
  keyring component service APIs.

  Following keyring component services implemented:
  - keyring_generate
  - keyring_keys_metadata_iterator
  - keyring_component_status
  - keyring_reader
  - keyring_reload
  - keyring_writer

  For rest of the services, wrappers use keyring plugin's APIs.
*/
/* clang-format on */
struct my_h_keyring_reader_object_keyring_proxy {
  unsigned char *data_;
  char *data_type_;
  size_t data_size_;
  size_t data_type_size_;
};

constexpr size_t KEYRING_PROXY_MAX_ID_LENGTH = 1024 + 1;
struct my_h_keyring_keys_metadata_iterator_keyring_proxy {
  void *iterator;
  char data_id[KEYRING_PROXY_MAX_ID_LENGTH];
  char auth_id[KEYRING_PROXY_MAX_ID_LENGTH];
  bool iterator_valid;
};

namespace keyring_proxy {

/**
  @class Callback

  @brief Class that stores callback function reference as well as the result
         of the callback function call (invoke method). Callback is called
         using the plugin descriptor pointer, so the callback can call plugin
         exposed function.
*/
class Callback {
 public:
  /**
    Constructor.

    @param callback Lambda function that is called using the invoke method.
  */
  explicit Callback(std::function<bool(st_mysql_keyring *keyring)> callback)
      : m_callback(std::move(callback)), m_result(true) {}

  /**
    Invoke the underlying callback using the specified parameter and store
    the result of the operation.

    @param keyring Keyring plugin descriptor pointer.
  */
  void invoke(st_mysql_keyring *keyring) { m_result = m_callback(keyring); }

  /**
    Result of the invoke operation.

    @return Result of the invoke operation.
  */
  bool result() const { return m_result; }

 private:
  /**
    Callback function.
  */
  const std::function<bool(st_mysql_keyring *keyring)> m_callback;

  /**
    Result of the _callback function call.
  */
  bool m_result;
};

/**
  Callback function that is called on the plugin.

  @param plugin Plugin reference.
  @param arg    Opaque Callback pointer.

  @return This function always returns true.
*/
static bool key_plugin_cb_fn(THD *, plugin_ref plugin, void *arg) {
  plugin = my_plugin_lock(nullptr, &plugin);
  if (plugin) {
    auto *callback = reinterpret_cast<Callback *>(arg);
    callback->invoke(
        reinterpret_cast<st_mysql_keyring *>(plugin_decl(plugin)->info));
  }
  plugin_unlock(nullptr, plugin);
  // this function should get executed only for the first plugin. This is why
  // it always returns error. plugin_foreach will stop after first iteration.
  return true;
}

/**
  Iterate over plugins of the MYSQL_KEYRING_PLUGIN type and call the function
  specified by the argument.

  @param fn           Function that can call plugin defined function.

  @return Result of the fn call.
*/
static bool iterate_plugins(std::function<bool(st_mysql_keyring *keyring)> fn) {
  Callback callback(fn);
  plugin_foreach(current_thd, key_plugin_cb_fn, MYSQL_KEYRING_PLUGIN,
                 &callback);
  return callback.result();
}

/**
  A class that implements proxy keyring component
  services and calls keyring plugin APIs underneath.

  Methods assume that keyring access check has been
  performed.
*/
class Keyring_proxy_imp {
 public:
  /* keyring_generator */
  /**
    Generate data and store in keyring

    @param [in]  data_id   Data Identifier
    @param [in]  auth_id   Authorization ID
    @param [in]  data_type Type of data. Assumed null terminated.
    @param [in]  data_size Size of the data to be generated

    @returns status of the operation
      @retval true  Success - Key generated and stored in keyring.
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(generate,
                            (const char *data_id, const char *auth_id,
                             const char *data_type, size_t data_size)) {
    try {
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_generate(data_id, data_type, auth_id,
                                           data_size);
      });
      return retval;
    } catch (...) {
      return true;
    }
  }

  /* keyring_keys_metdata_iterator */
  /**
    Forward iterator initialization

    @param [out] forward_iterator metadata iterator

    @returns Status of the operation
      @retval true  Success
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(init, (my_h_keyring_keys_metadata_iterator *
                                   forward_iterator)) {
    try {
      auto *local_object =
          new my_h_keyring_keys_metadata_iterator_keyring_proxy();
      local_object->iterator = nullptr;
      memset(local_object->data_id, 0, KEYRING_PROXY_MAX_ID_LENGTH);
      memset(local_object->auth_id, 0, KEYRING_PROXY_MAX_ID_LENGTH);
      local_object->iterator_valid = false;
      bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        keyring->mysql_key_iterator_init(&local_object->iterator);
        return false;
      });

      if (retval) {
        delete local_object;
        return true;
      }

      /*
        Keyrin plugin uses mysql_key_iterator_get_key() to move
        iterator forward as well. With keyring components design,
        this changes. next() method is responsible for moving the
        iterator. While get_length()/get() methods retrieve information.
        Hence, we will move iterator forward one step right after
        creation and cache the value.

        get_length/get methods will retrieve information
        from cached value.

        next() will call mysql_key_iterator_get_key() again
        and cache new values if any.
      */
      retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_iterator_get_key(local_object->iterator,
                                                   local_object->data_id,
                                                   local_object->auth_id);
      });

      if (retval) {
        /*
          This means there is not data in keyring.
          so we set validity to false.
        */
        local_object->iterator_valid = false;
      } else
        local_object->iterator_valid = true;

      *forward_iterator =
          reinterpret_cast<my_h_keyring_keys_metadata_iterator>(local_object);
      return false;
    } catch (...) {
      return true;
    }
  }

  /**
    Iterator deinitialization

    Note: forward_iterator should not be used after call to deinit

    @param [in, out] forward_iterator metadata iterator

    @returns Status of the operation
      @retval true  Success
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(
      deinit, (my_h_keyring_keys_metadata_iterator forward_iterator)) {
    try {
      auto *local_object =
          reinterpret_cast<my_h_keyring_keys_metadata_iterator_keyring_proxy *>(
              forward_iterator);
      if (local_object == nullptr) return false;
      memset(local_object->data_id, 0, KEYRING_PROXY_MAX_ID_LENGTH);
      memset(local_object->auth_id, 0, KEYRING_PROXY_MAX_ID_LENGTH);
      local_object->iterator_valid = false;
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        keyring->mysql_key_iterator_deinit(local_object->iterator);
        return false;
      });
      delete local_object;
      return retval;
    } catch (...) {
      return true;
    }
  }

  /**
    Validity of iterator

    @param [in] forward_iterator Metadata iterator handle

    @returns always returns true
  */
  static DEFINE_BOOL_METHOD(
      is_valid, (my_h_keyring_keys_metadata_iterator forward_iterator)) {
    try {
      const auto *local_object =
          reinterpret_cast<my_h_keyring_keys_metadata_iterator_keyring_proxy *>(
              forward_iterator);
      if (local_object == nullptr || !local_object->iterator_valid)
        return false;
      return true;
    } catch (...) {
      return false;
    }
  }

  /**
    @param [in, out] forward_iterator Iterator object

    @returns status of next operation
      @retval true  Success
      @retval false End of iterator
  */

  static DEFINE_BOOL_METHOD(
      next, (my_h_keyring_keys_metadata_iterator forward_iterator)) {
    try {
      auto *local_object =
          reinterpret_cast<my_h_keyring_keys_metadata_iterator_keyring_proxy *>(
              forward_iterator);
      if (local_object == nullptr || !local_object->iterator_valid) return true;
      memset(local_object->data_id, 0, KEYRING_PROXY_MAX_ID_LENGTH);
      memset(local_object->auth_id, 0, KEYRING_PROXY_MAX_ID_LENGTH);
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_iterator_get_key(local_object->iterator,
                                                   local_object->data_id,
                                                   local_object->auth_id);
      });
      if (retval) local_object->iterator_valid = false;
      return retval;
    } catch (...) {
      return true;
    }
  }

  /**
    Fetch metadata for current key pointed by iterator and move
    the iterator forward

    @param [in]  forward_iterator forward_iterator metadata iterator
    @param [out] data_id_length   ID information of current data
    @param [out] auth_id_length   Owner of the key

    @returns Status of the operation
      @retval true  Success
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(
      get_length, (my_h_keyring_keys_metadata_iterator forward_iterator,
                   size_t *data_id_length, size_t *auth_id_length)) {
    try {
      if (data_id_length == nullptr || auth_id_length == nullptr) {
        return true;
      }
      *data_id_length = 0;
      *auth_id_length = 0;
      const auto *local_object =
          reinterpret_cast<my_h_keyring_keys_metadata_iterator_keyring_proxy *>(
              forward_iterator);
      if (local_object == nullptr || !local_object->iterator_valid) return true;
      *data_id_length = strlen(local_object->data_id);
      *auth_id_length = strlen(local_object->auth_id);
      return false;
    } catch (...) {
      return true;
    }
  }
  /**
    Fetch metadata for current key pointed by iterator and move
    the iterator forward

    @param [in]  forward_iterator forward_iterator metadata iterator
    @param [out] data_id          ID information of current data
    @param [in]  data_id_length   Length of data_id buffer
    @param [out] auth_id          Owner of the key
    @param [in]  auth_id_length   Length of auth_id buffer

    @returns Status of the operation
      @retval true  Success
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(
      get, (my_h_keyring_keys_metadata_iterator forward_iterator, char *data_id,
            size_t data_id_length, char *auth_id, size_t auth_id_length)) {
    try {
      if (data_id == nullptr || auth_id == nullptr) {
        return true;
      }

      const auto *local_object =
          reinterpret_cast<my_h_keyring_keys_metadata_iterator_keyring_proxy *>(
              forward_iterator);
      if (local_object == nullptr || !local_object->iterator_valid) return true;

      if (data_id_length < strlen(local_object->data_id)) return true;
      if (auth_id_length < strlen(local_object->auth_id)) return true;

      memcpy(data_id, local_object->data_id, strlen(local_object->data_id));
      memcpy(auth_id, local_object->auth_id, strlen(local_object->auth_id));
      return false;
    } catch (...) {
      return true;
    }
  }

  /* keyring_reader */
  /**
    Initialize reader

    @param [in]  data_id          Data Identifier
    @param [in]  auth_id          Authorization ID
    @param [out] reader_object    Reader object

    @returns status of the operation
      @retval true  Success
      @retval false Failure
  */
  static DEFINE_BOOL_METHOD(reader_init,
                            (const char *data_id, const char *auth_id,
                             my_h_keyring_reader_object *reader_object)) {
    try {
      unsigned char *key = nullptr;
      char *key_type = nullptr;
      size_t key_size = 0;
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_fetch(data_id, &key_type, auth_id,
                                        (void **)&key, &key_size);
      });

      if (!retval) {
        /*
          Keyring plugin returns success even if key is absent.
          We need to check if key is really present
        */
        if (key_size > 0 && key != nullptr) {
          auto *local_object = new my_h_keyring_reader_object_keyring_proxy{
              key, key_type, key_size, strlen(key_type)};
          key = nullptr;
          key_type = nullptr;
          key_size = 0;
          *reader_object =
              reinterpret_cast<my_h_keyring_reader_object>(local_object);
          /* Key present */
          return false;
        }
        /* Key absent */
        *reader_object = nullptr;
        return false;
      }
      /* Keyring plugin error */
      return true;
    } catch (...) {
      return true;
    }
  }

  /**
    Deinitialize reader

    @param [in] reader_object    Reader object

    @returns status of the operation
      @retval true  Success
      @retval false Failure
  */
  static DEFINE_BOOL_METHOD(reader_deinit,
                            (my_h_keyring_reader_object reader_object)) {
    try {
      auto *local_object =
          reinterpret_cast<my_h_keyring_reader_object_keyring_proxy *>(
              reader_object);
      if (!local_object) return false;
      memset(local_object->data_, 0, local_object->data_size_);
      memset(local_object->data_type_, 0, local_object->data_type_size_);

      my_free(local_object->data_);
      my_free(local_object->data_type_);

      local_object->data_ = nullptr;
      local_object->data_type_ = nullptr;

      delete local_object;
      return false;
    } catch (...) {
      return true;
    }
  }

  /**
    Fetch length of the data

    @param [in]  reader_object      reader object
    @param [out] data_size          Size of fetched data
    @param [out] data_type_size     Size of data type

    @returns status of the operation
      @retval true  Success
      @retval false Failure
  */
  static DEFINE_BOOL_METHOD(fetch_length,
                            (my_h_keyring_reader_object reader_object,
                             size_t *data_size, size_t *data_type_size)) {
    try {
      const auto *local_object =
          reinterpret_cast<my_h_keyring_reader_object_keyring_proxy *>(
              reader_object);
      if (!local_object) return true;

      *data_size = local_object->data_size_;
      *data_type_size = local_object->data_type_size_;
      return false;
    } catch (...) {
      return true;
    }
  }

  /**
    Fetches data from keyring

    @param [in]  reader_object           reader object
    @param [out] data_buffer             Out buffer for data
    @param [in]  data_buffer_length      Length of out buffer
    @param [out] data_size               Size of fetched data
    @param [out] data_type               Data type buffer
    @param [in]  data_type_buffer_length Datatype buffer length
    @param [in]  data_type_size          Size of data type buffer

    @returns status of the operation
      @retval true  Success
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(fetch,
                            (my_h_keyring_reader_object reader_object,
                             unsigned char *data_buffer,
                             size_t data_buffer_length, size_t *data_size,
                             char *data_type, size_t data_type_buffer_length,
                             size_t *data_type_size)) {
    try {
      const auto *local_object =
          reinterpret_cast<my_h_keyring_reader_object_keyring_proxy *>(
              reader_object);
      if (!local_object) return true;
      if (local_object->data_size_ > data_buffer_length ||
          local_object->data_type_size_ > data_type_buffer_length)
        return true;
      *data_size = local_object->data_size_;
      *data_type_size = local_object->data_type_size_;
      memcpy(data_buffer, local_object->data_, local_object->data_size_);
      memcpy(data_type, local_object->data_type_,
             local_object->data_type_size_);
      return false;
    } catch (...) {
      return true;
    }
  }

  /* keyring_writer */
  /**
    Store data in keyring

    @param [in]  data_id        Data Identifier
    @param [in]  auth_id        Authorization ID
    @param [in]  data           Data to be stored
    @param [in]  data_size      Size of data to be stored
    @param [in]  data_type      Type of data

    @returns status of the operation
      @retval true  Success
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(store, (const char *data_id, const char *auth_id,
                                    const unsigned char *data, size_t data_size,
                                    const char *data_type)) {
    try {
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_store(data_id, data_type, auth_id, data,
                                        data_size);
      });
      return retval;
    } catch (...) {
      return true;
    }
  }

  /**
    Remove data from keyring

    @param [in] data_id Data Identifier
    @param [in] auth_id Authorization ID

    @returns status of the operation
      @retval true  Success - Key removed successfully or key not present.
      @retval false Failure
  */

  static DEFINE_BOOL_METHOD(remove,
                            (const char *data_id, const char *auth_id)) {
    try {
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_remove(data_id, auth_id);
      });
      return retval;
    } catch (...) {
      return true;
    }
  }

  /**
    Keyring status

    @returns status whether keyring is active or not
  */
  static DEFINE_BOOL_METHOD(keyring_status, ()) {
    try {
      /*
        There is no direct way to find keyring plugin status.
        So we rely on mysql_key_fetch() which returns non-zero
        value in case keyring plugin is not functional.
      */
      char *key = nullptr;
      char *key_type = nullptr;
      size_t key_size;
      const bool retval = iterate_plugins([&](st_mysql_keyring *keyring) {
        return keyring->mysql_key_fetch("dummy_daemon_proxy_keyring_id",
                                        &key_type, nullptr, (void **)&key,
                                        &key_size);
      });
      if (!retval) {
        /* We are not interested in key data */
        if (key != nullptr) my_free(key);
        if (key_type != nullptr) my_free(key_type);
        return true;
      }
      return false;
    } catch (...) {
      return false;
    }
  }
};
}  // namespace keyring_proxy

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
BEGIN_SERVICE_IMPLEMENTATION(daemon_keyring_proxy, keyring_generator)
keyring_proxy::Keyring_proxy_imp::generate END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(daemon_keyring_proxy,
                             keyring_keys_metadata_iterator)
keyring_proxy::Keyring_proxy_imp::init,
    keyring_proxy::Keyring_proxy_imp::deinit,
    keyring_proxy::Keyring_proxy_imp::is_valid,
    keyring_proxy::Keyring_proxy_imp::next,
    keyring_proxy::Keyring_proxy_imp::get_length,
    keyring_proxy::Keyring_proxy_imp::get END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(daemon_keyring_proxy, keyring_component_status)
keyring_proxy::Keyring_proxy_imp::keyring_status END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(daemon_keyring_proxy, keyring_reader_with_status)
keyring_proxy::Keyring_proxy_imp::reader_init,
    keyring_proxy::Keyring_proxy_imp::reader_deinit,
    keyring_proxy::Keyring_proxy_imp::fetch_length,
    keyring_proxy::Keyring_proxy_imp::fetch END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(daemon_keyring_proxy, keyring_writer)
keyring_proxy::Keyring_proxy_imp::store,
    keyring_proxy::Keyring_proxy_imp::remove END_SERVICE_IMPLEMENTATION();

/** ======================================================================= */

/** Plugin related stuff */

struct st_mysql_daemon daemon_keyring_proxy_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

using keyring_generator_t = SERVICE_TYPE_NO_CONST(keyring_generator);
using keyring_keys_metadata_iterator_t =
    SERVICE_TYPE_NO_CONST(keyring_keys_metadata_iterator);
using keyring_component_status_t =
    SERVICE_TYPE_NO_CONST(keyring_component_status);
using keyring_reader_t = SERVICE_TYPE_NO_CONST(keyring_reader_with_status);
using keyring_writer_t = SERVICE_TYPE_NO_CONST(keyring_writer);

enum proxy_keyring_services_enum {
  GENERATOR = 0,
  KEYS_METADATA_ITERATOR,
  COMPONENT_STATUS,
  READER_WITH_STATUS,
  WRITER
};
static const char *proxy_keyring_service_names[] = {
    "keyring_generator.daemon_keyring_proxy",
    "keyring_keys_metadata_iterator.daemon_keyring_proxy",
    "keyring_component_status.daemon_keyring_proxy",
    "keyring_reader_with_status.daemon_keyring_proxy",
    "keyring_writer.daemon_keyring_proxy"};

static my_h_service proxy_keyring_service_handles[] = {
    reinterpret_cast<my_h_service>(const_cast<keyring_generator_t *>(
        &SERVICE_IMPLEMENTATION(daemon_keyring_proxy, keyring_generator))),
    reinterpret_cast<my_h_service>(
        const_cast<keyring_keys_metadata_iterator_t *>(&SERVICE_IMPLEMENTATION(
            daemon_keyring_proxy, keyring_keys_metadata_iterator))),
    reinterpret_cast<my_h_service>(
        const_cast<keyring_component_status_t *>(&SERVICE_IMPLEMENTATION(
            daemon_keyring_proxy, keyring_component_status))),
    reinterpret_cast<my_h_service>(
        const_cast<keyring_reader_t *>(&SERVICE_IMPLEMENTATION(
            daemon_keyring_proxy, keyring_reader_with_status))),
    reinterpret_cast<my_h_service>(const_cast<keyring_writer_t *>(
        &SERVICE_IMPLEMENTATION(daemon_keyring_proxy, keyring_writer))),
};

/**
  Initializes the plugin.
  Registers the proxy keyring services.
*/
static int daemon_keyring_proxy_plugin_init(void *) {
  const my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", srv_registry);
  bool retval;

  for (unsigned int i = GENERATOR; i <= WRITER; ++i) {
    retval = registrator->register_service(proxy_keyring_service_names[i],
                                           proxy_keyring_service_handles[i]);
    if (retval) return 1;
  }

  /*
    In case no keyring component was loaded, following will register
    proxy keyring services as default. This would enable keyring plugin
    usage.
  */
  set_srv_keyring_implementation_as_default();

  return 0;
}

/**
  De-initializes the plugin.
  Unregisters services.
*/
static int daemon_keyring_proxy_plugin_deinit(void *) {
  const my_service<SERVICE_TYPE(registry_registration)> registrator(
      "registry_registration", srv_registry);
  int retval = 0;

  for (unsigned int i = GENERATOR; i <= WRITER; ++i) {
    retval |= registrator->unregister(proxy_keyring_service_names[i]);
  }

  return retval;
}

/** Plugin Descriptor */
mysql_declare_plugin(daemon_keyring_proxy){
    MYSQL_DAEMON_PLUGIN,
    &daemon_keyring_proxy_plugin,
    "daemon_keyring_proxy_plugin",
    "Oracle",
    "A plugin that implements the keyring component "
    "services atop of the keyring plugin",
    PLUGIN_LICENSE_GPL,
    daemon_keyring_proxy_plugin_init,   /* Plugin Init */
    nullptr,                            /* Plugin Check uninstall */
    daemon_keyring_proxy_plugin_deinit, /* Plugin Deinit */
    0x0100,                             /* 1.0 */
    nullptr,                            /* Status Variables */
    nullptr,                            /* System Variables */
    nullptr,                            /* Config options */
    0,                                  /* Flags */
} mysql_declare_plugin_end;
