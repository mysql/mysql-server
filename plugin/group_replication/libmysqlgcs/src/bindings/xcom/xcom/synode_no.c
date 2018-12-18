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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"

#include <assert.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xdr_utils.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

int synode_eq(synode_no x, synode_no y) {
  return x.group_id == y.group_id && x.msgno == y.msgno && x.node == y.node;
}

int synode_lt(synode_no x, synode_no y) {
  assert(x.group_id == 0 || y.group_id == 0 || x.group_id == y.group_id);
  return (x.msgno < y.msgno) || (x.msgno == y.msgno && x.node < y.node);
}

int synode_gt(synode_no x, synode_no y) {
  assert(x.group_id == 0 || y.group_id == 0 || x.group_id == y.group_id);
  return (x.msgno > y.msgno) || (x.msgno == y.msgno && x.node > y.node);
}

#if 0
synode_no vp_count_to_synode(u_long high, u_long low, node_no nodeid,
                             uint32_t groupid)
{
  synode_no ret;

  ret.group_id = groupid;
  ret.msgno = (((u_longlong_t) high) << 32) | low;
  ret.node = nodeid;
  return ret;
}
#endif

/* purecov: begin deadcode */
synode_no vp_count_to_synode(u_long high, u_long low, node_no nodeid,
                             uint32_t groupid) {
  synode_no ret;

  ret.group_id = groupid;
  ret.msgno = (((uint64_t)high) << 32) | low;
  ret.node = nodeid;
  return ret;
}

#ifdef TASK_EVENT_TRACE
void add_synode_event(synode_no const synode) {
  add_unpad_event(string_arg("{"));
  add_event(uint_arg(synode.group_id));
  add_event(ulong_long_arg(synode.msgno));
  add_unpad_event(ulong_arg(synode.node));
  add_event(string_arg("}"));
}
#endif

void synode_array_move(synode_no_array *const to, synode_no_array *const from) {
  if (to->synode_no_array_val != NULL) free(to->synode_no_array_val);

  *to = *from;

  from->synode_no_array_len = 0;
  from->synode_no_array_val = NULL;
}

void synode_app_data_array_move(synode_app_data_array *const to,
                                synode_app_data_array *const from) {
  if (to->synode_app_data_array_val != NULL) {
    free(to->synode_app_data_array_val);
  }

  *to = *from;

  from->synode_app_data_array_len = 0;
  from->synode_app_data_array_val = NULL;
}

/* purecov: end */
