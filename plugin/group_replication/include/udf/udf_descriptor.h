/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef PLUGIN_GR_INCLUDE_UDF_DESCRIPTOR_H
#define PLUGIN_GR_INCLUDE_UDF_DESCRIPTOR_H

#include "mysql/udf_registration_types.h"

/**
 * Contains all the necessary data to register an UDF in MySQL.
 */
struct udf_descriptor {
  char const *name;
  enum Item_result result_type;
  Udf_func_any main_function;
  Udf_func_init init_function;
  Udf_func_deinit deinit_function;

  udf_descriptor(char const *udf_name, enum Item_result udf_result_type,
                 Udf_func_any udf_function, Udf_func_init udf_init,
                 Udf_func_deinit udf_deinit)
      : name(udf_name),
        result_type(udf_result_type),
        main_function(udf_function),
        init_function(udf_init),
        deinit_function(udf_deinit) {}
  udf_descriptor(udf_descriptor const &) = delete;
  udf_descriptor(udf_descriptor &&other) = default;
  udf_descriptor &operator=(udf_descriptor const &) = delete;
  udf_descriptor &operator=(udf_descriptor &&other) = default;
};

#endif /* PLUGIN_GR_INCLUDE_UDF_DESCRIPTOR_H */
