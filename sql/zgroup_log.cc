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


#include "mysqld_error.h"


int Group_log::open(const char *filename)
{
  DBUG_ENTER("Group_log::open(const char *)");
  int ret= group_log_file.open(filename, true);
  DBUG_RETURN(ret);
}


enum_return_status Group_log::write_subgroup(const Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::write_subgroup(const Subgroup *)");
  uchar *p= write_buf;

  if (!group_log_file.is_open())
  {
    my_error(ER_ERROR_ON_WRITE, MYF(0),
             group_log_file.get_last_filename(), errno);
    RETURN_REPORTED_ERROR;
  }

  *p= 255, p++;
  *p= 26, p++;
  *p= subgroup->type, p++;
#define PACK(FIELD, N) int ## N ## store(p, subgroup->FIELD); p+= N
  PACK(sidno, 4);
  PACK(gno, 8);
  PACK(binlog_no, 8);
  PACK(binlog_pos, 8);
  PACK(binlog_length, 8);
  PACK(binlog_offset_after_last_statement, 8);
  *p= subgroup->group_end ? 1 : 0, p++;
  *p= subgroup->group_commit ? 1 : 0, p++;

  read_state.lgid++;

  my_off_t len= p - write_buf;
  my_off_t written= group_log_file.append(len, write_buf);
  if (written != len)
  {
    /**
      If the thread is killed in the middle of the write, it is not
      safe to append more data after the partial write.  Hence we
      close it, and in the beginning of this function we don't assert
      that the file is open; we just return write error if it is
      closed.

      @todo we could try to truncate the file first, and if successful
      keep it open
    */
    group_log_file.close();
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


Group_log::Reader::Reader(Group_log *_group_log,
                          const Group_set *group_set,
                          rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos,
                          enum_read_status *status)
  : output_sid_map(group_set->get_sid_map()),
    group_log(_group_log),
    rot_file_reader(&group_log->group_log_file, 0),
    has_peeked(true)
{
  DBUG_ENTER("Group_log::Reader::Reader");
  bool found_group= false, found_pos= false;
  read_state.offset= 0;
  read_state.lgid= 0;
  do
  {
    *status= do_read_subgroup(&peeked_subgroup);
    if (*status != READ_OK)
      DBUG_VOID_RETURN;
    found_group= found_group || group_set->contains_group(peeked_subgroup.sidno,
                                                          peeked_subgroup.gno);
    found_pos= found_pos || (peeked_subgroup.binlog_no > binlog_no ||
                             (peeked_subgroup.binlog_no == binlog_no &&
                              peeked_subgroup.binlog_pos >= binlog_pos));
  } while (!found_pos || !found_group);
  DBUG_VOID_RETURN;
}


enum_read_status Group_log::Reader::do_read_subgroup(Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::Reader::do_read_subgroup(Subgroup *)");
  my_off_t read_bytes=
    rot_file_reader.read(Read_state::FULL_SUBGROUP_SIZE, read_buf, MYF(0));
  if (read_bytes == MY_FILE_ERROR)
  {
    my_error(ER_ERROR_ON_READ, MYF(0),
             rot_file_reader.get_current_filename(), errno);
    DBUG_RETURN(READ_ERROR_IO);
  }
  if (read_bytes == 0)
    DBUG_RETURN(READ_EOF);
  if (read_buf[0] != Read_state::SPECIAL_TYPE ||
      (read_bytes >= 2 && read_buf[1] != Read_state::FULL_SUBGROUP))
  {
    rot_file_reader.seek(rot_file_reader.tell() - read_bytes);
    my_error(ER_FILE_FORMAT, MYF(0), rot_file_reader.get_current_filename());
    DBUG_RETURN(READ_ERROR_IO);
  }
  if (read_bytes < Read_state::FULL_SUBGROUP_SIZE)
  {
    rot_file_reader.seek(rot_file_reader.tell() - read_bytes);
    DBUG_RETURN(READ_TRUNCATED);
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
  subgroup->lgid= ++read_state.lgid;
  const Sid_map *log_sid_map= group_log->get_sid_map();
  if (output_sid_map != log_sid_map)
  {
    const rpl_sid *sid= log_sid_map->sidno_to_sid(subgroup->sidno);
    if (sid == NULL)
    {
      rot_file_reader.seek(rot_file_reader.tell() - read_bytes);
      my_error(ER_FILE_FORMAT, MYF(0), rot_file_reader.get_current_filename());
      DBUG_RETURN(READ_ERROR_IO);
    }
    subgroup->sidno= output_sid_map->add_permanent(sid);
    if (subgroup->sidno < 1)
      DBUG_RETURN(READ_ERROR_OTHER);
  }
  DBUG_RETURN(READ_OK);
}


enum_read_status Group_log::Reader::read_subgroup(Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::Reader::read_subgroup(Subgroup *)");
  if (has_peeked)
  {
    *subgroup= peeked_subgroup;
    has_peeked= false;
    DBUG_RETURN(READ_OK);
  }
  enum_read_status status= do_read_subgroup(subgroup);
  DBUG_RETURN(status);
}


#endif
