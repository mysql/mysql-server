/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_share.h"
#include "ndb_event_data.h"

#include <my_sys.h>

extern Ndb* g_ndb;

void
NDB_SHARE::destroy(NDB_SHARE* share)
{
  thr_lock_delete(&share->lock);
  pthread_mutex_destroy(&share->mutex);

#ifdef HAVE_NDB_BINLOG
  if (share->m_cfn_share && share->m_cfn_share->m_ex_tab && g_ndb)
  {
    NdbDictionary::Dictionary *dict= g_ndb->getDictionary();
    dict->removeTableGlobal(*(share->m_cfn_share->m_ex_tab), 0);
    share->m_cfn_share->m_ex_tab= 0;
  }
#endif
  share->new_op= 0;
  if (share->event_data)
  {
    delete share->event_data;
    share->event_data= 0;
  }
  free_root(&share->mem_root, MYF(0));
  my_free(share);
}

