/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "mysql/service_srv_session.h"

#ifdef __cplusplus
extern "C" {
#endif

  int srv_session_init_thread(const void *p)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  void srv_session_deinit_thread()
  {
    DBUG_ASSERT(0);
  }

  MYSQL_THD srv_session_info_get_thd(MYSQL_SESSION session)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  MYSQL_SESSION srv_session_open(srv_session_error_cb errok_cb,
                                                    void *plugin_ctx)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  int srv_session_close(MYSQL_SESSION session)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  int srv_session_detach(MYSQL_SESSION session)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  my_thread_id srv_session_info_get_session_id(MYSQL_SESSION)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  int srv_session_server_is_available()
  {
    DBUG_ASSERT(0);
    return 0;
  }

  int srv_session_info_set_client_port(Srv_session *session, uint16_t port)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  int srv_session_info_set_connection_type(Srv_session *session, enum_vio_type type)
  {
    DBUG_ASSERT(0);
    return 0;
  }

  int srv_session_info_killed(MYSQL_SESSION)
  {
    DBUG_ASSERT(0);
    return 0;
  }
#ifdef __cplusplus
}
#endif
