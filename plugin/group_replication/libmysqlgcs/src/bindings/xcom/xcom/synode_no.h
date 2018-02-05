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

#ifndef SYNODE_NO_H
#define SYNODE_NO_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_vp.h.gen"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xdr_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILENAME_SYNODE_FMT "%x_%PRIu64%u"
#define FILENAME_SYNODE_MEM(x) SY_MEM(x)
#define NULL_SYNODE \
  { 0ul, 0ull, 0ull }
#define SY_FMT "{" SY_FMT_DEF "}"
#define SY_FMT_DEF "%x %" PRIu64 " %u"
#define SY_MEM(s) (s).group_id, (uint64_t)(s).msgno, (s).node

int synode_eq(synode_no x, synode_no y);
int synode_gt(synode_no x, synode_no y);
int synode_lt(synode_no x, synode_no y);
static const synode_no null_synode = NULL_SYNODE;
synode_no vp_count_to_synode(u_long high, u_long low, node_no nodeid,
                             uint32_t groupid);
void add_synode_event(synode_no const synode);

#ifdef __cplusplus
}
#endif

#endif
