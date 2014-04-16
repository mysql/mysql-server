/*
   Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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
#include <stdlib.h>
#include "NdbWaitGroup.hpp"
#include "WakeupHandler.hpp"
#include "ndb_cluster_connection.hpp"
#include "TransporterFacade.hpp"
#include "ndb_cluster_connection_impl.hpp"
#include "NdbImpl.hpp"


int round_up(int num, int factor)
{
  return num + factor - 1 - (num - 1) % factor;
}


NdbWaitGroup::NdbWaitGroup(Ndb_cluster_connection *_conn, int ndbs) :
  m_pos_new(0),
  m_pos_wait(0),
  m_pos_ready(0),
  m_multiWaitHandler(0),
  m_pos_overflow(0),
  m_nodeId(0),
  m_active_version(0),
  m_conn(_conn)
{
  const int pointers_per_cache_line = NDB_CL / sizeof(Ndb *);

  /* round array size up to a whole cache line */
  m_array_size = round_up(ndbs, pointers_per_cache_line);

  /* m_pos is used in the version 1 api */
  m_pos = m_array_size;

  /* overflow list is 1/8 of array, also rounded up */
  m_overflow_size = m_array_size / 8;
  m_overflow_size = round_up(m_overflow_size, pointers_per_cache_line);

  /* Return point is somewhere in the array */
  m_pos_return = m_array_size / 3;
  
  /* Allocate the main array and the overflow list */
  m_array = (Ndb **) calloc(m_array_size, sizeof(Ndb *));
  m_overflow = (Ndb **) calloc(m_overflow_size, sizeof(Ndb *));

  /* Call into the TransporterFacade to set up wakeups */
  bool rc = m_conn->m_impl.m_transporter_facade->setupWakeup();
  require(rc);

  /* Get a new Ndb object to be the dedicated "wakeup object" for the group */
  m_wakeNdb = new Ndb(m_conn);
  require(m_wakeNdb);
  m_wakeNdb->init(1);
  m_nodeId = m_wakeNdb->theNode;

  /* Get a wakeup handler */
  m_multiWaitHandler = new MultiNdbWakeupHandler(m_wakeNdb);
  require(m_multiWaitHandler);
}


NdbWaitGroup::~NdbWaitGroup()
{
  delete m_multiWaitHandler;
  delete m_wakeNdb;
  free(m_array);
  free(m_overflow);
}


void NdbWaitGroup::wakeup()
{
  m_conn->m_impl.m_transporter_facade->requestWakeup();
}


/*  Old-API addNdb() 
*/
bool NdbWaitGroup::addNdb(Ndb *ndb)
{
  if(unlikely(ndb->theNode != Uint32(m_nodeId)))
  {
    return false; // Ndb belongs to wrong ndb_cluster_connection
  }

  if(unlikely(m_pos == 0))
  {
    return false; // array is full
  }

  m_array[--m_pos] = ndb;
  return true;
}


/*  Old-API version of wait().
    It is single-threaded without any concurrent push().
*/
int NdbWaitGroup::wait(Ndb ** & arrayHead    /* out */,
                       Uint32 timeout_millis,
                       int min_ndbs)
{
  int nready;
  int nwait = m_array_size - m_pos;
  Ndb ** ndblist = m_array + m_pos;
  arrayHead = NULL;
  m_active_version = 1;
  int wait_rc = m_multiWaitHandler->waitForInput(ndblist,
                                                 nwait,
                                                 min_ndbs,
                                                 timeout_millis,
                                                 &nready);
  if(wait_rc == 0) 
  {
    arrayHead = ndblist; 
    m_pos += nready;
    return nready;
  }
  return wait_rc ? -1 : nready;
}


/* Version 2 API */

