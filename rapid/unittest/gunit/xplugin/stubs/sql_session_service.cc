/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "mysql/service_srv_session.h"

#ifdef __cplusplus
extern "C" {
#endif

  int srv_session_init_thread(const void *p)
  {
    assert(0);
    return 0;
  }

  void srv_session_deinit_thread()
  {
    assert(0);
  }

  MYSQL_THD srv_session_info_get_thd(MYSQL_SESSION session)
  {
    assert(0);
    return 0;
  }

  MYSQL_SESSION srv_session_open(srv_session_error_cb errok_cb,
                                                    void *plugin_ctx)
  {
    assert(0);
    return 0;
  }

  int srv_session_close(MYSQL_SESSION session)
  {
    assert(0);
    return 0;
  }

  int srv_session_detach(MYSQL_SESSION session)
  {
    assert(0);
    return 0;
  }

  my_thread_id srv_session_info_get_session_id(MYSQL_SESSION)
  {
    assert(0);
    return 0;
  }

  int srv_session_server_is_available()
  {
    assert(0);
    return 0;
  }

  int srv_session_info_set_client_port(Srv_session *session, uint16_t port)
  {
    assert(0);
    return 0;
  }

  int srv_session_info_set_connection_type(Srv_session *session, enum_vio_type type)
  {
    assert(0);
    return 0;
  }

  int srv_session_info_killed(MYSQL_SESSION)
  {
    assert(0);
    return 0;
  }
#ifdef __cplusplus
}
#endif
