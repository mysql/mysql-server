/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "keyring/keyring_memory.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include <openssl/evp.h>

#include "mysql/harness/tls_cipher.h"

constexpr unsigned char kAesIv[] = {0x39, 0x62, 0x9f, 0x52, 0x7f, 0x76,
                                    0x9a, 0xae, 0xcd, 0xca, 0xf7, 0x04,
                                    0x65, 0x8e, 0x5d, 0x88};

constexpr std::uint32_t kKeyringDataSignature = 0x043d4d0a;

// Writes a raw data to buffer and returns new buffer offset.
// If buffer is null then doesn't write anything, but still returns the
// correct offset.
static std::size_t serialize(char *buffer, std::size_t offset, const void *data,
                             std::size_t data_size) {
  if (buffer) std::memcpy(buffer + offset, data, data_size);

  return offset + data_size;
}

// Writes a std::size_t value to buffer and returns new buffer offset.
// If buffer is null then doesn't write anything, but still returns the
// correct offset.
static std::size_t serialize(char *buffer, std::size_t offset,
                             std::size_t value) {
  auto value_u32 = static_cast<std::uint32_t>(value);

  return serialize(buffer, offset, &value_u32, sizeof(value_u32));
}

// Writes a std::string to buffer (its size and contents) and returns new buffer
// offset. If buffer is null then doesn't write anything, but still
// returns the correct offset.
static std::size_t serialize(char *buffer, std::size_t offset,
                             const std::string &value) {
  offset = serialize(buffer, offset, value.length());

  return serialize(buffer, offset, value.data(), value.length());
}

// Writes entity map to buffer and returns numbers of bytes written.
// If buffer is null then doesn't write anything, but still returns the
// number of bytes that would have been written.
// Warning: Buffer needs to be able to fit the entity map. It is possible to
// make sure what size is required by calling the function with buffer pointer
// set to null.
static std::size_t serialize(
    char *buffer,
    const std::map<std::string, std::map<std::string, std::string>> &entries) {
  // Save keyring file signature.
  auto offset = serialize(buffer, 0, &kKeyringDataSignature,
                          sizeof(kKeyringDataSignature));

  // Save keyring format version.
  offset =
      serialize(buffer, offset, mysql_harness::KeyringMemory::kFormatVersion);

  // Save number of keyring entries.
  offset = serialize(buffer, offset, entries.size());

  // Save entries.
  for (const auto &entry : entries) {
    // Save entry name.
    offset = serialize(buffer, offset, entry.first);

    // Save number of entry attributes.
    offset = serialize(buffer, offset, entry.second.size());

    // Save entry attributes.
    for (const auto &attribute : entry.second) {
      // Save attribute name.
      offset = serialize(buffer, offset, attribute.first);

      // Save attribute value.
      offset = serialize(buffer, offset, attribute.second);
    }
  }

  return offset;
}

// Verifies if buffer has enough space for more data.
static void check_buffer_size(std::size_t data_size, std::size_t buffer_size,
                              std::size_t offset) {
  if (offset + data_size > buffer_size)
    throw std::out_of_range("Keyring input buffer invalid.");
}

// Reads raw data from buffer and returns new buffer offset.
static std::size_t parse(const char *buffer, std::size_t buffer_size,
                         std::size_t offset, void *data,
                         std::size_t data_size) {
  check_buffer_size(data_size, buffer_size, offset);
  std::memcpy(data, buffer + offset, data_size);

  return offset + data_size;
}

// Reads a std::size_t value from buffer and returns new buffer offset.
static std::size_t parse(const char *buffer, std::size_t buffer_size,
                         std::size_t offset, std::size_t &value) {
  std::uint32_t value_u32;

  offset = parse(buffer, buffer_size, offset, &value_u32, sizeof(value_u32));
  value = static_cast<std::size_t>(value_u32);

  return offset;
}

// Reads a std::string value from buffer and returns new buffer offset.
static std::size_t parse(const char *buffer, std::size_t buffer_size,
                         std::size_t offset, std::string &value) {
  std::size_t string_length;
  offset = parse(buffer, buffer_size, offset, string_length);

  check_buffer_size(string_length, buffer_size, offset);
  value.assign(buffer + offset, string_length);
  offset += string_length;

  return offset;
}

