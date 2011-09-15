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


enum_group_status Group_log::open(const char *filename)
{
  DBUG_ENTER("Group_log::open(const char *)");
  enum_group_status ret= group_log_file.open(filename, true);
  DBUG_RETURN(ret);
}


enum_group_status Group_log::write_subgroup(const Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::write_subgroup(const Subgroup *)");
  uchar *p= write_buf;

  *p= 255, p++;
  *p= 26, p++;
  *p= subgroup->type, p++;
  int4store(p, subgroup->sidno); p+= 4;
  int8store(p, subgroup->gno); p+= 8;
  int8store(p, subgroup->binlog_no); p+= 8;
  int8store(p, subgroup->binlog_pos); p+= 8;
  int8store(p, subgroup->binlog_length); p+= 8;
  int8store(p, subgroup->binlog_offset_after_last_statement); p+= 8;
  *p= subgroup->group_end ? 1 : 0, p++;
  *p= subgroup->group_commit ? 1 : 0, p++;

  read_state.lgid++;

  my_off_t len= p - write_buf;
  my_off_t written= group_log_file.append(len, write_buf);
  DBUG_RETURN(written == len ? GS_SUCCESS : GS_ERROR_IO);
}


Group_log::Reader::Reader(Group_log *group_log,
                          const Group_set *group_set,
                          rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos,
                          enum_group_status *status)
  : sid_map(group_set->get_sid_map()),
    rot_file_reader(&group_log->group_log_file, 0),
    has_peeked(true)
{
  DBUG_ENTER("Group_log::Reader::Reader");
  do
  {
    *status= do_read_subgroup(&peeked_subgroup);
    if (*status != GS_SUCCESS)
      DBUG_VOID_RETURN;
  } while ((peeked_subgroup.binlog_no < binlog_no ||
            (peeked_subgroup.binlog_no == binlog_no &&
             peeked_subgroup.binlog_pos < binlog_pos)) ||
           !group_set->contains_group(peeked_subgroup.sidno,
                                      peeked_subgroup.gno));
  DBUG_VOID_RETURN;
}


enum_group_status Group_log::Reader::do_read_subgroup(Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::Reader::do_read_subgroup(Subgroup *)");
  my_off_t read_bytes=
    rot_file_reader.read(Read_state::FULL_SUBGROUP_SIZE, read_buf);
  if (read_bytes < 0)
    DBUG_RETURN(GS_ERROR_IO);
  if (read_bytes == 0)
    DBUG_RETURN(GS_END_OF_FILE);
  if (read_buf[0] != Read_state::SPECIAL_TYPE)
    DBUG_RETURN(GS_ERROR_PARSE);
  if (read_bytes == 1)
  {
    rot_file_reader.seek(rot_file_reader.tell() - read_bytes);
    DBUG_RETURN(GS_END_OF_FILE);
  }
  if (read_buf[1] != Read_state::FULL_SUBGROUP)
  {
    rot_file_reader.seek(rot_file_reader.tell() - read_bytes);
    DBUG_RETURN(GS_ERROR_PARSE);
  }
  if (read_bytes != Read_state::FULL_SUBGROUP_SIZE)
  {
    rot_file_reader.seek(rot_file_reader.tell() - read_bytes);
    DBUG_RETURN(GS_END_OF_FILE);
  }
#define UNPACK(FIELD, N) subgroup->FIELD= sint ## N ## korr(p), p += N
  uchar *p= read_buf;
  UNPACK(sidno, 4);
  UNPACK(gno, 8);
  UNPACK(binlog_no, 8);
  UNPACK(binlog_pos, 8);
  UNPACK(binlog_length, 8);
  UNPACK(binlog_offset_after_last_statement, 8);
  subgroup->group_end= *p == 1 ? true : false, p++;
  subgroup->group_commit= *p == 1 ? true : false, p++;
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_log::Reader::read_subgroup(Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::Reader::read_subgroup(Subgroup *)");
  if (has_peeked)
  {
    *subgroup= peeked_subgroup;
    has_peeked= false;
    DBUG_RETURN(GS_SUCCESS);
  }
  enum_group_status status= do_read_subgroup(subgroup);
  DBUG_RETURN(status);
}


#endif
