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

#include "zgtids.h"
#include "sql_class.h"
#include "binlog.h"


#ifdef HAVE_GTID


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
  const Gtid_specification *spec= &thd->variables.gtid_next;
  // merge with previous group if possible
  Cached_group *prev= get_last_group();
  if (prev != NULL && prev->spec.equals(spec))
    DBUG_RETURN(EXTEND_EXISTING_GROUP);
  // otherwise add a new group
  Cached_group *group= allocate_group();
  if (group ==  NULL)
    DBUG_RETURN(ERROR);
  group->spec= *spec;
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


bool Group_cache::contains_gtid(rpl_sidno sidno, rpl_gno gno) const
{
  int n_groups= get_n_groups();
  for (int i= 0; i < n_groups; i++)
  {
    const Cached_group *group= get_unsafe_pointer(i);
    if (group->spec.equals(sidno, gno))
      return true;
  }
  return false;
}


Group_cache::enum_add_group_status
Group_cache::add_empty_group(rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Group_cache::add_empty_group");
  // merge with previous group if possible
  Cached_group *prev= get_last_group();
  if (prev != NULL && prev->spec.equals(sidno, gno))
    DBUG_RETURN(EXTEND_EXISTING_GROUP);
  // otherwise add new group
  Cached_group *group= allocate_group();
  if (group == NULL)
    DBUG_RETURN(ERROR);
  group->spec.type= GTID_GROUP;
  group->spec.gtid.sidno= sidno;
  group->spec.gtid.gno= gno;
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


enum_return_status
Group_cache::add_empty_group_if_missing(const Gtid_state *gls,
                                        rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Group_cache::add_empty_group_if_missing(Gtid_state *, rpl_sidno, rpl_gno)");
  if (!gls->is_logged(sidno, gno) && !contains_gtid(sidno, gno))
    if (add_empty_group(sidno, gno) == ERROR)
      RETURN_REPORTED_ERROR;
  RETURN_OK;
}


enum_return_status
Group_cache::add_empty_groups_if_missing(const Gtid_state *gls,
                                         const Gtid_set *gtid_set)
{
  DBUG_ENTER("Group_cache::add_empty_groups_if_missing(Gtid_state *, Gtid_set *)");
  /*
    @todo: This algorithm is
    O(n_groups_in_cache*n_groups_in_gtid_set) because contains_group
    is O(n_groups_in_cache).  We can optimize this to
    O(n_groups_in_cache+n_groups_in_gtid_set), as follows: create a
    HASH and copy all groups from the cache to the HASH.  Then use the
    HASH to detect if the group is in the cache or not.  This has some
    overhead so should only be used if
    n_groups_in_cache*n_groups_in_gtid_set is significantly bigger
    than n_groups_in_cache+n_groups_in_gtid_set elements. /Sven
  */
  Gtid_set::Gtid_iterator git(gtid_set);
  Gtid g= git.get();
  while (g.sidno)
  {
    PROPAGATE_REPORTED_ERROR(add_empty_group_if_missing(gls, g.sidno, g.gno));
    git.next();
    g= git.get();
  }
  RETURN_OK;
}


enum_return_status
Group_cache::update_gtid_state(const THD *thd, Gtid_state *gls) const
{
  DBUG_ENTER("Group_cache::update_gtid_state");

  // todo: would be enough to use the set of GTIDs in this cache, in
  // case it contains fewer GTIDs than gtid_next_list /sven
  const Gtid_set *lock_set= thd->variables.gtid_next_list.get_gtid_set();
  int n_groups= get_n_groups();
  rpl_sidno lock_sidno= 0;

  if (lock_set != NULL)
    gls->lock_sidnos(lock_set);
  else
  {
    DBUG_ASSERT(n_groups <= 1);
    lock_sidno= n_groups > 0 ? get_unsafe_pointer(0)->spec.gtid.sidno : 0;
    if (lock_sidno)
      gls->lock_sidno(lock_sidno);
  }

  enum_return_status ret= RETURN_STATUS_OK;
  bool updated= false;

  for (int i= 0; i < n_groups; i++)
  {
    Cached_group *group= get_unsafe_pointer(i);
    if (group->spec.type == GTID_GROUP)
    {
      DBUG_ASSERT(lock_set != NULL ?
                  lock_set->contains_sidno(group->spec.gtid.sidno) :
                  (lock_sidno > 0 && group->spec.gtid.sidno == lock_sidno));
      updated= true;
      ret= gls->log_group(group->spec.gtid.sidno, group->spec.gtid.gno);
      if (ret != RETURN_STATUS_OK)
        break;
    }
    DBUG_ASSERT(group->spec.type != AUTOMATIC_GROUP);
  }

  if (lock_set != NULL)
  {
    if (updated)
      gls->broadcast_sidnos(lock_set);
    gls->unlock_sidnos(lock_set);
  }
  else if (lock_sidno != 0)
  {
    if (updated)
      gls->broadcast_sidno(lock_sidno);
    gls->unlock_sidno(lock_sidno);
  }
  RETURN_STATUS(ret);
}


enum_return_status Group_cache::generate_automatic_gno(const THD *thd,
                                                       Gtid_state *gls)
{
  DBUG_ENTER("Group_cache::generate_automatic_gno");
  DBUG_ASSERT(thd->variables.gtid_next.type == AUTOMATIC_GROUP);
  DBUG_ASSERT(thd->variables.gtid_next_list.get_gtid_set() == NULL);
  int n_groups= get_n_groups();
  enum_group_type automatic_type= INVALID_GROUP;
  rpl_gno automatic_gno= 0;
  rpl_sidno automatic_sidno= 0;
  for (int i= 0; i < n_groups; i++)
  {
    Cached_group *group= get_unsafe_pointer(i);
    if (group->spec.type == AUTOMATIC_GROUP)
    {
      if (automatic_type == INVALID_GROUP)
      {
        /*
        if (global_variables.gtid_mode == OFF || global_variables.gtid_mode == UPGRADE_STEP_1)
        {
          automatic_type= ANONYMOUS_GROUP;
        }
        else
        {
        */
        automatic_type= GTID_GROUP;
        automatic_sidno= gls->get_server_sidno();
        gls->lock_sidno(automatic_sidno);
        automatic_gno= gls->get_automatic_gno(automatic_sidno);
        if (automatic_gno == -1)
          RETURN_REPORTED_ERROR;
        gls->acquire_ownership(automatic_sidno, automatic_gno, thd);
        gls->unlock_sidno(automatic_sidno);
      }
      group->spec.type= automatic_type;
      group->spec.gtid.gno= automatic_gno;
      group->spec.gtid.sidno= automatic_sidno;
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
    PROPAGATE_REPORTED_ERROR(gs->_add(group->spec.gtid.sidno, group->spec.gtid.gno));
  }
  RETURN_OK;
}


#endif
