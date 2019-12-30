/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "keyring/master_key_file.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

#include "keyring/keyring_memory.h"  // decryption_error
#include "my_aes.h"
#include "mysql/harness/filesystem.h"

namespace mysql_harness {
static const std::array<char, 5> kMasterKeyFileSignature = {'M', 'R', 'K', 'F',
                                                            '\0'};
constexpr auto kAesMode = my_aes_256_cbc;
static constexpr unsigned char kAesIv[] = {0x39, 0x62, 0x9f, 0x52, 0x7f, 0x76,
                                           0x9a, 0xae, 0xcd, 0xca, 0xf7, 0x04,
                                           0x65, 0x8e, 0x5d, 0x88};

void MasterKeyFile::load() {
  if (Path(path_).is_directory())
    throw invalid_master_keyfile(path_ + " is a directory");

  std::ifstream f;
  f.open(path_, std::ios_base::binary | std::ios_base::in);
  if (f.fail()) {
    throw std::system_error(std::error_code(errno, std::generic_category()),
                            "Can't open file '" + path_ + "'");
  }
  // Verify keyring file's access permissions.
  try {
    // throws std::system_error if permissions are not supported by OS and/or
    // filesystem throws std::runtime_error on bad permissions or error in
    // retrieval file not existing is ok
    check_file_access_rights(path_);

  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, ignore it
#endif
      throw;
  }

  f.seekg(0, f.end);
  std::size_t file_size = static_cast<std::size_t>(f.tellg());
  f.seekg(0, f.beg);

  std::array<char, kMasterKeyFileSignature.size()> buf;
  f.read(buf.data(), buf.size());
  if (f.fail() || (buf != kMasterKeyFileSignature)) {
    throw invalid_master_keyfile("Master key file '" + path_ +
                                 "' has invalid file signature");
  }
  file_size -= buf.size();

  entries_.clear();
  try {
    while (!f.eof()) {
      uint32_t length;
      f.read(reinterpret_cast<char *>(&length), sizeof(length));
      if (f.eof()) {
        break;
      }
      if (f.fail()) {
        throw std::runtime_error("Invalid master-key-file '" + path_ +
                                 "': length-read");
      }
      file_size -= sizeof(length);

      if (length > file_size) {
        throw std::runtime_error("Invalid master-key-file '" + path_ +
                                 "': field-length " + std::to_string(length) +
                                 " bytes, but only " +
                                 std::to_string(file_size) + " bytes left");
      }

      std::string data;
      data.resize(length);
      f.read(&data[0], static_cast<std::streamsize>(data.size()));
      if (f.fail()) {
        throw std::runtime_error("Invalid master-key-file '" + path_ +
                                 "': data-read");
      }
      auto nul_pos = data.find('\0');
      if (nul_pos == data.npos) {
        throw std::runtime_error("Invalid master-key-file '" + path_ +
                                 "': file-sep");
      }

      file_size -= data.size();

      entries_.emplace_back(data.substr(0, nul_pos), data.substr(nul_pos + 1));
    }
  } catch (const std::exception &e) {
    throw std::runtime_error("Error reading from master key file '" + path_ +
                             "': " + e.what());
  }
  f.close();
}

void MasterKeyFile::save() {
  std::ofstream f;
  f.open(path_,
         std::ios_base::binary | std::ios_base::trunc | std::ios_base::out);
  if (f.fail()) {
    throw std::system_error(errno, std::generic_category(),
                            "Could not open master key file " + path_);
  }
  try {
    try {
      make_file_private(path_);
    } catch (const std::system_error &e) {
#ifdef _WIN32
      if (e.code() !=
          std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
      // if the filesystem can't set permissions, the test later would fail
#endif
        throw;
    }
  } catch (std::exception &e) {
    throw std::runtime_error("Could not set permissions of master key file " +
                             path_ + ": " + e.what());
  }
  f.write(kMasterKeyFileSignature.data(), kMasterKeyFileSignature.size());
  for (auto &entry : entries_) {
    uint32_t length =
        static_cast<uint32_t>(entry.first.length() + entry.second.length() + 1);
    f.write(reinterpret_cast<char *>(&length), sizeof(length));
    // write name of the entry
    f.write(entry.first.data(),
            static_cast<std::streamsize>(entry.first.length() + 1));
    // write encrypted entry data
    f.write(entry.second.data(),
            static_cast<std::streamsize>(entry.second.length()));
  }
  f.close();
}

void MasterKeyFile::add(const std::string &id, const std::string &value,
                        const std::string &key) {
  auto aes_buffer_size =
      my_aes_get_size(static_cast<uint32_t>(value.length()), my_aes_256_cbc);
  std::vector<char> aes_buffer(static_cast<size_t>(aes_buffer_size));

  auto encrypted_size = my_aes_encrypt(
      reinterpret_cast<const unsigned char *>(value.data()),
      static_cast<uint32_t>(value.length()),
      reinterpret_cast<unsigned char *>(aes_buffer.data()),
      reinterpret_cast<const unsigned char *>(key.data()),
      static_cast<uint32_t>(key.length()), my_aes_256_cbc, kAesIv);
  if (encrypted_size < 0) {
    throw std::runtime_error("Could not encrypt master key data");
  }
  aes_buffer.resize(static_cast<std::size_t>(encrypted_size));

  add_encrypted(id, std::string(&aes_buffer[0], aes_buffer.size()));
}

void MasterKeyFile::add_encrypted(const std::string &id,
                                  const std::string &buf) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&id](const auto &v) { return v.first == id; });
  if (it != entries_.end()) {
    // found ...
    throw std::invalid_argument("id must be unique");
  }
  entries_.push_back(std::make_pair(id, buf));
}

std::string MasterKeyFile::get_encrypted(const std::string &id) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&id](const auto &v) { return v.first == id; });

  if (it == entries_.end()) {
    throw std::out_of_range("id not found");
  }

  return it->second;
}

std::string MasterKeyFile::get(const std::string &id,
                               const std::string &key) const {
  std::string encrypted;
  try {
    encrypted = get_encrypted(id);
  } catch (const std::out_of_range & /* e */) {
    return "";
  }

  std::vector<char> decrypted_buffer(encrypted.size());

  auto decrypted_size =
      my_aes_decrypt(reinterpret_cast<const unsigned char *>(encrypted.data()),
                     static_cast<uint32_t>(encrypted.length()),
                     reinterpret_cast<unsigned char *>(decrypted_buffer.data()),
                     reinterpret_cast<const unsigned char *>(key.data()),
                     static_cast<uint32_t>(key.length()), kAesMode, kAesIv);

  if (decrypted_size < 0) throw decryption_error("Decryption failed.");

  // std::string() wants an 'unsigned ...', but my_aes_decript gives an
  // signed int. Due to the use of 'auto', we don't know the target type
  // at static_cast<> time and have to let the compiler do the work for us
  // and let it figure out the right unsigned type at compile time.
  return std::string(
      &decrypted_buffer[0],
      static_cast<std::make_unsigned<decltype(decrypted_size)>::type>(
          decrypted_size));
}

bool MasterKeyFile::remove(const std::string &id) {
  bool changed{false};
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->first == id) {
      it = entries_.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }

  return changed;
}
}  // namespace mysql_harness
