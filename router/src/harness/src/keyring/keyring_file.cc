/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "keyring/keyring_file.h"

#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "mysql/harness/filesystem.h"

static constexpr auto kKeyringFileSignature =
    std::to_array<char>({'M', 'R', 'K', 'R'});

namespace mysql_harness {

void KeyringFile::set_header(const std::string &data) { header_ = data; }

void KeyringFile::save(const std::string &file_name,
                       const std::string &key) const {
  if (key.empty()) {
    throw std::runtime_error("Keyring encryption key must not be blank");
  }
  // Serialize keyring.
  auto buffer = serialize(key);

  // Save keyring data to file.
  std::ofstream file;

  file.exceptions(std::ofstream::failbit | std::ofstream::badbit);

#ifndef _WIN32
  try {
    file.open(file_name, std::ofstream::out | std::ofstream::binary |
                             std::ofstream::trunc);
  } catch (const std::exception &e) {
    throw std::system_error(
        errno, std::generic_category(),
        "Failed to open keyring file for writing: " + file_name);
  }
#else
  // For Microsoft Windows, on repeated saving of files (like our unit tests)
  // the file opening sometimes fails with "Access Denied", since it works fine
  // when disabling indexing of file contents for the whole folder we assume the
  // indexer is not releasing the file fast enough. So here we simply retry the
  // opening of the file.
  int retries = 5;
  do {
    try {
      file.open(file_name, std::ofstream::out | std::ofstream::binary |
                               std::ofstream::trunc);
      break;
    } catch (const std::exception &) {
      if (retries-- > 0) {
        Sleep(100);
        continue;
      }
      throw std::system_error(
          errno, std::generic_category(),
          "Failed to open keyring file for writing: " + file_name);
    }
  } while (true);
#endif

  try {
    make_file_private(file_name);
  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, ignore it
#endif
      throw;
  }

  try {
    // write signature
    file.write(kKeyringFileSignature.data(), kKeyringFileSignature.size());
    // write header
    uint32_t header_size = static_cast<uint32_t>(header_.size());
    file.write(reinterpret_cast<char *>(&header_size), sizeof(header_size));
    if (header_.size() > 0)
      file.write(header_.data(), static_cast<std::streamsize>(header_.size()));
    // write data
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Failed to save keyring file: ") +
                             e.what());
  }
}

static void verify_file_permissions(const std::string &file_name) {
  // Verify keyring file's access permissions.
  try {
    // throws std::system_error if permissions are not supported by OS and/or
    // filesystem throws std::runtime_error on bad permissions or error in
    // retrieval file not existing is ok
    check_file_access_rights(file_name);

  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, ignore it
#endif
      throw;
  }
}

void KeyringFile::load(const std::string &file_name, const std::string &key) {
  // throws std::runtime_error with appropriate error message on verification
  // failure
  verify_file_permissions(file_name);

  // Read keyring data from file.
  std::ifstream file;

  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(file_name,
              std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
  } catch (const std::exception &) {
    throw std::system_error(
        errno, std::generic_category(),
        std::string("Failed to open keyring file: ") + file_name);
  }

  file.seekg(0, file.end);
  std::size_t file_size = static_cast<std::size_t>(file.tellg());

  // read and check signature
  file.seekg(0, file.beg);
  {
    std::array<char, kKeyringFileSignature.size()> sig;
    try {
      file.read(sig.data(), sig.size());
    } catch (const std::ios_base::failure &) {
      throw std::runtime_error("Failure reading contents of keyring file " +
                               file_name);
    }
    if (kKeyringFileSignature != sig) {
      throw std::runtime_error("Invalid data found in keyring file " +
                               file_name);
    }
  }
  // read header, if there's one
  {
    uint32_t header_size;
    file.read(reinterpret_cast<char *>(&header_size), sizeof(header_size));
    if (header_size > 0) {
      if (header_size >
          file_size - kKeyringFileSignature.size() - sizeof(header_size)) {
        throw std::runtime_error("Invalid data found in keyring file " +
                                 file_name);
      }
      header_.resize(header_size);
      file.read(&header_[0], static_cast<std::streamsize>(header_.size()));
    }
  }

  std::size_t data_size = file_size - static_cast<std::size_t>(file.tellg());

  std::vector<char> buffer(static_cast<std::size_t>(data_size));
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

  // Parse keyring data.
  parse(key, buffer.data(), buffer.size());
}

std::string KeyringFile::read_header(const std::string &file_name) {
  // throws std::runtime_error with appropriate error message on verification
  // failure
  verify_file_permissions(file_name);

  // Read keyring data from file.
  std::ifstream file;

  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(file_name,
              std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
  } catch (const std::exception &) {
    throw std::system_error(
        errno, std::generic_category(),
        std::string("Failed to open keyring file: ") + file_name);
  }

  // assumes the file doesn't change while we read it

  std::size_t file_size = static_cast<std::size_t>(file.tellg());
  if (file_size < kKeyringFileSignature.size() + 4) {
    throw std::runtime_error("reading file-header of '" + file_name +
                             "' failed: File is too small");
  }

  file.seekg(0);
  // read and check signature
  {
    std::array<char, kKeyringFileSignature.size()> sig;
    try {
      file.read(sig.data(), sig.size());
    } catch (const std::exception &e) {
      throw std::runtime_error("reading file-header of '" + file_name +
                               "' failed: " + e.what());
    }
    if (sig != kKeyringFileSignature) {
      throw std::runtime_error("Invalid data found in keyring file " +
                               file_name);
    }
  }
  // read header, if there's one
  std::string header;
  {
    uint32_t header_size;
    file.read(reinterpret_cast<char *>(&header_size), sizeof(header_size));
    if (header_size > 0) {
      if (header_size >
          file_size - kKeyringFileSignature.size() - sizeof(header_size)) {
        throw std::runtime_error("Invalid data found in keyring file " +
                                 file_name);
      }
      header.resize(header_size);
      file.read(&header[0], static_cast<std::streamsize>(header.size()));
    }
  }
  return header;
}
}  // namespace mysql_harness
