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


enum_return_status Group_log::open(const char *filename, bool writable)
{
  DBUG_ENTER("Group_log::open(const char *)");
  PROPAGATE_REPORTED_ERROR(rot_file.open(filename, writable));
  RETURN_OK;
}


enum_return_status Group_log::close()
{
  DBUG_ENTER("Group_log::close()");
  PROPAGATE_REPORTED_ERROR(rot_file.close());
  RETURN_OK;
}


#ifndef MYSQL_CLIENT
enum_return_status
Group_log::write_subgroup(const THD *thd, const Cached_subgroup *subgroup,
                          rpl_binlog_no binlog_no, rpl_binlog_pos binlog_pos,
                          rpl_binlog_pos offset_after_last_statement,
                          bool group_commit)
{
  DBUG_ENTER("Group_log::write_subgroup(const Subgroup *)");

  if (!rot_file.is_open())
  {
    BINLOG_ERROR(("Error writing file '%-.200s' (errno: %d)",
                  rot_file.get_source_name(), errno),
                 (ER_ERROR_ON_WRITE, MYF(0), rot_file.get_source_name(),
                  errno));
    RETURN_REPORTED_ERROR;
  }

  Rpl_owner_id id;
  id.copy_from(thd);
  if (encoder.append(&rot_file, subgroup, binlog_no, binlog_pos,
                     offset_after_last_statement, group_commit,
                     id.owner_type) != APPEND_OK)
  {
    rot_file.close();
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}
#endif // ifndef MYSQL_CLIENT


Group_log::Group_log_reader::Group_log_reader(Group_log *_group_log,
                                              Sid_map *_output_sid_map)
  : output_sid_map(_output_sid_map),
    group_log(_group_log),
    rot_file_reader(&group_log->rot_file, 0),
    has_peeked(false)
{
  DBUG_ASSERT(output_sid_map != NULL);
}


enum_read_status Group_log::Group_log_reader::seek(const Group_set *_group_set,
                                                   bool _exclude,
                                                   bool _include_anonymous,
                                                   rpl_lgid _first_lgid,
                                                   rpl_lgid _last_lgid,
                                                   rpl_binlog_no binlog_no,
                                                   rpl_binlog_pos binlog_pos)
{
  DBUG_ENTER("Group_log::Group_log_reader::seek");
  DBUG_ASSERT(group_set == NULL || output_sid_map == group_set->get_sid_map());

  group_set= _group_set;
  exclude_group_set= _exclude;
  include_anonymous= _include_anonymous;
  first_lgid= _first_lgid;
  last_lgid= _last_lgid;
  do
  {
    PROPAGATE_READ_STATUS(do_read_subgroup(&peeked_subgroup, NULL));
    has_peeked= true;
  } while (binlog_no != 0 &&
           (peeked_subgroup.binlog_no < binlog_no ||
            (peeked_subgroup.binlog_no == binlog_no &&
             peeked_subgroup.binlog_pos < binlog_pos)));
  DBUG_RETURN(READ_OK);
}


bool Group_log::Group_log_reader::subgroup_in_valid_set(Subgroup *subgroup)
{
  if (first_lgid != -1 && subgroup->lgid < first_lgid)
    return false;
  if (last_lgid != -1 && subgroup->lgid > last_lgid)
    return false;
  if (group_set == NULL)
    return true;
  if (subgroup->type == ANONYMOUS_SUBGROUP)
    return include_anonymous;
  return group_set->contains_group(subgroup->sidno, subgroup->gno) !=
    exclude_group_set;
}


enum_read_status Group_log::Group_log_reader::do_read_subgroup(
  Subgroup *subgroup, uint32 *owner_type)
{
  DBUG_ENTER("Group_log::Reader::do_read_subgroup(Subgroup *)");

  do
  {
    // read one subgroup
    PROPAGATE_READ_STATUS(decoder.read(&rot_file_reader, subgroup, owner_type));
    const Sid_map *log_sid_map= group_log->get_sid_map();
    if (output_sid_map != log_sid_map)
    {
      const rpl_sid *sid= log_sid_map->sidno_to_sid(subgroup->sidno);
      READER_CHECK_FORMAT(&rot_file_reader, sid != NULL);
      subgroup->sidno= output_sid_map->add_permanent(sid);
      READER_CHECK_FORMAT(&rot_file_reader, subgroup->sidno < 1);
    }
    // skip sub-groups that are outside the valid set of groups
  } while (!subgroup_in_valid_set(subgroup));

  DBUG_RETURN(READ_OK);
}


enum_read_status Group_log::Group_log_reader::read_subgroup(
  Subgroup *subgroup, uint32 *owner_type)
{
  DBUG_ENTER("Group_log::Reader::read_subgroup(Subgroup *)");
  if (has_peeked)
  {
    *subgroup= peeked_subgroup;
    has_peeked= false;
  }
  else
    PROPAGATE_READ_STATUS(do_read_subgroup(subgroup, owner_type));
  DBUG_RETURN(READ_OK);
}


enum_read_status Group_log::Group_log_reader::peek_subgroup(
  Subgroup **subgroup_p, uint32 *owner_type)
{
  DBUG_ENTER("Group_log::Reader::read_subgroup(Subgroup *)");
  if (!has_peeked)
  {
    PROPAGATE_READ_STATUS(do_read_subgroup(&peeked_subgroup, owner_type));
    has_peeked= true;
  }
  *subgroup_p= &peeked_subgroup;
  DBUG_RETURN(READ_OK);
}


#endif
