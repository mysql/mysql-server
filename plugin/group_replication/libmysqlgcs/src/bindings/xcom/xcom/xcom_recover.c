/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <assert.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_recover.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

extern task_env *boot;
extern task_env *net_boot;
extern task_env *net_recover;
extern task_env *killer;

extern synode_no executed_msg; /* The message we are waiting to execute */

start_t start_type;
int client_boot_done = 0;
int netboot_ok = 0;
int booting = 0;

/* purecov: begin deadcode */
void init_recover_vars() {
  start_type = IDLE;
  client_boot_done = 0;
  netboot_ok = 0;
  booting = 0;
}
/* purecov: end */

static synode_no log_start; /* Redo log from this synode */
static synode_no log_end;   /* Redo log until this synode */

static synode_no log_start; /* Redo log from this synode */
static synode_no log_end;   /* Redo log until this synode */

void xcom_recover_init() {
  log_start = null_synode;
  log_end = null_synode;
}

void set_log_group_id(uint32_t group_id) {
  log_start.group_id = group_id;
  log_end.group_id = group_id;
}

/* purecov: begin deadcode */
int log_prefetch_task(task_arg arg MY_ATTRIBUTE((unused))) {
  DECL_ENV
  int self;
  int n;
  END_ENV;

  TASK_BEGIN

  ep->self = 0;
  ep->n = 0;

  MAY_DBG(FN; NDBG(ep->self, d); NDBG(task_now(), f));

  assert(log_start.msgno != 0);

  while (net_recover && (!synode_gt(executed_msg, log_end))) {
    request_values(log_start, log_end);
    ep->n++;
    if (ep->n > 1) {
      G_WARNING("log_prefetch_task retry %d", ep->n);
    }
    TASK_DELAY(1.0);
  }
  FINALLY
  MAY_DBG(FN; STRLIT(" exit "); NDBG(ep->self, d); NDBG(task_now(), f));
  TASK_END;
}

int xcom_booted() { return get_maxnodes(get_site_def()) > 0 && netboot_ok; }
/* purecov: end */
