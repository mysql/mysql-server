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

#ifndef KEYRING_KEY_INCLUDED
#define KEYRING_KEY_INCLUDED

#include <memory>

#include "my_inttypes.h"
#include "plugin/keyring/common/i_keyring_key.h"
#include "plugin/keyring/common/keyring_memory.h"

namespace keyring {

struct Key : IKey {
  Key(const char *a_key_id, const char *a_key_type, const char *a_user_id,
      const void *a_key, size_t a_key_len);
  Key(const Key &other);
  Key(IKey *other);
  Key();

  ~Key() override;

  bool load_from_buffer(uchar *buffer, size_t *number_of_bytes_read_from_buffer,
                        size_t input_buffer_size) override;
  void store_in_buffer(uchar *buffer, size_t *buffer_position) const override;
  std::string *get_key_signature() const override;
  std::string *get_key_type_as_string() override;
  Key_type get_key_type() const override;
  std::string *get_key_id() override;
  std::string *get_user_id() override;
  uchar *get_key_data() override;
  size_t get_key_data_size() override;
  size_t get_key_pod_size() const override;
  uchar *release_key_data() override;
  void xor_data() override;
  void set_key_data(uchar *key_data, size_t key_data_size) override;
  void set_key_type(const std::string *key_type) override;
  bool is_key_type_valid() override;
  bool is_key_id_valid() override;
  bool is_key_valid() override;
  bool is_key_length_valid() override;

 protected:
  void set_key_type_enum(const std::string *key_type) override;

 private:
  void init(const char *a_key_id, const char *a_key_type, const char *a_user_id,
            const void *a_key, size_t a_key_len);

  void clear_key_data();
  void create_key_signature() const;
  bool load_string_from_buffer(const uchar *buffer, size_t *buffer_position,
                               size_t key_pod_size, std::string *string,
                               size_t string_length);
  inline void store_field_length(uchar *buffer, size_t *buffer_position,
                                 size_t length) const;
  inline void store_field(uchar *buffer, size_t *buffer_position,
                          const char *field, size_t field_length) const;
  bool load_field_size(const uchar *buffer, size_t *buffer_position,
                       size_t key_pod_size, size_t *field_length);

 protected:
  std::string key_id;
  std::string key_type;
  std::string user_id;
  std::unique_ptr<uchar[]> key;
  size_t key_len;
  mutable std::string key_signature;
  Key_type key_type_enum;
};

}  // namespace keyring

#endif  // KEYRING_KEY_INCLUDED
