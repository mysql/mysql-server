/*
   Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <scope_guard.h>

#include "components.h"
#include "options.h"   /* command line options */
#include "utilities.h" /* Error logging */

using options::Options;

namespace components {

registry_type_t *components_registry = nullptr;
dynamic_loader_type_t *components_dynamic_loader = nullptr;

void init_components_subsystem() {
  minimal_chassis_init((&components_registry), nullptr);
  components_registry->acquire(
      "dynamic_loader",
      reinterpret_cast<my_h_service *>(&components_dynamic_loader));
}

void deinit_components_subsystem() {
  components_registry->release(
      reinterpret_cast<my_h_service>(components_dynamic_loader));
  minimal_chassis_deinit(components_registry, nullptr);
}

Keyring_component_load::Keyring_component_load(const std::string component_name,
                                               const std::string type)
    : dynamic_loader_(components_dynamic_loader),
      component_path_("file://"),
      type_(type) {
  if (Options::s_component_dir != nullptr)
    component_path_.append(Options::s_component_dir);
  component_path_ += "/" + component_name;

  log_debug << "Loading: " << component_path_ << std::endl;

  const char *urn[] = {component_path_.c_str()};
  const bool load_status = dynamic_loader_->load(urn, 1);
  if (load_status)
    log_error << "Failed to load " << type_ << " keyring: " << component_path_
              << std::endl;
  else
    log_debug << "Successfully loaded " << type_
              << " keyring: " << component_path_ << std::endl;
  ok_ = !load_status;
}

Keyring_component_load::~Keyring_component_load() {
  if (ok_) {
    const char *urn[] = {component_path_.c_str()};
    log_debug << "Unloading: " << component_path_ << std::endl;
    const bool load_status = dynamic_loader_->unload(urn, 1);
    if (load_status)
      log_error << "Failed to unload " << type_
                << " keyring: " << component_path_ << std::endl;
    else
      log_debug << "Successfully unloaded " << type_
                << " keyring: " << component_path_ << std::endl;
    ok_ = false;
  }
}

Keyring_services::Keyring_services(const std::string implementation_name,
                                   const std::string instance_path)
    : registry_(components_registry),
      implementation_name_(implementation_name),
      keyring_load_service_(
          std::string{"keyring_load."}.append(implementation_name).c_str(),
          registry_),
      ok_(false) {
  if (keyring_load_service_) {
    log_error << "Failed to acquire keyring_load service" << std::endl;
    return;
  }

  if (keyring_load_service_->load(
          Options::s_component_dir,
          instance_path.length() ? instance_path.c_str() : nullptr) != 0) {
    const std::string message("Failed to initialize keyring");
    log_error << message << std::endl;
    return;
  }

  log_debug << "Successfully acquired keyring error service handles for "
            << implementation_name_ << std::endl;

  ok_ = true;
}

Keyring_services::~Keyring_services() {
  ok_ = false;
  if (!registry_) return;

  log_debug << "Successfully released keyring error service handles for "
            << implementation_name_ << std::endl;
}

Source_keyring_services::Source_keyring_services(
    const std::string implementation_name, const std::string instance_path)
    : Keyring_services(implementation_name, instance_path),
      keyring_keys_metadata_service_("keyring_keys_metadata_iterator",
                                     keyring_load_service_, registry_),
      keyring_reader_service_("keyring_reader_with_status",
                              keyring_load_service_, registry_) {
  if (keyring_keys_metadata_service_ || keyring_reader_service_) {
    log_error << "Failed to acquire keyring metadata iterator and keyring "
                 "reader services for "
              << implementation_name_ << std::endl;
    ok_ = false;
    return;
  }

  log_debug << "Successfully acquired keyring metarata iterator and kering "
               "reader services' handles for "
            << implementation_name_ << std::endl;
}

Source_keyring_services::~Source_keyring_services() {
  if (registry_ == nullptr) return;

  log_debug << "Successfully released keyring metadata iterator and reader "
               "service handles for "
            << implementation_name_ << std::endl;
}

Destination_keyring_services::Destination_keyring_services(
    const std::string implementation_name, const std::string instance_path)
    : Keyring_services(implementation_name, instance_path),
      keyring_writer_service_("keyring_writer", keyring_load_service_,
                              registry_) {
  if (keyring_writer_service_) {
    log_error << "Failed to acquire keyring writer service handle for "
              << implementation_name_ << std::endl;
    ok_ = false;
    return;
  }

  log_debug << "Successfully acquired keyring writer service handle for "
            << implementation_name_ << std::endl;
}

Destination_keyring_services::~Destination_keyring_services() {
  if (!registry_) return;

  log_debug << "Successfully released keyring writer service handle for "
            << implementation_name_ << std::endl;
}

Keyring_migrate::Keyring_migrate(Source_keyring_services &src,
                                 Destination_keyring_services &dst,
                                 bool online_migration)
    : src_(src), dst_(dst), mysql_connection_(online_migration) {
  if (!src_.ok() || !dst_.ok()) return;
  if (online_migration && !mysql_connection_.ok()) return;
  if (lock_source_keyring() == false) {
    log_error << "Failed to lock source keyring" << std::endl;
    return;
  }
  auto iterator = src_.metadata_iterator();
  if (iterator->init(&iterator_) != 0) {
    log_error << "Error creating source keyring iterator" << std::endl;
    return;
  }
  ok_ = true;
}

bool Keyring_migrate::lock_source_keyring() {
  if (Options::s_online_migration == false) return true;
  if (!mysql_connection_.ok()) return false;
  const std::string lock_statement("SET GLOBAL KEYRING_OPERATIONS=0");
  return mysql_connection_.execute(lock_statement);
}

bool Keyring_migrate::unlock_source_keyring() {
  if (Options::s_online_migration == false || !mysql_connection_.ok())
    return true;
  const std::string unlock_statement("SET GLOBAL KEYRING_OPERATIONS=1");
  return mysql_connection_.execute(unlock_statement);
}

bool Keyring_migrate::migrate_keys() {
  if (!ok_)
    log_error << "Cannot migrate keys. Check that source and destination "
                 "keyrings are initialized properly."
              << std::endl;

  auto metadata_iterator = src_.metadata_iterator();
  auto reader = src_.reader();
  auto writer = dst_.writer();
  size_t migrated_count = 0;
  size_t skipped_count = 0;
  bool retval = true;
  bool next_ok = true;

  size_t data_id_length = 0;
  size_t auth_id_length = 0;

  for (; metadata_iterator->is_valid(iterator_) && next_ok;
       next_ok = !metadata_iterator->next(iterator_)) {
    data_id_length = 0;
    auth_id_length = 0;
    /* Fetch length */
    if (metadata_iterator->get_length(iterator_, &data_id_length,
                                      &auth_id_length) != 0) {
      log_error << "Could not fetch next available key content from keyring"
                << std::endl;
      retval = false;
      break;
    }

    const std::unique_ptr<char[]> data_id(new char[data_id_length + 1]);
    const std::unique_ptr<char[]> auth_id(new char[auth_id_length + 1]);

    if (data_id.get() == nullptr || auth_id.get() == nullptr) {
      log_error << "Failed to allocated required memory for data_id and auth_id"
                << std::endl;
      retval = false;
      break;
    }
    /* Fetch metadata of next available key */
    if (metadata_iterator->get(iterator_, data_id.get(), data_id_length + 1,
                               auth_id.get(), auth_id_length + 1) != 0) {
      log_error << "Could not fetch next available key content from keyring"
                << std::endl;
      retval = false;
      break;
    }

    /* Fetch key details */
    my_h_keyring_reader_object reader_object = nullptr;
    const bool status =
        reader->init(data_id.get(), auth_id.get(), &reader_object);

    if (status == true) {
      log_error << "Keyring reported error" << std::endl;
      retval = false;
      break;
    }

    if (reader_object == nullptr) {
      log_warning << "Could not find data pointed by data_id: " << data_id.get()
                  << ", auth_id: " << auth_id.get() << ". Skipping"
                  << std::endl;
      ++skipped_count;
      continue;
    }

    auto cleanup_guard = create_scope_guard([&] {
      if (reader_object != nullptr) {
        if (reader->deinit(reader_object) != 0)
          log_error << "Failed to deallocated reader_object" << std::endl;
      }
      reader_object = nullptr;
    });

    size_t data_size, data_type_size;
    if (reader->fetch_length(reader_object, &data_size, &data_type_size) != 0) {
      log_warning << "Could not find data pointed by data_id: " << data_id.get()
                  << ", auth_id: " << auth_id.get() << ". Skipping"
                  << std::endl;
      ++skipped_count;
      continue;
    }

    if (data_size > maximum_size_) {
      log_warning << "Length (" << data_size
                  << ") of data identified by data_id: " << data_id.get()
                  << ", auth_id: " << auth_id.get()
                  << " exceeds maximum supported"
                     " length by migration tool ("
                  << maximum_size_ << "). Skipping" << std::endl;
      ++skipped_count;
      continue;
    }

    const std::unique_ptr<unsigned char[]> data_buffer(
        new unsigned char[data_size]);
    const std::unique_ptr<char[]> data_type_buffer(
        new char[data_type_size + 1]);

    if (data_buffer.get() == nullptr || data_type_buffer.get() == nullptr) {
      log_error << "Failed to allocated required memory for data pointed by "
                   "data_id: "
                << data_id.get() << ", auth_id: " << auth_id.get()
                << ". Stopping." << std::endl;
      retval = false;
      break;
    }

    memset(data_buffer.get(), 0, data_size);
    memset(data_type_buffer.get(), 0, data_type_size + 1);

    if (reader->fetch(reader_object, data_buffer.get(), data_size, &data_size,
                      data_type_buffer.get(), data_type_size + 1,
                      &data_type_size) != 0) {
      log_warning << "Could not find data pointed by data_id: " << data_id.get()
                  << ", auth_id: " << auth_id.get() << ". Skipping"
                  << std::endl;
      ++skipped_count;
      continue;
    }

    /* Write to destination keyring */
    if (data_size > 0 && data_type_size > 0) {
      const bool write_status =
          writer->store(data_id.get(), auth_id.get(), data_buffer.get(),
                        data_size, data_type_buffer.get());
      memset(data_buffer.get(), 0, data_size);
      memset(data_type_buffer.get(), 0, data_type_size + 1);
      if (write_status == true) {
        log_error << "Failed to write data pointed by data_id: "
                  << data_id.get() << ", auth_id: " << auth_id.get()
                  << " into destination keyring" << std::endl;
        retval = false;
        break;
      }
      log_debug << "Successfully migrated data with data_id: " << data_id.get()
                << ", auth_id: " << auth_id.get() << "." << std::endl;
    }

    ++migrated_count;
  }

  if (metadata_iterator->deinit(iterator_) != 0) {
    log_error << "Failed to deinitialize source iterator" << std::endl;
    retval = false;
  }
  iterator_ = nullptr;

  if (retval) {
    log_info << "Successfully migrated " << migrated_count << " keys. Skipped "
             << skipped_count << " keys." << std::endl;
  } else {
    log_error << "Failed to migrate all keys to destination keyring. Please "
                 "check log for more details"
              << std::endl;
  }

  return retval;
}

Keyring_migrate::~Keyring_migrate() {
  if (unlock_source_keyring() == false) {
    log_error << "Failed to unlock source keyring. Please unlock it manually."
              << std::endl;
  }
}

}  // namespace components
