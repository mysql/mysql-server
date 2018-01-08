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

#ifndef MYSQL_REGISTRY_METADATA_H
#define MYSQL_REGISTRY_METADATA_H

#include <string>
#include <memory>
#include <unordered_map>

typedef std::string my_string;

typedef std::unordered_map<my_string, my_string>
  my_unordered_string_to_string_map;


/**
  The metadata class used by both the registry and the dynamic loader services
  to store and manage metadata information related to specified objects.
*/
class my_metadata
{
protected:
  my_unordered_string_to_string_map data;

public:

  my_metadata();
  my_metadata(my_metadata &other);

  /**
    The data type of the iterator used.
    Keep consistent with the type of the underlying data structure
  */
  class const_iterator
    : public my_unordered_string_to_string_map::const_iterator
  {
    my_unordered_string_to_string_map* data;

  public:
    const_iterator(my_unordered_string_to_string_map::const_iterator iter,
      my_unordered_string_to_string_map* data_arg);

    my_unordered_string_to_string_map* get_data();
    bool is_at_end();
  };

  /* update service */
  bool set_value(const char* name, const char* value);

  /* query service */
  bool get_value(const char* name, const char** value);

  /* enumeration service */

  const_iterator create_iterator();

  static bool release_iterator(const_iterator* iter);
};

#endif /* MYSQL_REGISTRY_METADATA_H */
