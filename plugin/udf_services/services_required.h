/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef SERVICE_HANDLERS_INCLUDED
#define SERVICE_HANDLERS_INCLUDED

#include <mysql/components/my_service.h>
#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>
#include <string>

/**
  A wrapper that shares an error string and provides method to
  access last error string.
*/
class Error_capture {
 public:
  static std::string get_last_error();

 protected:
  static const char *s_message;
};

/*
  A wrapper that provides the methods to acquire and release the
  plugin registry service
*/
class Registry_service : public Error_capture {
 public:
  static bool acquire();
  static void release();
  static SERVICE_TYPE(registry) * get();

 private:
  static SERVICE_TYPE(registry) * h_registry;
};

class Udf_registration : public Error_capture {
 public:
  static bool acquire();
  static void release();
  static bool add(const char *func_name, enum Item_result return_type,
                  Udf_func_any func, Udf_func_init init_func,
                  Udf_func_deinit deinit_func);
  static bool remove(const char *name, int *was_present);

 private:
  static my_service<SERVICE_TYPE(udf_registration)> *h_service;
};

/*
  A wrapper that provides the methods to acquire, release the
  string conversion service. It also provides a convenient method
  to convert a buffer from one from one charset to another charset.
*/
class Character_set_converter : public Error_capture {
 public:
  static bool acquire();
  static void release();
  static bool convert(const std::string &out_charset_name,
                      const std::string &in_charset_name,
                      const std::string &in_buffer, size_t out_buffer_length,
                      char *out_buffer);
  static SERVICE_TYPE(mysql_string_converter) * get();

 private:
  static my_service<SERVICE_TYPE(mysql_string_converter)> *h_service;
};

/*
  A wrapper that provides the method to acquire/release the UDF_extension
  service.
*/
class Udf_metadata : public Error_capture {
 public:
  static bool acquire();
  static void release();
  static SERVICE_TYPE(mysql_udf_metadata) * get();

 private:
  static my_service<SERVICE_TYPE(mysql_udf_metadata)> *h_service;
};

#endif  // !SERVICE_HANDLERS_INCLUDED
