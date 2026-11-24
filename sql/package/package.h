/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifndef SQL_PACKAGE_PACKAGE_INCLUDED
#define SQL_PACKAGE_PACKAGE_INCLUDED

#include "sql/package/package_common.h"
#include "sql/package/proc.h"
#include "sql/psi_memory_key.h"

/**
  Internal native mysql package.

  Package is the container of all native elements, so It's the singleton
  instance, and initilized when mysqld reboot.
*/
namespace im {

/* Native procedures container */
class Package : public PSI_memory_base {
  typedef Package_element_map<Proc> Proc_map;

  /* Type selector for element map */
  template <typename T>
  struct Type_selector {};

 public:
  explicit Package(PSI_memory_key key)
      : PSI_memory_base(key), m_proc_map(key) {}

  virtual ~Package();

  /* Disable copy and assign function */
  Package(const Package &) = delete;
  Package(const Package &&) = delete;
  Package &operator=(const Package &) = delete;
  /**
    Global singleton package container.

    @retval     package instance
  */
  static Package *instance();

  /**
    Register native element

    @param[in]      schema_name     Element schema.
    @param[in]      element_name    Element name
    @param[in]      element         Object instance

    @retval         false           Failure
    @retval         true            Success
  */
  template <typename T>
  bool register_element(const std::string &schema_name,
                        const std::string &element_name, T *element) {
    std::pair<typename Package_element_map<T>::iterator, bool> it;

    it = m_map<T>()->insert(typename Package_element_map<T>::value_type(
        typename Package_element_map<T>::key_type(schema_name, element_name),
        element));

    return it.second;
  }

  /**
    Lookup element

    @param[in]      schema_name     Element schema.
    @param[in]      element_name    Element name

    @retval         element         Object instance
 */
  template <typename T>
  const T *lookup_element(const std::string &schema_name,
                          const std::string &element_name) {
    typename Package_element_map<T>::const_iterator it;

    it = m_map<T>()->find(
        typename Package_element_map<T>::key_type(schema_name, element_name));

    if (it == m_map<T>()->end())
      return nullptr;
    else
      return it->second;
  }

  template <typename T>
  const Proc_map *get_all_element() {
    return m_map<T>();
  }

 private:
  /* Proc map getting */
  Proc_map *m_map(Type_selector<Proc>) { return &m_proc_map; }

  /* Template map getting */
  template <typename T>
  Package_element_map<T> *m_map() {
    return m_map(Type_selector<T>());
  }

  /* Clear all elements */
  void clear_elements() {
    for (auto it = m_proc_map.cbegin(); it != m_proc_map.cend(); ++it) {
      delete it->second;
    }
    m_proc_map.clear();
  }

 private:
  /* Native proc container */
  Proc_map m_proc_map;
};

} /* namespace im */

#endif
