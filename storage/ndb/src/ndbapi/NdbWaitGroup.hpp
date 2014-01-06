/*
 Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NdbWaitGroup_H
#define NdbWaitGroup_H

#include "NdbMutex.h"

class Ndb_cluster_connection;
class Ndb;
class MultiNdbWakeupHandler;

/* NdbWaitGroup extends the Asynchronous NDB API, allowing you to wait
   for asynchronous operations to complete on multiple Ndb objects at once.

   All Ndb objects within a poll group must belong to the same cluster
   connection, and only one poll group per cluster connection is currently
   supported.  You instantiate this poll group using
   Ndb_cluster_connection::create_multi_ndb_wait_group().

   Then, after using Ndb::sendPreparedTransactions() to send async operations
   on a particular Ndb object, you can use NdbWaitGroup::addNdb() to add it
   to the group.

   NdbWaitGroup::wait() returns whenever some Ndb's are ready for polling; you can
   then call Ndb::pollNdb(0, 1) on the ones that are ready.
*/

/* Hard upper limit on the number of Ndb objects in an NdbWaitGroup.
   A client trying to grow beyond this would hit an assert, but we expect
   to hit MAX_NO_THREADS in TransporterFacade first (error 4105).
*/   
#define NDBWAITGROUP_MAX_SIZE 262144 

class NdbWaitGroup : private NdbLockable {
friend class Ndb_cluster_connection;
friend class Ndb_cluster_connection_impl;
private:

  /** The private constructor is used only by ndb_cluster_connection.
      It allocates and initializes an NdbWaitGroup with an initial array 
      of Ndb objects.
      In the version 1 API, the initial size is the fixed maximum size.
      In the version 2 API, the array will grow as needed.
  */
  NdbWaitGroup(Ndb_cluster_connection *conn, int initial_size);

  /** The destructor is also private */
  ~NdbWaitGroup();

public:

  /****** VERSION 2 API *******/

  /** Push an Ndb object onto the wait queue.
      This is thread-safe: multiple threads can call push().
      Returns 0 on success, non-zero on error.
      
      Error return codes:
        -1: ndb does not belong to this Ndb_cluster_connection.
  */
  int push(Ndb *ndb);

  /** Wait for Ndbs to be ready for polling and report the number that are 
      ready. 
      wait() will return when:
        (a) at least pct_ready % of pushed Ndbs are ready for polling, or
        (b) at least timeout_millis milliseconds have expired, or
        (c) the NdbWaitGroup receives a wakeup() call. 
      pct_ready must be a value between 0 and 100. 
      If pct_ready is 0, wait() will return immediately. 
      If pct_ready is > 0 but no Ndbs have pushed, wait() will sleep until 
      a wakeup or timeout occurs.

      Only a single thread may use wait(). 

      Returns the number of Ndbs ready for polling.
  */
  int wait(Uint32 timeout_millis, int pct_ready = 50); 

  /** Returns an Ndb ready for polling.
      This is thread-safe: multiple threads can call pop().      

      Returns NULL if no Ndbs are ready.
  */
  Ndb * pop();



  /****** COMMON API *********/

  /** Wake up the thread that is currently waiting on this group.
      This can be used by other threads to signal a condition to the
      waiting thread.
      If no thread is currently waiting, then delivery is not guaranteed.
  */
  void wakeup();

  
  /****** VERSION 1 API *********/

  /** Add an Ndb object to the group. 
       
      Returns true on success, false on error.  Error could be that the Ndb
      is created from the wrong Ndb_cluster_connection, or is already in the
      group, or that the group is full.
  */
  bool addNdb(Ndb *);

  /** wait for Ndbs to be ready.
      arrayhead (OUT): on return will hold the list of ready Ndbs.
      The call will return when:
        (a) at least min_ready Ndbs are ready for polling, or
        (b) timeout milliseconds have elapsed, or
        (c) another thread has called NdbWaitGroup::wakeup()

     The return value is the number of Ndb objects ready for polling, or -1
     if a timeout occured.

      On return, arrayHead is set to point to the first element of
      the array of Ndb object pointers that are ready for polling, and those
      objects are implicitly no longer in the group.  These Ndb *'s must be
      read from arrayHead before before any further calls to addNdb().
  */
  int wait(Ndb ** & arrayHead, Uint32 timeout_millis, int min_ready = 1 );

private:  /* private instance variables */
   Ndb **m_array;
   Int32 m_pos;
   Uint32 m_array_size, m_pos_return;
   Uint32 m_pos_new, m_pos_wait, m_pos_ready;

   MultiNdbWakeupHandler *m_multiWaitHandler;
   Ndb **m_overflow;
   Int32 m_overflow_size, m_pos_overflow;
   Int32 m_nodeId, m_active_version;

   Ndb_cluster_connection *m_conn;
   Ndb *m_wakeNdb;

private:
   void resize_list(void);
};


#endif

