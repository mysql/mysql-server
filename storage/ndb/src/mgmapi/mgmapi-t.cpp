/*
   Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <util/NdbTap.hpp>

#include <mgmapi/mgmapi.h>
#include <mgmapi/mgmapi_debug.h>
#include "mgmapi_internal.h"
#include "mgmcommon/NdbMgm.hpp"

TAPTEST(mgmapi) {
  // Check behaviour of error translation functions with NULL handle
  OK(ndb_mgm_get_latest_error(nullptr) == NDB_MGM_ILLEGAL_SERVER_HANDLE);
  OK(strcmp(ndb_mgm_get_latest_error_msg(nullptr), "Illegal server handle") ==
     0);
  OK(strcmp(ndb_mgm_get_latest_error_desc(nullptr), "") == 0);
  OK(ndb_mgm_get_latest_error_line(nullptr) == 0);

  // Check behaviour of functions with NULL handle
  // and not connected handle
  NdbMgmHandle h = nullptr;
  for (int lap = 1; lap <= 2; lap++) {
    switch (lap) {
      case 1:
        // Test with NULL handle
        assert(h == nullptr);
        break;

      case 2:
        // Create handle but don't connect
        h = ndb_mgm_create_handle();
        break;

      default:
        assert(false);
        break;
    }

    OK(ndb_mgm_disconnect(h) == -1);
    OK(ndb_mgm_get_status(h) == nullptr);
    OK(ndb_mgm_get_status2(h, nullptr) == nullptr);
    OK(ndb_mgm_get_status3(h, nullptr) == nullptr);
    OK(ndb_mgm_enter_single_user(h, 1, nullptr) == -1);
    OK(ndb_mgm_exit_single_user(h, nullptr) == -1);
    OK(ndb_mgm_stop(h, 1, nullptr) == -1);
    OK(ndb_mgm_stop2(h, 1, nullptr, 1) == -1);
    OK(ndb_mgm_stop3(h, 1, nullptr, 2, nullptr) == -1);
    OK(ndb_mgm_stop4(h, 1, nullptr, 2, 3, nullptr) == -1);
    OK(ndb_mgm_restart(h, 1, nullptr) == -1);
    OK(ndb_mgm_restart2(h, 1, nullptr, 2, 3, 4) == -1);
    OK(ndb_mgm_restart3(h, 1, nullptr, 2, 3, 4, nullptr) == -1);
    OK(ndb_mgm_restart4(h, 1, nullptr, 2, 3, 4, 5, nullptr) == -1);
    OK(ndb_mgm_get_clusterlog_severity_filter(h, nullptr, 1) == -1);
    OK(ndb_mgm_get_clusterlog_severity_filter_old(h) == nullptr);
    OK(ndb_mgm_set_clusterlog_severity_filter(h, NDB_MGM_EVENT_SEVERITY_ON, 1,
                                              nullptr) == -1);
    OK(ndb_mgm_get_clusterlog_loglevel(h, nullptr, 1) == -1);
    OK(ndb_mgm_get_clusterlog_loglevel_old(h) == nullptr);
    OK(ndb_mgm_set_clusterlog_loglevel(h, 1, NDB_MGM_EVENT_CATEGORY_STARTUP, 2,
                                       nullptr) == -1);
    OK(ndb_mgm_set_loglevel_node(h, 1, NDB_MGM_EVENT_CATEGORY_BACKUP, 2,
                                 nullptr) == -1);
    OK(ndb_mgm_dump_state(h, 1, nullptr, 2, nullptr) == -1);
    OK(ndb_mgm_get_configuration_from_node(h, 1) == nullptr);
    OK(ndb_mgm_start_signallog(h, 1, nullptr) == -1);
    OK(ndb_mgm_stop_signallog(h, 1, nullptr) == -1);
    OK(ndb_mgm_log_signals(h, 1, NDB_MGM_SIGNAL_LOG_MODE_IN, nullptr,
                           nullptr) == -1);
    OK(ndb_mgm_set_trace(h, 1, 2, nullptr) == -1);
    OK(ndb_mgm_insert_error(h, 1, 2, nullptr) == -1);
    OK(ndb_mgm_insert_error2(h, 1, 2, 3, nullptr) == -1);
    OK(ndb_mgm_start(h, 1, nullptr) == -1);
    OK(ndb_mgm_start_backup3(h, 1, nullptr, nullptr, 2, 3) == -1);
    OK(ndb_mgm_start_backup2(h, 1, nullptr, nullptr, 2) == -1);
    OK(ndb_mgm_start_backup(h, 1, nullptr, nullptr) == -1);
    OK(ndb_mgm_abort_backup(h, 1, nullptr) == -1);
    OK(ndb_mgm_get_configuration2(h, 1, NDB_MGM_NODE_TYPE_API, 2) == nullptr);
    OK(ndb_mgm_get_configuration(h, 1) == nullptr);

    OK(ndb_mgm_alloc_nodeid(h, 1, 2, 3) == -1);
    OK(ndb_mgm_set_int_parameter(h, 1, 2, 3, nullptr) == -1);
    OK(ndb_mgm_set_int64_parameter(h, 1, 2, 3, nullptr) == -1);
    OK(ndb_mgm_set_string_parameter(h, 1, 2, nullptr, nullptr) == -1);
    OK(ndb_mgm_purge_stale_sessions(h, nullptr) == -1);
    OK(ndb_mgm_check_connection(h) == -1);
    OK(ndb_mgm_set_connection_int_parameter(h, 1, 2, 3, 4) == -1);
    OK(ndb_mgm_get_connection_int_parameter(h, 1, 2, 3, nullptr) == -1);
    OK(ndb_mgm_get_mgmd_nodeid(h) == 0);  // Zero is an invalid nodeid
    OK(ndb_mgm_report_event(h, nullptr, 1) == -1);
    OK(ndb_mgm_end_session(h) == -1);
    OK(ndb_mgm_get_version(h, nullptr, nullptr, nullptr, 1, nullptr) == -1);
    OK(ndb_mgm_get_session_id(h) == 0);  // Zero is invalid zession id
    OK(ndb_mgm_get_session(h, 1, nullptr, nullptr) == -1);
    OK(ndb_mgm_set_configuration(h, nullptr) == -1);
    OK(ndb_mgm_create_nodegroup(h, nullptr, nullptr, nullptr) == -1);
    OK(ndb_mgm_drop_nodegroup(h, 1, nullptr) == -1);
    OK(ndb_mgm_dump_events(h, NDB_LE_Connected, 1, nullptr) == nullptr);
    OK(ndb_mgm_set_dynamic_ports(h, 1, nullptr, 2) == -1);
  }

  // Exceptions are these functions which does not check connected
  // since they don't communicate with the server, test only
  // with NULL handle
  OK(ndb_mgm_set_configuration_nodeid(nullptr, 1) == -1);
  OK(ndb_mgm_get_configuration_nodeid(nullptr) ==
     0);  // Zero is an invalid nodeid

  // Destroy handle
  ndb_mgm_destroy_handle(&h);

  // Check parsing of bind address, with and without port
  // Neither bindaddress or port is possible to check, only return code
  {
    ndb_mgm::handle_ptr handle(ndb_mgm_create_handle());
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost") == 0);
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost:12345") == 0);
    // Illegal values
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost:65536") == -1);
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost:-5") == -1);
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost:mysql") == -1);
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost:2147483648") == -1);
    OK(ndb_mgm_set_bindaddress(handle.get(), "localhost:-2147483649") == -1);
  }
  return 1;  // OK
}
