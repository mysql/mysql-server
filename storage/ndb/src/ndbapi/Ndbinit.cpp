/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"
#include "NdbApiSignal.hpp"
#include "NdbImpl.hpp"
#include <ConfigRetriever.hpp>
#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include "ObjectMap.hpp"
#include "NdbUtil.hpp"
#include <NdbEnv.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#ifdef VM_TRACE
static bool g_first_create_ndb = true;
static bool g_force_short_signals = false;
#endif

Ndb::Ndb( Ndb_cluster_connection *ndb_cluster_connection,
	  const char* aDataBase , const char* aSchema)
  : theImpl(NULL)
{
  DBUG_ENTER("Ndb::Ndb()");
  DBUG_PRINT("enter",("Ndb::Ndb this: 0x%lx", (long) this));
  setup(ndb_cluster_connection, aDataBase, aSchema);
  DBUG_VOID_RETURN;
}

void Ndb::setup(Ndb_cluster_connection *ndb_cluster_connection,
	  const char* aDataBase , const char* aSchema)
{
  DBUG_ENTER("Ndb::setup");

  assert(theImpl == NULL);
  theImpl= new NdbImpl(ndb_cluster_connection,*this);
  theDictionary= &(theImpl->m_dictionary);

  thePreparedTransactionsArray= NULL;
  theSentTransactionsArray= NULL;
  theCompletedTransactionsArray= NULL;
  theNoOfPreparedTransactions= 0;
  theNoOfSentTransactions= 0;
  theNoOfCompletedTransactions= 0;
  theRemainingStartTransactions= 0;
  theMaxNoOfTransactions= 0;
  theMinNoOfEventsToWakeUp= 0;
  theTransactionList= NULL;
  theConnectionArray= NULL;
  theConnectionArrayLast= NULL;
  the_last_check_time = NdbTick_CurrentMillisecond();
  theFirstTransId= 0;
  theRestartGCI= 0;
  theNdbBlockNumber= -1;
  theInitState= NotConstructed;

  theNode= 0;
  theFirstTransId= 0;
  theMyRef= 0;

  fullyQualifiedNames = true;

#ifdef POORMANSPURIFY
  cgetSignals =0;
  cfreeSignals = 0;
  cnewSignals = 0;
  creleaseSignals = 0;
#endif

  theError.code = 0;

  theConnectionArray = new NdbConnection * [MAX_NDB_NODES];
  theConnectionArrayLast = new NdbConnection * [MAX_NDB_NODES];
  theCommitAckSignal = NULL;
  theCachedMinDbNodeVersion = 0;
  
  int i;
  for (i = 0; i < MAX_NDB_NODES ; i++) {
    theConnectionArray[i] = NULL;
    theConnectionArrayLast[i] = NULL;
  }//for
  m_sys_tab_0 = NULL;

  theImpl->m_dbname.assign(aDataBase);
  theImpl->m_schemaname.assign(aSchema);
  theImpl->update_prefix();

  // Signal that the constructor has finished OK
  if (theInitState == NotConstructed)
    theInitState = NotInitialised;

  {
    // theImpl->theWaiter.m_mutex must be set before this
    theEventBuffer= new NdbEventBuffer(this);
    if (theEventBuffer == NULL) {
      ndbout_c("Failed NdbEventBuffer()");
      exit(-1);
    }
  }

  theImpl->m_ndb_cluster_connection.link_ndb_object(this);

  DBUG_VOID_RETURN;
}


/*****************************************************************************
 * ~Ndb();
 *
 * Remark:        Disconnect with the database. 
 *****************************************************************************/
