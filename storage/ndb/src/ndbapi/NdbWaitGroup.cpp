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

#include <ndb_global.h>
#include "NdbWaitGroup.hpp"
#include "WakeupHandler.hpp"
#include "ndb_cluster_connection.hpp"
#include "TransporterFacade.hpp"
#include "ndb_cluster_connection_impl.hpp"
#include "NdbImpl.hpp"

NdbWaitGroup::NdbWaitGroup(Ndb_cluster_connection *_conn, int _ndbs) :
  m_conn(_conn),
  m_multiWaitHandler(0),
  m_array_size(_ndbs),
  m_count(0),
  m_nodeId(0)
{
  /* Allocate the array of Ndbs */
  m_array = new Ndb *[m_array_size];

  /* Call into the TransporterFacade to set up wakeups */
  bool rc = m_conn->m_impl.m_transporter_facade->setupWakeup();
  assert(rc);

  /* Get a new Ndb object to be the dedicated "wakeup object" for the group */
  m_wakeNdb = new Ndb(m_conn);
  assert(m_wakeNdb);
  m_wakeNdb->init(1);
  m_nodeId = m_wakeNdb->theNode;

  /* Get a wakeup handler */
  m_multiWaitHandler = new MultiNdbWakeupHandler(m_wakeNdb);
}


NdbWaitGroup::~NdbWaitGroup()
{
  while (m_count > 0)
  {
    m_multiWaitHandler->unregisterNdb(m_array[topDownIdx(m_count--)]);
  }

  delete m_multiWaitHandler;
  delete m_wakeNdb;
  delete[] m_array;
}


bool NdbWaitGroup::addNdb(Ndb *ndb)
{
  if (unlikely(ndb->theNode != Uint32(m_nodeId)))
  {
    return false; // Ndb belongs to wrong ndb_cluster_connection
  }

  if (unlikely(m_count == m_array_size))
  {
    return false; // array is full
  }

  if (unlikely(m_multiWaitHandler->ndbIsRegistered(ndb)))
  {
    return false; // duplicate of item already in group
  }

  m_count++;
  m_array[topDownIdx(m_count)] = ndb;
  return true;
}


void NdbWaitGroup::wakeup()
{
  m_conn->m_impl.m_transporter_facade->requestWakeup();
}


int NdbWaitGroup::wait(Ndb ** & arrayHead    /* out */,
                       Uint32 timeout_millis,
                       int min_ndbs)
{
  arrayHead = NULL;
  Ndb ** ndblist = m_array + topDownIdx(m_count);

  int wait_rc;
  int nready;
  {
    PollGuard pg(* m_wakeNdb->theImpl);   // get ready to poll
    wait_rc = m_multiWaitHandler->waitForInput(ndblist, m_count, min_ndbs,
                                               & pg, timeout_millis);
    nready = m_multiWaitHandler->getNumReadyNdbs();

    if (wait_rc == 0)
    {
      arrayHead = ndblist;   // success
      for(int i = 0 ; i < nready ; i++)  // remove ready Ndbs from group
      {
        m_multiWaitHandler->unregisterNdb(m_array[topDownIdx(m_count)]);
        m_count--;
      }
    }
  }   /* release PollGuard */

  return wait_rc ? -1 : nready;
}

