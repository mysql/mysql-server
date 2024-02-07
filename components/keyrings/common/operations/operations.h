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

#ifndef OPERATIONS_INCLUDED
#define OPERATIONS_INCLUDED

#include <memory> /* std::unique_ptr */

#include "components/keyrings/common/data/data_extension.h"
#include "components/keyrings/common/data/meta.h"
#include "components/keyrings/common/memstore/cache.h"
#include "components/keyrings/common/memstore/iterator.h"

/* clang-format off */
/**
  @page PAGE_COMPONENT_KEYRING_COMMON Common Keyring Implementation Infrastructure

  keyring_common library includes modules that can be used in various keyring
  implementation. This library provides implementation of following:

  1.  A JSON configuration file reader and parser
  2.  A data reader/writer using JSON as format
  3.  A backup file based file reader/writer
  4.  Sensitive data container
  5.  Metadata - Used to uniquely identify sensitive data
  6.  A wrapper to define extension over sensitive data
      E.g. ID as provided by key management server
  7.  An in-memory cache to store Metadata OR {Metadata, Data}
  8.  An iterator over cache
  9.  Operations class to interface services APIs with implementation
  10. AES encryption/decryption operations
  11. Set of utility functions such as random data generator, hex converter etc.

  Following diagram shows interactions between various parts of keyring_common
  library and how they can be used to implement a keyring component.

  The diagram uses a file based backend as example and how it can use
  various modules provided by keyring_common library.

  @startuml
  package Error_logging {
    object LogErr

    LogErr -[#red]-> Service_implementation_templates
    LogErr -[#red]-> Component_implementation
  }

  package Component_implementation {
    object Service_implementation
    object Backend
    object Component_specific_constraints

    Service_implementation <-[#red]- Backend
    Backend <.[#red]. Json_data
    Backend <.[#red]. Backup_aware_file_ops
    Service_implementation -[#red]-> Operations
    Backend <.[#red]. Encryption
    Service_implementation <-[#red]- Config_reader
    Component_specific_constraints -[#red]-> Service_implementation_templates
    Service_implementation <-[#red]- service_definition_headers

    Component_specific_constraints -[hidden]left- LogErr
  }

  package Library_mysys {
    object Hex_tools

    Backend -[hidden]right- Hex_tools
  }

  package Keyring_common {
    package service_definition_headers {
      object Writer_definition
      object Load_definition
      object Reader_definition
      object Metadata_query_definition
      object Keys_metadata_iterator_definition
      object Generator_definition
      object Encryption_definition

      Load_definition -[hidden]up- Writer_definition
      Writer_definition -[hidden]up- Reader_definition
      Reader_definition -[hidden]up- Metadata_query_definition
      Metadata_query_definition -[hidden]up- Keys_metadata_iterator_definition
      Keys_metadata_iterator_definition -[hidden]up- Generator_definition
      Generator_definition -[hidden]up- Encryption_definition
    }

    package Config_reader {
      object Json_config_reader
    }

    package Backup_aware_file_ops {
      object File_writer
      object File_reader

      File_reader <-[#red]left- File_writer
    }

    package Json_data {
      object Json_writer
      object Json_reader

      Hex_tools -[#red]-> Json_reader
      Hex_tools -[#red]-> Json_writer
    }

    package Data_representation {
      object Data
      object Metadata
    }

    package Memstore {
      object Iterator
      object Cache

      Cache <-[#red]- Data_representation
      Cache <-[#red]- Iterator
    }

    package Operations {
      object Operation_manager

      Operation_manager <-[#red]- Memstore
      Operation_manager -[#red]-> Service_implementation_templates
    }

    package Utils {
      object PRNG
    }

    package Encryption {
      object AES
    }

    PRNG -[hidden]up- AES

    package Internal_error_logging {
      object Internal_LogErr

      Internal_LogErr .[#red].> LogErr
    }

    package Service_implementation_templates {
      object Writer_template
      object Reader_template
      object Metadata_query_template
      object Keys_metadata_iterator_template
      object Generator_template
      object Encryption_template

      Writer_template -[hidden]up- Reader_template
      Reader_template -[hidden]up- Metadata_query_template
      Metadata_query_template -[hidden]up- Keys_metadata_iterator_template
      Keys_metadata_iterator_template -[hidden]up- Generator_template
      Generator_template -[hidden]up- Encryption_template

      Encryption -[#red]right-> Encryption_template
      PRNG -[#red]-> Generator_template
    }

    Service_implementation -[hidden]down- Operations
  }
  @enduml
*/

