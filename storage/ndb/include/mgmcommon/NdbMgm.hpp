/*
   Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_NDBMGM_HPP
#define NDB_NDBMGM_HPP

#include <memory>

#include "mgmapi/mgmapi.h"

namespace ndb_mgm {

/*
  Helper classes to own, manage and dispose objects created using the
  MySQL Cluster Management API (MGM API)
*/

// ndb_mgm_handle
struct handle_deleter {
  void operator()(ndb_mgm_handle *handle) { ndb_mgm_destroy_handle(&handle); }
};
using handle_ptr = std::unique_ptr<ndb_mgm_handle, handle_deleter>;

// ndb_mgm_configuration
struct config_deleter {
  void operator()(ndb_mgm_configuration *conf) {
    ndb_mgm_destroy_configuration(conf);
  }
};
using config_ptr = std::unique_ptr<ndb_mgm_configuration, config_deleter>;

// ndb_mgm_configuration_iterator
struct config_iter_deleter {
  void operator()(ndb_mgm_configuration_iterator *iter) {
    ndb_mgm_destroy_iterator(iter);
  }
};
using config_iter_ptr =
    std::unique_ptr<ndb_mgm_configuration_iterator, config_iter_deleter>;

// ndb_mgm_cluster_state
struct cluster_state_deleter {
  void operator()(ndb_mgm_cluster_state *state) { free(state); }
};
using cluster_state_ptr =
    std::unique_ptr<ndb_mgm_cluster_state, cluster_state_deleter>;

// ndb_mgm_cluster2_state
struct cluster_state2_deleter {
  void operator()(ndb_mgm_cluster_state2 *state) { free(state); }
};
using cluster_state2_ptr =
    std::unique_ptr<ndb_mgm_cluster_state2, cluster_state2_deleter>;

// ndb_log_event_handle
struct logevent_handle_deleter {
  void operator()(ndb_logevent_handle *handle) {
    ndb_mgm_destroy_logevent_handle(&handle);
  }
};
using logevent_handle_ptr =
    std::unique_ptr<ndb_logevent_handle, logevent_handle_deleter>;

// ndb_mgm_events
struct events_deleter {
  void operator()(ndb_mgm_events *events) { free(events); }
};
using events_ptr = std::unique_ptr<ndb_mgm_events, events_deleter>;

}  // namespace ndb_mgm

#endif
