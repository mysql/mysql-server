/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

using namespace std;

#include "gcs_certifier.h"
#include "gcs_plugin.h"

Certifier:: Certifier() :
  next_seqno(1)
{}

rpl_gno Certifier::certify(Transaction_context_log_event* tcle)
{
  DBUG_ENTER("Certifier::certify");
  item_list *item;
  rpl_gno seq_no;

  item= tcle->get_write_set();
  if( item== NULL)
    DBUG_RETURN(-1);
  for (item_list::iterator it= item->begin();
       it != item->end();
       ++it)
  {
    seq_no= get_seqno(*it);
    DBUG_PRINT("info", ("sequence number in certifier: %llu", seq_no));
    DBUG_PRINT("info", ("snapshot timestamp in certifier: %llu", tcle->get_snapshot_timestamp()));
    if(seq_no > 0 && (seq_no < tcle->get_snapshot_timestamp()))
      DBUG_RETURN(0);
  }
  next_seqno++;

  for(item_list::iterator it= item->begin();
      it != item->end();
      ++it)
  {
    seq_no= get_seqno(*it);
    add_item(*it, (next_seqno-1));
  }
  DBUG_RETURN(next_seqno-1);
}

bool Certifier::add_item(const char* item, rpl_gno seq_no)
{
  DBUG_ENTER("Certifier::add_item");

  cert_db::iterator it;
  pair<cert_db::iterator, bool> ret;

  if(!item)
    DBUG_RETURN(item);

  /* convert item to string for persistance in map */
  string item_str(item);

  it= item_to_seqno_map.find(item_str);
  if(it == item_to_seqno_map.end())
  {
    ret= item_to_seqno_map.insert(pair<string, rpl_gno >(item_str, seq_no));
    DBUG_RETURN(!ret.second);
  }
  else
  {
    it->second= seq_no;
    DBUG_RETURN(false);
  }
}

rpl_gno Certifier::get_seqno(const char* item)
{
  DBUG_ENTER("Certifier::get_seqno");

  if (!item)
    DBUG_RETURN(0);

  cert_db::iterator it;
  string item_str(item);

  it= item_to_seqno_map.find(item_str);

  if (it == item_to_seqno_map.end())
    DBUG_RETURN(0);
  else
    DBUG_RETURN(it->second);
}
