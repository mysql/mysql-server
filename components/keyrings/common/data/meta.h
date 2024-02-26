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

#ifndef META_INCLUDED
#define META_INCLUDED

#include <string>

namespace keyring_common {
namespace meta {

/**
  Common metadata.
  Usually provided by:
  - consumer of keyring APIs
  - Keyring backend when data is fetched
*/

class Metadata final {
 public:
  Metadata(const std::string key_id, const std::string owner_id);
  Metadata(const char *key_id, const char *owner_id);
  Metadata();

  Metadata(const Metadata &src);

  Metadata(Metadata &&src) noexcept;

  Metadata &operator=(const Metadata &src);

  Metadata &operator=(Metadata &&src) noexcept;

  /** Destructor */
  ~Metadata();

  /** Get key ID */
  const std::string key_id() const;

  /** Get owner info */
  const std::string owner_id() const;

  /** Validity of metadata object */
  bool valid() const;

  /* For unordered map */

  const std::string hash_key() const { return hash_key_; }
  bool operator==(const Metadata &other) const {
    return key_id_ == other.key_id_ && owner_id_ == other.owner_id_;
  }
  struct Hash {
    size_t operator()(const Metadata &metadata) const {
      return std::hash<std::string>()(metadata.hash_key());
    }
  };

 private:
  void create_hash_key();

  /** Consumer specific key id*/
  std::string key_id_;
  /** Owner information */
  std::string owner_id_;
  /** Hash key */
  std::string hash_key_;
  /** Validity of metadata */
  bool valid_{false};
};

}  // namespace meta
}  // namespace keyring_common

#endif  // !META_INCLUDED
