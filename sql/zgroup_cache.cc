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


enum_return_status Group_cache::add_group(const Cached_group *group)
{
  DBUG_ENTER("Group_cache::add_group(Cached_group *)");

  // if possible, merge the group with previous group in the cache
  int n_groups= get_n_groups();
  if (n_groups > 0)
  {
    Cached_group *prev= get_unsafe_pointer(n_groups - 1);
    if (prev->type == group->type &&
        (group->type != GTID_GROUP ||
         (prev->sidno == group->sidno &&
          prev->gno == group->gno)))
    {
      prev->binlog_length += group->binlog_length;
      RETURN_OK;
    }
  }

  DBUG_PRINT("zwen", ("inserting type%d %d:%lld", group->type, group->sidno, group->gno));

  // if sub-group could not be merged with previous sub-group, append it
  if (insert_dynamic(&groups, group) != 0)
  {
    BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
    RETURN_REPORTED_ERROR;
  }

  // Update the internal status of this Group_cache (see comment above
  // definition of enum_group_cache_type).
  if (group->type == GTID_GROUP)
  {
    /*
      @todo: currently group_is_logged() requires a linear scan
      through the cache. if this becomes a performance problem, we can
      add a Gtid_set Group_cache::logged_groups and add logged groups
      to it here. /Sven
    */
  }
  RETURN_OK;
}


enum_return_status
Group_cache::add_logged_group(const THD *thd, my_off_t length)
{
  DBUG_ENTER("Group_cache::add_logged_group(THD *, my_off_t)");
  const Gtid_specification *spec= &thd->variables.gtid_next;
  DBUG_PRINT("zwen", ("spec=type:%d %d:%lld", spec->type, spec->gtid.sidno, spec->gtid.gno));
  Cached_group cs=
    {
      spec->type,
      spec->gtid.sidno,
      spec->gtid.gno,
      length,
    };
  PROPAGATE_REPORTED_ERROR(add_group(&cs));
  RETURN_OK;
}


bool Group_cache::contains_gtid(rpl_sidno sidno, rpl_gno gno) const
{
  int n_groups= get_n_groups();
  for (int i= 0; i < n_groups; i++)
  {
    const Cached_group *cs= get_unsafe_pointer(i);
    if (cs->type == GTID_GROUP && cs->gno == gno && cs->sidno == sidno)
      return true;
  }
  return false;
}


enum_return_status
Group_cache::add_empty_group(rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Group_cache::add_empty_group");
  Cached_group cs=
    {
      GTID_GROUP, sidno, gno, 0/*binlog_length*/
    };
  PROPAGATE_REPORTED_ERROR(add_group(&cs));
  RETURN_OK;
}

enum_return_status
Group_cache::add_empty_group_if_missing(const Gtid_state *gls,
                                        rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Group_cache::add_empty_group_if_missing(Gtid_state *, rpl_sidno, rpl_gno)");
  if (!gls->is_logged(sidno, gno) && !contains_gtid(sidno, gno))
    PROPAGATE_REPORTED_ERROR(add_empty_group(sidno, gno));
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
    lock_sidno= n_groups > 0 ? get_unsafe_pointer(0)->sidno : 0;
    if (lock_sidno)
      gls->lock_sidno(lock_sidno);
  }

  enum_return_status ret= RETURN_STATUS_OK;
  bool updated= false;

  for (int i= 0; i < n_groups; i++)
  {
    Cached_group *cs= get_unsafe_pointer(i);
    if (cs->type == GTID_GROUP)
    {
      DBUG_ASSERT(lock_set != NULL ? lock_set->contains_sidno(cs->sidno) :
                  (lock_sidno > 0 && cs->sidno == lock_sidno));
      updated= true;
      ret= gls->log_group(cs->sidno, cs->gno);
      if (ret != RETURN_STATUS_OK)
        break;
    }
    DBUG_ASSERT(cs->type != AUTOMATIC_GROUP);
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
  if (thd->variables.gtid_next.type != AUTOMATIC_GROUP)
    RETURN_OK;
  DBUG_ASSERT(thd->variables.gtid_next_list.get_gtid_set() == NULL);
  int n_groups= get_n_groups();
  enum_group_type automatic_type= INVALID_GROUP;
  rpl_gno automatic_gno= 0;
  rpl_sidno automatic_sidno= 0;
  for (int i= 0; i < n_groups; i++)
  {
    Cached_group *cs= get_unsafe_pointer(i);
    if (cs->type == AUTOMATIC_GROUP)
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
      cs->type= automatic_type;
      cs->gno= automatic_gno;
      cs->sidno= automatic_sidno;
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
    Cached_group *cs= get_unsafe_pointer(i);
    PROPAGATE_REPORTED_ERROR(gs->_add(cs->sidno, cs->gno));
  }
  RETURN_OK;
}


#endif
