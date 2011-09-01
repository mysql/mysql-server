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
#include "sql_class.h"
#include "binlog.h"


#ifdef HAVE_UGID


Group_cache::Group_cache()
{
  DBUG_ENTER("Group_cache::Group_cache");
  my_init_dynamic_array(&subgroups, sizeof(Cached_subgroup), 8, 8);
  DBUG_VOID_RETURN;
}


Group_cache::~Group_cache()
{
  DBUG_ENTER("Group_cache::~Group_cache");
  delete_dynamic(&subgroups);
  DBUG_VOID_RETURN;
}


void Group_cache::clear()
{
  DBUG_ENTER("Group_cache::clear");
  subgroups.elements= 0;
  DBUG_VOID_RETURN;
}


enum_group_status Group_cache::add_subgroup(const Cached_subgroup *group)
{
  DBUG_ENTER("Group_cache::add_subgroup(Cached_Subgroup *)");

  // if possible, merge the subgroup with previous subgroup in the cache
  int n_subgroups= get_n_subgroups();
  if (n_subgroups > 0)
  {
    Cached_subgroup *prev= get_unsafe_pointer(n_subgroups - 1);
    if ((prev->type == group->type || 
         (prev->type == NORMAL_SUBGROUP && group->type == DUMMY_SUBGROUP) ||
         (prev->type == DUMMY_SUBGROUP && group->type == NORMAL_SUBGROUP)) &&
         prev->sidno == group->sidno &&
         prev->gno == group->gno)
    {
      prev->binlog_length += group->binlog_length;
      prev->group_end= group->group_end;
      if (prev->type == DUMMY_SUBGROUP && group->type == NORMAL_SUBGROUP)
        prev->type= NORMAL_SUBGROUP;
      DBUG_RETURN(GS_SUCCESS);
    }
  }

  //printf("group=%p group->type=%d group->sidno=%d group->gno=%lld group->blen=%lld group->group_end=%d\n",
  //group, group->type, group->sidno, group->gno, group->binlog_length, group->group_end);

  // if sub-group could not be merged with previous sub-group, append it
  if (insert_dynamic(&subgroups, group) != 0)
    DBUG_RETURN(GS_ERROR_OUT_OF_MEMORY);

  // Update the internal status of this Group_cache (see comment above
  // definition of enum_group_cache_type).
  if (group->type == DUMMY_SUBGROUP || group->type == NORMAL_SUBGROUP)
  {
    if (group->group_end)
    {
      /*
        @todo: currently group_is_ended() requires a linear scan
        through the cache. if this becomes a performance problem, we
        can add a Group_set Group_cache::ended_groups and add ended
        groups to it here. /Sven
      */
    }
  }
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_cache::add_logged_subgroup(const THD *thd,
                                                   my_off_t length)
{
  DBUG_ENTER("Group_cache::add_logged_subgroup(THD *, my_off_t)");
  const Ugid_specification *spec= &thd->variables.ugid_next;
  Ugid_specification::enum_type type= spec->type;
  Cached_subgroup cs=
    {
      type == Ugid_specification::ANONYMOUS ? ANONYMOUS_SUBGROUP :
      NORMAL_SUBGROUP,
      spec->group.sidno,
      spec->group.gno,
      length,
      thd->variables.ugid_end
    };
  if (type == Ugid_specification::AUTOMATIC && spec->group.sidno == 0)
    cs.sidno= mysql_bin_log.server_uuid_sidno;
  enum_group_status ret= add_subgroup(&cs);
  DBUG_RETURN(ret);
}


bool Group_cache::contains_group(rpl_sidno sidno, rpl_gno gno) const
{
  int n_subgroups= get_n_subgroups();
  for (int i= 0; i < n_subgroups; i++)
  {
    const Cached_subgroup *cs= get_unsafe_pointer(i);
    if ((cs->type == NORMAL_SUBGROUP || cs->type == DUMMY_SUBGROUP) &&
        cs->gno == gno && cs->sidno == sidno)
      return true;
  }
  return false;
}


bool Group_cache::group_is_ended(rpl_sidno sidno, rpl_gno gno) const
{
  int n_subgroups= get_n_subgroups();
  for (int i= 0; i < n_subgroups; i++)
  {
    const Cached_subgroup *cs= get_unsafe_pointer(i);
    if (cs->gno == gno && cs->sidno == sidno && cs->group_end)
      return true;
  }
  return false;
}


enum_group_status
Group_cache::add_dummy_subgroup(rpl_sidno sidno, rpl_gno gno, bool group_end)
{
  DBUG_ENTER("Group_cache::add_dummy_subgroup");
  Cached_subgroup cs=
    {
      DUMMY_SUBGROUP, sidno, gno, 0/*binlog_length*/, group_end
    };
  DBUG_RETURN(add_subgroup(&cs));
}


enum_group_status
Group_cache::add_dummy_subgroup_if_missing(const Group_log_state *gls,
                                           rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Group_cache::add_dummy_subgroup_if_missing(Group_log_state *, rpl_sidno, rpl_gno)");
  if (!gls->is_ended(sidno, gno) && !gls->is_partial(sidno, gno) &&
      !contains_group(sidno, gno))
    DBUG_RETURN(add_dummy_subgroup(sidno, gno, false));
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status
Group_cache::add_dummy_subgroups_if_missing(const Group_log_state *gls,
                                            const Group_set *group_set)
{
  DBUG_ENTER("Group_cache::add_dummy_subgroups_if_missing(Group_log_state *, Group_set *)");
  //printf("Group_cache::add_dummy_subgroups_if_missing(Group_log_state *, Group_set *)");
  /*
    @todo: This algorithm is
    O(n_groups_in_cache*n_groups_in_group_set) because contains_group
    is O(n_groups_in_cache).  We can optimize this to
    O(n_groups_in_cache+n_groups_in_group_set), as follows: create a
    HASH and copy all groups from the cache to the HASH.  Then use the
    HASH to detect if the group is in the cache or not.  This has some
    overhead so should only be used if
    n_groups_in_cache*n_groups_in_group_set is significantly bigger
    than n_groups_in_cache+n_groups_in_group_set elements. /Sven
  */
  Group_set::Group_iterator git(group_set);
  Group g= git.get();
  while (g.sidno) {
    //printf("adding if missing: %d %lld\n", g.sidno, g.gno);
    GROUP_STATUS_THROW(add_dummy_subgroup_if_missing(gls, g.sidno, g.gno));
    git.next();
    g= git.get();
    //printf("next group: %d %lld\n", g.sidno, g.gno);
  }
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status
Group_cache::update_group_log_state(const THD *thd, Group_log_state *gls) const
{
  DBUG_ENTER("Group_cache::update_group_log_state");

  const Group_set *lock_set= thd->variables.ugid_next_list.get_group_set();
  int n_subgroups= get_n_subgroups();
  rpl_sidno lock_sidno= 0;

  if (lock_set != NULL)
    gls->lock_sidnos(lock_set);
  else
  {
    DBUG_ASSERT(n_subgroups <= 1);
    lock_sidno= n_subgroups > 0 ? get_unsafe_pointer(0)->sidno : 0;
    if (lock_sidno)
      gls->lock_sidno(lock_sidno);
  }

  enum_group_status ret= GS_SUCCESS;
  bool updated= false;

/*
  printf("Group_cache(%p)->update_group_log_state n_subgroups=%d\n",
         this, n_subgroups
         );
*/
  for (int i= 0; i < n_subgroups; i++)
  {
    Cached_subgroup *cs= get_unsafe_pointer(i);
    /*
    printf("i=%d cs=%p cs->type=%d cs->sidno=%d cs->gno=%lld cs->blen=%lld cs->group_end=%d\n",
           i, cs, cs->type, cs->sidno, cs->gno, cs->binlog_length, cs->group_end);
    */
    if (cs->type == NORMAL_SUBGROUP || cs->type == DUMMY_SUBGROUP)
    {
      DBUG_ASSERT(lock_set != NULL ? lock_set->contains_sidno(cs->sidno) :
                  (lock_sidno > 0 && cs->sidno == lock_sidno));
      if (cs->group_end)
      {
        updated= true;
        ret= gls->end_group(cs->sidno, cs->gno);
        if (ret != GS_SUCCESS)
          break;
      }
      else if (!gls->mark_partial(cs->sidno, cs->gno))
        updated= true;
    }
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
  DBUG_RETURN(ret);
}


void Group_cache::generate_automatic_gno(const THD *thd, Group_log_state *gls)
{
  DBUG_ENTER("Group_cache::generate_automatic_gno");
  if (thd->variables.ugid_next.type != Ugid_specification::AUTOMATIC)
    DBUG_VOID_RETURN;
  DBUG_ASSERT(thd->variables.ugid_next_list.get_group_set() == NULL);
  int n_subgroups= get_n_subgroups();
  rpl_gno automatic_gno= 0;
  rpl_sidno sidno= 0;
  Cached_subgroup *last_automatic_subgroup= NULL;
  for (int i= 0; i < n_subgroups; i++)
  {
    Cached_subgroup *cs= get_unsafe_pointer(i);
    if (cs->type == NORMAL_SUBGROUP && cs->gno <= 0)
    {
      if (automatic_gno == 0)
      {
        sidno= cs->sidno;
        gls->lock_sidno(sidno);
        automatic_gno= gls->get_automatic_gno(sidno);
        gls->acquire_ownership(sidno, automatic_gno, thd);
        gls->unlock_sidno(sidno);
      }
      cs->gno= automatic_gno;
      cs->sidno= sidno;
      last_automatic_subgroup= cs;
    }
  }
  if (last_automatic_subgroup != NULL)
    last_automatic_subgroup->group_end= true;
  DBUG_VOID_RETURN;
}


enum_group_status
Group_cache::write_to_log(Group_cache *trx_group_cache,
                          rpl_binlog_pos offset_after_last_statement)
{
  DBUG_ENTER("Group_cache::write_to_log(Group_cache *)");
  //printf("Group_cache::write_to_log(Group_cache *)\n");

  int n_subgroups= get_n_subgroups();

  /*
    If this is the stmt group cache, and the trx_group_cache contains
    a group that is ended in this cache, then we have to clear the end
    flag in this cache here and add an ended dummy subgroup to the
    trx_group_cache.
  */
  if (trx_group_cache != this)
  {
    for (int i= 0; i < n_subgroups; i++)
    {
      Cached_subgroup *cs= get_unsafe_pointer(i);
      if (cs->group_end && trx_group_cache->contains_group(cs->sidno, cs->gno))
      {
        cs->group_end= false;
        if (!trx_group_cache->group_is_ended(cs->sidno, cs->gno))
        {
          GROUP_STATUS_THROW(trx_group_cache->
                             add_dummy_subgroup(cs->sidno, cs->gno, true));
        }
      }
    }
  }

#ifndef NO_DBUG
  /*
    Assert that UGID is valid for all groups. This ensures that group
    numbers have been generated for automatic subgroups.
  */
  {
    int n_subgroups= get_n_subgroups();
    for (int i= 0; i < n_subgroups; i++)
    {
      Cached_subgroup *cs= get_unsafe_pointer(i);
      DBUG_ASSERT(cs->type == ANONYMOUS_SUBGROUP || (cs->sidno > 0 && cs->gno > 0));
    }
  }
#endif

  /*
    Find the last non-dummy group so that we can set
    offset_after_last_statement for it.
  */
  Cached_subgroup *last_non_dummy= NULL;
  // offset_after_last_statement is -1 if this Group_cache contains
  // only dummy groups.
#ifdef NO_DBUG
  if (offset_after_last_statement != -1)
#endif
  {
    for (int i= n_subgroups - 1; i >= 0; i--)
    {
      Cached_subgroup *cs= get_unsafe_pointer(i);
      if (cs->type != DUMMY_SUBGROUP)
      {
        last_non_dummy= cs;
        break;
      }
    }
    DBUG_ASSERT((last_non_dummy != NULL &&
                 offset_after_last_statement != -1) ||
                (last_non_dummy == NULL &&
                 offset_after_last_statement == -1));
  }

  // @todo write to log when log has been implemented

  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_cache::get_ended_groups(Group_set *gs) const
{
  DBUG_ENTER("Group_cache::get_groups");
  int n_subgroups= get_n_subgroups();
  GROUP_STATUS_THROW(gs->ensure_sidno(gs->get_sid_map()->get_max_sidno()));
  for (int i= 0; i < n_subgroups; i++)
  {
    Cached_subgroup *cs= get_unsafe_pointer(i);
    if (cs->group_end)
      GROUP_STATUS_THROW(gs->_add(cs->sidno, cs->gno));
  }
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_cache::get_partial_groups(Group_set *gs) const
{
  DBUG_ENTER("Group_cache::get_groups");
  Sid_map *sid_map= gs->get_sid_map();
  GROUP_STATUS_THROW(gs->ensure_sidno(sid_map->get_max_sidno()));
  Group_set ended_groups(sid_map);
  GROUP_STATUS_THROW(get_ended_groups(&ended_groups));
  int n_subgroups= get_n_subgroups();
  for (int i= 0; i < n_subgroups; i++)
  {
    Cached_subgroup *cs= get_unsafe_pointer(i);
    if (!ended_groups.contains_group(cs->sidno, cs->gno))
      GROUP_STATUS_THROW(gs->_add(cs->sidno, cs->gno));
  }
  DBUG_RETURN(GS_SUCCESS);
}


#endif
