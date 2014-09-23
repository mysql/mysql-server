/* Copyright (c) 2014 Oracle and/or its affiliates. All rights reserved.

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

const char* Multisource_info::default_channel= "";


bool Multisource_info::add_mi(const char* channel_name, Master_info* mi)
{
  DBUG_ENTER("Multisource_info::add_mi");

  mi_map::const_iterator it;
  std::pair<mi_map::iterator, bool>  ret;
  bool res= false;

  /* The check of mi exceeding MAX_CHANNELS shall be done in the caller */
  DBUG_ASSERT(current_mi_count < MAX_CHANNELS);

  /* implicit type cast from const char* to string */
  ret= channel_to_mi.insert(std::pair<std::string, Master_info* >(channel_name, mi));

  /* If a map insert fails, ret.second is false */
  if(!ret.second)
    DBUG_RETURN(true);

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  res= add_mi_to_rpl_pfs_mi(mi);
#endif
  current_mi_count++;

  DBUG_RETURN(res);

}

Master_info*  Multisource_info::get_mi(const char* channel_name)
{
  DBUG_ENTER("Multisource_info::get_mi");
  mi_map::iterator it;

  DBUG_ASSERT(channel_name != 0);

  it= channel_to_mi.find(channel_name);

  if (it == channel_to_mi.end())
    DBUG_RETURN(0);
  else
    DBUG_RETURN(it->second);
}


bool Multisource_info::delete_mi(const char* channel_name)
{
  DBUG_ENTER("Multisource_info::delete_mi");

  Master_info *mi= 0;
  mi_map::iterator it;

  DBUG_ASSERT(channel_name != 0);

  it= channel_to_mi.find(channel_name);

  if (it == channel_to_mi.end())
    DBUG_RETURN(true);

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
  channel_to_mi.erase(it);

  /* delete the master info */
  if (mi)
  {
    if(mi->rli)
    {
      delete mi->rli;
    }
    delete mi;
  }

  DBUG_RETURN(false);

}


const char*
Multisource_info::get_channel_with_host_port(char *host, uint port)
{
  DBUG_ENTER("Multisource_info::get_channel_with_host_port");
  Master_info *mi;

  if (!host || !port)
    DBUG_RETURN(0);

  for (mi_map::iterator it= channel_to_mi.begin();
       it != channel_to_mi.end(); it++)
  {
    mi= it->second;

    if (mi && mi->host && !strcmp(host, mi->host) && port == mi->port)
      DBUG_RETURN((const char*)mi->get_channel());
  }
  DBUG_RETURN(0);
}


#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE

bool Multisource_info::add_mi_to_rpl_pfs_mi(Master_info *mi)
{
  DBUG_ENTER("Multisource_info::add_mi_to_rpl_pfs_mi");

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

  if ( pos < MAX_CHANNELS)
    DBUG_RETURN(rpl_pfs_mi[pos]);

  DBUG_RETURN(0);
}
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */
