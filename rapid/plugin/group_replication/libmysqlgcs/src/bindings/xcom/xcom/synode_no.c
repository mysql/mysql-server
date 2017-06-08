/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#include "simset.h"
#include "task.h"
#include "task_debug.h"
#include "xcom_common.h"
#include "xcom_profile.h"
#include "xcom_vp.h"
#include "xdr_utils.h"

#include "synode_no.h"

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
define_xdr_funcs(synode_no)

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

/* purecov: end */