/* 
    QUEUE 

    A = Array                                                       m_array
    MAX = Array Size                                                m_array_size 
    RETURNPOINT = Some point between 0 and MAX                      m_pos_return
    N = New (recently pushed to list)       NC = New Cursor         m_pos_new
    W = Waiting (on NDB network i/o)        WC = Waiting Cursor     m_pos_wait
    R = Returned (from NDB, ready to poll)  RC = Returned Cursor    m_pos_ready

    init:  NC = WC = RC = 0.    

    push:  A[NC] = X
           NC += 1                      # NC is index of next new item
           If(NC == MAX) List is full
    
    wait:  # Maintenance tasks:
              (1) If list is full, resize
              (2) If NC > RETURNPOINT, shift list downwad so A[WC] becomes A[0]
           # Wait for all the newly arrived items
           nwait = NC - WC
           nready = waitForInput(WC, nwait)
           WC += nready                 # WC is start index of the next wait
    
    pop:   IF (RC != WC)
             RETURNVAL = A[RC]
             RC += 1                    # RC is index of next ready item
                
    Many threads can push and pop; only one thread can use wait.
*/

int NdbWaitGroup::push(Ndb *ndb) 
{
  if(unlikely(ndb->theNode != Uint32(m_nodeId)))
  {
    return -1;
  }

  lock();
  if(unlikely(m_pos_new == m_array_size))  // array is full
  {
    if(unlikely(m_pos_overflow == m_overflow_size))  // overflow list is full
    {
      m_overflow_size *= 2;
      assert(m_overflow_size < NDBWAITGROUP_MAX_SIZE);
      m_overflow = (Ndb **) realloc(m_overflow, m_overflow_size * sizeof(Ndb*));
    }
    m_overflow[m_pos_overflow++] = ndb;
  }
  else 
  {
    m_array[m_pos_new++] = ndb;   // common case
  }
  unlock();

  return 0;
}


/* wait() takes the lock before and after wait (not during).
   In 7.2, shifting or resizing the list requires a PollGuard,
   but in 7.3, the underlying wakeupHandler will only touch the 
   array during wait() so no lock is needed.
*/
int NdbWaitGroup::wait(Uint32 timeout_millis, int pct_ready) 
{
  int nready, nwait;
  m_active_version = 2;
  assert(pct_ready >=0 && pct_ready <= 100);

  lock();

  /* Resize list if full */
  if(unlikely(m_pos_new == m_array_size))
  {
    resize_list();
  }

  /* On last pop, if list has advanced past return point, shift back to 0 */
  if(m_pos_ready &&                  /* Not at zero */
     m_pos_ready == m_pos_wait &&    /* Cannot currently pop */
     m_pos_new > m_pos_return)       /* NC > RETURNPOINT */
  {
    for(Uint32 i = m_pos_wait; i < m_pos_new; i++)
    {
      m_array[i - m_pos_wait] = m_array[i];
    }
    m_pos_new -= m_pos_wait;
    m_pos_ready = m_pos_wait = 0;         
  }  

  /* Number of items to wait for */
  nwait = m_pos_new - m_pos_wait;
  unlock();

  /********** ENTER WAIT **********/
  int min_ndbs = nwait * pct_ready / 100 ;
  if(min_ndbs == 0 && pct_ready > 0) min_ndbs = 1;
  Ndb ** arrayHead = m_array + m_pos_wait;
  m_multiWaitHandler->waitForInput(arrayHead,
                                   nwait,
                                   min_ndbs,
                                   timeout_millis,
                                   &nready);
  /********** EXIT WAIT *********/

  lock();
  m_pos_wait += nready;
  unlock();
  
  return nready;
}


Ndb * NdbWaitGroup::pop()
{
  Ndb * r = 0;

  lock();
  if(m_pos_ready < m_pos_wait) 
  {
    r = m_array[m_pos_ready++];
  }
  unlock();

  return r;
}


/* Private internal methods */

void NdbWaitGroup::resize_list() 
{
  Uint32 size_required = m_array_size + m_pos_overflow + 1;
  while(m_array_size < size_required)
  {  
    m_array_size *= 2;
    m_pos_return *= 2; 
  }
  assert(m_array_size < NDBWAITGROUP_MAX_SIZE);

  /* Reallocate */
  m_array = (Ndb **) realloc(m_array, m_array_size * sizeof(Ndb *));
  
  /* Copy from the overflow list to the new list. */
  while(m_pos_overflow) 
  {
    m_array[m_pos_new++] = m_overflow[--m_pos_overflow];
  }    
}

