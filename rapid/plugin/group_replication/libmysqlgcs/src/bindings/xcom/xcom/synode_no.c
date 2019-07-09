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

#include <rpc/rpc.h>
#include <assert.h>
#include <stdlib.h>

#include "xcom_common.h"
#include "xdr_utils.h"
#include "simset.h"
#include "xcom_vp.h"
#include "task.h"
#include "task_debug.h"

#include "synode_no.h"

#if 0
static void init_synode_no(synode_no *synode, uint32_t group_id,
                           synode_cnt msgno, unsigned int node)
{
  synode->group_id = group_id;
  synode->msgno = msgno;
  synode->node = node;
}
#endif

int	synode_eq(synode_no x, synode_no y)
{
  return x.group_id == y.group_id &&
    x.msgno == y.msgno &&
    x.node == y.node;
}

int	synode_lt(synode_no x, synode_no y)
{
  assert(x.group_id == 0 || y.group_id == 0 || x.group_id == y.group_id);
  return (x.msgno < y.msgno) ||
    (x.msgno == y.msgno &&
     x.node < y.node);
}

int	synode_gt(synode_no x, synode_no y)
{
  assert(x.group_id == 0 || y.group_id == 0 || x.group_id == y.group_id);
  return (x.msgno > y.msgno) ||
    (x.msgno == y.msgno &&
     x.node > y.node);
}

/* purecov: begin deadcode */
synode_no vp_count_to_synode(u_long high, u_long low, node_no nodeid,
                             uint32_t groupid)
{
  synode_no ret;

  ret.group_id = groupid;
  ret.msgno = (((uint64_t) high) << 32) | low;
  ret.node = nodeid;
  return ret;
}

void	add_synode_event(synode_no const synode)
{
	add_unpad_event(string_arg("{"));
	add_event(uint_arg(synode.group_id));
	add_event(ulong_long_arg(synode.msgno));
	add_unpad_event(ulong_arg(synode.node));
	add_event(string_arg("}"));
}
/* purecov: end */

