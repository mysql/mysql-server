/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include <iterator>

#include "plugin/x/ngs/include/ngs/client_list.h"

using namespace ngs;

Client_list::Client_list()
: m_clients_lock(KEY_rwlock_x_client_list_clients)
{
}

Client_list::~Client_list()
{
}

void Client_list::add(Client_ptr client)
{
  RWLock_writelock guard(m_clients_lock);
  m_clients.push_back(client);
}

void Client_list::remove(const uint64_t client_id)
{
  RWLock_writelock guard(m_clients_lock);
  Match_client matcher(client_id);

  m_clients.remove_if(matcher);
}

Client_list::Match_client::Match_client(uint64_t client_id)
: m_id(client_id)
{
}

bool Client_list::Match_client::operator () (Client_ptr client)
{
  if (client->client_id_num() == m_id)
  {
    return true;
  }

  return false;
}

Client_ptr Client_list::find(uint64_t client_id)
{
  RWLock_readlock guard(m_clients_lock);
  Match_client    matcher(client_id);

  std::list<Client_ptr>::iterator i = std::find_if(m_clients.begin(), m_clients.end(), matcher);

  if (m_clients.end() == i)
    return Client_ptr();

  return *i;
}


size_t Client_list::size()
{
  RWLock_readlock guard(m_clients_lock);

  return m_clients.size();
}


void Client_list::get_all_clients(std::vector<Client_ptr> &result)
{
  RWLock_readlock guard(m_clients_lock);

  result.clear();
  result.reserve(m_clients.size());

  std::copy(m_clients.begin(), m_clients.end(), std::back_inserter(result));
}
