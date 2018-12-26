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

#ifndef SYNODE_NO_H
#define SYNODE_NO_H

#ifdef __cplusplus
extern "C" {
#endif


#define FILENAME_SYNODE_FMT "%x_%llu_%u"
#define FILENAME_SYNODE_MEM(x) SY_MEM(x)
#define NULL_SYNODE {0ul,0ull,0ull}
#define SY_FMT "{" SY_FMT_DEF "}"
#define SY_FMT_DEF "%x %llu %u"
#define SY_MEM(s) (s).group_id, (long long unsigned int)(s).msgno, (s).node

int	synode_eq(synode_no x, synode_no y);
int	synode_gt(synode_no x, synode_no y);
int	synode_lt(synode_no x, synode_no y);
static const synode_no null_synode = NULL_SYNODE;
synode_no vp_count_to_synode(u_long high, u_long low, node_no nodeid, uint32_t groupid);
void	add_synode_event(synode_no const synode);

#ifdef __cplusplus
}
#endif

#endif

