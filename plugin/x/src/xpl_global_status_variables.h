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

#ifndef _XPL_GLOBAL_STATUS_VARIABLES_H_
#define _XPL_GLOBAL_STATUS_VARIABLES_H_

#include "plugin/x/ngs/include/ngs/common_status_variables.h"

namespace xpl {

class Global_status_variables : public ngs::Common_status_variables {
 public:
  static Global_status_variables &instance() {
    static Global_status_variables singleton;

    return singleton;
  }

  void reset() { *this = Global_status_variables(); }

  Variable m_sessions_count;
  Variable m_worker_thread_count;
  Variable m_active_worker_thread_count;
  Variable m_closed_sessions_count;
  Variable m_sessions_fatal_errors_count;
  Variable m_init_errors_count;
  Variable m_closed_connections_count;
  Variable m_accepted_connections_count;
  Variable m_rejected_connections_count;
  Variable m_connection_errors_count;
  Variable m_connection_accept_errors_count;
  Variable m_accepted_sessions_count;
  Variable m_rejected_sessions_count;
  Variable m_killed_sessions_count;
  Variable m_aborted_clients;

 private:
  Global_status_variables() {}
  Global_status_variables(const Global_status_variables &);
};

}  // namespace xpl

#endif  // _XPL_GLOBAL_STATUS_VARIABLES_H_
