/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_HTTP_AUTH_BACKEND_INCLUDED
#define ROUTER_HTTP_AUTH_BACKEND_INCLUDED

#include <map>
#include <string>
#include <system_error>

#include <sys/stat.h>
#include <sys/types.h>

/**
 * Base class of all AuthBackends.
 */
class HttpAuthBackend {
 public:
  /**
   * authentication username with authdata against backend.
   */
  virtual std::error_code authenticate(const std::string &username,
                                       const std::string &authdata) = 0;

  /**
   * destructor.
   */
  virtual ~HttpAuthBackend() = 0;
};

struct FileMeta {
  using stat_res = std::pair<std::error_code, struct stat>;

  FileMeta() : res{} {}
  FileMeta(const std::string &filename) : res{stat(filename)} {}

  /**
   * calls the systems stat().
   */
  static stat_res stat(const std::string &filename);

  stat_res res;
};

/**
 * check if a file was modified.
 */
class FileModified {
 public:
  FileModified() {}
  explicit FileModified(const FileMeta &meta) : meta_(meta) {}

  /**
   * check if two FileModified's are equal.
   */
  bool operator==(const FileModified &b);

 private:
  FileMeta meta_;
};

/**
 * hashed key store.
 *
 * - each line contains username and auth-data, seperated by colon
 * - auth-data should be based on PHC
 *
 * PHC
 * :  `$<id>[$<param>=<value>(,<param>=<value>)*][$<salt>[$<hash>]]`
 *
 *   id          | name          | supported
 * -------------:|---------------|-----
 *             1 | md5_crypt     | never
 *             2 | bcrypt        | never
 *            2a | bcrypt        | no
 *            2b | bcrypt        | no
 *             5 | sha256_crypt  | yes
 *             6 | sha512_crypt  | yes
 * pbkdf2-sha256 | pkbdf2_sha256 | no
 * pbkdf2-sha512 | pkbdf2_sha512 | no
 *        scrypt | scrypt        | no
 *        argon2 | argon2        | no
 *        bcrypt | bcrypt        | no
 *
 * @see
 * https://github.com/P-H-C/phc-string-format/blob/master/phc-sf-spec.md
 */
class HttpAuthBackendHtpasswd : public HttpAuthBackend {
 public:
  using key_type = std::string;
  using value_type = std::string;

  /**
   * iterator
   */
  using iterator = std::map<key_type, value_type>::iterator;

  /**
   * const iterator
   */
  using const_iterator = std::map<key_type, value_type>::const_iterator;

  /**
   * replace cache with content from file.
   */
  std::error_code from_file(const std::string &filename);

  /**
   * replace cache with content of input-stream.
   *
   * @returns error-code
   * @retval false when no error happened
   */
  std::error_code from_stream(std::istream &is);

  /**
   * write cache content to output-stream.
   */
  void to_stream(std::ostream &os);

  /**
   * remove username from credential cache.
   */
  size_t erase(const key_type &username) {
    return credentials_.erase(username);
  }

  /**
   * set username and password in cache.
   *
   * if username exists in cache, overwrite entry with password,
   * otherwise create new entry for username
   */
  void set(const key_type &username, const value_type &authdata) {
    auto res = credentials_.insert({username, authdata});
    if (!res.second) {
      auto elem_it = res.first;

      elem_it->second = authdata;
    }
  }

  /**
   * find username in cache.
   */
  iterator find(const key_type &username) {
    return credentials_.find(username);
  }

  /**
   * find username in cache.
   */
  const_iterator find(const key_type &username) const {
    return credentials_.find(username);
  }

  /**
   * end iterator.
   */
  iterator end() noexcept { return credentials_.end(); }

  /**
   * const end iterator.
   */
  const_iterator end() const noexcept { return credentials_.end(); }

  /**
   * begin iterator.
   */
  iterator begin() noexcept { return credentials_.begin(); }

  /**
   * const begin iterator.
   */
  const_iterator begin() const noexcept { return credentials_.begin(); }

  /**
   * validate user and authdata against backend.
   *
   * @returns error
   * @retval false no authentication error
   */
  std::error_code authenticate(const key_type &username,
                               const value_type &authdata) override;

 private:
  std::error_code from_stream_(std::istream &is);
  bool is_file_{false};
  std::string filename_;
  FileModified file_meta_;

  std::map<key_type, value_type> credentials_;
};

#endif