/**
  @page PAGE_COMPONENT_KEYRING_WRITE_NEW "How to write a new keyring component"

  Common keyring implementation infrastructure provides useful parts to minimize
  efforts involved in writing a new component. This page will provide details about
  how a new keyring component can be implemented using the common infrastructure

  Any keyring component can be divided in 3 parts:

  1. Configuration Management
  1. Service Implementation
  3. Backend Management

  @section configuration_management Configuration Management
  A binary must be able to load a keyring component on top of minimal chassis.
  Hence, the component cannot use option management provided by underlying binary
  to read configuration for the keyring.

  One way to read configuration is to accept it as a JSON file.
  A sample configuration file may look like following:
  @code
  {
    "version" : "1.0.0"
    "config_key_1" : "config_data_1",
    "config_key_2" : "config_data_2",
    "config_key_3" : "config_data_3"
  }
  @endcode

  Various (key, value) pair would help component get details required
  to communicate with backend and initialize keyring.

  You can use @ref keyring_common::config::Config_reader
  to read a JSON file and extract value of individual configuration parameters.

  Depending upon the services implemented by the component, it is possible that:

  A> Configuration data can be queried at runtime

  B> Configuration can be changed at runtime

  @section service_implementation Service Implementation
  Please refer to @ref PAGE_KEYRING_COMPONENT for description of various services
  that a keyring component should implement. You may take a look at
  @ref group_keyring_component_services_inventory for details about specific services
  and various APIs.

  If you are writing a keyring component that should work with MySQL server,
  the component must implement following services.
  1. Keyring reader service: @ref s_mysql_keyring_reader_with_status
     Use @ref keyring_common::service_definition::Keyring_reader_service_impl
     for service class declaration.
     Use @ref KEYRING_READER_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  2. Keyring writer service: @ref s_mysql_keyring_writer
     Use @ref keyring_common::service_definition::Keyring_writer_service_impl
     for service class declaration.
     Use @ref KEYRING_WRITER_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  3. Keyring generator service: @ref s_mysql_keyring_generator
     Use @ref keyring_common::service_definition::Keyring_generator_service_impl
     for service class declaration.
     Use @ref KEYRING_GENERATOR_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  4. Keyring keys metadata iterator service: @ref s_mysql_keyring_keys_metadata_iterator
     Use @ref keyring_common::service_definition::Keyring_keys_metadata_iterator_service_impl
     for service class declaration.
     Use @ref KEYRING_KEYS_METADATA_FORWARD_ITERATOR_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  5. Keyring component metadata query service: @ref s_mysql_keyring_component_metadata_query
     Use @ref keyring_common::service_definition::Keyring_metadata_query_service_impl
     for service class declaration.
     Use @ref KEYRING_COMPONENT_METADATA_QUERY_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  6. Keyring load service: @ref s_mysql_keyring_load
     Use @ref keyring_common::service_definition::Keyring_load_service_impl
     for service class declaration.
     Use @ref KEYRING_LOAD_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  7. Keyring AES service: @ref s_mysql_keyring_aes
     Use @ref keyring_common::service_definition::Keyring_aes_service_impl
     for service class declaration.
     Use @ref KEYRING_AES_IMPLEMENTOR to add service implementation details
     required by component infrastructure.
  8. Keyring status service: @ref s_mysql_keyring_component_status
     Use @ref keyring_common::service_definition::Keyring_metadata_query_service_impl
     for service class declaration.
     Use @ref KEYRING_COMPONENT_STATUS_IMPLEMENTOR to add service implementation details
     required by component infrastructure.

  Please note that key of type AES and SECRET must be supported.

  In addition, you will also need to implement log_builtins and log_builtins_string
  services if you are planning to use keyring common's implementation templates.
  The common keyring infrastrcture has a bare-minimum implementation of these
  services. You can use @ref keyring_common::service_definition::Log_builtins_keyring
  for service class declaration. Use @ref KEYRING_LOG_BUILTINS_IMPLEMENTOR and
  @ref KEYRING_LOG_BUILTINS_STRING_IMPLEMENTOR to add service implementation details
  required by component infrastrcture.

  Common keyring infrastructure also provides implementation for some of the above
  mentioned services. This will allow implementor to create a minimal definition of
  service APIs and use templates provided by common infra.

  In order to do that, common keyring infrastrcture requires following:

  1. A handle to @ref keyring_common::operations::Keyring_operations object
     This class provides a wrapper over keyring backend and expect that implementor provides
     handle to a backend class with set of APIs required to manage keys. Please see
     @ref backend_management for more details.
  2. A handle to @ref keyring_common::service_implementation::Component_callbacks object
     This class provides callback methods which are implemented by individual component.
     For example: Keyring state, Maximum supported data length, metadata vector creation etc.

  Assumping that above mentioned dependencies are satisfied, following are some inputs
  on how various services can be implemented.
  1. Keyring reader service: @ref s_mysql_keyring_reader_with_status
     See @ref keyring_common::service_implementation::init_reader_template
     See @ref keyring_common::service_implementation::deinit_reader_template
     See @ref keyring_common::service_implementation::fetch_length_template
     See @ref keyring_common::service_implementation::fetch_template
  2. Keyring writer service: @ref s_mysql_keyring_writer
     See @ref keyring_common::service_implementation::store_template
     See @ref keyring_common::service_implementation::remove_template
  3. Keyring generator service: @ref s_mysql_keyring_generator
     See @ref keyring_common::service_implementation::generate_template
  4. Keyring keys metadata iterator service: @ref s_mysql_keyring_keys_metadata_iterator
     See @ref keyring_common::service_implementation::init_keys_metadata_iterator_template
     See @ref keyring_common::service_implementation::deinit_keys_metadata_iterator_template
     See @ref keyring_common::service_implementation::keys_metadata_iterator_is_valid
     See @ref keyring_common::service_implementation::keys_metadata_iterator_next
     See @ref keyring_common::service_implementation::keys_metadata_get_length_template
     See @ref keyring_common::service_implementation::keys_metadata_get_template
  5. Keyring component metadata query service: @ref s_mysql_keyring_component_metadata_query
     See @ref keyring_common::service_implementation::keyring_metadata_query_init_template
     See @ref keyring_common::service_implementation::keyring_metadata_query_deinit_template
     See @ref keyring_common::service_implementation::keyring_metadata_query_is_valid_template
     See @ref keyring_common::service_implementation::keyring_metadata_query_next_template
     See @ref keyring_common::service_implementation::keyring_metadata_query_get_length_template
     See @ref keyring_common::service_implementation::keyring_metadata_query_get_template
  6. Keyring load service: @ref s_mysql_keyring_load
     This service is specific to individual keyring component because it has strong
     dependency on component specific configuration details. The component should implement
     it as per its own requirement.
  7. Keyring AES service: @ref s_mysql_keyring_aes
     See @ref keyring_common::service_implementation::aes_get_encrypted_size_template
     See @ref keyring_common::service_implementation::aes_encrypt_template
     See @ref keyring_common::service_implementation::aes_decrypt_template
  8. Keyring status service: @ref s_mysql_keyring_component_status
     See @ref keyring_common::service_implementation::keyring_metadata_query_keyring_initialized_template

  @section backend_management Backend Management
  This part would be different for each keyring component. Backend can be a remote server,
  something that stored locally or a combination of both. Regardless of the backend, if
  @ref keyring_common::operations::Keyring_operations is to be used, component must implement
  a class similar to following:

  @code
  template <typename Data_extension>
  class Backend {
   public:
      // Fetch data

      // @param [in]  metadata Key
      // @param [out] data     Value

      // @returns Status of find operation
      //   @retval false Entry found. Check data.
      //   @retval true  Entry missing.
    bool get(const keyring_common::meta::Metadata &metadata,
             keyring_common::data::Data &data) const;

      // Store data

      // @param [in]      metadata Key
      // @param [in, out] data     Value

      // @returns Status of store operation
      //   @retval false Entry stored successfully
      //   @retval true  Failure
    bool store(const keyring_common::meta::Metadata &metadata,
               keyring_common::data::Data &data);

      // Erase data located at given key

      // @param [in] metadata Key
      // @param [in] data     Value - not used.

      // @returns Status of erase operation
      //   @retval false Data deleted
      //   @retval true  Could not find or delete data
    bool erase(const keyring_common::meta::Metadata &metadata,
               keyring_common::data::Data &data);

      // Generate random data and store it

      // @param [in]  metadata Key
      // @param [out] data     Generated value
      // @param [in]  length   Length of data to be generated

      // @returns Status of generate + store operation
      //   @retval false Data generated and stored successfully
      //   @retval true  Error
    bool generate(const keyring_common::meta::Metadata &metadata,
                  keyring_common::data::Data &data, size_t length);

      // Populate cache

      // @param [in] operations  Handle to operations class

      // @returns status of cache insertion
      //   @retval false Success
      //   @retval true  Failure
    bool load_cache(
        keyring_common::operations::Keyring_operations<Keyring_file_backend>
            &operations);

      // Maximum data length supported
    size_t maximum_data_length() const;

      // Get number of elements stored in backend
    size_t size() const;
  };
  @endcode

  Common keyring infrastructure includes implementation that may help file management.
  It provides a content agnostic, backup aware file reader and writer.

  Writer works in following manner:
  - Write content to backup file
  - If backup file exists, copy its content to main file
  - Remove backup file

  Reader works in following manner:
  - Invoke writer to process backup file, if it exists
  - Read from main file

  Please see @ref keyring_common::data_file::File_writer for writer details and
  @ref keyring_common::data_file::File_reader for reader details.

  @section in_member_data_management In-memory Data Management

  Common keyring infrastructure also provides useful classes to manage data in memory.

  1. To cache metadata related to data, use @ref keyring_common::meta::Metadata
  2. To cache data in memory, use @ref keyring_common::data::Data
  3. To cache backend specific details for data (e.g. ID generated by backend),
     use @ref keyring_common::data::Data_extension
  4. A reader/writer pair for JSON data that can be used to store data, related metadata
     and data extension if any. See @ref keyring_common::json_data::Json_reader
     and @ref keyring_common::json_data::Json_writer.

     Expected JSON format is specified below:

     @code

     {
       "version": "1.0",
       "elements": [
         {
           "user": "<user_name>",
           "data_id": "<name>",
           "data_type": "<data_type>",
           "data": "<hex_of_data>",
           "extension": [
           ]
         },
         ...
         ...
       ]
     }

     @endcode
*/

