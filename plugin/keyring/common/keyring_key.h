/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef KEYRING_KEY_INCLUDED
#define KEYRING_KEY_INCLUDED

#include <my_global.h>
#include "i_keyring_key.h"
#include "keyring_memory.h"
#include <boost/move/unique_ptr.hpp>

namespace keyring {

struct Key : IKey
{
  Key(const char *a_key_id, const char *a_key_type, const char *a_user_id,
      const void *a_key, size_t a_key_len);
  Key(const Key& other);
  Key(IKey *other);
  Key();

  ~Key();

  my_bool load_from_buffer(uchar* buffer, size_t *buffer_position,
                           size_t input_buffer_size);
  void store_in_buffer(uchar* buffer, size_t *buffer_position) const;
  std::string* get_key_signature() const;
  std::string* get_key_type();
  std::string* get_key_id();
  std::string* get_user_id();
  uchar* get_key_data();
  size_t get_key_data_size();
  size_t get_key_pod_size() const;
  uchar* release_key_data();
  void xor_data();
  void set_key_data(uchar *key_data, size_t key_data_size);
  void set_key_type(const std::string *key_type);
  my_bool is_key_type_valid();
  my_bool is_key_id_valid();
  my_bool is_key_valid();
  my_bool is_key_length_valid();

private:
  void init(const char *a_key_id, const char *a_key_type, const char *a_user_id,
            const void *a_key, size_t a_key_len);

  void clear_key_data();
  void create_key_signature() const;
  my_bool load_string_from_buffer(const uchar *buffer, size_t *buffer_position,
                                  size_t key_pod_size, std::string *string,
                                  size_t string_length);
  inline void store_field_length(uchar *buffer, size_t *buffer_position,
                                 size_t length) const;
  inline void store_field(uchar *buffer, size_t *buffer_position,
                          const char *field, size_t field_length) const;
  my_bool load_field_size(const uchar *buffer, size_t *buffer_position,
                          size_t key_pod_size, size_t *field_length);
protected:
  std::string key_id;
  std::string key_type;
  std::string user_id;
  boost::movelib::unique_ptr<uchar[]> key;
  size_t key_len;
  mutable std::string key_signature;
};

} //namespace keyring

#endif //KEYRING_KEY_INCLUDED
