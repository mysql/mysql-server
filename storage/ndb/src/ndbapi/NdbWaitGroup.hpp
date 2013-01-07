/*
 Copyright (c) 2011 Oracle and/or its affiliates. All rights reserved.

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

class NdbWaitGroup {
friend class Ndb_cluster_connection;
friend class Ndb_cluster_connection_impl;
private:

  /** The private constructor is used only by ndb_cluster_connection.
      It allocates an initializes an NdbWaitGroup with an array of size
      max_ndb_objects.
  */
  NdbWaitGroup(Ndb_cluster_connection *conn, int max_ndb_objects);

  /** The destructor is also private */
  ~NdbWaitGroup();

public:

  /** Add an Ndb object to the group.

      Returns true on success, false on error.  Error could be that the Ndb
      is created from the wrong Ndb_cluster_connection, or is already in the
      group, or that the group is full.
  */
  bool addNdb(Ndb *);

  /** Wake up the thread that is currently waiting on this group.
      This can be used by other threads to signal a condition to the
      waiting thread.
      If no thread is currently waiting, then delivery is not guaranteed.
  */
  void wakeup();

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

private:   /* private internal methods */
   int topDownIdx(int n) { return m_array_size - n; }

private:  /* private instance variables */
   Ndb_cluster_connection *m_conn;
   MultiNdbWakeupHandler *m_multiWaitHandler;
   Ndb *m_wakeNdb;
   Ndb **m_array;
   int m_array_size;
   int m_count;
   int m_nodeId;
};


#endif

