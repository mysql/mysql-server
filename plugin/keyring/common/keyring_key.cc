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

#include "keyring_key.h"

namespace keyring {

Key::Key()
  : key(NULL)
  , key_len(0)
{}

Key::Key(const char *a_key_id, const char *a_key_type, const char *a_user_id,
    const void *a_key, size_t a_key_len)
{
  init(a_key_id, a_key_type, a_user_id, a_key, a_key_len);
}

Key::Key(const Key& other)
{
  init(other.key_id.c_str(), other.key_type.c_str(), other.user_id.c_str(),
       other.key.get(), other.key_len);
}

Key::Key(IKey *other)
{
  init(other->get_key_id()->c_str(), other->get_key_type()->c_str(),
       other->get_user_id()->c_str(), other->get_key_data(), other->get_key_data_size());
}

void Key::init(const char *a_key_id, const char *a_key_type, const char *a_user_id,
               const void *a_key, size_t a_key_len)
{
  if(a_key_id != NULL)
    key_id= a_key_id;

  if(a_key_type != NULL)
    key_type= a_key_type;

  if (a_user_id != NULL)
    user_id= a_user_id;

  key_len= a_key_len;
  if (a_key != NULL && key_len > 0)
  {
    key.reset(new uchar[a_key_len]);
    memcpy(key.get(), a_key, a_key_len);
  }
}


Key::~Key()
{
  if(key)
    memset(key.get(), 0, key_len);
}

void Key::store_field_length(uchar *buffer, size_t *buffer_position, size_t length) const
{
  *reinterpret_cast<size_t*>(buffer + *buffer_position)= length;
  *buffer_position+= sizeof(length);
}

void Key::store_field(uchar *buffer, size_t *buffer_position, const char *field, size_t field_length) const
{
  memcpy(buffer + *buffer_position, field, field_length);
  *buffer_position+= field_length;
}

void Key::store_in_buffer(uchar* buffer, size_t *buffer_position) const
{
  store_field_length(buffer, buffer_position, get_key_pod_size());
  store_field_length(buffer, buffer_position, key_id.length());
  store_field_length(buffer, buffer_position, key_type.length());
  store_field_length(buffer, buffer_position, user_id.length());
  store_field_length(buffer, buffer_position, key_len);

  store_field(buffer, buffer_position, key_id.c_str(), key_id.length());
  store_field(buffer, buffer_position, key_type.c_str(), key_type.length());
  store_field(buffer, buffer_position, user_id.c_str(), user_id.length());
  store_field(buffer, buffer_position, reinterpret_cast<char*>(key.get()),
              key_len);

  size_t padding= (sizeof(size_t) - (*buffer_position % sizeof(size_t))) %
                  sizeof(size_t);

  *buffer_position+= padding;
  DBUG_ASSERT(*buffer_position % sizeof(size_t) == 0);
}

my_bool Key::load_string_from_buffer(const uchar *buffer, size_t *buffer_position,
                                     size_t key_pod_size, std::string *string,
                                     size_t string_length)
{
  if (key_pod_size < *buffer_position + string_length)
    return TRUE;

  string->assign(reinterpret_cast<const char*>(buffer)+*buffer_position, string_length);
  *buffer_position+= string_length;

  return FALSE;
}

my_bool Key::load_field_size(const uchar *buffer, size_t *buffer_position,
                             size_t key_pod_size, size_t *field_length)
{
  if (key_pod_size <  *buffer_position + sizeof(size_t))
    return TRUE;
  *field_length= *reinterpret_cast<const size_t*>(buffer + *buffer_position);
  *buffer_position+= sizeof(size_t);

  return FALSE;
}

my_bool Key::load_from_buffer(uchar* buffer, size_t *number_of_bytes_read_from_buffer,
                              size_t input_buffer_size)
{
  size_t key_pod_size;
  size_t key_id_length;
  size_t key_type_length;
  size_t user_id_length;
  size_t buffer_position= 0;

  if (input_buffer_size <  buffer_position + sizeof(size_t))
    return TRUE;

  key_pod_size= *reinterpret_cast<size_t*>(buffer + buffer_position);

  if (input_buffer_size < buffer_position + key_pod_size)
    return TRUE;

  buffer_position+= sizeof(size_t);

  if (load_field_size(buffer, &buffer_position, key_pod_size, &key_id_length)   ||
      load_field_size(buffer, &buffer_position, key_pod_size, &key_type_length) ||
      load_field_size(buffer, &buffer_position, key_pod_size, &user_id_length)  ||
      load_field_size(buffer, &buffer_position, key_pod_size, &key_len))
    return TRUE;

 if (load_string_from_buffer(buffer, &buffer_position, key_pod_size, &key_id, key_id_length)     ||
     load_string_from_buffer(buffer, &buffer_position, key_pod_size, &key_type, key_type_length) ||
     load_string_from_buffer(buffer, &buffer_position, key_pod_size, &user_id, user_id_length))
    return TRUE;

  key.reset(new uchar[key_len]);
  memcpy(this->key.get(), buffer + buffer_position, key_len);
  buffer_position+= key_len;

  size_t padding= (sizeof(size_t) - (buffer_position % sizeof(size_t))) %
                  sizeof(size_t);
  buffer_position+= padding;
  DBUG_ASSERT(buffer_position % sizeof(size_t) == 0);

  *number_of_bytes_read_from_buffer= buffer_position;

  return FALSE;
}

/*!
  Calculates memory needed for key to be stored in POD format (serialized)
  The memory is aligned to sizeof(size_t)
*/
size_t Key::get_key_pod_size() const
{
  size_t key_pod_size= 4*sizeof(size_t) + key_id.length() + key_type.length() +
         user_id.length() + sizeof(key_len) + key_len;

  size_t padding= (sizeof(size_t) - (key_pod_size % sizeof(size_t))) %
                  sizeof(size_t);

  size_t key_pod_size_aligned= key_pod_size + padding;
  DBUG_ASSERT(key_pod_size_aligned % sizeof(size_t) == 0);
  return key_pod_size_aligned;
}

void Key::xor_data()
{
  if (key == NULL)
    return;
  static const char *obfuscate_str="*305=Ljt0*!@$Hnm(*-9-w;:";
  for(uint i=0, l=0; i < key_len; ++i, l=((l+1) % strlen(obfuscate_str)))
    key.get()[i]^= obfuscate_str[l];
}

my_bool Key::is_key_id_valid()
{
  return key_id.length() > 0;
}

my_bool Key::is_key_type_valid()
{
  return key_type.length() && (key_type == "AES" || key_type == "RSA" ||
                               key_type == "DSA");
}

my_bool Key::is_key_valid()
{
  return is_key_id_valid() || is_key_type_valid();
}

my_bool Key::is_key_length_valid()
{
  if(key_type == "AES")
    return key_len == 16 || key_len == 24 || key_len == 32;
  if (key_type == "RSA")
    return key_len == 128 || key_len == 256 || key_len == 512;
  if (key_type == "DSA")
    return key_len == 128 || key_len == 256 || key_len == 384;

  return FALSE;
}

uchar* Key::release_key_data()
{
  return key.release();
}

uchar* Key::get_key_data()
{
  return key.get();
}

size_t Key::get_key_data_size()
{
  return key_len;
}

void Key::set_key_data(uchar *key_data, size_t key_data_size)
{
  key.reset(key_data);
  key_len= key_data_size;
}

void Key::set_key_type(const std::string *key_type)
{
  this->key_type= *key_type;
}

// Key signature is ended with '\0'
void Key::create_key_signature() const
{
  if (key_id.empty())
    return;
  key_signature.append(key_id);
  key_signature.append(user_id);
}

std::string* Key::get_key_signature() const
{
  if (key_signature.empty() == TRUE)
    create_key_signature();
  return &key_signature;
}

std::string* Key::get_key_type()
{
  return &this->key_type;
}

std::string* Key::get_key_id()
{
  return &this->key_id;
}

std::string* Key::get_user_id()
{
  return &this->user_id;
}

} //namespace keyring
