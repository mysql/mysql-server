/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <cstdio> /* std::remove */
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "components/keyrings/common/data_file/reader.h"
#include "components/keyrings/common/data_file/writer.h"
#include "components/keyrings/common/encryption/aes.h"

namespace aes_encryption_unittest {

using keyring_common::aes_encryption::aes_decrypt;
using keyring_common::aes_encryption::aes_encrypt;
using keyring_common::aes_encryption::aes_return_status;
using keyring_common::aes_encryption::get_ciphertext_size;
using keyring_common::aes_encryption::Keyring_aes_opmode;
using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;

class AESEncryption_test : public ::testing::Test {};

TEST_F(AESEncryption_test, CiphertextsizeTest) {
  size_t input_size = 1024;
  size_t output_size =
      get_ciphertext_size(input_size, Keyring_aes_opmode::keyring_aes_256_cbc);
  EXPECT_TRUE(output_size > input_size);

  output_size =
      get_ciphertext_size(input_size, Keyring_aes_opmode::keyring_aes_256_ecb);
  EXPECT_TRUE(output_size > input_size);

  output_size =
      get_ciphertext_size(input_size, Keyring_aes_opmode::keyring_aes_256_cbc);
  EXPECT_TRUE(output_size > input_size);

  output_size =
      get_ciphertext_size(input_size, Keyring_aes_opmode::keyring_aes_256_cfb1);
  EXPECT_TRUE(output_size == input_size);

  output_size =
      get_ciphertext_size(input_size, Keyring_aes_opmode::keyring_aes_256_cfb8);
  EXPECT_TRUE(output_size == input_size);

  output_size = get_ciphertext_size(input_size,
                                    Keyring_aes_opmode::keyring_aes_256_cfb128);
  EXPECT_TRUE(output_size == input_size);

  output_size =
      get_ciphertext_size(input_size, Keyring_aes_opmode::keyring_aes_256_ofb);
  EXPECT_TRUE(output_size == input_size);
}

TEST_F(AESEncryption_test, EncryptDecryptTest) {
  std::string source_1("Quick brown fox jumped over the lazy dog");
  std::string pass1("Ac32=x133/#@$R");
  std::string iv1("abcdefgh12345678");
  std::unique_ptr<unsigned char[]> output_1, output_1_1;
  size_t output_1_size, output_1_1_size;
  output_1 = std::make_unique<unsigned char[]>(get_ciphertext_size(
      source_1.length(), Keyring_aes_opmode::keyring_aes_256_cbc));
  EXPECT_TRUE(output_1.get() != nullptr);
  aes_return_status source_1_encryption_status =
      aes_encrypt(reinterpret_cast<const unsigned char *>(source_1.c_str()),
                  static_cast<unsigned int>(source_1.length()), output_1.get(),
                  reinterpret_cast<const unsigned char *>(pass1.c_str()),
                  static_cast<unsigned int>(pass1.length()),
                  Keyring_aes_opmode::keyring_aes_256_cbc,
                  reinterpret_cast<const unsigned char *>(iv1.c_str()), true,
                  &output_1_size);
  EXPECT_TRUE(source_1_encryption_status == aes_return_status::AES_OP_OK);

  output_1_1 = std::make_unique<unsigned char[]>(get_ciphertext_size(
      source_1.length(), Keyring_aes_opmode::keyring_aes_256_cbc));
  EXPECT_TRUE(output_1_1.get() != nullptr);
  aes_return_status source_1_decryption_status = aes_decrypt(
      output_1.get(),
      static_cast<unsigned int>(get_ciphertext_size(
          source_1.length(), Keyring_aes_opmode::keyring_aes_256_cbc)),
      output_1_1.get(), reinterpret_cast<const unsigned char *>(pass1.c_str()),
      static_cast<unsigned int>(pass1.length()),
      Keyring_aes_opmode::keyring_aes_256_cbc,
      reinterpret_cast<const unsigned char *>(iv1.c_str()), true,
      &output_1_1_size);
  EXPECT_TRUE(source_1_decryption_status == aes_return_status::AES_OP_OK);

  std::string decrypted_source_1(reinterpret_cast<char *>(output_1_1.get()),
                                 output_1_1_size);

  EXPECT_TRUE(decrypted_source_1 == source_1);

  std::string source_2(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      "eiusmod "
      "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad "
      "minim "
      "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
      "ex ea "
      "commodo consequat. Duis aute irure dolor in reprehenderit in "
      "voluptate "
      "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
      "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
      "mollit anim id est laborum."
      " Curabitur pretium tincidunt lacus. Nulla gravida orci a odio. "
      "Nullam "
      "varius, turpis et commodo pharetra, est eros bibendum elit, nec "
      "luctus "
      "magna felis sollicitudin mauris. Integer in mauris eu nibh euismod "
      "gravida. Duis ac tellus et risus vulputate vehicula. Donec lobortis "
      "risus a elit. Etiam tempor. Ut ullamcorper, ligula eu tempor "
      "congue, "
      "eros est euismod turpis, id tincidunt sapien risus a quam. Maecenas "
      "fermentum consequat mi. Donec fermentum. Pellentesque malesuada "
      "nulla a "
      "mi. Duis sapien sem, aliquet nec, commodo eget, consequat quis, "
      "neque. "
      "Aliquam faucibus, elit ut dictum aliquet, felis nisl adipiscing "
      "sapien, "
      "sed malesuada diam lacus eget erat. Cras mollis scelerisque nunc. "
      "Nullam "
      "arcu. Aliquam consequat. Curabitur augue lorem, dapibus quis, "
      "laoreet "
      "et, pretium ac, nisi. Aenean magna nisl, mollis quis, molestie eu, "
      "feugiat in, orci. In hac habitasse platea dictumst.");
  std::string password_2(
      "Aliquam faucibus, elit ut dictum aliquet, felis nisl");
  std::string iv2("87654321hgfedcba");
  std::unique_ptr<unsigned char[]> output, output_2_2;
  size_t output_2_size, output_2_2_size;
  output = std::make_unique<unsigned char[]>(get_ciphertext_size(
      source_2.length(), Keyring_aes_opmode::keyring_aes_256_cbc));
  EXPECT_TRUE(output.get() != nullptr);
  aes_return_status source_encryption_status =
      aes_encrypt(reinterpret_cast<const unsigned char *>(source_2.c_str()),
                  static_cast<unsigned int>(source_2.length()), output.get(),
                  reinterpret_cast<const unsigned char *>(password_2.c_str()),
                  static_cast<unsigned int>(password_2.length()),
                  Keyring_aes_opmode::keyring_aes_256_cbc,
                  reinterpret_cast<const unsigned char *>(iv2.c_str()), true,
                  &output_2_size);
  EXPECT_TRUE(source_encryption_status == aes_return_status::AES_OP_OK);

  output_2_2 = std::make_unique<unsigned char[]>(get_ciphertext_size(
      source_2.length(), Keyring_aes_opmode::keyring_aes_256_cbc));
  EXPECT_TRUE(output_2_2.get() != nullptr);
  aes_return_status source_decryption_status = aes_decrypt(
      output.get(),
      static_cast<unsigned int>(get_ciphertext_size(
          source_2.length(), Keyring_aes_opmode::keyring_aes_256_cbc)),
      output_2_2.get(),
      reinterpret_cast<const unsigned char *>(password_2.c_str()),
      static_cast<unsigned int>(password_2.length()),
      Keyring_aes_opmode::keyring_aes_256_cbc,
      reinterpret_cast<const unsigned char *>(iv2.c_str()), true,
      &output_2_2_size);
  EXPECT_TRUE(source_decryption_status == aes_return_status::AES_OP_OK);

  std::string decrypted_source(reinterpret_cast<char *>(output_2_2.get()),
                               output_2_2_size);

  EXPECT_TRUE(decrypted_source == source_2);
}

TEST_F(AESEncryption_test, EncryptDecryptFileTest) {
  /* Encrypt the content */
  std::string source("quick brown fox jumped over the lazy dog");
  std::string password("pass1234");
  std::string iv("87654321hgfedcba");
  std::unique_ptr<unsigned char[]> output, output_2;
  size_t output_size, output_2_size;
  output = std::make_unique<unsigned char[]>(get_ciphertext_size(
      source.length(), Keyring_aes_opmode::keyring_aes_256_cbc));
  EXPECT_TRUE(output.get() != nullptr);
  aes_return_status source_encryption_status = aes_encrypt(
      reinterpret_cast<const unsigned char *>(source.c_str()),
      static_cast<unsigned int>(source.length()), output.get(),
      reinterpret_cast<const unsigned char *>(password.c_str()),
      static_cast<unsigned int>(password.length()),
      Keyring_aes_opmode::keyring_aes_256_cbc,
      reinterpret_cast<const unsigned char *>(iv.c_str()), true, &output_size);
  EXPECT_TRUE(source_encryption_status == aes_return_status::AES_OP_OK);

  /* Write the content to file and then read it */
  std::string file_name("encrypt_decrypt_file_test");
  // Content: IV + Encrypted Data
  std::string data(iv.c_str(), iv.length());
  data.append(reinterpret_cast<char *>(output.get()), output_size);
  EXPECT_TRUE(data.length() == (iv.length() + output_size));

  File_writer file_writer(file_name, data);
  ASSERT_TRUE(file_writer.valid());

  std::string read_data;
  File_reader file_reader(file_name, true, read_data);
  ASSERT_TRUE(file_reader.valid());

  ASSERT_TRUE(read_data == data);

  output_2 = std::make_unique<unsigned char[]>(
      get_ciphertext_size(read_data.length() - iv.length(),
                          Keyring_aes_opmode::keyring_aes_256_cbc));
  EXPECT_TRUE(output_2.get() != nullptr);
  aes_return_status source_decryption_status = aes_decrypt(
      reinterpret_cast<const unsigned char *>(read_data.c_str()) + iv.length(),
      static_cast<unsigned int>(read_data.length() - iv.length()),
      output_2.get(), reinterpret_cast<const unsigned char *>(password.c_str()),
      static_cast<unsigned int>(password.length()),
      Keyring_aes_opmode::keyring_aes_256_cbc,
      reinterpret_cast<const unsigned char *>(read_data.c_str()), true,
      &output_2_size);
  EXPECT_TRUE(source_decryption_status == aes_return_status::AES_OP_OK);

  std::string decrypted_source(reinterpret_cast<char *>(output_2.get()),
                               output_2_size);

  EXPECT_TRUE(decrypted_source == source);

  ASSERT_TRUE(std::remove(file_name.c_str()) == 0);
}

}  // namespace aes_encryption_unittest
