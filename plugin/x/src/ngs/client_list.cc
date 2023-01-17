/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/ngs/client_list.h"

#include <iterator>

#include "plugin/x/src/helper/multithread/rw_lock.h"

namespace ngs {

namespace details {

struct Match_client {
  explicit Match_client(const uint64_t client_id) : m_id(client_id) {}

  bool operator()(const Client_list::Client_ptr &client) {
    return client->client_id_num() == m_id;
  }

  uint64_t m_id;
};

}  // namespace details

Client_list::Client_list() : m_clients_lock(KEY_rwlock_x_client_list_clients) {}

void Client_list::add(std::shared_ptr<xpl::iface::Client> client) {
  xpl::RWLock_writelock guard(&m_clients_lock);
  m_clients.push_back(client);
}

void Client_list::remove(const uint64_t client_id) {
  xpl::RWLock_writelock guard(&m_clients_lock);
  details::Match_client matcher(client_id);

  m_clients.remove_if(matcher);
}

std::shared_ptr<xpl::iface::Client> Client_list::find(uint64_t client_id) {
  xpl::RWLock_readlock guard(&m_clients_lock);
  details::Match_client matcher(client_id);

  std::list<std::shared_ptr<xpl::iface::Client>>::iterator i =
      std::find_if(m_clients.begin(), m_clients.end(), matcher);

  if (m_clients.end() == i) return std::shared_ptr<xpl::iface::Client>();

  return *i;
}

size_t Client_list::size() {
  xpl::RWLock_readlock guard(&m_clients_lock);

  return m_clients.size();
}

void Client_list::get_all_clients(
    std::vector<std::shared_ptr<xpl::iface::Client>> *result) {
  xpl::RWLock_readlock guard(&m_clients_lock);

  result->clear();
  result->reserve(m_clients.size());

  std::copy(m_clients.begin(), m_clients.end(), std::back_inserter(*result));
}

}  // namespace ngs
