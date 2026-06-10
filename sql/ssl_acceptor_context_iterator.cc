/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include <algorithm> /* std::min */
#include <cstdio>
#include <string>
#include <vector>

#include "mysql/psi/mysql_tls_channel.h" /* For PFS instrumentation */
#include "sql/ssl_acceptor_context_iterator.h"
#include "sql/ssl_acceptor_context_operator.h"

using std::min;

class Ssl_acceptor_context_iterator_data {
 public:
  Ssl_acceptor_context_iterator_data() = default;
  Ssl_acceptor_context_iterator_data(const std::string &interface,
                                     const std::string &property,
                                     const std::string &value)
      : interface_(interface), property_(property), value_(value) {}
  std::string interface() const { return interface_; }
  std::string property() const { return property_; }
  std::string value() const { return value_; }

 private:
  std::string interface_;
  std::string property_;
  std::string value_;

  friend class Ssl_acceptor_context_iterator;
};

class Ssl_acceptor_context_iterator {
 public:
  using Ssl_acceptor_context_iterator_data_container =
      std::vector<Ssl_acceptor_context_iterator_data>;
  Ssl_acceptor_context_iterator(Ssl_acceptor_context_container *context_type) {
    Lock_and_access_ssl_acceptor_context context(context_type);
    const std::string channel_name = context.channel_name();
    Ssl_acceptor_context_iterator_data const first_one(
        channel_name, "Enabled", context.have_ssl() ? "Yes" : "No");
    data_.push_back(first_one);
    for (Ssl_acceptor_context_property_type type =
             Ssl_acceptor_context_property_type::accept_renegotiates;
         type != Ssl_acceptor_context_property_type::last; ++type) {
      Ssl_acceptor_context_iterator_data const one(channel_name,
                                                   Ssl_ctx_property_name(type),
                                                   context.show_property(type));
      data_.push_back(one);
    }
    /* Now set the iterator to beginning */
    it_ = data_.cbegin();
  }

  ~Ssl_acceptor_context_iterator() { it_ = data_.cend(); }

  bool get(Ssl_acceptor_context_iterator_data &data) {
    if (it_ == data_.cend()) return false;
    data = *it_;
    return true;
  }

  bool next() {
    if (it_ == data_.cend()) return false;
    ++it_;
    return it_ != data_.cend();
  }

 private:
  Ssl_acceptor_context_iterator_data_container data_;
  Ssl_acceptor_context_iterator_data_container::const_iterator it_;
};

bool init_mysql_main_iterator(property_iterator *it) {
  auto *container = new Ssl_acceptor_context_iterator(mysql_main);
  if (container == nullptr) return false;
  *it = reinterpret_cast<property_iterator>(container);
  return true;
}

bool init_mysql_admin_iterator(property_iterator *it) {
  if (mysql_admin == nullptr) return false;
  auto *container = new Ssl_acceptor_context_iterator(mysql_admin);
  if (container == nullptr) return false;
  *it = reinterpret_cast<property_iterator>(container);
  return true;
}

void deinit_tls_status_iterator(property_iterator it) {
  auto *container = reinterpret_cast<Ssl_acceptor_context_iterator *>(it);
  delete container;
}

bool get_tls_status(property_iterator it, TLS_channel_property *property) {
  auto *container = reinterpret_cast<Ssl_acceptor_context_iterator *>(it);
  if (container == nullptr || property == nullptr) return false;

  Ssl_acceptor_context_iterator_data data;
  if (!container->get(data)) return false;

  /* Copy interface */
  snprintf(property->channel_name, sizeof(property->channel_name), "%s",
           data.interface().c_str());

  /* Copy property name */
  snprintf(property->property_name, sizeof(property->property_name), "%s",
           data.property().c_str());

  /* Copy property value */
  snprintf(property->property_value, sizeof(property->property_value), "%s",
           data.value().c_str());

  return true;
}

bool next_tls_status(property_iterator it) {
  auto *container = reinterpret_cast<Ssl_acceptor_context_iterator *>(it);
  if (container == nullptr) return false;
  return container->next();
}

static struct TLS_channel_property_iterator mysql_main_iterator {
  init_mysql_main_iterator, deinit_tls_status_iterator, get_tls_status,
      next_tls_status,
};

static struct TLS_channel_property_iterator mysql_admin_iterator {
  init_mysql_admin_iterator, deinit_tls_status_iterator, get_tls_status,
      next_tls_status,
};

void init_tls_psi_keys() {
  /* Register server's TLS interfaces */
  mysql_tls_channel_register(&mysql_main_iterator);
  mysql_tls_channel_register(&mysql_admin_iterator);
}

void deinit_tls_psi_keys() {
  /* Un-register server's TLS interfaces */
  mysql_tls_channel_unregister(&mysql_main_iterator);
  mysql_tls_channel_unregister(&mysql_admin_iterator);
}
