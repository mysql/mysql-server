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


#include <ndb_global.h>

#include "NdbApiSignal.hpp"
#include "NdbImpl.hpp"
//#include "NdbSchemaOp.hpp"
//#include "NdbSchemaCon.hpp" 
#include "NdbOperation.hpp"
#include "NdbConnection.hpp"
#include "NdbRecAttr.hpp"
#include "IPCConfig.hpp"
#include "TransporterFacade.hpp"
#include "ConfigRetriever.hpp"
#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include "ObjectMap.hpp"

class NdbGlobalEventBufferHandle;
NdbGlobalEventBufferHandle *NdbGlobalEventBuffer_init(int);
void NdbGlobalEventBuffer_drop(NdbGlobalEventBufferHandle *);

/**
 * Static object for NDB
 */
static int theNoOfNdbObjects = 0;

static char *ndbConnectString = 0;

#if defined NDB_WIN32 || defined SCO
static NdbMutex & createNdbMutex = * NdbMutex_Create();
#else
static NdbMutex createNdbMutex = NDB_MUTEX_INITIALIZER;
#endif


/***************************************************************************
Ndb(const char* aDataBase);

Parameters:    aDataBase : Name of the database.
Remark:        Connect to the database.
***************************************************************************/
Ndb::Ndb( const char* aDataBase , const char* aSchema) :
  theNdbObjectIdMap(0),
  thePreparedTransactionsArray(NULL),
  theSentTransactionsArray(NULL),
  theCompletedTransactionsArray(NULL),
  theNoOfPreparedTransactions(0),
  theNoOfSentTransactions(0),
  theNoOfCompletedTransactions(0),
  theNoOfAllocatedTransactions(0),
  theMaxNoOfTransactions(0),
  theMinNoOfEventsToWakeUp(0),
  prefixEnd(NULL),
  theImpl(NULL),
  theDictionary(NULL),
  theConIdleList(NULL),
  theOpIdleList(NULL),
  theScanOpIdleList(NULL),
  theIndexOpIdleList(NULL),
//  theSchemaConIdleList(NULL),
//  theSchemaConToNdbList(NULL),
  theTransactionList(NULL),
  theConnectionArray(NULL),
  theRecAttrIdleList(NULL),
  theSignalIdleList(NULL),
  theLabelList(NULL),
  theBranchList(NULL),
  theSubroutineList(NULL),
  theCallList(NULL),
  theScanList(NULL),
  theNdbBlobIdleList(NULL),
  theNoOfDBnodes(0),
  theDBnodes(NULL),
  the_release_ind(NULL),
  the_last_check_time(0),
  theFirstTransId(0),
  theRestartGCI(0),
  theNdbBlockNumber(-1),
  theInitState(NotConstructed)
{
  fullyQualifiedNames = true;

  cgetSignals =0;
  cfreeSignals = 0;
  cnewSignals = 0;
  creleaseSignals = 0;
  theError.code = 0;

  theNdbObjectIdMap  = new NdbObjectIdMap(1024,1024);
  theConnectionArray = new NdbConnection * [MAX_NDB_NODES];
  theDBnodes         = new Uint32[MAX_NDB_NODES];
  the_release_ind    = new Uint8[MAX_NDB_NODES];
  theCommitAckSignal = NULL;
  
  theCurrentConnectCounter = 1;
  theCurrentConnectIndex = 0;
  int i;
  for (i = 0; i < MAX_NDB_NODES ; i++) {
    theConnectionArray[i] = NULL;
    the_release_ind[i] = 0;
    theDBnodes[i] = 0;
  }//forg
  for (i = 0; i < 2048 ; i++) {
    theFirstTupleId[i] = 0;
    theLastTupleId[i] = 0;
  }//for
  
  snprintf(theDataBase, sizeof(theDataBase), "%s",
           aDataBase ? aDataBase : "");
  snprintf(theDataBaseSchema, sizeof(theDataBaseSchema), "%s",
	   aSchema ? aSchema : "");

  int len = snprintf(prefixName, sizeof(prefixName), "%s%c%s%c",
                     theDataBase, table_name_separator,
                     theDataBaseSchema, table_name_separator);
  prefixEnd = prefixName + (len < sizeof(prefixName) ? len : 
                            sizeof(prefixName) - 1);

  NdbMutex_Lock(&createNdbMutex);
  
  TransporterFacade * m_facade = 0;
  if(theNoOfNdbObjects == 0){
    if ((m_facade = TransporterFacade::start_instance(ndbConnectString)) == 0)
      theInitState = InitConfigError;
  } else {
    m_facade = TransporterFacade::instance();
  }
  
  if(m_facade != 0){
    theWaiter.m_mutex = m_facade->theMutexPtr;
  } 
  
  // For keeping track of how many Ndb objects that exists.
  theNoOfNdbObjects += 1;
  
  // Signal that the constructor has finished OK
  if (theInitState == NotConstructed)
    theInitState = NotInitialised;

  theImpl = new NdbImpl();

  {
    NdbGlobalEventBufferHandle *h=
      NdbGlobalEventBuffer_init(NDB_MAX_ACTIVE_EVENTS);
    if (h == NULL) {
      ndbout_c("Failed NdbGlobalEventBuffer_init(%d)",NDB_MAX_ACTIVE_EVENTS);
      exit(-1);
    }
    theGlobalEventBufferHandle = h;
  }

  NdbMutex_Unlock(&createNdbMutex);
}