// Reads an entity map from a buffer.
static void parse(
    const char *buffer, std::size_t buffer_size,
    std::map<std::string, std::map<std::string, std::string>> &entries) {
  // Parse keyring file signature.
  std::uint32_t keyring_file_signature;
  auto offset = parse(buffer, buffer_size, 0, &keyring_file_signature,
                      sizeof(keyring_file_signature));

  if (keyring_file_signature != kKeyringDataSignature) {
    throw std::runtime_error(
        "Invalid keyring file signature. The file is damaged or decryption key "
        "is invalid.");
  }

  // Parse keyring format version.
  std::size_t keyring_version;
  offset = parse(buffer, buffer_size, offset, keyring_version);

  if (keyring_version != mysql_harness::KeyringMemory::kFormatVersion)
    throw std::runtime_error("Invalid keyring format version.");

  // Parse number of keyring entries.
  std::size_t entry_count;
  offset = parse(buffer, buffer_size, offset, entry_count);

  for (std::size_t entry_idx = 0; entry_idx < entry_count; ++entry_idx) {
    // Parse entry name.
    std::string entry_name;
    offset = parse(buffer, buffer_size, offset, entry_name);

    // Parse number of entry attributes.
    std::size_t attr_count;
    offset = parse(buffer, buffer_size, offset, attr_count);

    for (std::size_t attr_idx = 0; attr_idx < attr_count; ++attr_idx) {
      // Parse attribute name.
      std::string attr_name;
      offset = parse(buffer, buffer_size, offset, attr_name);

      // Parse attribute value.
      std::string attr_value;
      offset = parse(buffer, buffer_size, offset, attr_value);

      entries[entry_name].emplace(attr_name, attr_value);
    }
  }
}

namespace mysql_harness {

void KeyringMemory::store(const std::string &uid, const std::string &attribute,
                          const std::string &value) {
  entries_[uid][attribute] = value;
}

std::string KeyringMemory::fetch(const std::string &uid,
                                 const std::string &attribute) const {
  return entries_.at(uid).at(attribute);
}

bool KeyringMemory::remove(const std::string &uid) {
  return entries_.erase(uid) > 0;
}

bool KeyringMemory::remove_attribute(const std::string &uid,
                                     const std::string &attribute) {
  try {
    return entries_.at(uid).erase(attribute) > 0;
  } catch (std::out_of_range &) {
    // Ignore.
    return false;
  }
}

std::vector<char> KeyringMemory::serialize(const std::string &key) const {
  // Serialize keyring.
  auto buffer_size = ::serialize(nullptr, entries_);
  std::vector<char> buffer(buffer_size);

  ::serialize(buffer.data(), entries_);

  TlsCipher cipher(EVP_aes_256_cbc());

  // Encrypt buffer.
  std::vector<char> aes_buffer(cipher.size(buffer_size));

  auto encrypted_res = cipher.encrypt(
      reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size(),
      reinterpret_cast<uint8_t *>(aes_buffer.data()),
      reinterpret_cast<const uint8_t *>(key.data()), key.length(), kAesIv);

  if (!encrypted_res) {
    throw std::system_error(encrypted_res.error(),
                            "Keyring encryption failed.");
  }

  return aes_buffer;
}

void KeyringMemory::parse(const std::string &key, const char *buffer,
                          std::size_t buffer_size) {
  // Decrypt buffer.
  std::vector<char> decrypted_buffer(buffer_size);

  auto decrypted_res =
      TlsCipher(EVP_aes_256_cbc())
          .decrypt(reinterpret_cast<const uint8_t *>(buffer), buffer_size,
                   reinterpret_cast<uint8_t *>(decrypted_buffer.data()),
                   reinterpret_cast<const uint8_t *>(key.data()), key.length(),
                   kAesIv);

  if (!decrypted_res) {
    throw decryption_error("Keyring decryption failed.");
  }

  // Parse keyring data.
  ::parse(decrypted_buffer.data(), decrypted_res.value(), entries_);
}

}  // namespace mysql_harness
