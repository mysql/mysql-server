/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_msr.h"

#include "rpl_rli.h"     // Relay_log_info

const char* Multisource_info::default_channel= "";
const char* Multisource_info::group_replication_channel_names[] = {
  "group_replication_applier",
  "group_replication_recovery"
};

bool Multisource_info::add_mi(const char* channel_name, Master_info* mi)
{
  DBUG_ENTER("Multisource_info::add_mi");

  m_channel_map_lock->assert_some_wrlock();

  mi_map::const_iterator it;
  std::pair<mi_map::iterator, bool>  ret;
  bool res= false;

  /* The check of mi exceeding MAX_CHANNELS shall be done in the caller */
  DBUG_ASSERT(current_mi_count < MAX_CHANNELS);

  replication_channel_map::iterator map_it;
  enum_channel_type type= is_group_replication_channel_name(channel_name)
    ? GROUP_REPLICATION_CHANNEL: SLAVE_REPLICATION_CHANNEL;

  map_it= rep_channel_map.find(type);

  if (map_it == rep_channel_map.end())
  {
    std::pair<replication_channel_map::iterator, bool> map_ret =
      rep_channel_map.insert(replication_channel_map::value_type(type, mi_map()));

    if (!map_ret.second)
      DBUG_RETURN(true);

    map_it = rep_channel_map.find(type);
  }

  ret = map_it->second.insert(mi_map::value_type(channel_name, mi));

  /* If a map insert fails, ret.second is false */
  if(!ret.second)
    DBUG_RETURN(true);

  /* Save the pointer for the default_channel to avoid searching it */
  if (!strcmp(channel_name, get_default_channel()))
    default_channel_mi= mi;

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  res= add_mi_to_rpl_pfs_mi(mi);
#endif
  current_mi_count++;

  DBUG_RETURN(res);

}

Master_info* Multisource_info::get_mi(const char* channel_name)
{
  DBUG_ENTER("Multisource_info::get_mi");

  m_channel_map_lock->assert_some_lock();

  DBUG_ASSERT(channel_name != 0);

  mi_map::iterator it;
  replication_channel_map::iterator map_it;

  map_it= rep_channel_map.find(SLAVE_REPLICATION_CHANNEL);
  if (map_it != rep_channel_map.end())
  {
    it= map_it->second.find(channel_name);
  }

  if (map_it == rep_channel_map.end() || //If not a slave channel, maybe a group one
      it == map_it->second.end())
  {
    map_it= rep_channel_map.find(GROUP_REPLICATION_CHANNEL);
    if (map_it == rep_channel_map.end())
    {
      DBUG_RETURN(0);
    }
    it= map_it->second.find(channel_name);
    if (it == map_it->second.end())
    {
      DBUG_RETURN(0);
    }
  }

  DBUG_RETURN(it->second);
}

void Multisource_info::delete_mi(const char* channel_name)
{
  DBUG_ENTER("Multisource_info::delete_mi");

  m_channel_map_lock->assert_some_wrlock();

  Master_info *mi= 0;
  mi_map::iterator it;

  DBUG_ASSERT(channel_name != 0);

  replication_channel_map::iterator map_it;
  map_it= rep_channel_map.find(SLAVE_REPLICATION_CHANNEL);

  if (map_it != rep_channel_map.end())
  {
    it= map_it->second.find(channel_name);
  }
  if (map_it == rep_channel_map.end() || //If not a slave channel, maybe a group one
      it == map_it->second.end())
  {
    map_it= rep_channel_map.find(GROUP_REPLICATION_CHANNEL);
    DBUG_ASSERT(map_it != rep_channel_map.end());

    it= map_it->second.find(channel_name);
    DBUG_ASSERT(it != map_it->second.end());
  }

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  int index= -1;
  /* get the index of mi from rpl_pfs_mi */
  index= get_index_from_rpl_pfs_mi(channel_name);

  DBUG_ASSERT(index != -1);

  /* set the current index to  0  and decrease current_mi_count */
  rpl_pfs_mi[index] = 0;
#endif

  current_mi_count--;

  mi= it->second;
  it->second= 0;
  /* erase from the map */
  map_it->second.erase(it);

  if (default_channel_mi == mi)
    default_channel_mi= NULL;

  /* delete the master info */
  if (mi)
  {
    mi->channel_assert_some_wrlock();
    mi->wait_until_no_reference(current_thd);

    if(mi->rli)
    {
      delete mi->rli;
    }
    delete mi;
  }

  DBUG_VOID_RETURN;
}


bool Multisource_info::is_group_replication_channel_name(const char* channel,
                                                         bool is_applier)
{
  if (is_applier)
    return !strcmp(channel, group_replication_channel_names[0]);
  else
    return !strcmp(channel, group_replication_channel_names[0]) ||
           !strcmp(channel, group_replication_channel_names[1]);
}


#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE

bool Multisource_info::add_mi_to_rpl_pfs_mi(Master_info *mi)
{
  DBUG_ENTER("Multisource_info::add_mi_to_rpl_pfs_mi");

  m_channel_map_lock->assert_some_wrlock();

  bool res=true; // not added

  /* Point to this added mi in the rpl_pfs_mi*/
  for (uint i = 0; i < MAX_CHANNELS; i++)
  {
    if (rpl_pfs_mi[i] == 0)
    {
      rpl_pfs_mi[i] = mi;
      res= false;  // success
      break;
    }
  }
  DBUG_RETURN(res);
}


int Multisource_info::get_index_from_rpl_pfs_mi(const char * channel_name)
{
  m_channel_map_lock->assert_some_lock();

  Master_info* mi= 0;
  for (uint i= 0; i < MAX_CHANNELS; i++)
  {
    mi= rpl_pfs_mi[i];
    if (mi)
    {
      if ( !strcmp(mi->get_channel(), channel_name))
        return i;
    }
  }
  return -1;
}


Master_info*  Multisource_info::get_mi_at_pos(uint pos)
{
  DBUG_ENTER("Multisource_info::get_mi_at_pos");

  m_channel_map_lock->assert_some_lock();

  if ( pos < MAX_CHANNELS)
    DBUG_RETURN(rpl_pfs_mi[pos]);

  DBUG_RETURN(0);
}
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */

/* There is only one channel_map for the whole server */
Multisource_info channel_map;
