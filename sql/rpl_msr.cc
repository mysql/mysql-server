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
using namespace std;

const char* Multisource_info::default_channel= "";


bool Multisource_info::add_mi(const char* channel_name, Master_info* mi)
{
  DBUG_ENTER("Multisource_info::add_mi");

  mi_map::const_iterator it;
  pair<mi_map::iterator, bool>  ret;
  bool added_pfs_mi= false;

  /* The check of mi exceeding MAX_CHANNELS shall be done in the caller */
  DBUG_ASSERT(current_mi_count < MAX_CHANNELS);


  /* @TODO: convert to lowercase by default in all the functions */

  /* implicit type cast from const char* to string */
  ret= channel_to_mi.insert(pair<string, Master_info* >(channel_name, mi));

  /* If a map insert fails, ret.second is false */
  if(!ret.second)
    DBUG_RETURN(true);

  /* Point to this added mi in the pfs_mi*/
  for (uint i = 0; i < MAX_CHANNELS; i++)
  {
    if (pfs_mi[i] == 0)
    {
      pfs_mi[i] = mi;
      added_pfs_mi= true;
      current_mi_count++;
      break;
    }
  }

  DBUG_RETURN(!added_pfs_mi);

}

int Multisource_info::get_index_from_pfs_mi(const char * channel_name)
{
  Master_info* mi= 0;
  for (uint i= 0; i < MAX_CHANNELS; i++)
  {
    mi= pfs_mi[i];
    if (mi)
    {
      if ( !strcmp(mi->get_channel(), channel_name))
        return i;
    }
  }
  return -1;
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
  DBUG_ENTER("Multisource_info::remove_mi");

  Master_info *mi= 0;
  int index;
  mi_map::iterator it;

  DBUG_ASSERT(channel_name != 0);

  it= channel_to_mi.find(channel_name);

  if (it == channel_to_mi.end())
    DBUG_RETURN(true);

  /* get the index of mi from pfs_mi */
  index= get_index_from_pfs_mi(channel_name);

  DBUG_ASSERT(index != -1);

  mi= it->second;
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
    mi= 0;
  }

  /* set the current index to  0  and decrease current_mi_count */
  pfs_mi[index] = 0;
  current_mi_count--;

  DBUG_RETURN(false);

}


const char*  Multisource_info::get_channel_with_host_port(char *host, uint port)
{
  DBUG_ENTER("Multisource_info::same_host_port");
  Master_info *mi;


  for (mi_map::iterator it= channel_to_mi.begin(); it != channel_to_mi.end(); it++)
  {
    mi= it->second;

    if (host && host[0] && mi->host &&
        !strcmp(host, mi->host) && port == mi->port)
    {
      DBUG_RETURN((char*)mi->get_channel());
    }
  }
  DBUG_RETURN(0);
}


/*
   Every access from pfs asserts the LOCK_msr_map.
   Unneccasary performance issue?
*/
Master_info*  Multisource_info::get_mi_at_pos(uint pos)
{
  DBUG_ENTER("Multisource_info::get_mi_at_pos");

  if ( pos < MAX_CHANNELS)
    DBUG_RETURN(pfs_mi[pos]);
  DBUG_RETURN(0);
}