void Ndb::setConnectString(const char * connectString)
{
  if (ndbConnectString != 0) {
    free(ndbConnectString);
    ndbConnectString = 0;
  }
  if (connectString)
    ndbConnectString = strdup(connectString);
}

/*****************************************************************************
 * ~Ndb();
 *
 * Remark:        Disconnect with the database. 
 *****************************************************************************/
Ndb::~Ndb()
{ 
  doDisconnect();

  delete theDictionary;  
  delete theImpl;

  NdbGlobalEventBuffer_drop(theGlobalEventBufferHandle);

  if (TransporterFacade::instance() != NULL && theNdbBlockNumber > 0){
    TransporterFacade::instance()->close(theNdbBlockNumber);
  }

  NdbMutex_Lock(&createNdbMutex);

  theNoOfNdbObjects -= 1;
  if(theNoOfNdbObjects == 0){
    TransporterFacade::stop_instance();
  }//if

  NdbMutex_Unlock(&createNdbMutex);
  
//  if (theSchemaConToNdbList != NULL)
//    closeSchemaTransaction(theSchemaConToNdbList);
  while ( theConIdleList != NULL )
    freeNdbCon();
  while ( theSignalIdleList != NULL )
    freeSignal();
  while (theRecAttrIdleList != NULL)
    freeRecAttr(); 
  while (theOpIdleList != NULL)
    freeOperation();
  while (theScanOpIdleList != NULL)
    freeScanOperation();
  while (theIndexOpIdleList != NULL)
    freeIndexOperation();
  while (theLabelList != NULL)
    freeNdbLabel();
  while (theBranchList != NULL)
    freeNdbBranch();
   while (theSubroutineList != NULL)
    freeNdbSubroutine();
   while (theCallList != NULL)
    freeNdbCall();
  while (theScanList != NULL)
    freeNdbScanRec();
  while (theNdbBlobIdleList != NULL)
    freeNdbBlob();
  
  releaseTransactionArrays();
  startTransactionNodeSelectionData.release();

  delete []theConnectionArray;
  delete []theDBnodes;
  delete []the_release_ind;
  if(theCommitAckSignal != NULL){
    delete theCommitAckSignal; 
    theCommitAckSignal = NULL;
  }

  if(theNdbObjectIdMap != 0)
    delete theNdbObjectIdMap;

  /** 
   *  This sleep is to make sure that the transporter 
   *  send thread will come in and send any
   *  signal buffers that this thread may have allocated.
   *  If that doesn't happen an error will occur in OSE
   *  when trying to restore a signal buffer allocated by a thread
   *  that have been killed.
   */
#ifdef NDB_OSE
  NdbSleep_MilliSleep(50);
#endif

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
}

NdbWaiter::NdbWaiter(){
  m_node = 0;
  m_state = NO_WAIT;
  m_mutex = 0;
  m_condition = NdbCondition_Create();
}

NdbWaiter::~NdbWaiter(){
  NdbCondition_Destroy(m_condition);
}


