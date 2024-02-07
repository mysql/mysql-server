/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef UDF_SERVICE_IMPL_H
#define UDF_SERVICE_IMPL_H

#include "mysql/components/my_service.h"
#include "mysql/udf_registration_types.h"

#include <string>
#include <vector>

/**
  Contains all the necessary data to register an UDF in MySQL.
*/
typedef struct udf_data_t {
  const std::string m_name;
  const Item_result m_return_type;
  const Udf_func_string m_func;
  const Udf_func_init m_init_func;
  const Udf_func_deinit m_deinit_func;
  udf_data_t(const std::string &name, const Item_result return_type,
             const Udf_func_string func, const Udf_func_init init_func,
             const Udf_func_deinit deinit_func)
      : m_name(name),
        m_return_type(return_type),
        m_func(func),
        m_init_func(init_func),
        m_deinit_func(deinit_func) {}
  udf_data_t(udf_data_t const &) = delete;
  udf_data_t(udf_data_t &&other) = delete;
  udf_data_t &operator=(udf_data_t const &) = delete;
  udf_data_t &operator=(udf_data_t &&other) = delete;
} Udf_data;

/*
  Utility class for registering UDF service.

  For usage please check sql/rpl_async_conn_failover_udf.cc
*/
class Udf_service_impl {
 private:
  /* UDF registry service. */
  SERVICE_TYPE(registry) * m_registry{nullptr};

  /* List of registered udfs name. */
  std::vector<std::string> m_udfs_registered;

 public:
  Udf_service_impl() = default;
  virtual ~Udf_service_impl() = default;

  /**
    Initialize variables, acquires the mysql_service_mysql_udf_metadata from the
    registry service and register the Asynchronous Connection Failover's UDFs.
    If there is an error registering any UDF, all installed UDFs are
    unregistered.

    @retval true if there was an error
    @retval false if all UDFs were registered
   */
  virtual bool init() = 0;

  /**
    Release the mysql_service_mysql_udf_metadata service and unregisters the
    Asynchronous Connection Failover's UDFs.

    @retval true if there was an error
    @retval false if all UDFs were unregistered
   */
  bool deinit();

  /**
    Save UDF registry service.

    param[in]  r  UDF registry service.
  */
  void set_registry(SERVICE_TYPE(registry) * r) { m_registry = r; }

  /**
    Registers the Asynchronous Connection Failover's UDFs.
    If there is an error registering any UDF, all installed UDFs are
    unregistered.

    @retval true if there was an error
    @retval false if all UDFs were registered
   */
  bool register_udf(Udf_data &e);

  /**
    Unregisters the Asynchronous Connection Failover's UDFs.

    @retval true   if there was an error
    @retval false  if all UDFs were unregistered
   */
  bool unregister_udf(const std::string udf_name);
};

/*
  Used to load registered UDFs
*/
class Udf_load_service {
 private:
  /* List of registered udfs functions. */
  std::vector<Udf_service_impl *> m_udfs_registered;

  template <class T>
  void add() {
    T *obj = new T();
    m_udfs_registered.emplace_back(obj);
  }

  void register_udf();

  void unregister_udf();

 public:
  Udf_load_service();

  ~Udf_load_service();

  /**
    Registers the Asynchronous Connection Failover's UDFs.
    If there is an error registering any UDF, all installed UDFs are
    unregistered.

    @retval true if there was an error
    @retval false if all UDFs were registered
   */
  bool init();

  /**
    Unregisters the Asynchronous Connection Failover's UDFs.

    @retval true   if there was an error
    @retval false  if all UDFs were unregistered
   */
  bool deinit();
};

#endif /* UDF_SERVICE_IMPL_H */