/* clang-format on */

namespace keyring_common {

namespace operations {

/**
  Keyring operations
  A class to perform operations on keyring.
  If cache is enabled, operations uses it. Otherwise,
  backend is used.

  Assumptions:
  1. Backend implements interface to keyring backend.
  2. Backend is required to support same Data_extension as Keyring_operations

  Please see @ref PAGE_COMPONENT_KEYRING_WRITE_NEW for details about
  Backend class.
*/

template <typename Backend, typename Data_extension = data::Data>
class Keyring_operations {
 public:
  /**
    Constructor

    @param [in] cache_data Whether to cache data or not
    @param [in] backend    Pointer to keyring backend

    Populates the cache from backend
  */
  explicit Keyring_operations(bool cache_data, Backend *backend)
      : cache_(), cache_data_(cache_data), backend_(backend), valid_(false) {
    load_cache();
  }

  ~Keyring_operations() = default;

  /**
    Insert API to populate cache

    @param [in] metadata    Key to the data
    @param [in] secret_data Actual data

    @returns status of insertion
      @retval false Success
      @retval true  Failure
  */
  bool insert(const meta::Metadata &metadata, Data_extension secret_data) {
    /* valid_ = true implies cache is operational. Do not permit bulk insert */
    if (valid_ == true) return true;
    if (!cache_data_) secret_data.set_data(data::Data{});
    return !cache_.store(metadata, secret_data);
  }

