/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "components/mysql_server/my_metadata.h"

my_metadata::my_metadata() {}

my_metadata::my_metadata(my_metadata &other) : data(other.data) {}

my_metadata::const_iterator::const_iterator(
    my_unordered_string_to_string_map::const_iterator iter,
    my_unordered_string_to_string_map *data_arg)
    : my_unordered_string_to_string_map::const_iterator(iter), data(data_arg) {}

/**
  Gets pointer to structure the iterator is iterating over.
*/
my_unordered_string_to_string_map *my_metadata::const_iterator::get_data() {
  return data;
}

/**
  Checks if iterator is pointing behind the end element.
*/
bool my_metadata::const_iterator::is_at_end() {
  return (*static_cast<my_unordered_string_to_string_map::const_iterator *>(
             this)) == data->cend();
}

/**
  Sets a value for the name specified.
  Adds to the metadata list if absent. Copies the data into the structure,
  original values don't need to be around after this function is called.

  @param name The name string of the metadata pair to be added.
  @param value The value string of the metadata pair.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool my_metadata::set_value(const char *name, const char *value) {
  try {
    my_string key_str = name;
    my_string value_str = value;

    my_unordered_string_to_string_map::iterator it = data.find(key_str);
    if (it != data.end()) {
      it->second = value_str;
    } else {
      data.emplace(std::move(key_str), std::move(value_str));
    }
    return false;
  } catch (...) {
  }
  return true;
}

/**
  Gets value for the name specified.
  The value is unmodified if not found.
  Doesn't copy the result value, so it requires synchronization to prevent other
  threads to modify structure.

  @param name The name string of the metadata pair.
  @param [out] value The value string of the metadata pair.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool my_metadata::get_value(const char *name, const char **value) {
  try {
    my_unordered_string_to_string_map::const_iterator iter =
        data.find(my_string(name));
    if (iter == data.end()) {
      return true;
    }
    *value = iter->second.c_str();
    return false;
  } catch (...) {
  }
  return true;
}

/**
  Creates an iterator over the metadata.

  Positions on the first value.

  @return Reference to the iterator created.
*/
my_metadata::const_iterator my_metadata::create_iterator() {
  return my_metadata::const_iterator(data.begin(), &data);
}

/**
  Frees an iterator over the metadata.
  Iterator must be allocated through create_iterator();

  @param iter Pointer to the iterator to release.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool my_metadata::release_iterator(const_iterator *iter) {
  try {
    delete iter;
    return false;
  } catch (...) {
  }
  return true;
}
