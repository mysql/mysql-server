/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "rpl_gtid.h"
#include "sql_class.h"
#include "binlog.h"


Group_cache::Group_cache()
{
  DBUG_ENTER("Group_cache::Group_cache");
  my_init_dynamic_array(&groups, sizeof(Cached_group), 8, 8);
  DBUG_VOID_RETURN;
}


Group_cache::~Group_cache()
{
  DBUG_ENTER("Group_cache::~Group_cache");
  delete_dynamic(&groups);
  DBUG_VOID_RETURN;
}


void Group_cache::clear()
{
  DBUG_ENTER("Group_cache::clear");
  groups.elements= 0;
  DBUG_VOID_RETURN;
}


Group_cache::enum_add_group_status
Group_cache::add_logged_group(const THD *thd, my_off_t binlog_offset)
{
  DBUG_ENTER("Group_cache::add_logged_group(THD *, my_off_t)");
  const Gtid_specification &spec= thd->variables.gtid_next;
  DBUG_ASSERT(spec.type != UNDEFINED_GROUP);
  // merge with previous group if possible
  Cached_group *prev= get_last_group();
  if (prev != NULL && prev->spec.equals(spec))
    DBUG_RETURN(EXTEND_EXISTING_GROUP);
  // otherwise add a new group
  Cached_group *group= allocate_group();
  if (group ==  NULL)
    DBUG_RETURN(ERROR);
  group->spec= spec;
  group->binlog_offset= binlog_offset;
  // Update the internal status of this Group_cache (see comment above
  // definition of enum_group_cache_type).
  if (group->spec.type == GTID_GROUP)
  {
    /*
      @todo: currently group_is_logged() requires a linear scan
      through the cache. if this becomes a performance problem, we can
      add a Gtid_set Group_cache::logged_groups and add logged groups
      to it here. /Sven
    */
  }
  DBUG_RETURN(APPEND_NEW_GROUP);
}


bool Group_cache::contains_gtid(const Gtid &gtid) const
{
  int n_groups= get_n_groups();
  for (int i= 0; i < n_groups; i++)
  {
    const Cached_group *group= get_unsafe_pointer(i);
    if (group->spec.equals(gtid))
      return true;
  }
  return false;
}


/*
  Apparently this code is not being called. We need to
  investigate if this is a bug or this code is not
  necessary. /Alfranio
*/
#ifdef NON_ERROR_GTID
Group_cache::enum_add_group_status
Group_cache::add_empty_group(const Gtid &gtid)
{
  DBUG_ENTER("Group_cache::add_empty_group");
  // merge with previous group if possible
  Cached_group *prev= get_last_group();
  if (prev != NULL && prev->spec.equals(gtid))
    DBUG_RETURN(EXTEND_EXISTING_GROUP);
  // otherwise add new group
  Cached_group *group= allocate_group();
  if (group == NULL)
    DBUG_RETURN(ERROR);
  group->spec.type= GTID_GROUP;
  group->spec.gtid= gtid;
  group->binlog_offset= prev != NULL ? prev->binlog_offset : 0;
  // Update the internal status of this Group_cache (see comment above
  // definition of enum_group_cache_type).
  if (group->spec.type == GTID_GROUP)
  {
    /*
      @todo: currently group_is_logged() requires a linear scan
      through the cache. if this becomes a performance problem, we can
      add a Gtid_set Group_cache::logged_groups and add logged groups
      to it here. /Sven
    */
  }
  DBUG_RETURN(APPEND_NEW_GROUP);
}
#endif


enum_return_status Group_cache::generate_automatic_gno(THD *thd)
{
  DBUG_ENTER("Group_cache::generate_automatic_gno");
  DBUG_ASSERT(thd->variables.gtid_next.type == AUTOMATIC_GROUP);
  DBUG_ASSERT(thd->variables.gtid_next_list.get_gtid_set() == NULL);
  int n_groups= get_n_groups();
  enum_group_type automatic_type= INVALID_GROUP;
  Gtid automatic_gtid= { 0, 0 };
  for (int i= 0; i < n_groups; i++)
  {
    Cached_group *group= get_unsafe_pointer(i);
    if (group->spec.type == AUTOMATIC_GROUP)
    {
      if (automatic_type == INVALID_GROUP)
      {
        if (gtid_mode <= 1)
        {
          automatic_type= ANONYMOUS_GROUP;
        }
        else
        {
          automatic_type= GTID_GROUP;
          automatic_gtid.sidno= gtid_state->get_server_sidno();
          gtid_state->lock_sidno(automatic_gtid.sidno);
          automatic_gtid.gno=
            gtid_state->get_automatic_gno(automatic_gtid.sidno);
          if (automatic_gtid.gno == -1)
          {
            gtid_state->unlock_sidno(automatic_gtid.sidno);
            RETURN_REPORTED_ERROR;
          }
          gtid_state->acquire_ownership(thd, automatic_gtid);
          gtid_state->unlock_sidno(automatic_gtid.sidno);
        }
      }
      group->spec.type= automatic_type;
      group->spec.gtid= automatic_gtid;
    }
  }
  RETURN_OK;
}


enum_return_status Group_cache::get_gtids(Gtid_set *gs) const
{
  DBUG_ENTER("Group_cache::get_groups");
  int n_groups= get_n_groups();
  PROPAGATE_REPORTED_ERROR(gs->ensure_sidno(gs->get_sid_map()->get_max_sidno()));
  for (int i= 0; i < n_groups; i++)
  {
    Cached_group *group= get_unsafe_pointer(i);
    /*
      Cached groups only have GTIDs if SET @@SESSION.GTID_NEXT statement
      was executed before group.
    */
    if (group->spec.type == GTID_GROUP)
      PROPAGATE_REPORTED_ERROR(gs->_add_gtid(group->spec.gtid));
  }
  RETURN_OK;
}
