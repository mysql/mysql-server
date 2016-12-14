/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_CLIENT_LIST_H_
#define _NGS_CLIENT_LIST_H_

#include <algorithm>
#include <vector>
#include <list>
#include <ngs/thread.h>
#include <ngs/interface/client_interface.h>

namespace ngs
{

typedef ngs::shared_ptr<Client_interface> Client_ptr;

class Client_list
{
public:
  Client_list();
  ~Client_list();

  size_t size();

  void add(Client_ptr client);
  void remove(uint64_t client_id);
  Client_ptr find(const uint64_t client_id);

  template<typename Functor>
  void enumerate(Functor &matcher);

  void get_all_clients(std::vector<Client_ptr> &result);
private:
  struct Match_client
  {
    Match_client(uint64_t client_id);

    bool operator () (Client_ptr client);

    uint64_t m_id;
  };

  Client_list(const Client_list&);
  Client_list& operator=(const Client_list&);

  RWLock m_clients_lock;
  std::list<Client_ptr> m_clients;
};

template<typename Functor>
void Client_list::enumerate(Functor &matcher)
{
  RWLock_readlock guard(m_clients_lock);

  std::find_if(m_clients.begin(), m_clients.end(), matcher);
}


} // namespace ngs

#endif // _NGS_CLIENT_LIST_H_