Ndb::~Ndb()
{ 
  DBUG_ENTER("Ndb::~Ndb()");
  DBUG_PRINT("enter",("this: 0x%lx", (long) this));

  if (theImpl == NULL)
  {
    /* Help users find their bugs */
    g_eventLogger->warning("Deleting Ndb-object @%p which is already deleted?",
                           this);
    DBUG_VOID_RETURN;
  }

  if (m_sys_tab_0)
    getDictionary()->removeTableGlobal(*m_sys_tab_0, 0);

  if (theImpl->m_ev_op != 0)
  {
    g_eventLogger->warning("Deleting Ndb-object with NdbEventOperation still"
                           " active");
    printf("this: %p NdbEventOperation(s): ", this);
    for (NdbEventOperationImpl *op= theImpl->m_ev_op; op; op=op->m_next)
    {
      printf("%p ", op);
    }
    printf("\n");
    fflush(stdout);
  }

  assert(theImpl->m_ev_op == 0); // user should return NdbEventOperation's
  for (NdbEventOperationImpl *op= theImpl->m_ev_op; op; op=op->m_next)
  {
    if (op->m_state == NdbEventOperation::EO_EXECUTING && op->stop())
      g_eventLogger->error("stopping NdbEventOperation failed in Ndb destructor");
    op->m_magic_number= 0;
  }
  doDisconnect();

  /* Disconnect from transporter to stop signals from coming in */
  theImpl->close();

  delete theEventBuffer;
  theEventBuffer = NULL;

  releaseTransactionArrays();

  delete []theConnectionArray;
  theConnectionArray = NULL;
  delete []theConnectionArrayLast;
  theConnectionArrayLast = NULL;
  if(theCommitAckSignal != NULL){
    delete theCommitAckSignal; 
    theCommitAckSignal = NULL;
  }

  theImpl->m_ndb_cluster_connection.unlink_ndb_object(this);

  delete theImpl;
  theImpl = NULL;

#ifdef POORMANSPURIFY
#ifdef POORMANSGUI
  ndbout << "cnewSignals=" << cnewSignals << endl;
  ndbout << "cfreeSignals=" << cfreeSignals << endl;
  ndbout << "cgetSignals=" << cgetSignals << endl;
  ndbout << "creleaseSignals=" << creleaseSignals << endl;
#endif
  // Poor mans purifier
  assert(cnewSignals == cfreeSignals);
  assert(cgetSignals == creleaseSignals);
#endif
  DBUG_VOID_RETURN;
}

NdbWaiter::NdbWaiter(trp_client* clnt)
  : m_clnt(clnt)
{
  m_node = 0;
  m_state = NO_WAIT;
}

NdbWaiter::~NdbWaiter()
{
  m_clnt = NULL;
}

NdbImpl::NdbImpl(Ndb_cluster_connection *ndb_cluster_connection,
		 Ndb& ndb)
  : m_ndb(ndb),
    m_next_ndb_object(0),
    m_prev_ndb_object(0),
    m_ndb_cluster_connection(ndb_cluster_connection->m_impl),
    m_transporter_facade(ndb_cluster_connection->m_impl.m_transporter_facade),
    m_dictionary(ndb),
    theCurrentConnectIndex(0),
    theNdbObjectIdMap(1024,1024),
    theNoOfDBnodes(0),
    theWaiter(this),
    wakeHandler(0),
    m_ev_op(0),
    customData(0),
    send_TC_COMMIT_ACK_immediate_flag(false)
{
  int i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    the_release_ind[i] = 0;
  }
  m_optimized_node_selection=
    m_ndb_cluster_connection.m_optimized_node_selection;

  m_systemPrefix.assfmt("%s%c%s%c", NDB_SYSTEM_DATABASE, table_name_separator,
			NDB_SYSTEM_SCHEMA, table_name_separator);

  forceShortRequests = false;

#ifdef VM_TRACE
  if (g_first_create_ndb)
  {
    g_first_create_ndb = false;
    const char* f= NdbEnv_GetEnv("NDB_FORCE_SHORT_REQUESTS", (char*)0, 0);
    if (f != 0 && *f != 0 && *f != '0' && *f != 'n' && *f != 'N')
    {
      g_force_short_signals = true;
    }
  }
  forceShortRequests = g_force_short_signals;
#endif

  for (i = 0; i < Ndb::NumClientStatistics; i++)
    clientStats[i] = 0;
}

NdbImpl::~NdbImpl()
{
  m_next_ndb_object = NULL;
  m_prev_ndb_object = NULL;
  theWaiter = NULL;
  wakeHandler = NULL;
  m_ev_op = NULL;
}

