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
  int ret= rot_file.open(filename, true);
  DBUG_RETURN(ret);
}


enum_return_status
Group_log::write_subgroup(const Cached_subgroup *subgroup,
                          rpl_binlog_pos offset_after_last_statement,
                          bool group_commit, const THD *thd)
{
  DBUG_ENTER("Group_log::write_subgroup(const Subgroup *)");

  if (!rot_file.is_open())
  {
    my_error(ER_ERROR_ON_WRITE, MYF(0), rot_file.get_source_name(), errno);
    RETURN_REPORTED_ERROR;
  }

  Rpl_owner_id id;
  id.copy_from(thd);
  if (encoder.append(&rot_file, subgroup, offset_after_last_statement,
                     group_commit, id.owner_type) != APPEND_OK)
  {
    rot_file.close();
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


Group_log::Group_log_reader::Group_log_reader(
  Group_log *_group_log, const Group_set *group_set,
  rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos, enum_read_status *status)
  : output_sid_map(group_set->get_sid_map()),
    group_log(_group_log),
    rot_file_reader(&group_log->rot_file, 0),
    has_peeked(true)
{
  DBUG_ENTER("Group_log::Reader::Reader");
  bool found_group= false, found_pos= false;
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


enum_read_status Group_log::Group_log_reader::do_read_subgroup(
  Subgroup *subgroup)
{
  DBUG_ENTER("Group_log::Reader::do_read_subgroup(Subgroup *)");
  my_off_t saved_pos;
  if (rot_file_reader.tell(&saved_pos) != RETURN_STATUS_OK)
    DBUG_RETURN(READ_ERROR);
  PROPAGATE_READ_STATUS(decoder.read(&rot_file_reader, subgroup));
  const Sid_map *log_sid_map= group_log->get_sid_map();
  if (output_sid_map != log_sid_map)
  {
    const rpl_sid *sid= log_sid_map->sidno_to_sid(subgroup->sidno);
    READER_CHECK_FORMAT(&rot_file_reader, saved_pos, sid != NULL);
    subgroup->sidno= output_sid_map->add_permanent(sid);
    READER_CHECK_FORMAT(&rot_file_reader, saved_pos, subgroup->sidno < 1);
  }
  DBUG_RETURN(READ_OK);
}


enum_read_status Group_log::Group_log_reader::read_subgroup(Subgroup *subgroup)
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
