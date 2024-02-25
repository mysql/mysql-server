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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include "xcom/xcom_interface.h"

#include <assert.h>
#include <stdlib.h>

#include "xcom/app_data.h"
#include "xcom/bitset.h"
#include "xcom/node_list.h"
#include "xcom/node_no.h"
#include "xcom/node_set.h"
#include "xcom/pax_msg.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_def.h"
#include "xcom/site_struct.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_cache.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_transport.h"
#include "xcom/xcom_vp_str.h"
#include "xdr_gen/xcom_vp.h"

static xcom_full_data_receiver xcom_full_receive_data;
static xcom_full_local_view_receiver xcom_full_receive_local_view;
static xcom_full_global_view_receiver xcom_full_receive_global_view;

static xcom_data_receiver xcom_receive_data;
static xcom_local_view_receiver xcom_receive_local_view;
static xcom_global_view_receiver xcom_receive_global_view;

#ifdef XCOM_STANDALONE
/* purecov: begin deadcode */
void set_xcom_full_data_receiver(xcom_full_data_receiver x) {
  xcom_full_receive_data = x;
}
/* purecov: end */

/* purecov: begin deadcode */
void set_xcom_full_local_view_receiver(xcom_full_local_view_receiver x) {
  xcom_full_receive_local_view = x;
}
/* purecov: end */

/* purecov: begin deadcode */
void set_xcom_full_global_view_receiver(xcom_full_global_view_receiver x) {
  xcom_full_receive_global_view = x;
}
/* purecov: end */
#endif

void set_xcom_data_receiver(xcom_data_receiver x) { xcom_receive_data = x; }

void set_xcom_local_view_receiver(xcom_local_view_receiver x) {
  xcom_receive_local_view = x;
}

void set_xcom_global_view_receiver(xcom_global_view_receiver x) {
  xcom_receive_global_view = x;
}

static xcom_config_receiver xcom_receive_config = nullptr;

/* purecov: begin deadcode */
void set_xcom_config_receiver(xcom_config_receiver x) {
  xcom_receive_config = x;
}
/* purecov: end */

xcom_logger xcom_log = nullptr;
xcom_debugger xcom_debug = nullptr;
xcom_debugger_check xcom_debug_check = nullptr;
int64_t xcom_debug_options = GCS_DEBUG_NONE;

void set_xcom_logger(xcom_logger x) { xcom_log = x; }

void set_xcom_debugger(xcom_debugger x) { xcom_debug = x; }

void set_xcom_debugger_check(xcom_debugger_check x) { xcom_debug_check = x; }

/* Deliver message to application */

void deliver_to_app(pax_machine *pma, app_data_ptr app,
                    delivery_status app_status) {
  site_def const *site = nullptr;
  int full_doit = xcom_full_receive_data != nullptr;
  int doit = (xcom_receive_data != nullptr && app_status == delivery_ok);

  if (app_status == delivery_ok) {
    if (!pma) {
      g_critical(
          "A fatal error ocurred that prevents XCom from delivering a message "
          "that achieved consensus. XCom cannot proceed without compromising "
          "correctness. XCom will now crash.");
    }
    assert(pma && "pma must not be a null pointer");
  }

  if (!(full_doit || doit)) return;

  IFDBG(D_NONE, FN; PTREXP(pma); PTREXP(app); NDBG(app_status, d);
        COPY_AND_FREE_GOUT(dbg_app_data(app)));
  if (pma)
    site = find_site_def(pma->synode);
  else
    site = get_site_def();

  while (app) {
    if (app->body.c_t == app_type) { /* Decode application data */
      if (!(app->unique_id.node == app->app_key.node &&
            app->unique_id.msgno == app->app_key.msgno)) {
        IFDBG(D_BASE, FN; if (pma) SYCEXP(pma->synode); SYCEXP(app->unique_id);
              SYCEXP(app->app_key));
      }
      if (full_doit) {
        /* purecov: begin deadcode */
        xcom_full_receive_data(site, pma, app, app_status);
        /* purecov: end */
      } else {
        if (doit) {
          u_int copy_len = 0;
          char *copy = (char *)xcom_malloc(app->body.app_u_u.data.data_len);
          if (copy == nullptr) {
            /* purecov: begin inspected */
            G_ERROR("Unable to allocate memory for the received message.");
            /* purecov: end */
          } else {
            memcpy(copy, app->body.app_u_u.data.data_val,
                   app->body.app_u_u.data.data_len);
            copy_len = app->body.app_u_u.data.data_len;
          }
          ADD_DBG(D_EXEC, add_synode_event(pma->synode););
          synode_no origin = pma->synode;
          origin.node = app->unique_id.node;
          xcom_receive_data(pma->synode, origin, site, detector_node_set(site),
                            copy_len, cache_get_last_removed(), copy);
        } else {
          /* purecov: begin deadcode */
          G_TRACE("Data message was not delivered.");
          /* purecov: end */
        }
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
    if (xcom_full_receive_local_view) {
      /* purecov: begin deadcode */
      xcom_full_receive_local_view(site, detector_node_set(site));
      /* purecov: end */
    } else if (xcom_receive_local_view) {
      xcom_receive_local_view(site->start, detector_node_set(site));
    }
  }
}

#ifdef SUPPRESS_DUPLICATE_VIEWS
static node_set delivered_node_set;
static site_def const *delivered_site;

static int not_duplicate_view(site_def const *site, node_set const ns) {
  int retval;
  retval = !(site == delivered_site && equal_node_set(delivered_node_set, ns));
  delivered_site = site;
  copy_node_set(&ns, &delivered_node_set);
  return retval;
}
#endif

void deliver_global_view_msg(site_def const *site, node_set const ns,
                             synode_no message_id) {
  if (site) {
#ifdef SUPPRESS_DUPLICATE_VIEWS
    if (not_duplicate_view(site, ns)) {
#endif
      if (xcom_full_receive_global_view) {
        /* purecov: begin deadcode */
        xcom_full_receive_global_view(site, message_id, clone_node_set(ns));
        /* purecov: end */
      } else if (xcom_receive_global_view) {
        xcom_receive_global_view(site->start, message_id, clone_node_set(ns),
                                 site->event_horizon);
      }
#ifdef SUPPRESS_DUPLICATE_VIEWS
    }
#endif
  }
}

void deliver_config(app_data_ptr a) {
  if (xcom_receive_config) {
    /* purecov: begin deadcode */
    xcom_receive_config(a);
    /* purecov: end */
  }
}

void deinit_xcom_interface() {
#ifdef SUPPRESS_DUPLICATE_VIEWS
  free_node_set(&delivered_node_set);
#endif
}
