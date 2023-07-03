/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef UDF_SERVICE_UTIL_H
#define UDF_SERVICE_UTIL_H

#include "mysql/components/my_service.h"

constexpr int MAX_CHARSET_LEN = 100;

/*
  This is a UDF utility class to set charset for arguments and
  return values of UDF.

  For usage please check sql/rpl_async_conn_failover_udf.cc.
*/
class Udf_charset_service {
 private:
  /* Name of service registry to be acquired. */
  static std::string m_service_name;

  /* The charset to be used for argument and return values. */
  static std::string m_charset_name;

  /*
    The extension type used by UDF metadata
    (sql/server_component/udf_metadata_imp.cc).
  */
  static std::string m_arg_type;

  /**
    Release the udf_metadata_service from the registry service.

    @retval true   if service could not be acquired
    @retval false  Otherwise
  */
  static bool deinit();

  /**
    Acquires the udf_metadata_service from the registry service.

    @retval true   if service could not be acquired
    @retval false  Otherwise
  */
  static bool init();

 public:
  Udf_charset_service() = default;

  ~Udf_charset_service() = default;

  /**
    Get service name.

    @return Service name.
  */
  static std::string get_service_name() { return m_service_name; }

  /**
    Set the specified character set.

    @param[in] charset_name  Character set that has to be set.
  */
  static void set_charset(std::string charset_name) {
    m_charset_name = charset_name;
  }

  /**
    Get the current character set getting used.

    @retval  Character set name.
  */
  std::string get_charset() { return m_charset_name; }

  /**
    Set the specified character set of UDF return value.

    @param[in] initid        UDF_INIT structure

    @retval true Could not set the character set of return value
    @retval false Otherwise
  */
  static bool set_return_value_charset(UDF_INIT *initid);

  /**
    Set the specified character set of all UDF arguments.

    @param[in] args          UDF_ARGS structure

    @retval true  Could not set the character set of any of the argument
    @retval false Otherwise
  */
  static bool set_args_charset(UDF_ARGS *args);
};

#endif /* UDF_SERVICE_UTIL_H */
