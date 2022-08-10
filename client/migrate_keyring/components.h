/*
   Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef COMPONENTS_INCLUDED
#define COMPONENTS_INCLUDED

#include <mysql/components/minimal_chassis.h> /* Minimal chassis */
#include <mysql/components/my_service.h>
#include <mysql/components/services/keyring_keys_metadata_iterator.h>
#include <mysql/components/services/keyring_load.h>
#include <mysql/components/services/keyring_reader_with_status.h>
#include <mysql/components/services/keyring_writer.h>

#include "client/logger.h" /* Log */
#include "options.h"       /* Mysql_connection */
namespace components {
using registry_type_t = SERVICE_TYPE_NO_CONST(registry);
using dynamic_loader_type_t = SERVICE_TYPE_NO_CONST(dynamic_loader);
using keyring_load_t = SERVICE_TYPE_NO_CONST(keyring_load);
using keyring_keys_metadata_iterator_t =
    SERVICE_TYPE_NO_CONST(keyring_keys_metadata_iterator);
using keyring_reader_with_status_t =
    SERVICE_TYPE_NO_CONST(keyring_reader_with_status);
using keyring_writer_t = SERVICE_TYPE_NO_CONST(keyring_writer);

using const_registry_type_t = SERVICE_TYPE(registry);
using const_dynamic_loader_type_t = SERVICE_TYPE(dynamic_loader);
using const_keyring_keys_metadata_iterator_t =
    SERVICE_TYPE(keyring_keys_metadata_iterator);
using const_keyring_load_t = SERVICE_TYPE(keyring_load);
using const_keyring_reader_with_status_t =
    SERVICE_TYPE(keyring_reader_with_status);
using const_keyring_writer_t = SERVICE_TYPE(keyring_writer);

void init_components_subsystem();
void deinit_components_subsystem();

class Keyring_component_load final {
 public:
  Keyring_component_load(const std::string component_name,
                         const std::string type);

  ~Keyring_component_load();

  bool ok() { return ok_; }

 private:
  dynamic_loader_type_t *dynamic_loader_;
  std::string component_path_;
  std::string type_;
  bool ok_;
};

class Keyring_services {
 public:
  Keyring_services(const std::string implementation_name,
                   const std::string instance_path);

  virtual ~Keyring_services();

  bool ok() { return ok_; }

 protected:
  registry_type_t *registry_;
  std::string implementation_name_;
  my_service<const_keyring_load_t> keyring_load_service_;
  bool ok_;
};

class Source_keyring_services final : public Keyring_services {
 public:
  Source_keyring_services(const std::string implementation_name,
                          const std::string instance_path);

  ~Source_keyring_services();

  const_keyring_keys_metadata_iterator_t *metadata_iterator() {
    return keyring_keys_metadata_service_;
  }
  const_keyring_reader_with_status_t *reader() {
    return keyring_reader_service_;
  }

 private:
  my_service<const_keyring_keys_metadata_iterator_t>
      keyring_keys_metadata_service_;
  my_service<const_keyring_reader_with_status_t> keyring_reader_service_;
};

class Destination_keyring_services final : public Keyring_services {
 public:
  Destination_keyring_services(const std::string implementation_name,
                               const std::string instance_path);

  ~Destination_keyring_services();

  const_keyring_writer_t *writer() { return keyring_writer_service_; }

 private:
  my_service<const_keyring_writer_t> keyring_writer_service_;
};

class Keyring_migrate final {
 public:
  Keyring_migrate(Source_keyring_services &src,
                  Destination_keyring_services &dst, bool online_migration);

  bool migrate_keys();

  ~Keyring_migrate();

  bool lock_source_keyring();
  bool unlock_source_keyring();
  bool ok() { return ok_; }

 private:
  Source_keyring_services &src_;
  Destination_keyring_services &dst_;
  my_h_keyring_keys_metadata_iterator iterator_{nullptr};
  options::Mysql_connection mysql_connection_;
  bool ok_{false};
  const size_t maximum_size_{16384};
};

}  // namespace components

#endif  // !COMPONENTS_INCLUDED
