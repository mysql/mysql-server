/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_LIST_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_LIST_H_

#include <algorithm>
#include <list>
#include <vector>

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/thread.h"

namespace ngs {

typedef ngs::shared_ptr<Client_interface> Client_ptr;

class Client_list {
 public:
  Client_list();
  ~Client_list();

  size_t size();

  void add(Client_ptr client);
  void remove(uint64_t client_id);
  Client_ptr find(const uint64_t client_id);

  /**
    Enumerate clients.

    Each client present on the list is passed to 'matcher'
    using it as functor which takes one argument.
    Enumeration process can be stopped by 'matcher' any time,
    its done by returning 'true'.
   */
  template <typename Functor>
  void enumerate(Functor &matcher);
  template <typename Functor>
  void enumerate(const Functor &matcher);

  void get_all_clients(std::vector<Client_ptr> &result);

 private:
  struct Match_client {
    Match_client(uint64_t client_id);

    bool operator()(Client_ptr client);

    uint64_t m_id;
  };

  Client_list(const Client_list &);
  Client_list &operator=(const Client_list &);

  RWLock m_clients_lock;
  std::list<Client_ptr> m_clients;
};

template <typename Functor>
void Client_list::enumerate(Functor &matcher) {
  RWLock_readlock guard(m_clients_lock);

  /*
    Matcher can stop enumeration process by returning
    'true'. 'std::find_if' is used as stoppable enumeration
    dispatcher.
   */
  std::find_if(m_clients.begin(), m_clients.end(), matcher);
}

template <typename Functor>
void Client_list::enumerate(const Functor &matcher) {
  RWLock_readlock guard(m_clients_lock);

  /*
    Matcher can stop enumeration process by returning
    'true'. 'std::find_if' is used as stoppable enumeration
    dispatcher.
   */
  std::find_if(m_clients.begin(), m_clients.end(), matcher);
}

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_LIST_H_