 public:
  /**
    Search API

    @param [in]  metadata Key to the data
    @param [out] data     Fetched data

    @returns status of search operation
      @retval false Success - data contains required information
      @retval true  Failure - data is not valid
  */
  bool get(const meta::Metadata &metadata, data::Data &data) {
    Data_extension fetched_data;
    if (!metadata.valid()) return true;
    if (!cache_.get(metadata, fetched_data)) return true;
    if (!cache_data_) {
      /* Fetch data from backend */
      if ((*backend_).get(metadata, fetched_data)) return true;
    }
    data = fetched_data.get_data();
    return false;
  }

  /**
    Get Backend-specific data extension

    @param [in]  metadata Key to the data
    @param [out] data     Fetched data extension

    @returns status of search operation
      @retval false Success - data contains required information
      @retval true  Failure - data is not valid

    NOTE: get_data_extension NEVER returns data.
          It only returns Data extension information.
  */
  bool get_data_extension(const meta::Metadata &metadata,
                          Data_extension &data) {
    if (!metadata.valid()) return true;
    if (!cache_.get(metadata, data)) return true;
    if (cache_data_) data.set_data(data::Data{});
    return false;
  }

  /**
    Store API

    @param [in] metadata Key to the data
    @param [in] data     Data to be stored

    @returns status of store operation
      @retval false Success - data stored
      @retval true  Failure
  */
  bool store(const meta::Metadata &metadata, const data::Data &data) {
    Data_extension stored_data(data);
    if (!metadata.valid()) return true;
    Data_extension fetched_data;
    if (cache_.get(metadata, fetched_data)) return true;
    if ((*backend_).store(metadata, stored_data)) return true;
    /*
      Note: We always cache metadata.
      So we always consult the operation's cache first
      to see if valid (data_id, auth_id) pair exists.

      This would also retrieve backend specific metadata
      if any.

      Once we confirm that key is present, we then fetch
      the data either from the cache(if caching is ON)
      or from the backend(if caching is OFF).
    */
    if (!cache_data_) {
      /* Just store the metadata */
      stored_data.set_data(data::Data{});
    }
    if (!cache_.store(metadata, stored_data)) {
      /* Failed caching the data. Remove it from backend too. */
      (void)(*backend_).erase(metadata, stored_data);
      return true;
    }
    return false;
  }

