/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "zgroups.h"


#ifdef HAVE_UGID


Subgroup_coder::Subgroup_coder()
  : last_lgid(0), last_offset(0),
    last_sidno(0), last_group_commit(0),
    partial_groups(NULL), ended_groups(NULL),
    last_binlog_no(0), last_binlog_pos(0), last_binlog_length(0),
    last_binlog_offset_after_last_statement(0)
{
  my_init_dynamic_array(&sidno_array, sizeof(Gno_and_owner_type), 8, 8);
}


enum_append_status Subgroup_coder::append(
  Appender *appender, const Cached_subgroup *cs,
  rpl_binlog_no binlog_no, rpl_binlog_pos _binlog_pos,
  rpl_binlog_pos offset_after_last_statement,
  bool group_commit, uint32 owner_type)
{
  DBUG_ENTER("Subgroup_coder::append");
/*
  if (binlog_no != last_binlog_no)
*/
  uchar buf[2 + FULL_SUBGROUP_SIZE];
  uchar *p= buf;
  // write type code
  *p= SPECIAL_TYPE, p++;
  *p= FULL_SUBGROUP, p++;
  // write group information
  *p= cs->type, p++;
  int4store(p, cs->sidno); p+= 4;
  int8store(p, cs->gno); p+= 8;
  int8store(p, binlog_no); p+= 8;
  int8store(p, _binlog_pos); p+= 8;
  int8store(p, cs->binlog_length); p+= 8;
  int8store(p, offset_after_last_statement); p+= 8;
  int4store(p, owner_type); p+= 4;
  *p= cs->group_end ? 1 : 0, p++;
  *p= group_commit ? 1 : 0, p++;
  // append
  DBUG_ASSERT(p - buf == 2 + FULL_SUBGROUP_SIZE);
  PROPAGATE_APPEND_STATUS(appender->append(buf, p - buf));
  // update state
  last_lgid++;
  DBUG_RETURN(APPEND_OK);
}
#endif
