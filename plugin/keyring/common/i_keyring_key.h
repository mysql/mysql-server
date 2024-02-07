/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_I_KEY_H
#define MYSQL_I_KEY_H

#include <string>

#include "plugin/keyring/common/keyring_memory.h"

namespace keyring {

constexpr const char *AES = "AES";
constexpr const char *RSA = "RSA";
constexpr const char *DSA = "DSA";
constexpr const char *SECRET = "SECRET";

enum class Operation_type { fetch = 0, store, remove, generate };

enum class Key_type { aes = 0, rsa, dsa, secret, unknown };

struct IKey : public Keyring_alloc {
  // key_signature:= key_id || user_id
  virtual std::string *get_key_signature() const = 0;
  virtual std::string *get_key_type_as_string() = 0;
  virtual Key_type get_key_type() const = 0;
  virtual std::string *get_key_id() = 0;
  virtual std::string *get_user_id() = 0;
  virtual uchar *get_key_data() = 0;
  virtual size_t get_key_data_size() = 0;
  virtual size_t get_key_pod_size() const = 0;
  virtual uchar *release_key_data() = 0;
  virtual void xor_data() = 0;
  virtual void set_key_data(uchar *key_data, size_t key_data_size) = 0;
  virtual void set_key_type(const std::string *key_type) = 0;
  virtual bool load_from_buffer(uchar *buffer, size_t *buffer_position,
                                size_t input_buffer_size) = 0;
  virtual void store_in_buffer(uchar *buffer,
                               size_t *buffer_position) const = 0;
  virtual bool is_key_type_valid() = 0;
  virtual bool is_key_id_valid() = 0;
  virtual bool is_key_valid() = 0;
  virtual bool is_key_length_valid() = 0;

  virtual ~IKey() = default;

 protected:
  virtual void set_key_type_enum(const std::string *key_type) = 0;
};

}  // namespace keyring
#endif  // MYSQL_I_KEY_H
