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

#ifndef MYSQL_I_KEY_H
#define MYSQL_I_KEY_H

#include <string>
#include "keyring_memory.h"

namespace keyring {

struct IKey : public Keyring_alloc
{
  //key_signature:= key_id || user_id
  virtual std::string* get_key_signature() const= 0;
  virtual std::string* get_key_type()= 0;
  virtual std::string* get_key_id()= 0;
  virtual std::string* get_user_id()= 0;
  virtual uchar* get_key_data()= 0;
  virtual size_t get_key_data_size()= 0;
  virtual size_t get_key_pod_size() const = 0;
  virtual uchar* release_key_data()= 0;
  virtual void xor_data()= 0;
  virtual void set_key_data(uchar *key_data, size_t key_data_size)= 0;
  virtual void set_key_type(const std::string *key_type)= 0;
  virtual my_bool load_from_buffer(uchar* buffer, size_t *buffer_position,
                                   size_t input_buffer_size)= 0;
  virtual void store_in_buffer(uchar* buffer, size_t *buffer_position) const = 0;
  virtual my_bool is_key_type_valid()= 0;
  virtual my_bool is_key_id_valid()= 0;
  virtual my_bool is_key_valid()= 0;
  virtual my_bool is_key_length_valid()= 0;

  virtual ~IKey() {}
};

}//namespace keyring
#endif //MYSQL_I_KEY_H