  /**
    Remove API

    @param [in] metadata Key to the data

    @returns status of remove operation
      @retval false Success - data removed OR does not exist
      @retval true  Failure
  */
  bool erase(const meta::Metadata &metadata) {
    if (!metadata.valid()) return true;
    Data_extension fetched_data;
    /* Get backend specific extension from cache */
    if (!cache_.get(metadata, fetched_data)) return true;
    /* Remove it from backend */
    if ((*backend_).erase(metadata, fetched_data)) return true;
    /* Remove it from the cache */
    cache_.erase(metadata);
    return false;
  }

  /**
    Generate API

    @param [in] metadata Key for the data
    @param [in] type     Type of data
    @param [in] length   Length of data to be generated

    @returns status of data generation
      @retval false Success - data generated and stored in backend
      @retval true  Failure
  */
  bool generate(const meta::Metadata &metadata, const data::Type type,
                size_t length) {
    if (!metadata.valid()) return true;
    data::Data g_data(type);
    Data_extension generated_data(g_data);
    if (cache_.get(metadata, generated_data)) return true;
    if ((*backend_).generate(metadata, generated_data, length)) return true;
    if (!cache_data_) generated_data.set_data(data::Data{});
    if (!cache_.store(metadata, generated_data)) {
      /* Failed to cache the data, remove it from backend too */
      (void)(*backend_).erase(metadata, generated_data);
      return true;
    }
    return false;
  }

  /**
    Iterator creation for read

    If data is cached, iterator on cached data is returned.
    Otherwise iterator created by backend is returned.

    @param [out] it       Forward iterator to metadata
    @param [in]  metadata Metadata for the record to be searched

    @returns Status
      @retval false Success
      @retval true  Failure
  */
  bool init_read_iterator(
      std::unique_ptr<iterator::Iterator<Data_extension>> &it,
      const meta::Metadata &metadata) {
    if (!valid()) return true;
    if (!metadata.valid()) return true;
    it = std::make_unique<iterator::Iterator<Data_extension>>(cache_, metadata);
    return (it.get() == nullptr);
  }

