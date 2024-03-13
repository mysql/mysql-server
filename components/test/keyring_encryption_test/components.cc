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
#include "options.h" /* command line options */

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

  const auto writer = aes_service_.writer();
  const auto aes = aes_service_.aes();

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

  if (aes->encrypt("aes_key_invalid", "keyring_aes_test", mode.c_str(),
                   block_size,
                   reinterpret_cast<const unsigned char *>(iv1.c_str()),
                   padding, plaintext, plaintext_length, output_1.get(),
                   ciphertext_length, &ciphertext_length) == false) {
    std::cerr << "Failed negative test for AES-CBC-256" << std::endl;
    return false;
  }
  if (aes->encrypt("secret_key_1", "keyring_aes_test", mode.c_str(), block_size,
                   reinterpret_cast<const unsigned char *>(iv1.c_str()),
                   padding, plaintext, plaintext_length, output_1.get(),
                   ciphertext_length, &ciphertext_length) == false) {
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

  if (aes->decrypt("aes_key_invalid", "keyring_aes_test", mode.c_str(),
                   block_size,
                   reinterpret_cast<const unsigned char *>(iv1.c_str()),
                   padding, output_1.get(), ciphertext_length, output_2.get(),
                   decrypted_length, &decrypted_length) == false) {
    std::cerr << "Failed negative test for AES-CBC-256" << std::endl;
    return false;
  }

  if (aes->decrypt("secret_key_1", "keyring_aes_test", mode.c_str(), block_size,
                   reinterpret_cast<const unsigned char *>(iv1.c_str()),
                   padding, output_1.get(), ciphertext_length, output_2.get(),
                   decrypted_length, &decrypted_length) == false) {
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
