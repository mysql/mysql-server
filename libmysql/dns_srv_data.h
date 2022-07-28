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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DNS_SRV_DATA_H
#define DNS_SRV_DATA_H

#include <assert.h>
#include <cstdlib>
#include <list>
#include <map>
#include <string>

/**
 A RFC2782 compliant SRV records storage

 Stores host/port/weight/priority entries
 into a data structure that then allows retrieving
 these via the Dns_srv_data::pop_next() method.

 Entries can be stored by calling Dns_srv_data::add()
 in any order.

 This usage pattern is roughly as follows:
 1. Dns_srv_data construct
 2. one or more Dns_srv_data::add()
 3. one or more Dns_srv_data::pop_next()
 4. Dns_srv_data destructor

 @sa mysql_real_connect_dns_srv
*/
class Dns_srv_data {
  class Dns_entry {
    std::string host_;
    unsigned port_{0}, weight_{0};
    unsigned long weight_sum_{0};

    Dns_entry() = delete;  // disable copy constructor

   public:
    Dns_entry(const std::string &host, unsigned port, unsigned weight)
        : host_(host), port_(port), weight_(weight) {}

    unsigned port() const { return port_; }
    std::string host() const { return host_; }
    unsigned long weight_sum() const { return weight_sum_; }
    void add_weight_sum(unsigned long &weight_sum) {
      weight_sum_ = (weight_sum += weight_);
    }
  };
  using dns_entry_list_t = std::list<Dns_entry>;
  using dns_entry_data_t = std::map<unsigned, Dns_srv_data::dns_entry_list_t>;
  dns_entry_data_t data_;

 public:
  void clear() { data_.clear(); }
  void add(const std::string &host, unsigned port, unsigned priority,
           unsigned weight) {
    dns_entry_data_t::iterator list = data_.find(priority);
    if (list == data_.cend())
      data_.emplace(priority,
                    dns_entry_list_t(1, Dns_entry(host, port, weight)));
    else {
      // RFC2782: put the 0 weight at the front, rest at the back
      if (weight > 0)
        list->second.emplace_back(host, port, weight);
      else
        list->second.emplace_front(host, port, weight);
    }
  }
  bool pop_next(std::string &host, unsigned &port) {
    if (data_.empty()) return true;

    dns_entry_list_t &list = data_.begin()->second;
    assert(!list.empty());

    unsigned long sum = 0;
    for (Dns_entry &elt : list) elt.add_weight_sum(sum);

    unsigned long draw = (std::rand() * 1UL * sum) / RAND_MAX;

    dns_entry_list_t::const_iterator iter = list.cbegin();
    while (iter->weight_sum() < draw) iter++;
    assert(iter != list.end());

    host = iter->host();
    port = iter->port();

    list.erase(iter);
    if (list.empty()) data_.erase(data_.begin());
    return false;
  }
};

#endif  // !DNS_SRV_DATA_H
