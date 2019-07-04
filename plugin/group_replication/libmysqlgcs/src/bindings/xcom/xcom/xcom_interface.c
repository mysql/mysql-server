/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_interface.h"

#include <assert.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/bitset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/pax_msg.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_cache.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_vp_str.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

static xcom_data_receiver xcom_receive_data;
static xcom_local_view_receiver xcom_receive_local_view;
static xcom_global_view_receiver xcom_receive_global_view;
xcom_logger xcom_log = NULL;
xcom_debugger xcom_debug = NULL;
xcom_debugger_check xcom_debug_check = NULL;
int64_t xcom_debug_options = GCS_DEBUG_NONE;

void set_xcom_data_receiver(xcom_data_receiver x) { xcom_receive_data = x; }

void set_xcom_local_view_receiver(xcom_local_view_receiver x) {
  xcom_receive_local_view = x;
}

void set_xcom_global_view_receiver(xcom_global_view_receiver x) {
  xcom_receive_global_view = x;
}

void set_xcom_logger(xcom_logger x) { xcom_log = x; }

void set_xcom_debugger(xcom_debugger x) { xcom_debug = x; }

void set_xcom_debugger_check(xcom_debugger_check x) { xcom_debug_check = x; }

/* {{{ Deliver message to application */

void deliver_to_app(pax_machine *pma, app_data_ptr app,
                    delivery_status app_status) {
  site_def const *site = 0;

  DBGOUT(FN; PTREXP(pma); PTREXP(app); NDBG(app_status, d);
         COPY_AND_FREE_GOUT(dbg_app_data(app)));
  if (pma)
    site = find_site_def(pma->synode);
  else
    site = get_site_def();
  while (app) {
    DBGOUT(FN; STREXP(cargo_type_to_str(app->body.c_t)));
    if (app->body.c_t == app_type) { /* Decode application data */
      if (app_status == delivery_ok) {
        char *copy = malloc(app->body.app_u_u.data.data_len);
        if (copy == NULL && app->body.app_u_u.data.data_len != 0) {
          app->body.app_u_u.data.data_len = 0;
          G_ERROR("Unable to allocate memory for the received message.");
        } else
          memcpy(copy, app->body.app_u_u.data.data_val,
                 app->body.app_u_u.data.data_len);
        ADD_EVENTS(add_synode_event(pma->synode););

        xcom_receive_data(pma->synode, detector_node_set(site),
                          app->body.app_u_u.data.data_len,
                          cache_get_last_removed(), copy);
      } else {
        G_TRACE("Data message was not delivered.");
      }
    } else if (app_status == delivery_ok) {
      G_ERROR("Data message has wrong type %s ",
              cargo_type_to_str(app->body.c_t));
    }
    app = app->next;
  }
}

/**
   Deliver a view message
*/

void deliver_view_msg(site_def const *site) {
  if (site) {
    xcom_receive_local_view(site->start, detector_node_set(site));
  }
}

void deliver_global_view_msg(site_def const *site, synode_no message_id) {
  if (site) {
    xcom_receive_global_view(site->start, message_id,
                             clone_node_set(site->global_node_set),
                             site->event_horizon);
  }
}

/* }}} */
