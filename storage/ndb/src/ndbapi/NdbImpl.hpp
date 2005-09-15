/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDB_IMPL_HPP
#define NDB_IMPL_HPP

#include <ndb_global.h>
#include <Ndb.hpp>
#include <NdbOut.hpp>
#include <NdbError.hpp>
#include <NdbCondition.h>
#include <NdbReceiver.hpp>
#include <NdbOperation.hpp>
#include <kernel/ndb_limits.h>

#include <NdbTick.h>

#include "ndb_cluster_connection_impl.hpp"
#include "NdbDictionaryImpl.hpp"
#include "ObjectMap.hpp"

/**
 * Private parts of the Ndb object (corresponding to Ndb.hpp in public API)
 */
class NdbImpl {
public:
  NdbImpl(Ndb_cluster_connection *, Ndb&);
  ~NdbImpl();

  int send_event_report(Uint32 *data, Uint32 length);

  Ndb &m_ndb;

  Ndb_cluster_connection_impl &m_ndb_cluster_connection;

  NdbDictionaryImpl m_dictionary;

  // Ensure good distribution of connects
  Uint32 theCurrentConnectIndex;
  Ndb_cluster_connection_node_iter m_node_iter;

  NdbObjectIdMap theNdbObjectIdMap;

  Uint32 theNoOfDBnodes; // The number of DB nodes  
  Uint8 theDBnodes[MAX_NDB_NODES]; // The node number of the DB nodes

 // 1 indicates to release all connections to node 
  Uint32 the_release_ind[MAX_NDB_NODES];

  NdbWaiter             theWaiter;

  NdbEventOperationImpl *m_ev_op;

  int m_optimized_node_selection;


  BaseString m_dbname; // Database name
  BaseString m_schemaname; // Schema name

  BaseString m_prefix; // Buffer for preformatted internal name <db>/<schema>/

  void update_prefix()
  {
    m_prefix.assfmt("%s%c%s%c", m_dbname.c_str(), table_name_separator,
                    m_schemaname.c_str(), table_name_separator);
  }

};

#ifdef VM_TRACE
#define TRACE_DEBUG(x) ndbout << x << endl;
#else
#define TRACE_DEBUG(x)
#endif

#define CHECK_STATUS_MACRO \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_RETURN(-1);}}
#define CHECK_STATUS_MACRO_VOID \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_VOID_RETURN;}}
#define CHECK_STATUS_MACRO_ZERO \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_RETURN(0);}}
#define CHECK_STATUS_MACRO_NULL \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_RETURN(NULL);}}

inline
void *
Ndb::int2void(Uint32 val){
  return theImpl->theNdbObjectIdMap.getObject(val);
}

inline
NdbReceiver *
Ndb::void2rec(void* val){
  return (NdbReceiver*)val;
}

inline
NdbTransaction *
Ndb::void2con(void* val){
  return (NdbTransaction*)val;
}

inline
NdbOperation*
Ndb::void2rec_op(void* val){
  return (NdbOperation*)(void2rec(val)->getOwner());
}

inline
NdbIndexOperation*
Ndb::void2rec_iop(void* val){
  return (NdbIndexOperation*)(void2rec(val)->getOwner());
}

inline 
NdbTransaction * 
NdbReceiver::getTransaction(){ 
  return ((NdbOperation*)m_owner)->theNdbCon;
}


inline
int
Ndb::checkInitState()
{
  theError.code = 0;

  if (theInitState != Initialised)
    return -1;
  return 0;
}

Uint32 convertEndian(Uint32 Data);

enum LockMode { 
  Read, 
  Update,
  Insert,
  Delete 
};

#endif
