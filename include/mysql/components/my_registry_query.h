/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MY_REGISTRY_QUERY_H
#define MY_REGISTRY_QUERY_H

#include <mysql/components/my_service.h>
#include <mysql/components/service.h>
#include <mysql/components/services/registry.h>
#include <cstring>
#include <string>
#include <unordered_set>
#include "scope_guard.h"

/**
  A registry query convenience class

  Uses the registry query service to produce an unique set of service names
  that match the pattern supplied.

  Typical use is:

  @code
    My_registry_query_string string_list("foo", h_registry);
    if (string_list.init()) error_out();
    for (auto name : string_list) {
       <do stuff with names>
    }
  @endcode

  @sa @ref My_registry_query_and_acquire
*/
class My_registry_query_string : public std::unordered_set<std::string> {
  const std::string m_service_name;
  SERVICE_TYPE(registry) * m_registry;
  SERVICE_TYPE(registry_query) * m_registry_query;
  my_h_service m_reg_query_handle{nullptr};

 public:
  bool init() {
    this->erase(this->begin(), this->end());
    SERVICE_TYPE(registry_query) *svc = m_registry_query;
    if (svc == nullptr) {
      my_service<SERVICE_TYPE(registry_query)> msvc("registry_query",
                                                    m_registry);
      if (!msvc.is_valid()) return true;
      svc = msvc.untie();
    }
    auto x = create_scope_guard([&] {
      if (svc != m_registry_query)
        m_registry->release(reinterpret_cast<my_h_service>(
            const_cast<SERVICE_TYPE_NO_CONST(registry_query) *>(svc)));
    });

    my_h_service_iterator iter;
    if (!svc->create(m_service_name.c_str(), &iter)) {
      while (!svc->is_valid(iter)) {
        const char *name{nullptr};
        if (svc->get(iter, &name)) return true;
        if (name == nullptr) return true;
        size_t name_len = strlen(name);
        if (strncmp(name, m_service_name.c_str(), m_service_name.length()) ||
            name_len < m_service_name.length() ||
            (name[m_service_name.length()] != '.' &&
             name[m_service_name.length()] != 0))
          break;
        this->emplace(name);
        if (svc->next(iter)) break;
      }
      svc->release(iter);
    }
    return false;
  }

  My_registry_query_string(const char *service_name,
                           SERVICE_TYPE(registry) * reg,
                           SERVICE_TYPE(registry_query) *reg_query = nullptr)
      : m_service_name(service_name),
        m_registry(reg),
        m_registry_query(reg_query) {
    if (m_registry_query == nullptr) {
      if (!reg->acquire("registry_query", &m_reg_query_handle)) {
        m_registry_query = reinterpret_cast<SERVICE_TYPE(registry_query) *>(
            m_reg_query_handle);
      }
    }
  }

  My_registry_query_string(const My_registry_query_string &other) = delete;
  My_registry_query_string(My_registry_query_string &&other) = delete;
  ~My_registry_query_string() {
    if (m_reg_query_handle) m_registry->release(m_reg_query_handle);
  }
};

/**
  A service acquiring registry query convenience class

  Uses the @ref My_registry_query_string class to get a list of
  service names matching the pattern, acquires references for these
  and keeps them until the instance's destruction.

  Typical use pattern is:
  @code
  My_registry_query_and_acquire<SERVICE_TYPE(foo)> qry("foo", registry_ref);
  if (qry->init()) error_out();
  for (SERVICE_TYPE(foo) *x : qry) {
    x->method();
  }
  @endcode

  @sa @ref My_registry_query_string
*/

template <class ServiceType>
class My_registry_query_and_acquire : public std::unordered_set<ServiceType *> {
  const std::string m_service_name;
  SERVICE_TYPE(registry) * m_registry;
  My_registry_query_string m_string_list;

 public:
  My_registry_query_and_acquire(
      const char *service_name, SERVICE_TYPE(registry) * reg,
      SERVICE_TYPE(registry_query) *reg_query = nullptr)
      : m_service_name(service_name),
        m_registry(reg),
        m_string_list(service_name, reg, reg_query) {}

  My_registry_query_and_acquire(const My_registry_query_and_acquire &other) =
      delete;
  My_registry_query_and_acquire(My_registry_query_and_acquire &&other) = delete;

  ~My_registry_query_and_acquire() { reset(); }

  /**
    @brief Call this method to populate the data into the class

    @retval true failure
    @retval false success
  */
  bool init() {
    if (m_string_list.init()) return true;
    for (auto name : m_string_list) {
      my_h_service hsvc;
      if (m_registry->acquire(name.c_str(), &hsvc)) return true;
      auto res = this->insert(reinterpret_cast<ServiceType *>(hsvc));
      if (!res.second) m_registry->release(hsvc);
    }
    m_string_list.clear();
    return false;
  }

  /**
    @brief Properly releases and disposes of all the references

    Called by the destructor too
  */
  void reset() {
    for (auto svc : *this) {
      m_registry->release(reinterpret_cast<my_h_service>(
          const_cast<void *>(reinterpret_cast<const void *>(svc))));
    }
    this->erase(this->begin(), this->end());
  }
};
#endif /* MY_REGISTRY_QUERY_H */
