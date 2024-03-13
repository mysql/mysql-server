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

#ifndef COMPONENTS_INCLUDED
#define COMPONENTS_INCLUDED

#include <mysql/components/minimal_chassis.h> /* Minimal chassis */
#include <mysql/components/my_service.h>
#include <mysql/components/service.h>
#include <mysql/components/services/dynamic_loader.h>
#include <mysql/components/services/keyring_aes.h>
#include <mysql/components/services/keyring_load.h>
#include <mysql/components/services/keyring_reader_with_status.h>
#include <mysql/components/services/keyring_writer.h>
#include <mysql/components/services/registry.h>

namespace components {
using registry_type_t = SERVICE_TYPE_NO_CONST(registry);
using dynamic_loader_type_t = SERVICE_TYPE_NO_CONST(dynamic_loader);

using const_keyring_aes_t = SERVICE_TYPE(keyring_aes);
using const_keyring_load_t = SERVICE_TYPE(keyring_load);
using const_keyring_reader_with_status_t =
    SERVICE_TYPE(keyring_reader_with_status);
using const_keyring_writer_t = SERVICE_TYPE(keyring_writer);

void init_components_subsystem();
void deinit_components_subsystem();

class Keyring_component_load final {
 public:
  Keyring_component_load(const std::string &component_name);

  ~Keyring_component_load();

  bool ok() { return ok_; }

 private:
  dynamic_loader_type_t *dynamic_loader_;
  std::string component_path_;
  bool ok_;
};

class Keyring_services {
 public:
  Keyring_services(const std::string &implementation_name);

  virtual ~Keyring_services();

  bool ok() { return ok_; }

 protected:
  registry_type_t *registry_;
  std::string implementation_name_;
  my_service<const_keyring_load_t> keyring_load_service_;
  bool ok_;
};

class AES_encryption_keyring_services final : public Keyring_services {
 public:
  AES_encryption_keyring_services(const std::string &implementation_name);

  ~AES_encryption_keyring_services();

  const_keyring_aes_t *aes() { return keyring_aes_service_; }

  const_keyring_writer_t *writer() { return keyring_writer_service_; }

 private:
  my_service<const_keyring_aes_t> keyring_aes_service_;
  my_service<const_keyring_writer_t> keyring_writer_service_;
};

class Keyring_encryption_test final {
 public:
  Keyring_encryption_test(AES_encryption_keyring_services &aes_service);

  bool test_aes();

  ~Keyring_encryption_test() = default;

  bool ok() { return ok_; }

 private:
  AES_encryption_keyring_services &aes_service_;
  bool ok_;
};

}  // namespace components

#endif  // !COMPONENTS_INCLUDED
