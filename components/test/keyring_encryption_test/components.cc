/*
   Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "components.h"
#include <scope_guard.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "components/keyrings/common/component_helpers/include/keyring_log_builtins_definition.h"
#include "mysql/components/services/component_status_var_service.h"
#include "mysql/components/services/registry.h"
#include "options.h" /* command line options */

using options::Options;

namespace components {

SERVICE_TYPE_NO_CONST(registry) *components_registry = nullptr;
SERVICE_TYPE_NO_CONST(dynamic_loader) *components_dynamic_loader = nullptr;
SERVICE_TYPE_NO_CONST(registry_registration) *reg_reg = nullptr;

/*
  We need to register a dummy status variable registration service
  since some of the keyring components are exposing status vars now.
*/
namespace dummy_status_variable_registration_implementation {
DEFINE_BOOL_METHOD(register_variable, (SHOW_VAR * /*status_var*/)) {
  return false;
}

DEFINE_BOOL_METHOD(unregister_variable, (SHOW_VAR * /*status_var*/)) {
  return false;
}

void setup() {
  static BEGIN_SERVICE_IMPLEMENTATION(
      keyring_encryption_test, status_variable_registration) register_variable,
      unregister_variable, END_SERVICE_IMPLEMENTATION();

  reg_reg->register_service(
      "status_variable_registration.keyring_encryption_test",
      (my_h_service) const_cast<void *>((const void *)&SERVICE_IMPLEMENTATION(
          keyring_encryption_test, status_variable_registration)));
}

void teardown() {
  reg_reg->unregister("status_variable_registration.keyring_encryption_test");
}
}  // namespace dummy_status_variable_registration_implementation

/*
  We need to register log_builins implementation because keyring components
  depend on it (in terms of REQUIRES_SERVICE_PLACEHOLDER)
  and minchassis does not provide it
*/
namespace log_builtins_component_helper {
KEYRING_LOG_BUILTINS_IMPLEMENTOR(keyring_encryption_test);
KEYRING_LOG_BUILTINS_STRING_IMPLEMENTOR(keyring_encryption_test);

void setup() {
  reg_reg->register_service(
      "log_builtins.keyring_encryption_test",
      (my_h_service) const_cast<void *>((const void *)&SERVICE_IMPLEMENTATION(
          keyring_encryption_test, log_builtins)));

  reg_reg->register_service(
      "log_builtins_string.keyring_encryption_test",
      (my_h_service) const_cast<void *>((const void *)&SERVICE_IMPLEMENTATION(
          keyring_encryption_test, log_builtins_string)));
}

void teardown() {
  reg_reg->unregister("log_builtins.keyring_encryption_test");
  reg_reg->unregister("log_builtins_string.keyring_encryption_test");
}

}  // namespace log_builtins_component_helper

void init_components_subsystem() {
  minimal_chassis_init((&components_registry), nullptr);
  components_registry->acquire(
      "dynamic_loader",
      reinterpret_cast<my_h_service *>(&components_dynamic_loader));
  components_registry->acquire("registry_registration",
                               reinterpret_cast<my_h_service *>(&reg_reg));
  dummy_status_variable_registration_implementation::setup();
  log_builtins_component_helper::setup();
}

void deinit_components_subsystem() {
  log_builtins_component_helper::teardown();
  dummy_status_variable_registration_implementation::teardown();
  components_registry->release(reinterpret_cast<my_h_service>(reg_reg));
  components_registry->release(
      reinterpret_cast<my_h_service>(components_dynamic_loader));
  minimal_chassis_deinit(components_registry, nullptr);
}

Keyring_component_load::Keyring_component_load(
    const std::string &component_name)
    : dynamic_loader_(components_dynamic_loader), component_path_("file://") {
  if (Options::s_component_dir != nullptr)
    component_path_.append(Options::s_component_dir);
  component_path_ += "/" + component_name;

  const char *urn[] = {component_path_.c_str()};
  const bool load_status = dynamic_loader_->load(urn, 1);
  ok_ = !load_status;
}

Keyring_component_load::~Keyring_component_load() {
  if (ok_) {
    const char *urn[] = {component_path_.c_str()};
    (void)dynamic_loader_->unload(urn, 1);
    ok_ = false;
  }
}

Keyring_services::Keyring_services(const std::string &implementation_name)
    : registry_(components_registry),
      implementation_name_(implementation_name),
      keyring_load_service_(
          std::string{"keyring_load."}.append(implementation_name).c_str(),
          registry_),
      ok_(false) {
  if (keyring_load_service_) return;

  /* We do not support non-default location for config file yet */
  if (keyring_load_service_->load(Options::s_component_dir, nullptr) != 0)
    return;

  ok_ = true;
}

Keyring_services::~Keyring_services() {
  ok_ = false;
  if (!registry_) return;
}

AES_encryption_keyring_services::AES_encryption_keyring_services(
    const std::string &implementation_name)
    : Keyring_services(implementation_name),
      keyring_aes_service_("keyring_aes", keyring_load_service_, registry_),
      keyring_writer_service_("keyring_writer", keyring_load_service_,
                              registry_) {
  if (keyring_aes_service_ || keyring_writer_service_) {
    ok_ = false;
    return;
  }
}

AES_encryption_keyring_services::~AES_encryption_keyring_services() {
  if (registry_ == nullptr) return;
}

Keyring_encryption_test::Keyring_encryption_test(
    AES_encryption_keyring_services &aes_service)
    : aes_service_(aes_service), ok_(false) {
  if (!aes_service_.ok()) return;
  ok_ = true;
}

bool Keyring_encryption_test::test_aes() {
  if (!ok_) return false;

  const auto *const writer = aes_service_.writer();
  const auto *const aes = aes_service_.aes();

  const std::string aes_key_1("AES_test_key_1");
  if (writer->store("aes_key_1", "keyring_aes_test",
                    reinterpret_cast<const unsigned char *>(aes_key_1.c_str()),
                    aes_key_1.length(), "AES") != 0) {
    std::cerr << "Failed to store key [aes_key_1, keyring_aes_test] in keyring"
              << std::endl;
    return false;
  }

  if (writer->store("secret_key_1", "keyring_aes_test",
                    reinterpret_cast<const unsigned char *>(aes_key_1.c_str()),
                    aes_key_1.length(), "SECRET") != 0) {
    std::cerr
        << "Failed to store key [secret_key_1, keyring_aes_test] in keyring"
        << std::endl;
    return false;
  }
  const std::string mode("cbc");
  constexpr size_t block_size = 256;
  constexpr bool padding = true;
  constexpr unsigned char plaintext[] =
      "Quick brown fox jumped over the lazy dog.";
  const size_t plaintext_length =
      strlen(reinterpret_cast<const char *>(plaintext));
  size_t ciphertext_length = 0;
  if (aes->get_size(plaintext_length, mode.c_str(), block_size,
                    &ciphertext_length) != 0) {
    std::cerr << "Failed to obtain ciphertext size" << std::endl;
    return false;
  }

  std::unique_ptr<unsigned char[]> output_1;
  output_1 = std::make_unique<unsigned char[]>(ciphertext_length);
  if (output_1 == nullptr) {
    std::cerr << "Failed to allocate memory for output buffer" << std::endl;
    return false;
  }
  const std::string iv1("abcdefgh12345678");

  if (!static_cast<bool>(aes->encrypt(
          "aes_key_invalid", "keyring_aes_test", mode.c_str(), block_size,
          reinterpret_cast<const unsigned char *>(iv1.c_str()), padding,
          plaintext, plaintext_length, output_1.get(), ciphertext_length,
          &ciphertext_length))) {
    std::cerr << "Failed negative test for AES-CBC-256" << std::endl;
    return false;
  }
  if (!static_cast<bool>(aes->encrypt(
          "secret_key_1", "keyring_aes_test", mode.c_str(), block_size,
          reinterpret_cast<const unsigned char *>(iv1.c_str()), padding,
          plaintext, plaintext_length, output_1.get(), ciphertext_length,
          &ciphertext_length))) {
    std::cerr << "Failed negative test for AES-CBC-256" << std::endl;
    return false;
  }

  std::cout << "Plaintext: '" << plaintext << "'" << std::endl;
  if (aes->encrypt("aes_key_1", "keyring_aes_test", mode.c_str(), block_size,
                   reinterpret_cast<const unsigned char *>(iv1.c_str()),
                   padding, plaintext, plaintext_length, output_1.get(),
                   ciphertext_length, &ciphertext_length) != 0) {
    std::cerr << "Failed to encrypt plaintext using AES-CBC-256" << std::endl;
    return false;
  }
  std::cout << "Successfully encrypted plaintext using AES-CBC-256"
            << std::endl;

  size_t decrypted_length = 0;
  if (aes->get_size(ciphertext_length, mode.c_str(), block_size,
                    &decrypted_length) != 0) {
    std::cerr << "Failed to obtain painttext size" << std::endl;
    return false;
  }

  std::unique_ptr<unsigned char[]> output_2;
  output_2 = std::make_unique<unsigned char[]>(decrypted_length);
  if (output_2 == nullptr) {
    std::cerr << "Failed to allocate memory for output buffer" << std::endl;
    return false;
  }
  memset(output_2.get(), 0, decrypted_length);

  if (!static_cast<bool>(aes->decrypt(
          "aes_key_invalid", "keyring_aes_test", mode.c_str(), block_size,
          reinterpret_cast<const unsigned char *>(iv1.c_str()), padding,
          output_1.get(), ciphertext_length, output_2.get(), decrypted_length,
          &decrypted_length))) {
    std::cerr << "Failed negative test for AES-CBC-256" << std::endl;
    return false;
  }

  if (!static_cast<bool>(aes->decrypt(
          "secret_key_1", "keyring_aes_test", mode.c_str(), block_size,
          reinterpret_cast<const unsigned char *>(iv1.c_str()), padding,
          output_1.get(), ciphertext_length, output_2.get(), decrypted_length,
          &decrypted_length))) {
    std::cerr << "Failed negative test for AES-CBC-256" << std::endl;
    return false;
  }

  if (aes->decrypt("aes_key_1", "keyring_aes_test", mode.c_str(), block_size,
                   reinterpret_cast<const unsigned char *>(iv1.c_str()),
                   padding, output_1.get(), ciphertext_length, output_2.get(),
                   decrypted_length, &decrypted_length) != 0) {
    std::cerr << "Failed to decrypt plaintext using AES-CBC-256" << std::endl;
    return false;
  }
  std::cout << "Successfully decrypted plaintext using AES-CBC-256"
            << std::endl;

  const std::string decrypted_output{reinterpret_cast<char *>(output_2.get()),
                                     decrypted_length};
  std::cout << "Decrypted plaintext: '" << decrypted_output << "'" << std::endl;

  if (writer->remove("secret_key_1", "keyring_aes_test") != 0) {
    std::cerr
        << "Failed to remove key [secret_key_1, keyring_aes_test] from keyring"
        << std::endl;
    return false;
  }

  if (writer->remove("aes_key_1", "keyring_aes_test") != 0) {
    std::cerr
        << "Failed to remove key [aes_key_1, keyring_aes_test] from keyring"
        << std::endl;
    return false;
  }

  return true;
}

}  // namespace components
