/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "http_auth_backend.h"

#include <algorithm>
#include <fstream>
#include <istream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

#include "digest.h"
#include "http_auth_error.h"
#include "kdf_pbkdf2.h"
#include "kdf_sha_crypt.h"

std::pair<std::error_code, struct stat> FileMeta::stat(
    const std::string &filename) {
  struct stat st;

  if (-1 == ::stat(filename.c_str(), &st)) {
#undef STAT_FUNC
    // errno on windows too
    return {std::error_code(errno, std::system_category()), {}};
  } else {
    return {{}, st};
  }
}

static bool st_mtime_eq(const struct stat &a, const struct stat &b) {
#if defined(__linux__) || defined(__FreeBSD__)
  // .st_mtim is timespec
  return (a.st_mtim.tv_sec == b.st_mtim.tv_sec) &&
         (a.st_mtim.tv_nsec == b.st_mtim.tv_nsec);
#elif defined(__APPLE__)
  return (a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec) &&
         (a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec);
#else
  // windows, solaris
  return a.st_mtime == b.st_mtime;
#endif
}

bool FileModified::operator==(const FileModified &b) {
  // ignores atime, ctime, dev and rdev
  return (meta_.res.first == b.meta_.res.first) &&
         (meta_.res.second.st_size == b.meta_.res.second.st_size) &&
         (meta_.res.second.st_mode == b.meta_.res.second.st_mode) &&
         (meta_.res.second.st_uid == b.meta_.res.second.st_uid) &&
         (meta_.res.second.st_gid == b.meta_.res.second.st_gid) &&
         st_mtime_eq(meta_.res.second, b.meta_.res.second);
}

std::error_code HttpAuthBackendHtpasswd::from_file(
    const std::string &filename) {
  is_file_ = true;
  filename_ = filename;

  {
    FileModified cur_meta{FileMeta(filename)};
    if (cur_meta == file_meta_) {
      // not changed
      return {};
    } else {
      // update cache
      file_meta_ = cur_meta;
    }
  }

  std::fstream f(filename, std::ios::in);
  if (!f.is_open()) {
    return std::error_code(errno, std::system_category());
  }

  if (auto ec = from_stream_(f)) {
    return ec;
  }

  return {};
}

std::error_code HttpAuthBackendHtpasswd::from_stream(std::istream &is) {
  is_file_ = false;

  return from_stream_(is);
}

std::error_code HttpAuthBackendHtpasswd::from_stream_(std::istream &is) {
  decltype(credentials_) creds;

  // split line by line
  for (std::string line; std::getline(is, line);) {
    // split line by colon
    auto sep_it = std::find(line.begin(), line.end(), ':');

    // fail if colon isn't found
    if (sep_it == line.end()) return make_error_code(McfErrc::kParseError);

    // forbid empty username
    if (line.begin() == sep_it) return make_error_code(McfErrc::kParseError);
    // forbid empty auth-part
    if (line.end() == sep_it + 1) return make_error_code(McfErrc::kParseError);

    // username : data
    std::string username{line.begin(), sep_it};
    std::string auth_data{sep_it + 1, line.end()};

    // fails, if username is already in the map.
    creds.insert({username, auth_data});
  }

  // assign creds only after no parse-error
  credentials_ = creds;
  credentials_cache_.clear();

  return {};
}

void HttpAuthBackendHtpasswd::to_stream(std::ostream &os) {
  for (auto &kv : credentials_) {
    std::string line;

    line += kv.first;
    line += ":";
    line += kv.second;

    os << line << std::string("\n");
  }
}

std::error_code HttpAuthBackendHtpasswd::authenticate(
    const std::string &username, const std::string &password) {
  if (is_file_) {
    // if file changed, reload it
    if (auto ec = from_file(filename_)) {
      return ec;
    }
  }

  const auto it = credentials_.find(username);
  if (it == credentials_.end()) {
    return make_error_code(McfErrc::kUserNotFound);
  }

  auto mcf_line = it->second;

  if (mcf_line.size() < 1) return make_error_code(McfErrc::kParseError);
  if (mcf_line[0] != '$') return make_error_code(McfErrc::kParseError);

  auto mcf_id_it = std::find(mcf_line.begin() + 1, mcf_line.end(), '$');
  // no terminating $ found
  if (mcf_id_it == mcf_line.end()) return make_error_code(McfErrc::kParseError);
  std::string mcf_id(mcf_line.begin() + 1, mcf_id_it);

  try {
    std::string derived;
    std::string hash = hash_password(password);
    std::error_code validate_error;

    const auto cacheIt = credentials_cache_.find(username);

    if (cacheIt != credentials_cache_.end() && cacheIt->second == hash) {
      return {};
    } else if (ShaCryptMcfAdaptor::supports_mcf_id(mcf_id)) {
      validate_error = ShaCryptMcfAdaptor::validate(mcf_line, password);
    } else if (Pbkdf2McfAdaptor::supports_mcf_id(mcf_id)) {
      validate_error = Pbkdf2McfAdaptor::validate(mcf_line, password);
    } else
      return make_error_code(McfErrc::kUnknownScheme);

    if (!validate_error) credentials_cache_[username] = hash;

    return validate_error;
  } catch (const std::exception &) {
    // treat all exceptions as parse-errors
    return make_error_code(McfErrc::kParseError);
  }
}

std::string HttpAuthBackendHtpasswd::hash_password(
    const std::string &password) {
  static const uint32_t digest_size = Digest::digest_size(Digest::Type::Sha256);
  std::string result(digest_size, '\0');
  Digest sha256(Digest::Type::Sha256);

  sha256.update(password);
  sha256.finalize(result);

  sha256.reinit();

  sha256.update(result);
  sha256.finalize(result);

  return result;
}

HttpAuthBackend::~HttpAuthBackend() = default;
