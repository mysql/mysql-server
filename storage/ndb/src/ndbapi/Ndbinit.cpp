/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <NdbEnv.h>
#include <NdbSleep.h>
#include <ndb_limits.h>
#include <ConfigRetriever.hpp>
#include <NdbOut.hpp>
#include "API.hpp"
#include "NdbApiSignal.hpp"
#include "NdbImpl.hpp"
#include "NdbUtil.hpp"
#include "ObjectMap.hpp"

#include <EventLogger.hpp>

#ifdef VM_TRACE
static bool g_first_create_ndb = true;
static bool g_force_short_signals = false;
static bool g_force_acc_table_scans = false;
#endif

Ndb::Ndb(Ndb_cluster_connection *ndb_cluster_connection, const char *aDataBase,
         const char *aSchema)
    : theImpl(nullptr) {
  DBUG_ENTER("Ndb::Ndb()");
  DBUG_PRINT("enter", ("Ndb::Ndb this: %p", this));
  setup(ndb_cluster_connection, aDataBase, aSchema);
  DBUG_VOID_RETURN;
}

void Ndb::setup(Ndb_cluster_connection *ndb_cluster_connection,
                const char *aDataBase, const char *aSchema) {
  DBUG_ENTER("Ndb::setup");

  assert(theImpl == nullptr);
  theImpl = new NdbImpl(ndb_cluster_connection, *this);
  theDictionary = &(theImpl->m_dictionary);

  thePreparedTransactionsArray = nullptr;
  theSentTransactionsArray = nullptr;
  theCompletedTransactionsArray = nullptr;
  theNoOfPreparedTransactions = 0;
  theNoOfSentTransactions = 0;
  theNoOfCompletedTransactions = 0;
  theRemainingStartTransactions = 0;
  theMaxNoOfTransactions = 0;
  theMinNoOfEventsToWakeUp = 0;
  theTransactionList = nullptr;
  theConnectionArray = nullptr;
  theConnectionArrayLast = nullptr;
  the_last_check_time = NdbTick_CurrentMillisecond();
  theFirstTransId = 0;
  theRestartGCI = 0;
  theNdbBlockNumber = -1;
  theInitState = NotConstructed;

  theNode = 0;
  theMyRef = 0;

#ifdef POORMANSPURIFY
  cgetSignals = 0;
  cfreeSignals = 0;
  cnewSignals = 0;
  creleaseSignals = 0;
#endif

  theError.code = 0;

  theConnectionArray = new NdbConnection *[MAX_NDB_NODES];
  theConnectionArrayLast = new NdbConnection *[MAX_NDB_NODES];
  theCommitAckSignal = nullptr;
  theCachedMinDbNodeVersion = 0;

  int i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    theConnectionArray[i] = nullptr;
    theConnectionArrayLast[i] = nullptr;
  }  // for
  m_sys_tab_0 = nullptr;

  theImpl->m_dbname.assign(aDataBase);
  theImpl->m_schemaname.assign(aSchema);

  // Signal that the constructor has finished OK
  if (theInitState == NotConstructed) theInitState = NotInitialised;

  {
    // theImpl->theWaiter.m_mutex must be set before this
    theEventBuffer = new NdbEventBuffer(this);
    if (theEventBuffer == nullptr) {
      g_eventLogger->info("Failed NdbEventBuffer()");
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
Ndb::~Ndb() {
  DBUG_ENTER("Ndb::~Ndb()");
  DBUG_PRINT("enter", ("this: %p", this));

  if (theImpl == nullptr) {
    /* Help users find their bugs */
    g_eventLogger->warning("Deleting Ndb-object @%p which is already deleted?",
                           this);
    DBUG_VOID_RETURN;
  }

  if (m_sys_tab_0) getDictionary()->removeTableGlobal(*m_sys_tab_0, 0);

  if (theImpl->m_ev_op != nullptr) {
    g_eventLogger->warning(
        "Deleting Ndb-object with NdbEventOperation still"
        " active");
    g_eventLogger->info("this: %p NdbEventOperation(s): ", this);
    for (NdbEventOperationImpl *op = theImpl->m_ev_op; op; op = op->m_next) {
      g_eventLogger->info("%p ", op);
    }
  }

  assert(theImpl->m_ev_op ==
         nullptr);  // user should return NdbEventOperation's
  for (NdbEventOperationImpl *op = theImpl->m_ev_op; op; op = op->m_next) {
    if (op->m_state == NdbEventOperation::EO_EXECUTING && op->stop())
      g_eventLogger->error(
          "stopping NdbEventOperation failed in Ndb destructor");
    op->m_magic_number = 0;
  }
  doDisconnect();

  /**
   * Update ndb_cluster_connection next transid map
   *
   * Must be done *before* releasing the block reference so that
   * another Ndb reusing the reference does not overlap
   */
  if (theNdbBlockNumber > 0) {
    theImpl->m_ndb_cluster_connection.set_next_transid(theNdbBlockNumber,
                                                       Uint32(theFirstTransId));
  }

  /* Disconnect from transporter to stop signals from coming in */
  theImpl->close();

  delete theEventBuffer;
  theEventBuffer = nullptr;

  releaseTransactionArrays();

  delete[] theConnectionArray;
  theConnectionArray = nullptr;
  delete[] theConnectionArrayLast;
  theConnectionArrayLast = nullptr;
  if (theCommitAckSignal != nullptr) {
    delete theCommitAckSignal;
    theCommitAckSignal = nullptr;
  }

  theImpl->m_ndb_cluster_connection.unlink_ndb_object(this);

  delete theImpl;
  theImpl = nullptr;

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

NdbImpl::NdbImpl(Ndb_cluster_connection *ndb_cluster_connection, Ndb &ndb)
    : m_ndb(ndb),
      m_next_ndb_object(nullptr),
      m_prev_ndb_object(nullptr),
      m_ndb_cluster_connection(ndb_cluster_connection->m_impl),
      m_transporter_facade(ndb_cluster_connection->m_impl.m_transporter_facade),
      m_dictionary(ndb),
      theCurrentConnectIndex(0),
/**
 * m_mutex is passed to theNdbObjectIdMap since it's needed to guard
 * expand() of theNdbObjectIdMap.
 */
#ifdef TEST_MAP_REALLOC
      theNdbObjectIdMap(1, 1, m_mutex),
#else
      theNdbObjectIdMap(1024, 1024, m_mutex),
#endif
      theNoOfDBnodes(0),
      theWaiter(this),
      wakeHandler(nullptr),
      m_ev_op(nullptr),
      customData(0),
      send_TC_COMMIT_ACK_immediate_flag(false) {
  int i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    the_release_ind[i] = 0;
  }
  m_optimized_node_selection =
      m_ndb_cluster_connection.m_optimized_node_selection;

  forceShortRequests = false;

#ifdef VM_TRACE
  if (g_first_create_ndb) {
    g_first_create_ndb = false;
    const char *f =
        NdbEnv_GetEnv("NDB_FORCE_SHORT_REQUESTS", (char *)nullptr, 0);
    if (f != nullptr && *f != 0 && *f != '0' && *f != 'n' && *f != 'N') {
      g_force_short_signals = true;
    }

    f = NdbEnv_GetEnv("NDB_FORCE_ACC_TABLE_SCANS", (char *)nullptr, 0);
    if (f != nullptr && *f != 0 && *f != '0' && *f != 'n' && *f != 'N') {
      g_force_acc_table_scans = true;
    }
  }
  forceAccTableScans = g_force_acc_table_scans;
  forceShortRequests = g_force_short_signals;
#endif

  for (i = 0; i < Ndb::NumClientStatistics; i++) clientStats[i] = 0;
}

NdbImpl::~NdbImpl() {
  m_next_ndb_object = nullptr;
  m_prev_ndb_object = nullptr;
  wakeHandler = nullptr;
  m_ev_op = nullptr;
}