  /**
    Iterator creation

    @param [out] it     Forward iterator to metadata
    @param [in]  cached Iterator type

    @returns Status
      @retval false Success
      @retval true  Failure
  */
  bool init_forward_iterator(
      std::unique_ptr<iterator::Iterator<Data_extension>> &it, bool cached) {
    if (!valid()) return true;
    it = std::make_unique<iterator::Iterator<Data_extension>>(cache_, cached);
    return (it.get() == nullptr);
  }

  /**
    Iterator destruction

    @param [in, out] it Forward iterator to metadata

  */
  void deinit_forward_iterator(
      std::unique_ptr<iterator::Iterator<Data_extension>> &it) {
    it.reset(nullptr);
  }

  /**
    Check iterator validity

    @param [in] it Forward iterator to metadata

    @returns Status
      @retval true  Valid
      @retval false Invalid
  */
  bool is_valid(std::unique_ptr<iterator::Iterator<Data_extension>> &it) {
    return (valid() && it.get() != nullptr && (*it).valid(cache_.version()));
  }

  /**
    Move iterator forward

    @param [in] it Forward iterator to metadata

    @returns Status
      @retval false Success
      @retval true  Failure
  */
  bool next(std::unique_ptr<iterator::Iterator<Data_extension>> &it) {
    if (valid() && it.get() != nullptr) return !((*it).next(cache_.version()));
    return true;
  }

  /**
    Get data from iterator

    @param [in]  it        Forward iterator to metadata
    @param [out] metadata  Metadata for given key
    @param [out] data      Data for given key (Including extension)

    @returns Status
      @retval false Success
      @retval true  Failure
  */
  bool get_iterator_data(
      std::unique_ptr<iterator::Iterator<Data_extension>> &it,
      meta::Metadata &metadata, Data_extension &data) {
    if (!valid() || it.get() == nullptr || !(*it).valid(cache_.version()))
      return true;
    if (!(*it).metadata(cache_.version(), metadata)) return true;
    if (cache_data_) {
      if (!(*it).data(cache_.version(), data)) return true;
    } else {
      cache_.get(metadata, data);
      if ((*backend_).get(metadata, data)) return true;
    }
    return !metadata.valid();
  }

  /**
    Get metadata from iterator

    @param [in]  it        Forward iterator to metadata
    @param [out] metadata  Metadata for given key
    @param [out] data      Extension for given key

    @returns Status
      @retval false Success
      @retval true  Failure
  */
  bool get_iterator_metadata(
      std::unique_ptr<iterator::Iterator<Data_extension>> &it,
      meta::Metadata &metadata, Data_extension &data) {
    if (!valid() || it.get() == nullptr || !(*it).valid(cache_.version()))
      return true;
    if (!(*it).metadata(cache_.version(), metadata)) return true;
    if (!(*it).data(cache_.version(), data)) return true;
    if (cache_data_) data.set_data(data::Data{});
    return !metadata.valid();
  }

  /**
    Maximum data length supported

    @returns Maximum length supported
  */
  size_t maximum_data_length() const {
    return (*backend_).maximum_data_length();
  }

  /** Keyring size */
  size_t keyring_size() { return cache_.size(); }

  /** Validity */
  bool valid() { return valid_; }

 private:
  void load_cache() {
    Backend *backend = backend_.get();
    /* Clear the cache */
    cache_.clear();

    valid_ = false;
    if (backend == nullptr || backend->size() == 0) {
      valid_ = true;
      return;
    }

    if (backend->load_cache(*this) == true) return;

    /*
      If we fail to load metadata (and data) for all keys,
      wipe the cache clean.
    */
    if (backend->size() != cache_.size()) {
      cache_.clear();
    } else {
      valid_ = true;
    }
  }

  /**  Metadata cache */
  cache::Datacache<Data_extension> cache_;
  /** Flag to cache data */
  bool cache_data_;
  /** Keyring backend */
  std::unique_ptr<Backend> backend_;
  /** Validity */
  bool valid_;
};

}  // namespace operations

}  // namespace keyring_common

#endif  // !OPERATIONS_INCLUDED
