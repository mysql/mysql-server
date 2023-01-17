/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef METADATA_CACHE_GR_NOTIFICATION_LISTENER_INCLUDED
#define METADATA_CACHE_GR_NOTIFICATION_LISTENER_INCLUDED

#include <functional>
#include <memory>
#include <vector>

#include "mysqlrouter/metadata_cache.h"

class GRNotificationListener {
 public:
  GRNotificationListener(const mysqlrouter::UserCredentials &user_credentials);

  ~GRNotificationListener();
  GRNotificationListener(GRNotificationListener &) = delete;
  GRNotificationListener &operator=(GRNotificationListener &) = delete;

  using NotificationClb = std::function<void()>;

  void setup(const metadata_cache::ClusterTopology &cluster_topology,
             const NotificationClb &notification_clb);

 private:
  // let's hide the x-client stuff in the pimpl so that those including us
  // didn't need to inclde that too
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // METADATA_CACHE_GR_NOTIFICATION_LISTENER_INCLUDED
