/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_NGS_CLIENT_LIST_H_
#define PLUGIN_X_SRC_NGS_CLIENT_LIST_H_

#include <algorithm>
#include <list>
#include <memory>
#include <vector>

#include "plugin/x/src/helper/multithread/lock_container.h"
#include "plugin/x/src/helper/multithread/rw_lock.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/ngs/thread.h"

namespace ngs {

class Client_list {
 public:
  using Client_ptr = std::shared_ptr<xpl::iface::Client>;
  using Client_ptr_list = std::list<Client_ptr>;
  using Client_ptr_list_with_lock =
      xpl::Locked_container<Client_ptr_list, xpl::RWLock_writelock,
                            xpl::RWLock>;

 public:
  Client_list();

  size_t size();

  void add(std::shared_ptr<xpl::iface::Client> client);
  void remove(uint64_t client_id);
  std::shared_ptr<xpl::iface::Client> find(const uint64_t client_id);

  /**
    Enumerate clients.

    Each client present on the list is passed to 'matcher'
    using it as functor which takes one argument.
    Enumeration process can be stopped by 'matcher' any time,
    its done by returning 'true'.
   */
  template <typename Functor>
  void enumerate(Functor *matcher);

  /* Please note that this method doesn't take pointer as in previous overload.
   * Previous method can't have non-const references, because code standard
   * forces the use of a pointer. Still having object instead pointer (const
   * reference) in this method allows us to consume lambda function.
   */
  template <typename Functor>
  void enumerate(const Functor &matcher);

  Client_ptr_list_with_lock direct_access() {
    return Client_ptr_list_with_lock(&m_clients, &m_clients_lock);
  }

  void get_all_clients(
      std::vector<std::shared_ptr<xpl::iface::Client>> *result);

 private:
  Client_list(const Client_list &);
  Client_list &operator=(const Client_list &);

  xpl::RWLock m_clients_lock;
  Client_ptr_list m_clients;
};

template <typename Functor>
void Client_list::enumerate(Functor *matcher) {
  xpl::RWLock_readlock guard(&m_clients_lock);

  for (auto &c : m_clients)
    if ((*matcher)(c)) break;
}

template <typename Functor>
void Client_list::enumerate(const Functor &matcher) {
  xpl::RWLock_readlock guard(&m_clients_lock);

  for (auto &c : m_clients)
    if (matcher(c)) break;
}

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_CLIENT_LIST_H_
