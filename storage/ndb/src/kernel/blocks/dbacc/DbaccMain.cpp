/* Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include <cstdint>
#include <cstring>

#define DBACC_C
#include "Dbacc.hpp"

#include <AttributeHeader.hpp>
#include <Bitmask.hpp>
#include <signaldata/AccFrag.hpp>
#include <signaldata/AccScan.hpp>
#include <signaldata/NextScan.hpp>
#include <signaldata/AccLock.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/TuxMaint.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include <KeyDescriptor.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <md5_hash.hpp>
#include <EventLogger.hpp>

extern EventLogger* g_eventLogger;

#ifdef VM_TRACE
#define ACC_DEBUG(x) ndbout << "DBACC: "<< x << endl;
#else
#define ACC_DEBUG(x)
#endif

#ifdef ACC_SAFE_QUEUE
#define vlqrequire(x) do { if (unlikely(!(x))) {\
   dump_lock_queue(loPtr); \
   ndbabort(); } } while(0)
#else
#define vlqrequire(x) ndbrequire(x)
#define dump_lock_queue(x)
#endif

#ifdef VM_TRACE
//#define DO_TRANSIENT_POOL_STAT 1
#endif

// primary key is stored in TUP
#include "../dbtup/Dbtup.hpp"
#include "../dblqh/Dblqh.hpp"
/**
 * DBACC interface description
 * ---------------------------
 * DBACC is a block that performs a mapping between a key and a local key.
 * DBACC stands for DataBase ACCess Manager.
 * DBACC also handles row locks, each element in DBACC is referring to a
 * row through a local key. This row can be locked in DBACC.
 *
 * It has the following services it provides:
 * 1) ACCKEYREQ
 *    This is the by far most important interface. The user normally sends
 *    in a key, this key is a concatenation of a number of primary key
 *    columns in the table. Each column will be rounded up to the nearest
 *    4 bytes and the columns will be concatenated.
 *
 *    The ACCKEYREQ interface can be used to insert a key element, to delete
 *    a key element and to get the local key given a key.
 *
 *    The actual insert happens immediately in the prepare phase. But the
 *    insert must be followed by a later call to the signal ACCMINUPDATE
 *    that provides the local key for the inserted element.
 *
 *    The actual delete happens when the delete is committed through the
 *    ACC_COMMITREQ interface. The ACC_COMMITREQ signal also removes any
 *    row locks owned by the operation started by ACCKEYREQ.
 *
 *    Normally ACCKEYREQ responds immediate, in this case the return
 *    signal is passed in the signal object when returning from the
 *    execACCKEYREQ method. The return could come later if the row
 *    was locked, in this case a specific ACCKEYCONF signal is sent
 *    later where we have also locked the row.
 *
 *    So the basic ACCKEYREQ service works like this:
 *    1) Receive ACCKEYREQ, handle it and respond with ACCKEYCONF either
 *       immediate or at a later time. The message can also be immediately
 *       refused with an ACCKEYREF signal passed back immediately.
 *    2) For inserts the local key is provided later with a ACCMINUPDATE
 *       signal.
 *    3) The locks can be taken over by another operation, this operation
 *       can be initiated both through the ACCKEYREQ service or through
 *       the scan service. The takeover is initiated by a ACCKEYREQ call
 *       that has the take over flag set and that calls ACC_TO_REQ.
 *    4) Operations can be committed through ACC_COMMITREQ and they can
 *       aborted through ACC_ABORTREQ.
 *
 * 2) ACC_LOCKREQ
 *    The ACC_LOCKREQ service provides an interface to lock a row through
 *    a local key. It also provides a service to unlock a row through the
 *    same interface. This service is mainly used by blocks performing
 *    various types of scan services where the scan requires a lock to be
 *    taken on the row.
 *    The ACC_LOCKREQ interface is an interface built on top of the
 *    ACCKEYREQ service.
 *
 * 3) Scan service
 *    ACC can handle up to 12 concurrent full partition scans. The partition
 *    is scanned in hash table order.
 *    The ACC_LOCKREQ interface is an interface built on top of the
 *    ACCKEYREQ service.
 *
 * 3) Scan service
 *    ACC can handle up to 12 concurrent full partition scans. The partition
 *    is scanned in hash table order.
 *
 *    A scan is started up through the ACC_SCANREQ signal.
 *    After that the NEXT_SCANREQ provides a service to get the next row,
 *    to commit the previous row, to commit the previous and get the next
 *    row, to close the scan and to abort the scan.
 *
 *    For each row the row is represented by its local key. This is returned
 *    in the NEXT_SCANCONF signal. Actually this signal is often returned
 *    through a call to the LQH object through the method exec_next_scan_conf.
 *
 * 4) ACCFRAGREQ service
 *    The ACCFRAG service is used to add a new partition to handle in DBACC.
 * 5) DROP_TAB_REQ and DROP_FRAG_REQ service
 *    These services assist in dropping a partition and a table from DBACC.
 *
 * DBACC uses the following services:
 * ----------------------------------
 *
 * 1) prepareTUPKEYREQ
 *    This prepares DBTUP to read a row and to prefetch the row such that we
 *    can avoid lengthy cache misses. It provides a local key and a reference
 *    to the fragment information in DBTUP.
 *
 * 2) prepare_scanTUPKEYREQ
 *    This prepares DBTUP to read a row that we are scanning. It provides
 *    the local key to DBTUP for this service.
 *
 * 3) accReadPk
 *    This reads the primary key in DBACC format from DBTUP provided the
 *    local key.
 *
 * 4) readPrimaryKeys
 *    This reads the primary key in DBACC format from DBLQH using the
 *    operation record as key.
 *
 * Reading the primary key is performed as a last step in ensuring that
 * the hash entry refers to the primary key we are looking for.
 *
 * Overview description
 * ....................
 * On a very high level DBACC maps keys to local keys and it performs a row
 * locking service for rows. It implements this using the LH^3 data structure.
 *
 * Local keys
 * ----------
 * ACC stores local keys that are row ids. The ACC implementation is agnostic
 * to whether it is a logical row id or a physical row id. It only matters in
 * communication to other services.
 *
 * Internal complexity
 * -------------------
 * The services provided by DBACC are fairly simple, much of the complexity
 * comes from handling scans while the data structure is constantly changing.
 * A lock service is inherently complex and never simple to implement.
 *
 * The hash data structure stores each row as one element of 8 bytes that
 * resides in a container, the container has an 8 byte header and there can
 * be up to 144 containers in a 8 kByte page. The pages are filled to around
 * 70% in the normal case. Thus each row requires about 15 bytes of memory
 * in DBACC.
 *
 * On a higher level each table fragment replica in NDB have one DBACC
 * partition. This can be either a normal table, a unique index table,
 * or a BLOB table.
 */

#define JAM_FILE_ID 345

// Index pages used by ACC instances, used by CMVMI to report index memory usage
extern Uint32 g_acc_pages_used[1 + MAX_NDBMT_LQH_WORKERS];

void
Dbacc::prepare_scan_ctx(Uint32 scanPtrI)
{
  (void)scanPtrI;
}

// Signal entries and statement blocks
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       COMMON SIGNAL RECEPTION MODULE                                              */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/* CONTINUEB                                       CONTINUE SIGNAL                   */
/* ******************------------------------------+                                 */
/*   SENDER: ACC,    LEVEL B       */
void Dbacc::execCONTINUEB(Signal* signal) 
{
  jamEntry();
  Uint32 tcase = signal->theData[0];
  switch (tcase) {
  case ZINITIALISE_RECORDS:
    jam();
    initialiseRecordsLab(signal,
                         signal->theData[1],
                         signal->theData[3],
                         signal->theData[4]);
    return;
    break;
  case ZREL_ROOT_FRAG:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      releaseRootFragResources(signal, tableId);
      break;
    }
  case ZREL_FRAG:
    {
      jam();
      Uint32 fragIndex = signal->theData[1];
      releaseFragResources(signal, fragIndex);
      break;
    }
  case ZREL_DIR:
    {
      jam();
      releaseDirResources(signal);
      break;
    }
  case ZACC_SHRINK_TRANSIENT_POOLS:
  {
    jam();
    Uint32 pool_index = signal->theData[1];
    ndbassert(signal->getLength() == 2);
    shrinkTransientPools(pool_index);
    break;
  }
#if (defined(VM_TRACE) || \
     defined(ERROR_INSERT)) && \
    defined(DO_TRANSIENT_POOL_STAT)

  case ZACC_TRANSIENT_POOL_STAT:
  {
    for (Uint32 pool_index = 0;
         pool_index < c_transient_pool_count;
         pool_index++)
    {
      g_eventLogger->info(
        "DBACC %u: Transient slot pool %u %p: Entry size %u:"
       " Free %u: Used %u: Used high %u: Size %u: For shrink %u",
       instance(),
       pool_index,
       c_transient_pools[pool_index],
       c_transient_pools[pool_index]->getEntrySize(),
       c_transient_pools[pool_index]->getNoOfFree(),
       c_transient_pools[pool_index]->getUsed(),
       c_transient_pools[pool_index]->getUsedHi(),
       c_transient_pools[pool_index]->getSize(),
       c_transient_pools_shrinking.get(pool_index));
    }
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 5000, 1);
    break;
  }
#endif
  default:
    ndbabort();
  }//switch
  return;
}//Dbacc::execCONTINUEB()

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/*       END OF COMMON SIGNAL RECEPTION MODULE                               */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/*       SYSTEM RESTART MODULE                                               */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
void Dbacc::execNDB_STTOR(Signal* signal) 
{
  jamEntry();
  BlockReference ndbcntrRef = signal->theData[0];
  Uint32 startphase = signal->theData[2];
  switch (startphase) {
  case ZSPH1:
    jam();
    break;
  case ZSPH2:
    break;
  case ZSPH3:
    break;
  case ZSPH6:
    jam();
    break;
  default:
    jam();
    /*empty*/;
    break;
  }//switch
  signal->theData[0] = reference();
  sendSignal(ndbcntrRef, GSN_NDB_STTORRY, signal, 1, JBB);
  return;
}//Dbacc::execNDB_STTOR()

/* ******************--------------------------------------------------------------- */
/* STTOR                                              START /  RESTART               */
/* ******************------------------------------+                                 */
/*   SENDER: ANY,    LEVEL B       */
void Dbacc::execSTTOR(Signal* signal) 
{
  jamEntry();
  Uint32 tstartphase = signal->theData[1];
  switch (tstartphase) {
  case 1:
    jam();
    if (m_is_query_block)
    {
      ndbrequire((c_tup = (Dbtup*)globalData.getBlock(DBQTUP,
                                                      instance())) != 0);
      ndbrequire((c_lqh = (Dblqh*)globalData.getBlock(DBQLQH,
                                                      instance())) != 0);
    }
    else
    {
      ndbrequire((c_tup = (Dbtup*)globalData.getBlock(DBTUP,
                                                      instance())) != 0);
      ndbrequire((c_lqh = (Dblqh*)globalData.getBlock(DBLQH,
                                                      instance())) != 0);
    }
    break;
  case 3:
#if (defined(VM_TRACE) || \
     defined(ERROR_INSERT)) && \
    defined(DO_TRANSIENT_POOL_STAT)

    /* Start reporting statistics for transient pools */
    signal->theData[0] = ZACC_TRANSIENT_POOL_STAT;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
#endif
    jam();
    break;
  }
  Uint32 signalkey = signal->theData[6];
  if (m_is_query_block)
  {
    signal->theData[0] = signalkey;
    signal->theData[1] = 3;
    signal->theData[2] = 2;
    signal->theData[3] = ZSPH1;
    signal->theData[4] = ZSPH3;
    signal->theData[5] = 255;
    sendSignal(DBQACC_REF, GSN_STTORRY, signal, 6, JBB);
  }
  else
  {
    signal->theData[0] = signalkey;
    signal->theData[1] = 3;
    signal->theData[2] = 2;
    signal->theData[3] = ZSPH1;
    signal->theData[4] = ZSPH3;
    signal->theData[5] = 255;
    BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : DBACC_REF;
    sendSignal(cntrRef, GSN_STTORRY, signal, 6, JBB);
  }
}//Dbacc::execSTTOR()

/* --------------------------------------------------------------------------------- */
/* ZSPH1                                                                             */
/* --------------------------------------------------------------------------------- */
void Dbacc::initialiseRecordsLab(Signal* signal,
                                 Uint32 index,
                                 Uint32 ref,
                                 Uint32 data)
{
  switch (index)
  {
  case 0:
    jam();
    initialiseTableRec();
    break;
  case 1:
  case 2:
    break;
  case 3:
    jam();
    break;
  case 4:
    jam();
    break;
  case 5:
    jam();
    break;
  case 6:
    jam();
    initialiseFragRec();
    break;
  case 7:
    jam();
    break;
  case 8:
    jam();
    initialisePageRec();
    break;
  case 9:
    jam();
    break;
  case 10:
    jam();

    {
      ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = data;
      sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
		 ReadConfigConf::SignalLength, JBB);
    }
    return;
    break;
  default:
    ndbabort();
  }//switch

  signal->theData[0] = ZINITIALISE_RECORDS;
  signal->theData[1] = index + 1;
  signal->theData[2] = 0;
  signal->theData[3] = ref;
  signal->theData[4] = data;
  sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
  return;
}//Dbacc::initialiseRecordsLab()

void Dbacc::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);
  
  jamEntry();

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_ACC_FRAGMENT, &cfragmentsize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_ACC_TABLE, &ctablesize));
  initRecords(p);

  initialiseRecordsLab(signal, 0, ref, senderData);
  return;
}

/* --------------------------------------------------------------------------------- */
/* INITIALISE_FRAG_REC                                                               */
/*              INITIALATES THE FRAGMENT RECORDS.                                    */
/* --------------------------------------------------------------------------------- */
void Dbacc::initialiseFragRec()
{
  if (m_is_query_block)
  {
    cfirstfreefrag = RNIL;
    return;
  }
  FragmentrecPtr regFragPtr;
  ndbrequire(cfragmentsize > 0);
  for (regFragPtr.i = 0; regFragPtr.i < cfragmentsize; regFragPtr.i++) {
    jam();
    refresh_watch_dog();
    ptrAss(regFragPtr, fragmentrec);
    initFragGeneral(regFragPtr);
    regFragPtr.p->nextfreefrag = regFragPtr.i + 1;
  }//for
  regFragPtr.i = cfragmentsize - 1;
  ptrAss(regFragPtr, fragmentrec);
  regFragPtr.p->nextfreefrag = RNIL;
  cfirstfreefrag = 0;
}//Dbacc::initialiseFragRec()

/* --------------------------------------------------------------------------------- */
/* INITIALISE_PAGE_REC                                                               */
/*              INITIALATES THE PAGE RECORDS.                                        */
/* --------------------------------------------------------------------------------- */
void Dbacc::initialisePageRec()
{
  cnoOfAllocatedPages = 0;
  cnoOfAllocatedPagesMax = 0;
}//Dbacc::initialisePageRec()


/* --------------------------------------------------------------------------------- */
/* INITIALISE_TABLE_REC                                                              */
/*              INITIALATES THE TABLE RECORDS.                                       */
/* --------------------------------------------------------------------------------- */
void Dbacc::initialiseTableRec()
{
  if (m_is_query_block)
  {
    return;
  }
  ndbrequire(ctablesize > 0);
  for (tabptr.i = 0; tabptr.i < ctablesize; tabptr.i++) {
    refresh_watch_dog();
    ptrAss(tabptr, tabrec);
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragholder); i++) {
      tabptr.p->fragholder[i] = RNIL;
      tabptr.p->fragptrholder[i] = RNIL;
    }//for
  }//for
}//Dbacc::initialiseTableRec()

void Dbacc::set_tup_fragptr(Uint32 fragptr, Uint32 tup_fragptr)
{
  fragrecptr.i = fragptr;
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  fragrecptr.p->tupFragptr = tup_fragptr;
}

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF SYSTEM RESTART MODULE                                                */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       ADD/DELETE FRAGMENT MODULE                                                  */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
void Dbacc::execACCFRAGREQ(Signal* signal) 
{
  const AccFragReq * const req = (AccFragReq*)&signal->theData[0];
  jamEntry();
  if (ERROR_INSERTED(3001)) {
    jam();
    addFragRefuse(signal, 1);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }
  tabptr.i = req->tableId;
#ifndef VM_TRACE
  // config mismatch - do not crash if release compiled
  if (tabptr.i >= ctablesize) {
    jam();
    addFragRefuse(signal, 640);
    return;
  }
#endif
  ptrCheckGuard(tabptr, ctablesize, tabrec);
  ndbrequire((req->reqInfo & 0xF) == ZADDFRAG);
  ndbrequire(!getfragmentrec(fragrecptr, req->fragId));
  if (cfirstfreefrag == RNIL) {
    jam();
    addFragRefuse(signal, ZFULL_FRAGRECORD_ERROR);
    return;
  }//if

  ndbassert(req->localKeyLen == 1);
  if (req->localKeyLen != 1)
  {
    jam();
    addFragRefuse(signal, ZLOCAL_KEY_LENGTH_ERROR);
    return;
  }
  seizeFragrec();
  initFragGeneral(fragrecptr);
  initFragAdd(signal, fragrecptr);

  if (!addfragtotab(fragrecptr.i, req->fragId)) {
    jam();
    releaseFragRecord(fragrecptr);
    addFragRefuse(signal, ZFULL_FRAGRECORD_ERROR);
    return;
  }//if
  Page8Ptr spPageptr;
  ndbassert(!m_is_query_block);
  Uint32 result = seizePage(spPageptr,
                            Page32Lists::ANY_SUB_PAGE,
                            c_allow_use_of_spare_pages,
                            fragrecptr,
                            jamBuffer());
  if (result > ZLIMIT_OF_ERROR) {
    jam();
    addFragRefuse(signal, result);
    return;
  }//if
  if (!setPagePtr(fragrecptr.p->directory, 0, spPageptr.i))
  {
    jam();
    releasePage(spPageptr, fragrecptr, jamBuffer());
    addFragRefuse(signal, ZDIR_RANGE_FULL_ERROR);
    return;
  }

  initPage(spPageptr, 0);

  Uint32 userPtr = req->userPtr;
  BlockReference retRef = req->userRef;
  fragrecptr.p->rootState = ACTIVEROOT;

  AccFragConf * const conf = (AccFragConf*)&signal->theData[0];
  conf->userPtr = userPtr;
  conf->rootFragPtr = RNIL;
  conf->fragId[0] = fragrecptr.p->fragmentid;
  conf->fragId[1] = RNIL;
  conf->fragPtr[0] = fragrecptr.i;
  conf->fragPtr[1] = RNIL;
  conf->rootHashCheck = fragrecptr.p->roothashcheck;
  sendSignal(retRef, GSN_ACCFRAGCONF, signal, AccFragConf::SignalLength, JBB);
}//Dbacc::execACCFRAGREQ()

void Dbacc::addFragRefuse(Signal* signal, Uint32 errorCode) const
{
  const AccFragReq * const req = (AccFragReq*)&signal->theData[0];  
  AccFragRef * const ref = (AccFragRef*)&signal->theData[0];  
  Uint32 userPtr = req->userPtr;
  BlockReference retRef = req->userRef;

  ref->userPtr = userPtr;
  ref->errorCode = errorCode;
  sendSignal(retRef, GSN_ACCFRAGREF, signal, AccFragRef::SignalLength, JBB);
  return;
}//Dbacc::addFragRefuseEarly()

void
Dbacc::execDROP_TAB_REQ(Signal* signal){
  jamEntry();
  DropTabReq* req = (DropTabReq*)signal->getDataPtr();

  TabrecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctablesize, tabrec);
  
  tabPtr.p->tabUserRef = req->senderRef;
  tabPtr.p->tabUserPtr = req->senderData;
  tabPtr.p->tabUserGsn = GSN_DROP_TAB_REQ;

  signal->theData[0] = ZREL_ROOT_FRAG;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbacc::execDROP_FRAG_REQ(Signal* signal){
  jamEntry();
  DropFragReq* req = (DropFragReq*)signal->getDataPtr();

  TabrecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctablesize, tabrec);

  tabPtr.p->tabUserRef = req->senderRef;
  tabPtr.p->tabUserPtr = req->senderData;
  tabPtr.p->tabUserGsn = GSN_DROP_FRAG_REQ;

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->fragholder); i++)
  {
    jam();
    if (tabPtr.p->fragholder[i] == req->fragId)
    {
      jam();
      tabPtr.p->fragholder[i] = RNIL;
      releaseFragResources(signal, tabPtr.p->fragptrholder[i]);
      return;
    }//if
  }//for
  
  releaseRootFragResources(signal, req->tableId);
}

void Dbacc::releaseRootFragResources(Signal* signal, Uint32 tableId)
{
  TabrecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctablesize, tabrec);

  if (tabPtr.p->tabUserGsn == GSN_DROP_TAB_REQ)
  {
    jam();
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->fragholder); i++)
    {
      jam();
      if (tabPtr.p->fragholder[i] != RNIL)
      {
        jam();
        tabPtr.p->fragholder[i] = RNIL;
        releaseFragResources(signal, tabPtr.p->fragptrholder[i]);
        return;
      }
    }

    /**
     * Finished...
     */
    DropTabConf * const dropConf = (DropTabConf *)signal->getDataPtrSend();
    dropConf->senderRef = reference();
    dropConf->senderData = tabPtr.p->tabUserPtr;
    dropConf->tableId = tabPtr.i;
    sendSignal(tabPtr.p->tabUserRef, GSN_DROP_TAB_CONF,
               signal, DropTabConf::SignalLength, JBB);
  }
  else
  {
    ndbrequire(tabPtr.p->tabUserGsn == GSN_DROP_FRAG_REQ);

    DropFragConf * conf = (DropFragConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = tabPtr.p->tabUserPtr;
    conf->tableId = tabPtr.i;
    sendSignal(tabPtr.p->tabUserRef, GSN_DROP_FRAG_CONF,
               signal, DropFragConf::SignalLength, JBB);
  }
  
  tabPtr.p->tabUserPtr = RNIL;
  tabPtr.p->tabUserRef = 0;
  tabPtr.p->tabUserGsn = 0;
}//Dbacc::releaseRootFragResources()

void Dbacc::releaseFragResources(Signal* signal, Uint32 fragIndex)
{
  jam();
  FragmentrecPtr regFragPtr;
  regFragPtr.i = fragIndex;
  ptrCheckGuard(regFragPtr, cfragmentsize, fragmentrec);
  ndbrequire(regFragPtr.p->lockCount == 0);

  if (regFragPtr.p->expandOrShrinkQueued)
  {
    regFragPtr.p->level.clear();

    // slack > 0 ensures EXPANDCHECK2 will do nothing.
    regFragPtr.p->slack = 1;

    // slack <= slackCheck ensures SHRINKCHECK2 will do nothing.
    regFragPtr.p->slackCheck = regFragPtr.p->slack;

    /**
     * Wait out pending expand or shrink.
     * They need a valid Fragmentrec.
     */
    signal->theData[0] = ZREL_FRAG;
    signal->theData[1] = regFragPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  if (!regFragPtr.p->directory.isEmpty()) {
    jam();
    DynArr256::ReleaseIterator iter;
    DynArr256 dir(directoryPoolPtr, regFragPtr.p->directory);
    dir.init(iter);
    signal->theData[0] = ZREL_DIR;
    signal->theData[1] = regFragPtr.i;
    memcpy(&signal->theData[2], &iter, sizeof(iter));
    sendSignal(reference(), GSN_CONTINUEB, signal, 2 + sizeof(iter) / 4, JBB);
  } else {
    jam();
    {
      ndbassert(static_cast<Uint32>(regFragPtr.p->m_noOfAllocatedPages) == 
                  regFragPtr.p->sparsepages.getCount() +
                  regFragPtr.p->fullpages.getCount());
      regFragPtr.p->m_noOfAllocatedPages = 0;

      LocalPage8List freelist(c_page8_pool, cfreepages);
      cnoOfAllocatedPages -= regFragPtr.p->sparsepages.getCount();
      freelist.appendList(regFragPtr.p->sparsepages);
      cnoOfAllocatedPages -= regFragPtr.p->fullpages.getCount();
      freelist.appendList(regFragPtr.p->fullpages);
      ndbassert(pages.getCount() == cfreepages.getCount() + cnoOfAllocatedPages);
      ndbassert(pages.getCount() <= cpageCount);
    }
    jam();
    Uint32 tab = regFragPtr.p->mytabptr;
    releaseFragRecord(regFragPtr);
    signal->theData[0] = ZREL_ROOT_FRAG;
    signal->theData[1] = tab;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }//if
  ndbassert(validatePageCount());
}//Dbacc::releaseFragResources()

void Dbacc::releaseDirResources(Signal* signal)
{
  jam();
  Uint32 fragIndex = signal->theData[1];

  DynArr256::ReleaseIterator iter;
  memcpy(&iter, &signal->theData[2], sizeof(iter));

  FragmentrecPtr regFragPtr;
  regFragPtr.i = fragIndex;
  ptrCheckGuard(regFragPtr, cfragmentsize, fragmentrec);
  ndbrequire(regFragPtr.p->lockCount == 0);

  DynArr256::Head* directory;
  ndbrequire(signal->theData[0] == ZREL_DIR);
  directory = &regFragPtr.p->directory;

  DynArr256 dir(directoryPoolPtr, *directory);
  Uint32 ret = 0;
  Uint32 pagei;
  fragrecptr = regFragPtr;
  int count = 32;
  while (count > 0 &&
         (ret = dir.release(iter, &pagei)) != 0)
  {
    jam();
    count--;
    if (ret == 1 && pagei != RNIL)
    {
      jam();
      Page8Ptr rpPageptr;
      rpPageptr.i = pagei;
      c_page8_pool.getPtr(rpPageptr);
      releasePage(rpPageptr, fragrecptr, jamBuffer());
    }
  }
  while (ret == 0 && count > 0 && !cfreepages.isEmpty())
  {
    jam();
    Page8Ptr page;
    LocalPage8List freelist(c_page8_pool, cfreepages);
    freelist.removeFirst(page);
    pages.releasePage8(c_page_pool, page);
    Page32Ptr page32ptr;
    pages.dropLastPage32(c_page_pool, page32ptr, 5);
    if (page32ptr.i != RNIL)
    {
      jam();
      g_acc_pages_used[instance()]--;
      ndbassert(cpageCount >= 4);
      cpageCount -= 4; // 8KiB pages per 32KiB page
      m_ctx.m_mm.release_page(RT_DBACC_PAGE, page32ptr.i);
    }
    count--;
  }
  if (ret != 0 || !cfreepages.isEmpty())
  {
    jam();
    memcpy(&signal->theData[2], &iter, sizeof(iter));
    sendSignal(reference(), GSN_CONTINUEB, signal, 2 + sizeof(iter) / 4, JBB);
  }
  else
  {
    jam();
    signal->theData[0] = ZREL_FRAG;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}//Dbacc::releaseDirResources()

void Dbacc::releaseFragRecord(FragmentrecPtr regFragPtr)
{
  regFragPtr.p->nextfreefrag = cfirstfreefrag;
  for (Uint32 i = 0; i < NUM_ACC_FRAGMENT_MUTEXES; i++)
  {
    NdbMutex_Deinit(&regFragPtr.p->acc_frag_mutex[i]);
  }
  cfirstfreefrag = regFragPtr.i;
  initFragGeneral(regFragPtr);
  RSS_OP_FREE(cnoOfFreeFragrec);
}//Dbacc::releaseFragRecord()

/* -------------------------------------------------------------------------- */
/* ADDFRAGTOTAB                                                               */
/*       DESCRIPTION: PUTS A FRAGMENT ID AND A POINTER TO ITS RECORD INTO     */
/*                                TABLE ARRAY OF THE TABLE RECORD.            */
/* -------------------------------------------------------------------------- */
bool Dbacc::addfragtotab(Uint32 rootIndex, Uint32 fid) const
{
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragholder); i++) {
    jam();
    if (tabptr.p->fragholder[i] == RNIL) {
      jam();
      tabptr.p->fragholder[i] = fid;
      tabptr.p->fragptrholder[i] = rootIndex;
      return true;
    }//if
  }//for
  return false;
}//Dbacc::addfragtotab()

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF ADD/DELETE FRAGMENT MODULE                                           */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       CONNECTION MODULE                                                           */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/* ACCSEIZEREQ                                           SEIZE REQ                   */
/*                                                    SENDER: LQH,    LEVEL B        */
/*          ENTER ACCSEIZEREQ WITH                                                   */
/*                    TUSERPTR ,                     CONNECTION PTR OF LQH            */
/*                    TUSERBLOCKREF                  BLOCK REFERENCE OF LQH          */
/* ******************--------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/* ACCSEIZEREQ                                           SEIZE REQ                   */
/* ******************------------------------------+                                 */
/*   SENDER: LQH,    LEVEL B       */
void Dbacc::execACCSEIZEREQ(Signal* signal) 
{
  jamEntry();
  Uint32 userptr = signal->theData[0];
  /* CONNECTION PTR OF LQH            */
  BlockReference userblockref = signal->theData[1];
  /* BLOCK REFERENCE OF LQH          */
  if (!oprec_pool.seize(operationRecPtr))
  {
    jam();
    Uint32 result = ZCONNECT_SIZE_ERROR;
    signal->theData[0] = userptr;
    signal->theData[1] = result;
    sendSignal(userblockref, GSN_ACCSEIZEREF, signal, 2, JBB);
    return;
  }//if
  operationRecPtr.p->userptr = userptr;
  operationRecPtr.p->userblockref = userblockref;
  /* ******************************< */
  /* ACCSEIZECONF                    */
  /* ******************************< */
  signal->theData[0] = userptr;
  signal->theData[1] = operationRecPtr.i;
  sendSignal(userblockref, GSN_ACCSEIZECONF, signal, 2, JBB);
  return;
}//Dbacc::execACCSEIZEREQ()

Dbacc::Operationrec*
Dbacc::get_operation_ptr(Uint32 i)
{
  OperationrecPtr opPtr;
  opPtr.i = i;
  ndbrequire(oprec_pool.getValidPtr(opPtr));
  return opPtr.p;
}

bool Dbacc::seize_op_rec(Uint32 userptr,
                         BlockReference ref,
                         Uint32 &i_val,
                         Dbacc::Operationrec **ptr)
{
  OperationrecPtr opPtr;
  if (unlikely(!oprec_pool.seize(opPtr)))
  {
    jam();
    return false;
  }
  opPtr.p->userptr = userptr;
  opPtr.p->userblockref = ref;
  i_val = opPtr.i;
  *ptr = opPtr.p;
  return true;
}

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF CONNECTION MODULE                                                    */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       EXECUTE OPERATION MODULE                                                    */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* INIT_OP_REC                                                                       */
/*           INFORMATION WHICH IS RECEIVED BY ACCKEYREQ WILL BE SAVED                */
/*           IN THE OPERATION RECORD.                                                */
/* --------------------------------------------------------------------------------- */
void Dbacc::initOpRec(const AccKeyReq* signal, Uint32 siglen) const
{
  Uint32 Treqinfo;

  Treqinfo = signal->requestInfo;

  operationRecPtr.p->hashValue = LHBits32(signal->hashValue);
  operationRecPtr.p->tupkeylen = signal->keyLen;
  operationRecPtr.p->m_scanOpDeleteCountOpRef = RNIL;
  operationRecPtr.p->transId1 = signal->transId1;
  operationRecPtr.p->transId2 = signal->transId2;

  const bool readOp = AccKeyReq::getLockType(Treqinfo) == ZREAD;
  const bool dirtyOp = AccKeyReq::getDirtyOp(Treqinfo);
  const bool dirtyReadOp = readOp & dirtyOp;
  const bool noWait = AccKeyReq::getNoWait(Treqinfo);
  Uint32 operation = AccKeyReq::getOperation(Treqinfo);
  if (operation == ZREFRESH)
    operation = ZWRITE; /* Insert if !exist, otherwise lock */

  Uint32 opbits = 0;
  opbits |= operation;
  opbits |= readOp ? 0 : (Uint32) Operationrec::OP_LOCK_MODE;
  opbits |= readOp ? 0 : (Uint32) Operationrec::OP_ACC_LOCK_MODE;
  opbits |= dirtyReadOp ? (Uint32) Operationrec::OP_DIRTY_READ : 0;
  opbits |= noWait ? (Uint32) Operationrec::OP_NOWAIT : 0;
  if (AccKeyReq::getLockReq(Treqinfo))
  {
    opbits |= Operationrec::OP_LOCK_REQ;            // TUX LOCK_REQ

    /**
     * A lock req has SCAN_OP, it can't delete a row,
     *   so OP_COMMIT_DELETE_CHECK is set like for SCAN
     *   see initScanOpRec
     */
    opbits |= Operationrec::OP_COMMIT_DELETE_CHECK;

    /**
     * TODO: Looking at it now, I think it would be more natural
     *       to treat it as a ZREAD...
     */
  }
  
  //operationRecPtr.p->nodeType = AccKeyReq::getReplicaType(Treqinfo);
  ndbrequire(operationRecPtr.p->m_op_bits == Operationrec::OP_INITIAL);
  operationRecPtr.p->fid = fragrecptr.p->myfid;
  operationRecPtr.p->fragptr = fragrecptr.i;
  operationRecPtr.p->nextParallelQue = RNIL;
  operationRecPtr.p->prevParallelQue = RNIL;
  operationRecPtr.p->nextSerialQue = RNIL;
  operationRecPtr.p->prevSerialQue = RNIL;
  operationRecPtr.p->elementPage = RNIL;
  operationRecPtr.p->scanRecPtr = RNIL;
  operationRecPtr.p->m_op_bits = opbits;
  NdbTick_Invalidate(&operationRecPtr.p->m_lockTime);

  // bit to mark lock operation
  // undo log is not run via ACCKEYREQ

  if (operationRecPtr.p->tupkeylen == 0)
  {
    static_assert(AccKeyReq::SignalLength_localKey == 10);
    ndbassert(siglen == AccKeyReq::SignalLength_localKey);
  }
  else
  {
    static_assert(AccKeyReq::SignalLength_keyInfo == 8);
    ndbassert(siglen == AccKeyReq::SignalLength_keyInfo + operationRecPtr.p->tupkeylen);
  }
}//Dbacc::initOpRec()

/* --------------------------------------------------------------------------------- */
/* SEND_ACCKEYCONF                                                                   */
/* --------------------------------------------------------------------------------- */
void Dbacc::sendAcckeyconf(Signal* signal) const
{
  signal->theData[0] = operationRecPtr.p->userptr;
  signal->theData[1] = operationRecPtr.p->m_op_bits & Operationrec::OP_MASK;
  signal->theData[2] = operationRecPtr.p->fid;
  signal->theData[3] = operationRecPtr.p->localdata.m_page_no;
  signal->theData[4] = operationRecPtr.p->localdata.m_page_idx;
}//Dbacc::sendAcckeyconf()

/* ******************--------------------------------------------------------------- */
/* ACCKEYREQ                                         REQUEST FOR INSERT, DELETE,     */
/*                                                   RERAD AND UPDATE, A TUPLE.      */
/*                                                   SENDER: LQH,    LEVEL B         */
/*  SIGNAL DATA:      OPERATION_REC_PTR,             CONNECTION PTR                  */
/*                    TABPTR,                        TABLE ID = TABLE RECORD POINTER */
/*                    TREQINFO,                                                      */
/*                    THASHVALUE,                    HASH VALUE OF THE TUP           */
/*                    TKEYLEN,                       LENGTH OF THE PRIMARY KEYS      */
/*                    TKEY1,                         PRIMARY KEY 1                   */
/*                    TKEY2,                         PRIMARY KEY 2                   */
/*                    TKEY3,                         PRIMARY KEY 3                   */
/*                    TKEY4,                         PRIMARY KEY 4                   */
/* ******************--------------------------------------------------------------- */
void Dbacc::execACCKEYREQ(Signal* signal,
                          Uint32 opPtrI,
                          Dbacc::Operationrec *opPtrP) 
{
  jamEntryDebug();
  AccKeyReq* const req = reinterpret_cast<AccKeyReq*>(&signal->theData[0]);
  fragrecptr.i = req->fragmentPtr;        /* FRAGMENT RECORD POINTER         */
  ndbrequire(fragrecptr.i < cfragmentsize);
  ptrAss(fragrecptr, fragmentrec);
  operationRecPtr.i = opPtrI;
  operationRecPtr.p = opPtrP;
  initOpRec(req, signal->getLength());
  ndbrequire(Magic::check_ptr(operationRecPtr.p));

  /*---------------------------------------------------------------*/
  /*                                                               */
  /*       WE WILL USE THE HASH VALUE TO LOOK UP THE PROPER MEMORY */
  /*       PAGE AND MEMORY PAGE INDEX TO START THE SEARCH WITHIN.  */
  /*       WE REMEMBER THESE ADDRESS IF WE LATER NEED TO INSERT    */
  /*       THE ITEM AFTER NOT FINDING THE ITEM.                    */
  /*---------------------------------------------------------------*/
  OperationrecPtr lockOwnerPtr;
  Page8Ptr bucketPageptr;
  Uint32 bucketConidx;
  Page8Ptr elemPageptr;
  Uint32 elemConptr;
  Uint32 elemptr;

  /**
   * The below two mutexes are required to acquire for query threads.
   * The TUP page map mutex ensures that the LDM thread won't change
   * any mappings from logical page id to physical page id while we
   * are searching for a row in the ACC hash index. The LDM threads
   * are protected by this since there is only one LDM thread that
   * can change this page map.
   *
   * The ACC fragment mutexes are used to ensure that we either see
   * a row or not. This protects the local key in the elements and
   * it protects information in the lock queue about whether the
   * row has been deleted or not. Again the LDM thread is protected
   * without mutex, so both these mutexes are only acquired by
   * query threads.
   *
   * In the code below we will ensure that these mutexes are released
   * in all code paths that can be taken by the query threads. Those
   * code paths that cannot be taken by the query threads all have an
   * ndbassert on that m_is_in_query_thread is false.
   *
   * We need to release the ACC fragment mutex before calling 
   * prepareTUPKEYREQ since this function will acquire the TUP
   * page map mutex again and doing so without releasing the
   * ACC fragment mutex first would cause a mutex deadlock.
   */
  c_tup->acquire_frag_page_map_mutex_read();
  acquire_frag_mutex_get(fragrecptr.p, operationRecPtr);
  const Uint32 found = getElement(req,
                                  lockOwnerPtr,
                                  bucketPageptr,
                                  bucketConidx,
                                  elemPageptr,
                                  elemConptr,
                                  elemptr);
  c_tup->release_frag_page_map_mutex_read();

  Uint32 opbits = operationRecPtr.p->m_op_bits;

  if (unlikely(AccKeyReq::getTakeOver(req->requestInfo)))
  {
    /* Verify that lock taken over and operation are on same
     * element by checking that lockOwner match.
     */
    jamDebug();
    OperationrecPtr lockOpPtr;
    ndbassert(!m_is_query_block);
    lockOpPtr.i = req->lockConnectPtr;
    bool is_valid = oprec_pool.getValidPtr(lockOpPtr);
    if (lockOwnerPtr.i == RNIL ||
        !(lockOwnerPtr.i == lockOpPtr.i ||
        !is_valid ||
        lockOwnerPtr.i == lockOpPtr.p->m_lock_owner_ptr_i))
    {
      signal->theData[0] = Uint32(-1);
      signal->theData[1] = ZTO_OP_STATE_ERROR;
      operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
      ndbassert(!m_is_in_query_thread);
      return; /* Take over failed */
    }

    signal->theData[1] = req->lockConnectPtr;
    signal->theData[2] = operationRecPtr.p->transId1;
    signal->theData[3] = operationRecPtr.p->transId2;
    execACC_TO_REQ(signal);
    if (unlikely(signal->theData[0] == Uint32(-1)))
    {
      operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
      ndbassert(signal->theData[1] == ZTO_OP_STATE_ERROR);
      ndbassert(!m_is_in_query_thread);
      return; /* Take over failed */
    }
  }

  Uint32 op = opbits & Operationrec::OP_MASK;
  if (found == ZTRUE) 
  {
    switch (op) {
    case ZREAD:
    case ZUPDATE:
    case ZDELETE:
    case ZWRITE:
    case ZSCAN_OP:
      if (likely(!lockOwnerPtr.p))
      {
        release_frag_mutex_get(fragrecptr.p, operationRecPtr);
	if(unlikely(op == ZWRITE))
	{
	  jam();
	  opbits &= ~(Uint32)Operationrec::OP_MASK;
	  opbits |= (op = ZUPDATE);
	  operationRecPtr.p->m_op_bits = opbits; // store to get correct ACCKEYCONF
	}
	opbits |= Operationrec::OP_STATE_RUNNING;
	opbits |= Operationrec::OP_RUN_QUEUE;
        c_tup->prepareTUPKEYREQ(operationRecPtr.p->localdata.m_page_no,
                                operationRecPtr.p->localdata.m_page_idx,
                                fragrecptr.p->tupFragptr);
        sendAcckeyconf(signal);
        if (! (opbits & Operationrec::OP_DIRTY_READ))
        {
	  /*---------------------------------------------------------------*/
	  // It is not a dirty read. We proceed by locking and continue with
	  // the operation.
	  /*---------------------------------------------------------------*/
          jamDebug();
          ndbassert(!m_is_in_query_thread);
          Uint32 eh = elemPageptr.p->word32[elemptr];
          operationRecPtr.p->reducedHashValue =
            ElementHeader::getReducedHashValue(eh);
          operationRecPtr.p->elementPage = elemPageptr.i;
          operationRecPtr.p->elementContainer = elemConptr;
          operationRecPtr.p->elementPointer = elemptr;

	  eh = ElementHeader::setLocked(operationRecPtr.i);
	  fragrecptr.p->lockCount++;
	  opbits |= Operationrec::OP_LOCK_OWNER;
	  operationRecPtr.p->m_op_bits = opbits;

          /**
           * Ensure that any thread that reads element header also can see
           * the updates to the operation record. Only required when we are
           * using query threads.
           */
          query_thread_memory_barrier();
          elemPageptr.p->word32[elemptr] = eh;

          fragrecptr.p->
            m_lockStats.req_start_imm_ok((opbits & 
                                          Operationrec::OP_LOCK_MODE) 
                                         != ZREADLOCK,
                                         operationRecPtr.p->m_lockTime,
                                         getHighResTimer());
          
          return;
        }
        else
        {
          jamDebug();
	  /*---------------------------------------------------------------*/
	  // It is a dirty read. We do not lock anything. Set state to
	  // IDLE since no COMMIT call will come.
	  /*---------------------------------------------------------------*/
	  opbits = Operationrec::OP_EXECUTED_DIRTY_READ;
	  operationRecPtr.p->m_op_bits = opbits;
          return;
        }//if
      }
      else
      {
        jam();
        accIsLockedLab(signal, lockOwnerPtr);
        return;
      }//if
    case ZINSERT:
      jam();
      ndbassert(!m_is_in_query_thread);
      insertExistElemLab(signal, lockOwnerPtr);
      return;
    default:
      ndbabort();
    }//switch
  }
  else if (found == ZFALSE)
  {
    switch (op){
    case ZWRITE:
      opbits &= ~(Uint32)Operationrec::OP_MASK;
      opbits |= (op = ZINSERT);
      [[fallthrough]];
    case ZINSERT:
      jam();
      opbits |= Operationrec::OP_INSERT_IS_DONE;
      opbits |= Operationrec::OP_STATE_RUNNING;
      opbits |= Operationrec::OP_RUN_QUEUE;
      operationRecPtr.p->m_op_bits = opbits;
      insertelementLab(signal, bucketPageptr, bucketConidx);
      ndbassert(!m_is_in_query_thread);
      return;
    case ZREAD:
    case ZUPDATE:
    case ZDELETE:
    case ZSCAN_OP:
      jam();
      release_frag_mutex_get(fragrecptr.p, operationRecPtr);
      acckeyref1Lab(signal, ZREAD_ERROR);
      return;
    default:
      ndbabort();
    }//switch
  }
  else
  {
    jam();
    release_frag_mutex_get(fragrecptr.p, operationRecPtr);
    acckeyref1Lab(signal, found);
    return;
  }//if
  return;
}//Dbacc::execACCKEYREQ()

void
Dbacc::execACCKEY_ORD_no_ptr(Signal *signal,
                             Uint32 opPtrI)
{
  OperationrecPtr opPtr;
  opPtr.i = opPtrI;
  ndbrequire(oprec_pool.getValidPtr(opPtr));
  execACCKEY_ORD(signal, opPtrI, opPtr.p);
}

void
Dbacc::execACCKEY_ORD(Signal* signal,
                      Uint32 opPtrI,
                      Dbacc::Operationrec *opPtrP)
{
  jamEntryDebug();
  OperationrecPtr lastOp;
  lastOp.i = opPtrI;
  lastOp.p = opPtrP;
  Uint32 opbits = lastOp.p->m_op_bits;
  Uint32 opstate = opbits & Operationrec::OP_STATE_MASK;
  
  if (likely(opbits == Operationrec::OP_EXECUTED_DIRTY_READ))
  {
    jamDebug();
    lastOp.p->m_op_bits = Operationrec::OP_INITIAL;
    return;
  }
  else if (likely(opstate == Operationrec::OP_STATE_RUNNING))
  {
    opbits |= Operationrec::OP_STATE_EXECUTED;
    lastOp.p->m_op_bits = opbits;
    startNext(signal, lastOp);
    validate_lock_queue(lastOp);
    return;
  } 
  else
  {
  }

  g_eventLogger->info("bits: %.8x state: %.8x", opbits, opstate);
  ndbabort();
}

void
Dbacc::startNext(Signal* signal, OperationrecPtr lastOp)
{
  jam();
  OperationrecPtr nextOp;
  OperationrecPtr loPtr;
  OperationrecPtr tmp;
  nextOp.i = lastOp.p->nextParallelQue;
  loPtr.i = lastOp.p->m_lock_owner_ptr_i;
  const Uint32 opbits = lastOp.p->m_op_bits;
  
  if ((opbits & Operationrec::OP_STATE_MASK)!= Operationrec::OP_STATE_EXECUTED)
  {
    jam();
    return;
  }
  
  Uint32 nextbits;
  if (nextOp.i != RNIL)
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(nextOp));
    nextbits = nextOp.p->m_op_bits;
    goto checkop;
  }
  
  if ((opbits & Operationrec::OP_LOCK_OWNER) == 0)
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(loPtr));
  }
  else
  {
    jam();
    loPtr = lastOp;
  }
  
  nextOp.i = loPtr.p->nextSerialQue;
  ndbassert(loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
  
  if (nextOp.i == RNIL)
  {
    jam();
    return;
  }
  
  /**
   * There is an op in serial queue...
   *   Check if it can run
   */
  ndbrequire(oprec_pool.getValidPtr(nextOp));
  nextbits = nextOp.p->m_op_bits;
  
  {
    const bool same = nextOp.p->is_same_trans(lastOp.p);
    
    if (!same && ((opbits & Operationrec::OP_ACC_LOCK_MODE) ||
		  (nextbits & Operationrec::OP_LOCK_MODE)))
    {
      jam();
      /**
       * Not same transaction
       *  and either last hold exclusive lock
       *          or next need exclusive lock
       */
      return;
    }

    if (!same && (opbits & Operationrec::OP_ELEMENT_DISAPPEARED))
    {
      jam();
      /* This is the case described in Bug#19031389 with
       * T1: READ1, T1: READ2, T1:DELETE
       * T2: READ3
       * where out-of-order commits have left us with
       * T1: READ1, T1: READ2
       * T2: READ3
       * and then a commit of T1: READ1 or T1: READ2
       * causes us to consider whether to allow T2: READ3 to run
       *
       * The check above (!same_trans && (prev is EX || next is EX))
       * does not catch this case as the LOCK_MODE and ACC_LOCK_MODE
       * of the READ ops is not set as they were prepared *before* the
       * DELETE
       *
       * In general it might be nice if a transaction having a
       * mix of SH and EX locks were treated as all EX until it
       * fully commits
       *
       * However in the case of INS/UPD we are not (yet) aware of problems
       *
       * For DELETE, the problem is that allowing T2: READ3 to start (and
       * then immediately fail), messes up the reference counting for the
       * delete
       * So instead of that, lets not let it start until after the deleting
       * transaction is fully committed @ ACC
       *
       */
      return;
    }
    
    /**
     * same trans and X-lock already held -> Ok
     */
    if (same && (opbits & Operationrec::OP_ACC_LOCK_MODE))
    {
      jam();
      goto upgrade;
    }
  }

  /**
   * Fall through: No exclusive locks held
   * (There is a shared parallel queue)
   */
  ndbassert((opbits & Operationrec::OP_ACC_LOCK_MODE) == 0);

  /**
   * all shared lock...
   */
  if ((nextbits & Operationrec::OP_LOCK_MODE) == 0)
  {
    jam();
    goto upgrade;
  }
  
  /**
   * There is a shared parallel queue and exclusive op is requested.
   * We must check if there are other transactions in parallel queue...
   */
  tmp= loPtr;
  while (tmp.i != RNIL)
  {
    ndbrequire(oprec_pool.getValidPtr(tmp));
    if (!nextOp.p->is_same_trans(tmp.p))
    {
      jam();
      /**
       * parallel queue contained another transaction, dont let it run
       */
      return;
    }
    tmp.i = tmp.p->nextParallelQue;
  }
  
upgrade:
  /**
   * Move first op in serial queue to end of parallel queue
   */
  
  tmp.i = loPtr.p->nextSerialQue = nextOp.p->nextSerialQue;
  loPtr.p->m_lo_last_parallel_op_ptr_i = nextOp.i;
  nextOp.p->nextSerialQue = RNIL;
  nextOp.p->prevSerialQue = RNIL;
  nextOp.p->m_lock_owner_ptr_i = loPtr.i;
  nextOp.p->prevParallelQue = lastOp.i;
  lastOp.p->nextParallelQue = nextOp.i;
  nextbits |= (opbits & Operationrec::OP_ACC_LOCK_MODE);
  
  if (tmp.i != RNIL)
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(tmp));
    tmp.p->prevSerialQue = loPtr.i;
  }
  else
  {
    jam();
    loPtr.p->m_lo_last_serial_op_ptr_i = RNIL;
  }
  
  nextbits |= Operationrec::OP_RUN_QUEUE;
  
  /**
   * Currently no grouping of ops in serial queue
   */
  ndbrequire(nextOp.p->nextParallelQue == RNIL);

  /**
   * Track end-of-wait
   */
  {
    FragmentrecPtr frp;
    frp.i = nextOp.p->fragptr;
    ptrCheckGuard(frp, cfragmentsize, fragmentrec);
    
    frp.p->m_lockStats.wait_ok(((nextbits & 
                                 Operationrec::OP_LOCK_MODE) 
                                != ZREADLOCK),
                               nextOp.p->m_lockTime,
                               getHighResTimer());
  }
  
checkop:
  Uint32 errCode = 0;
  OperationrecPtr save = operationRecPtr;
  operationRecPtr = nextOp;
  
  Uint32 lastop = opbits & Operationrec::OP_MASK;
  Uint32 nextop = nextbits & Operationrec::OP_MASK;

  nextbits &= nextbits & ~(Uint32)Operationrec::OP_STATE_MASK;
  nextbits |= Operationrec::OP_STATE_RUNNING;

  if (lastop == ZDELETE)
  {
    jam();
    if (nextop != ZINSERT && nextop != ZWRITE)
    {
      errCode = ZREAD_ERROR;
      goto ref;
    }
    
    nextbits &= ~(Uint32)Operationrec::OP_MASK;
    nextbits &= ~(Uint32)Operationrec::OP_ELEMENT_DISAPPEARED;
    nextbits |= (nextop = ZINSERT);
    goto conf;
  }
  else if (nextop == ZINSERT)
  {
    jam();
    errCode = ZWRITE_ERROR;
    goto ref;
  }
  else if (nextop == ZWRITE)
  {
    jam();
    nextbits &= ~(Uint32)Operationrec::OP_MASK;
    nextbits |= (nextop = ZUPDATE);
    goto conf;
  }
  else
  {
    jam();
  }

conf:
  nextOp.p->m_op_bits = nextbits;
  nextOp.p->localdata = lastOp.p->localdata;
  
  if (nextop == ZSCAN_OP && (nextbits & Operationrec::OP_LOCK_REQ) == 0)
  {
    jam();
    takeOutScanLockQueue(nextOp.p->scanRecPtr);
    putReadyScanQueue(nextOp.p->scanRecPtr);
  }
  else
  {
    jam();
    fragrecptr.i = nextOp.p->fragptr;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    
    sendAcckeyconf(signal);
    sendSignal(nextOp.p->userblockref, GSN_ACCKEYCONF, 
	       signal, 6, JBB);
  }
  
  operationRecPtr = save;
  return;
  
ref:
  nextOp.p->m_op_bits = nextbits;
  
  if (nextop == ZSCAN_OP && (nextbits & Operationrec::OP_LOCK_REQ) == 0)
  {
    jam();
    nextOp.p->m_op_bits |= Operationrec::OP_ELEMENT_DISAPPEARED;
    takeOutScanLockQueue(nextOp.p->scanRecPtr);
    putReadyScanQueue(nextOp.p->scanRecPtr);
  }
  else
  {
    jam();
    signal->theData[0] = nextOp.p->userptr;
    signal->theData[1] = errCode;
    sendSignal(nextOp.p->userblockref, GSN_ACCKEYREF, signal, 
	       2, JBB);
  }    
  
  operationRecPtr = save;
  return;
}

void 
Dbacc::accIsLockedLab(Signal* signal, OperationrecPtr lockOwnerPtr)
{
  Uint32 bits = operationRecPtr.p->m_op_bits;
  validate_lock_queue(lockOwnerPtr);
  
  if ((bits & Operationrec::OP_DIRTY_READ) == 0)
  {
    Uint32 return_result;
    ndbassert(!m_is_in_query_thread);
    if ((bits & Operationrec::OP_LOCK_MODE) == ZREADLOCK)
    {
      jam();
      return_result = placeReadInLockQueue(lockOwnerPtr);
    }
    else
    {
      jam();
      return_result = placeWriteInLockQueue(lockOwnerPtr);
    }//if
    if (return_result == ZPARALLEL_QUEUE)
    {
      jamDebug();
      c_tup->prepareTUPKEYREQ(operationRecPtr.p->localdata.m_page_no,
                              operationRecPtr.p->localdata.m_page_idx,
                              fragrecptr.p->tupFragptr);

      fragrecptr.p->m_lockStats.req_start_imm_ok((bits & 
                                                  Operationrec::OP_LOCK_MODE) 
                                                 != ZREADLOCK,
                                                 operationRecPtr.p->m_lockTime,
                                                 getHighResTimer());

      sendAcckeyconf(signal);
      return;
    }
    else if (return_result == ZSERIAL_QUEUE)
    {
      jam();
      fragrecptr.p->m_lockStats.req_start((bits & 
                                           Operationrec::OP_LOCK_MODE) 
                                          != ZREADLOCK,
                                          operationRecPtr.p->m_lockTime,
                                          getHighResTimer());
      signal->theData[0] = RNIL;
      return;
    }
    else
    {
      jam();
      acckeyref1Lab(signal, return_result);
      return;
    }//if
    ndbabort();
  }
  else 
  {
    if (! (lockOwnerPtr.p->m_op_bits & Operationrec::OP_ELEMENT_DISAPPEARED) &&
	! lockOwnerPtr.p->localdata.isInvalid())
    {
      jamDebug();
      release_frag_mutex_get(fragrecptr.p, operationRecPtr);
      /* ---------------------------------------------------------------
       * It is a dirty read. We do not lock anything. Set state to
       * OP_EXECUTED_DIRTY_READ to prepare for COMMIT/ABORT call.
       * ---------------------------------------------------------------*/
      c_tup->prepareTUPKEYREQ(operationRecPtr.p->localdata.m_page_no,
                              operationRecPtr.p->localdata.m_page_idx,
                              fragrecptr.p->tupFragptr);
      sendAcckeyconf(signal);
      operationRecPtr.p->m_op_bits = Operationrec::OP_EXECUTED_DIRTY_READ;
      return;
    } 
    else 
    {
      jam();
      release_frag_mutex_get(fragrecptr.p, operationRecPtr);
      /*---------------------------------------------------------------*/
      // The tuple does not exist in the committed world currently.
      // Report read error.
      /*---------------------------------------------------------------*/
      acckeyref1Lab(signal, ZREAD_ERROR);
      return;
    }//if
  }//if
}//Dbacc::accIsLockedLab()

/* ------------------------------------------------------------------------ */
/*        I N S E R T      E X I S T      E L E M E N T                     */
/* ------------------------------------------------------------------------ */
void Dbacc::insertExistElemLab(Signal* signal,
                               OperationrecPtr lockOwnerPtr)
{
  if (!lockOwnerPtr.p)
  {
    jam();
    acckeyref1Lab(signal, ZWRITE_ERROR);/* THE ELEMENT ALREADY EXIST */
    return;
  }//if
  accIsLockedLab(signal, lockOwnerPtr);
}//Dbacc::insertExistElemLab()

/* --------------------------------------------------------------------------------- */
/* INSERTELEMENT                                                                     */
/* --------------------------------------------------------------------------------- */
void Dbacc::insertelementLab(Signal* signal,
                             Page8Ptr bucketPageptr,
                             Uint32 bucketConidx)
{
  if (unlikely(fragrecptr.p->dirRangeFull))
  {
    jam();
    acckeyref1Lab(signal, ZDIR_RANGE_FULL_ERROR);
    return;
  }
  if (unlikely(fragrecptr.p->sparsepages.isEmpty()))
  {
    jam();
    Uint32 result = allocOverflowPage();
    if (result > ZLIMIT_OF_ERROR) {
      jam();
      acckeyref1Lab(signal, result);
      return;
    }//if
  }//if
  ndbassert(operationRecPtr.p->tupkeylen <= fragrecptr.p->keyLength);
  ndbassert(!(operationRecPtr.p->m_op_bits & Operationrec::OP_LOCK_REQ));

  /**
   * We acquire the mutex before starting to insert the new element.
   * After releasing the mutex query threads can see the element and if
   * they require a lock they will be put into the lock queue and if
   * they are READ COMMITTED readers they will see an invalid local key
   * and thus decide the row still doesn't exist.
   */
  acquire_frag_mutex_hash(fragrecptr.p, operationRecPtr);
  fragrecptr.p->lockCount++;
  operationRecPtr.p->m_op_bits |= Operationrec::OP_LOCK_OWNER;

  operationRecPtr.p->reducedHashValue =
    fragrecptr.p->level.reduce(operationRecPtr.p->hashValue);
  const Uint32 tidrElemhead = ElementHeader::setLocked(operationRecPtr.i);
  Page8Ptr idrPageptr;
  idrPageptr = bucketPageptr;
  Uint32 tidrPageindex = bucketConidx;
  bool isforward = true;
  ndbassert(fragrecptr.p->localkeylen == 1);
  /* ----------------------------------------------------------------------- */
  /* WE SET THE LOCAL KEY TO MINUS ONE TO INDICATE IT IS NOT YET VALID.      */
  /* ----------------------------------------------------------------------- */
  Local_key localKey;
  localKey.setInvalid();
  operationRecPtr.p->localdata = localKey;
  Uint32 conptr;
  insertElement(Element(tidrElemhead, localKey.m_page_no),
                operationRecPtr,
                idrPageptr,
                tidrPageindex,
                isforward,
                conptr,
                Operationrec::ANY_SCANBITS,
                false);
  release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
  fragrecptr.p->m_lockStats.req_start_imm_ok(true /* Exclusive */,
                                             operationRecPtr.p->m_lockTime,
                                             getHighResTimer());
  c_tup->prepareTUPKEYREQ(localKey.m_page_no,
                          localKey.m_page_idx,
                          fragrecptr.p->tupFragptr);
  sendAcckeyconf(signal);

  fragrecptr.p->slack -= fragrecptr.p->elementLength;
  // EXPAND the structures if required:
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(3004) &&
      fragrecptr.p->fragmentid == 0 &&
      fragrecptr.p->level.getSize() != ERROR_INSERT_EXTRA)
  {
    if (!fragrecptr.p->expandOrShrinkQueued)
    {
      jam();
      signal->theData[0] = fragrecptr.i;
      fragrecptr.p->expandOrShrinkQueued = true;
      sendSignal(reference(), GSN_EXPANDCHECK2, signal, 1, JBB);
    }//if
  }
#endif
  if (fragrecptr.p->slack < 0 && !fragrecptr.p->level.isFull())
  {
    if (!fragrecptr.p->expandOrShrinkQueued)
    {
      jam();
      signal->theData[0] = fragrecptr.i;
      fragrecptr.p->expandOrShrinkQueued = true;
      sendSignal(reference(), GSN_EXPANDCHECK2, signal, 1, JBB);
    }//if
  }//if
  return;
}//Dbacc::insertelementLab()


/* ------------------------------------------------------------------------ */
/* GET_NO_PARALLEL_TRANSACTION                                              */
/* ------------------------------------------------------------------------ */
Uint32
Dbacc::getNoParallelTransaction(const Operationrec * op) const
{
  OperationrecPtr tmp;
  
  tmp.i= op->nextParallelQue;
  Uint32 transId[2] = { op->transId1, op->transId2 };
  while (tmp.i != RNIL) 
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(tmp));
    if (tmp.p->transId1 == transId[0] && tmp.p->transId2 == transId[1])
      tmp.i = tmp.p->nextParallelQue;
    else
      return 2;
  }
  return 1;
}//Dbacc::getNoParallelTransaction()

#ifdef VM_TRACE
Uint32
Dbacc::getNoParallelTransactionFull(Operationrec * op) const
{
  OperationrecPtr tmp;
  
  tmp.p = op;
  while ((tmp.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0)
  {
    tmp.i = tmp.p->prevParallelQue;
    if (tmp.i != RNIL)
    {
      ndbrequire(oprec_pool.getValidPtr(tmp));
    }
    else
    {
      break;
    }
  }    
  
  return getNoParallelTransaction(tmp.p);
}
#endif

#ifdef ACC_SAFE_QUEUE

/**
 * Beware that ACC_SAFE_QUEUE has the potential for an exponential
 * overhead with number of shared-locks held for the *same row*
 * when scanning the ParallelQue. This typically happens in a
 * join query, where the same row is joined by a unique key
 * multiple times.
 *
 * 'maxValidateCount' limits the validate of the ParallelQue
 * in order to avoid such exponential overhead.
 */
static constexpr int maxValidateCount = 42; //std::numeric_limits<int>::max();

bool
Dbacc::validate_parallel_queue(OperationrecPtr opPtr, Uint32 ownerPtrI) const
{
  int cnt = 0;
  while ((opPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0 &&
	 opPtr.p->prevParallelQue != RNIL)
  {
    if (cnt++ >= maxValidateCount) {
      // Upper limit reached, handle as a pass
      return true;
    }
    opPtr.i = opPtr.p->prevParallelQue;
    ndbrequire(oprec_pool.getValidPtr(opPtr));
  }    
  
  return (opPtr.i == ownerPtrI);
}

bool
Dbacc::validate_lock_queue(OperationrecPtr opPtr) const
{
  if (m_is_query_block)
  {
    return true;
  }

  // Common case: opPtr is lockOwner or last in ParallelQue.
  // In such cases we can find the lock owner. Used for later
  // validate, or to limit linear search of parallelQue.
  Uint32 ownerPtrI;
  if (opPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) {
    ownerPtrI = opPtr.i;
  } else if (opPtr.p->nextParallelQue == RNIL &&
	     opPtr.p->m_op_bits & Operationrec::OP_RUN_QUEUE) {
    ownerPtrI = opPtr.p->m_lock_owner_ptr_i;
  } else {
    ownerPtrI = RNIL;
  }

  // Find lock owner by traversing parallel and serial lists
  OperationrecPtr loPtr = opPtr;
  {
    int cnt = 0;
    while ((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0 &&
           loPtr.p->prevParallelQue != RNIL)
    {
      vlqrequire(loPtr.p->m_op_bits & Operationrec::OP_RUN_QUEUE);
      if (cnt++ >= maxValidateCount && ownerPtrI != RNIL) {
        // Upper limit reached, skip to end
        loPtr.i = ownerPtrI;
      } else {
        loPtr.i = loPtr.p->prevParallelQue;
      }
      ndbrequire(oprec_pool.getValidPtr(loPtr));
    }
  }

  while((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0 &&
	loPtr.p->prevSerialQue != RNIL)
  {
    vlqrequire((loPtr.p->m_op_bits & Operationrec::OP_RUN_QUEUE) == 0);
    loPtr.i = loPtr.p->prevSerialQue;
    ndbrequire(oprec_pool.getValidPtr(loPtr));
  }
  
  // Now we have lock owner...
  vlqrequire(loPtr.i == ownerPtrI || ownerPtrI == RNIL);
  vlqrequire(loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
  vlqrequire(loPtr.p->m_op_bits & Operationrec::OP_RUN_QUEUE);

  // 1 Validate page pointer
  {
    Page8Ptr pagePtr;
    pagePtr.i = loPtr.p->elementPage;
    c_page8_pool.getPtr(pagePtr);
    arrGuard(loPtr.p->elementPointer, 2048);
    Uint32 eh = pagePtr.p->word32[loPtr.p->elementPointer];
    vlqrequire(ElementHeader::getLocked(eh));
    vlqrequire(ElementHeader::getOpPtrI(eh) == loPtr.i);
  }

  // 2 Lock owner should always have same LOCK_MODE and ACC_LOCK_MODE
  if (loPtr.p->m_op_bits & Operationrec::OP_LOCK_MODE)
  {
    vlqrequire(loPtr.p->m_op_bits & Operationrec::OP_ACC_LOCK_MODE);
  }
  else
  {
    vlqrequire((loPtr.p->m_op_bits & Operationrec::OP_ACC_LOCK_MODE) == 0);
  }
  
  // 3 Lock owner should never be waiting...
  bool running = false;
  {
    Uint32 opstate = loPtr.p->m_op_bits & Operationrec::OP_STATE_MASK;
    if (opstate == Operationrec::OP_STATE_RUNNING)
      running = true;
    else
    {
      vlqrequire(opstate == Operationrec::OP_STATE_EXECUTED);
    }
  }
  
  // Validate parallel queue
  {
    bool many = false;
    bool orlockmode = loPtr.p->m_op_bits & Operationrec::OP_LOCK_MODE;
    bool aborting = false;
    OperationrecPtr lastP = loPtr;
    
    int cnt = 0;
    while (lastP.p->nextParallelQue != RNIL)
    {
      if (cnt++ >= maxValidateCount) {
        // Upper limit reached, skip to end
        lastP.i = loPtr.p->m_lo_last_parallel_op_ptr_i;
        ndbrequire(oprec_pool.getValidPtr(lastP));
        vlqrequire(lastP.p->nextParallelQue == RNIL);
        // Note that 'orlockmode', 'aborting' and 'many' are cumulative.
        // Thus it does not make sense to check lastP after skip.
        // (SerialQue will still be validated)
        break;
      } else {
        Uint32 prev = lastP.i;
        lastP.i = lastP.p->nextParallelQue;
        ndbrequire(oprec_pool.getValidPtr(lastP));
        vlqrequire(lastP.p->prevParallelQue == prev);
      }
      const Uint32 opbits = lastP.p->m_op_bits;
      many |= loPtr.p->is_same_trans(lastP.p) ? 0 : 1;
      orlockmode |= ((opbits & Operationrec::OP_LOCK_MODE) != 0);
      aborting |= ((opbits & Operationrec::OP_PENDING_ABORT) != 0);
      
      vlqrequire(opbits & Operationrec::OP_RUN_QUEUE);
      vlqrequire((opbits & Operationrec::OP_LOCK_OWNER) == 0);
      
      Uint32 opstate = opbits & Operationrec::OP_STATE_MASK;
      if (running) 
      {
	// If I found a running operation, 
	// all following should be waiting
	vlqrequire(opstate == Operationrec::OP_STATE_WAITING);
      }
      else
      {
	if (opstate == Operationrec::OP_STATE_RUNNING)
	  running = true;
	else
	  vlqrequire(opstate == Operationrec::OP_STATE_EXECUTED);
      }
      
      if (opbits & Operationrec::OP_LOCK_MODE)
      {
        vlqrequire(opbits & Operationrec::OP_ACC_LOCK_MODE);
      }
      else
      {
        vlqrequire((opbits & Operationrec::OP_MASK) == ZREAD ||
                   (opbits & Operationrec::OP_MASK) == ZSCAN_OP);

        // OP_ACC_LOCK_MODE has to reflect if any prior OperationrecPtr
        // in the parallel queue hold an exclusive lock (OP_LOCK_MODE)
        if (orlockmode)
        {
          vlqrequire((opbits & Operationrec::OP_ACC_LOCK_MODE) != 0);
        }
        else
        {
          vlqrequire((opbits & Operationrec::OP_ACC_LOCK_MODE) == 0);
        }
      }
      
      if (many)
      {
	vlqrequire(orlockmode == 0);
      }

      if (aborting)
      {
        vlqrequire(many == 0);
        /**
         * We might get here with an LQHKEYREQ after ABORT has started if we
         * are running with 3 replicas and the node information is updated
         * while the transaction is running. Thus it is not certain that
         * the new operation is in PENDING ABORT state.
         */
        //vlqrequire(lastP.p->m_op_bits & Operationrec::OP_PENDING_ABORT);
      }
    }
    
    if (lastP.i != loPtr.i)
    {
      vlqrequire(loPtr.p->m_lo_last_parallel_op_ptr_i == lastP.i);
      vlqrequire(lastP.p->m_lock_owner_ptr_i == loPtr.i);
    }
    else
    {
      vlqrequire(loPtr.p->m_lo_last_parallel_op_ptr_i == RNIL);
    }
  }
  
  // Validate serial queue  
  if (loPtr.p->nextSerialQue != RNIL)
  {
    Uint32 prev = loPtr.i;
    OperationrecPtr lastS;
    lastS.i = loPtr.p->nextSerialQue;
    while (true)
    {
      ndbrequire(oprec_pool.getValidPtr(lastS));
      vlqrequire(lastS.p->prevSerialQue == prev);
      vlqrequire(getNoParallelTransaction(lastS.p) == 1);
      vlqrequire((lastS.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0);
      vlqrequire((lastS.p->m_op_bits & Operationrec::OP_RUN_QUEUE) == 0);
      vlqrequire((lastS.p->m_op_bits & Operationrec::OP_STATE_MASK) == Operationrec::OP_STATE_WAITING);
      if (lastS.p->nextSerialQue == RNIL)
	break;
      prev = lastS.i;
      lastS.i = lastS.p->nextSerialQue;
    }
    
    vlqrequire(loPtr.p->m_lo_last_serial_op_ptr_i == lastS.i);
  }
  else
  {
    vlqrequire(loPtr.p->m_lo_last_serial_op_ptr_i == RNIL);
  }
  return true;
}

#endif // ACC_SAFE_QUEUE

NdbOut&
operator<<(NdbOut & out, Dbacc::OperationrecPtr ptr)
{
  Uint32 opbits = ptr.p->m_op_bits;
  out << "[ " << dec << ptr.i 
      << " [ " << hex << ptr.p->transId1 
      << " " << hex << ptr.p->transId2 << "] "
      << " bits: H'" << hex << opbits << " ";
  
  bool read = false;
  switch(opbits & Dbacc::Operationrec::OP_MASK){
  case ZREAD: out << "READ "; read = true; break;
  case ZINSERT: out << "INSERT "; break;
  case ZUPDATE: out << "UPDATE "; break;
  case ZDELETE: out << "DELETE "; break;
  case ZWRITE: out << "WRITE "; break;
  case ZSCAN_OP: out << "SCAN "; read = true; break;
  default:
    out << "<Unknown: H'" 
	<< hex << (opbits & Dbacc::Operationrec::OP_MASK)
	<< "> ";
  }
  
  if (read)
  {
    if (opbits & Dbacc::Operationrec::OP_LOCK_MODE)
      out << "(X)";
    else
      out << "(S)";
    if (opbits & Dbacc::Operationrec::OP_ACC_LOCK_MODE)
      out << "(X)";
    else
      out << "(S)";
  }

  if (opbits)
  {
    out << "(RQ)";
  }
  
  switch(opbits & Dbacc::Operationrec::OP_STATE_MASK){
  case Dbacc::Operationrec::OP_STATE_WAITING:
    out << " WAITING "; break;
  case Dbacc::Operationrec::OP_STATE_RUNNING:
    out << " RUNNING "; break;
  case Dbacc::Operationrec::OP_STATE_EXECUTED:
    out << " EXECUTED "; break;
  case Dbacc::Operationrec::OP_STATE_IDLE:
    out << " IDLE "; break;
  default:
    out << " <Unknown: H'" 
	<< hex << (opbits & Dbacc::Operationrec::OP_STATE_MASK)
	<< "> ";
  }
  
/*
    OP_MASK                 = 0x0000F // 4 bits for operation type
    ,OP_LOCK_MODE           = 0x00010 // 0 - shared lock, 1 = exclusive lock
    ,OP_ACC_LOCK_MODE       = 0x00020 // Or:de lock mode of all operation
                                      // before me
    ,OP_LOCK_OWNER          = 0x00040
    ,OP_RUN_QUEUE           = 0x00080 // In parallel queue of lock owner
    ,OP_DIRTY_READ          = 0x00100
    ,OP_LOCK_REQ            = 0x00200 // isAccLockReq
    ,OP_COMMIT_DELETE_CHECK = 0x00400
    ,OP_INSERT_IS_DONE      = 0x00800
    ,OP_ELEMENT_DISAPPEARED = 0x01000
    ,OP_PENDING_ABORT       = 0x02000

    
    ,OP_STATE_MASK          = 0xF0000
    ,OP_STATE_IDLE          = 0xF0000
    ,OP_STATE_WAITING       = 0x00000
    ,OP_STATE_RUNNING       = 0x10000
    ,OP_STATE_EXECUTED      = 0x30000
    
    ,OP_EXECUTED_DIRTY_READ = 0x3050F
    ,OP_INITIAL             = ~(Uint32)0
*/
  if (opbits & Dbacc::Operationrec::OP_LOCK_OWNER)
    out << "LO ";
  
  if (opbits & Dbacc::Operationrec::OP_DIRTY_READ)
    out << "DR ";
  
  if (opbits & Dbacc::Operationrec::OP_LOCK_REQ)
    out << "LOCK_REQ ";
  
  if (opbits & Dbacc::Operationrec::OP_COMMIT_DELETE_CHECK)
    out << "COMMIT_DELETE_CHECK ";

  if (opbits & Dbacc::Operationrec::OP_INSERT_IS_DONE)
    out << "INSERT_IS_DONE ";
  
  if (opbits & Dbacc::Operationrec::OP_ELEMENT_DISAPPEARED)
    out << "ELEMENT_DISAPPEARED ";

  if (opbits & Dbacc::Operationrec::OP_PENDING_ABORT)
    out << "PENDING_ABORT ";
  
  if (opbits & Dbacc::Operationrec::OP_LOCK_OWNER)
  {
    out << "last_parallel: " << dec << ptr.p->m_lo_last_parallel_op_ptr_i << " ";
    out << "last_serial: " << dec << ptr.p->m_lo_last_serial_op_ptr_i << " ";
  }
  
  out << "]";
  return out;
}

#ifdef ACC_SAFE_QUEUE

void
Dbacc::dump_lock_queue(OperationrecPtr loPtr)const
{
  if (m_is_query_block)
  {
    return;
  }
  if ((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0)
  {
    while ((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0 &&
	   loPtr.p->prevParallelQue != RNIL)
    {
      loPtr.i = loPtr.p->prevParallelQue;
      ndbrequire(oprec_pool.getValidPtr(loPtr));
    }
    
    while ((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0 &&
	   loPtr.p->prevSerialQue != RNIL)
    {
      loPtr.i = loPtr.p->prevSerialQue;
      ndbrequire(oprec_pool.getValidPtr(loPtr));
    }

    ndbassert(loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
  }
  
  ndbout << "-- HEAD --" << endl;
  OperationrecPtr tmp = loPtr;
  while (tmp.i != RNIL)
  {
    ndbrequire(oprec_pool.getValidPtr(tmp));
    ndbout << tmp << " ";
    tmp.i = tmp.p->nextParallelQue;
    
    if (tmp.i == loPtr.i)
    {
      ndbout << " <LOOP>";
      break;
    }
  }
  ndbout << endl;
  
  tmp.i = loPtr.p->nextSerialQue;
  while (tmp.i != RNIL)
  {
    ndbrequire(oprec_pool.getValidPtr(tmp));
    OperationrecPtr tmp2 = tmp;
    
    if (tmp.i == loPtr.i)
    {
      ndbout << "<LOOP S>" << endl;
      break;
    }

    while (tmp2.i != RNIL)
    {
      ndbrequire(oprec_pool.getValidPtr(tmp2));
      ndbout << tmp2 << " ";
      tmp2.i = tmp2.p->nextParallelQue;

      if (tmp2.i == tmp.i)
      {
	ndbout << "<LOOP 3>";
	break;
      }
    }
    ndbout << endl;
    tmp.i = tmp.p->nextSerialQue;
  }
}
#endif

/* -------------------------------------------------------------------------
 * PLACE_WRITE_IN_LOCK_QUEUE
 *	INPUT:		OPERATION_REC_PTR OUR OPERATION POINTER
 *			QUE_OPER_PTR	  LOCK QUEUE OWNER OPERATION POINTER
 *			PWI_PAGEPTR       PAGE POINTER OF ELEMENT
 *			TPWI_ELEMENTPTR   ELEMENT POINTER OF ELEMENT
 *	OUTPUT		TRESULT =
 *			ZPARALLEL_QUEUE	  OPERATION PLACED IN PARALLEL QUEUE
 *					  OPERATION CAN PROCEED NOW.
 *			ZSERIAL_QUEUE	  OPERATION PLACED IN SERIAL QUEUE
 *			ERROR CODE	  OPERATION NEEDS ABORTING
 * ------------------------------------------------------------------------- */
Uint32 
Dbacc::placeWriteInLockQueue(OperationrecPtr lockOwnerPtr) const
{
  OperationrecPtr lastOpPtr;
  lastOpPtr.i = lockOwnerPtr.p->m_lo_last_parallel_op_ptr_i;
  Uint32 opbits = operationRecPtr.p->m_op_bits;
  
  if (lastOpPtr.i == RNIL)
  {
    lastOpPtr = lockOwnerPtr;
  }
  else
  {
    ndbrequire(oprec_pool.getValidPtr(lastOpPtr));
  }
  
  ndbassert(validate_parallel_queue(lastOpPtr, lockOwnerPtr.i));

  Uint32 lastbits = lastOpPtr.p->m_op_bits;
  if (lastbits & Operationrec::OP_ACC_LOCK_MODE)
  {
    if(operationRecPtr.p->is_same_trans(lastOpPtr.p))
    {
      goto checkop;
    }
  }
  else
  {
    /**
     * We dont have an exclusive lock on operation and
     *   
     */
    jam();
    
    /**
     * Scan parallel queue to see if we are the only one
     */
    OperationrecPtr loopPtr = lockOwnerPtr;
    do
    {
      ndbrequire(oprec_pool.getValidPtr(loopPtr));
      if (!loopPtr.p->is_same_trans(operationRecPtr.p))
      {
	goto serial;
      }
      loopPtr.i = loopPtr.p->nextParallelQue;
    } while (loopPtr.i != RNIL);
    
    goto checkop;
  }
  
serial:
  jam();
  if (operationRecPtr.p->m_op_bits & Operationrec::OP_NOWAIT)
  {
    jam();
    return ZNOWAIT_ERROR;
  }
  placeSerialQueue(lockOwnerPtr, operationRecPtr);

  validate_lock_queue(lockOwnerPtr);
  
  return ZSERIAL_QUEUE;
  
checkop:
  /* 
     WE ARE PERFORMING AN READ EXCLUSIVE, INSERT, UPDATE OR DELETE IN THE SAME
     TRANSACTION WHERE WE PREVIOUSLY HAVE EXECUTED AN OPERATION.
     Read-All, Update-All, Insert-All and Delete-Insert are allowed
     combinations.
     Delete-Read, Delete-Update and Delete-Delete are not an allowed
     combination and will result in tuple not found error.
  */
  const Uint32 lstate = lastbits & Operationrec::OP_STATE_MASK;

  Uint32 retValue = ZSERIAL_QUEUE; // So that it gets blocked...
  if (lstate == Operationrec::OP_STATE_EXECUTED)
  {
    jam();

    /**
     * Since last operation has executed...we can now check operation types
     *   if not, we have to wait until it has executed 
     */
    const Uint32 op = opbits & Operationrec::OP_MASK;
    const Uint32 lop = lastbits & Operationrec::OP_MASK;
    if (op == ZINSERT && lop != ZDELETE)
    {
      jam();
      return ZWRITE_ERROR;
    }//if

    /**
     * NOTE. No checking op operation types, as one can read different save
     *       points...
     */

    if(op == ZWRITE)
    {
      opbits &= ~(Uint32)Operationrec::OP_MASK;
      opbits |= (lop == ZDELETE) ? ZINSERT : ZUPDATE;
    }
    
    opbits |= Operationrec::OP_STATE_RUNNING;
    operationRecPtr.p->localdata = lastOpPtr.p->localdata;
    retValue = ZPARALLEL_QUEUE;
  }
  
  opbits |= Operationrec::OP_RUN_QUEUE;
  operationRecPtr.p->m_op_bits = opbits;
  operationRecPtr.p->prevParallelQue = lastOpPtr.i;
  operationRecPtr.p->m_lock_owner_ptr_i = lockOwnerPtr.i;
  lastOpPtr.p->nextParallelQue = operationRecPtr.i;
  lockOwnerPtr.p->m_lo_last_parallel_op_ptr_i = operationRecPtr.i;
  
  validate_lock_queue(lockOwnerPtr);
  
  return retValue;
}//Dbacc::placeWriteInLockQueue()

Uint32 
Dbacc::placeReadInLockQueue(OperationrecPtr lockOwnerPtr) const
{
  OperationrecPtr lastOpPtr;
  OperationrecPtr loopPtr = lockOwnerPtr;
  lastOpPtr.i = lockOwnerPtr.p->m_lo_last_parallel_op_ptr_i;
  Uint32 opbits = operationRecPtr.p->m_op_bits;

  if (lastOpPtr.i == RNIL)
  {
    lastOpPtr = lockOwnerPtr;
  }
  else
  {
    ndbrequire(oprec_pool.getValidPtr(lastOpPtr));
  }

  ndbassert(validate_parallel_queue(lastOpPtr, lockOwnerPtr.i));
  
  /**
   * Last operation in parallel queue of lock owner is same trans
   *   and ACC_LOCK_MODE is exclusive, then we can proceed
   */
  Uint32 lastbits = lastOpPtr.p->m_op_bits;
  bool same = operationRecPtr.p->is_same_trans(lastOpPtr.p);
  if (same && (lastbits & Operationrec::OP_ACC_LOCK_MODE))
  {
    jam();
    opbits |= Operationrec::OP_LOCK_MODE; // Upgrade to X-lock
    goto checkop;
  }
  
  if ((lastbits & Operationrec::OP_ACC_LOCK_MODE) && !same)
  {
    jam();
    /**
     * Last op in serial queue had X-lock and was not our transaction...
     */
    goto serial;
  }

  if (lockOwnerPtr.p->nextSerialQue == RNIL)
  {
    jam();
    goto checkop;
  }

  /**
   * Scan parallel queue to see if we are already there...
   */
  do
  {
    ndbrequire(oprec_pool.getValidPtr(loopPtr));
    if (loopPtr.p->is_same_trans(operationRecPtr.p))
      goto checkop;
    loopPtr.i = loopPtr.p->nextParallelQue;
  } while (loopPtr.i != RNIL);

serial:
  if (operationRecPtr.p->m_op_bits & Operationrec::OP_NOWAIT)
  {
    jam();
    return ZNOWAIT_ERROR;
  }
  placeSerialQueue(lockOwnerPtr, operationRecPtr);
  
  validate_lock_queue(lockOwnerPtr);
  
  return ZSERIAL_QUEUE;

checkop:
  Uint32 lstate = lastbits & Operationrec::OP_STATE_MASK;
  
  Uint32 retValue = ZSERIAL_QUEUE; // So that it gets blocked...
  if (lstate == Operationrec::OP_STATE_EXECUTED)
  {
    jam();
    
    /**
     * NOTE. No checking op operation types, as one can read different save
     *       points...
     */
    
#if 0
    /**
     * Since last operation has executed...we can now check operation types
     *   if not, we have to wait until it has executed 
     */
    if (lop == ZDELETE)
    {
      jam();
      return ZREAD_ERROR;
    }
#endif
    
    opbits |= Operationrec::OP_STATE_RUNNING;
    operationRecPtr.p->localdata = lastOpPtr.p->localdata;
    retValue = ZPARALLEL_QUEUE;
  }
  opbits |= (lastbits & Operationrec::OP_ACC_LOCK_MODE);
  opbits |= Operationrec::OP_RUN_QUEUE;
  operationRecPtr.p->m_op_bits = opbits;
  
  operationRecPtr.p->prevParallelQue = lastOpPtr.i;
  operationRecPtr.p->m_lock_owner_ptr_i = lockOwnerPtr.i;
  lastOpPtr.p->nextParallelQue = operationRecPtr.i;
  lockOwnerPtr.p->m_lo_last_parallel_op_ptr_i = operationRecPtr.i;
  
  validate_lock_queue(lockOwnerPtr);
  
  return retValue;
}//Dbacc::placeReadInLockQueue

void Dbacc::placeSerialQueue(OperationrecPtr lockOwnerPtr,
			     OperationrecPtr opPtr)const
{
  OperationrecPtr lastOpPtr;
  lastOpPtr.i = lockOwnerPtr.p->m_lo_last_serial_op_ptr_i;

  if (lastOpPtr.i == RNIL)
  {
    // Lock owner is last...
    ndbrequire(lockOwnerPtr.p->nextSerialQue == RNIL);
    lastOpPtr = lockOwnerPtr;
  }
  else
  {
    ndbrequire(oprec_pool.getValidPtr(lastOpPtr));
  }
  
  operationRecPtr.p->prevSerialQue = lastOpPtr.i;
  lastOpPtr.p->nextSerialQue = opPtr.i;
  lockOwnerPtr.p->m_lo_last_serial_op_ptr_i = opPtr.i;
}

/* ------------------------------------------------------------------------- */
/* ACC KEYREQ END                                                            */
/* ------------------------------------------------------------------------- */
void Dbacc::acckeyref1Lab(Signal* signal, Uint32 result_code) const
{
  operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
  /* ************************<< */
  /* ACCKEYREF                  */
  /* ************************<< */
  signal->theData[0] = Uint32(-1);
  signal->theData[1] = result_code;
  return;
}//Dbacc::acckeyref1Lab()

/* ******************----------------------------------------------------- */
/* ACCMINUPDATE                                      UPDATE LOCAL KEY REQ  */
/*  DESCRIPTION: UPDATES LOCAL KEY OF AN ELEMENTS IN THE HASH TABLE        */
/*               THIS SIGNAL IS WAITED AFTER ANY INSERT REQ                */
/*          ENTER ACCMINUPDATE WITH         SENDER: LQH,    LEVEL B        */
/*                    OPERATION_REC_PTR,    OPERATION RECORD PTR           */
/*                    CLOCALKEY(0),         LOCAL KEY 1                    */
/*                    CLOCALKEY(1)          LOCAL KEY 2                    */
/* ******************----------------------------------------------------- */
void Dbacc::execACCMINUPDATE(Signal* signal,
                             Uint32 opPtrI,
                             Dbacc::Operationrec *opPtrP,
                             Uint32 page_no,
                             Uint32 page_idx)
{
  Page8Ptr ulkPageidptr;
  Uint32 tulkLocalPtr;
  Local_key localkey;

  operationRecPtr.i = opPtrI;
  operationRecPtr.p = opPtrP;
  jamEntry();
  localkey.m_page_no = page_no;
  localkey.m_page_idx = page_idx;
  Uint32 opbits = operationRecPtr.p->m_op_bits;
  fragrecptr.i = operationRecPtr.p->fragptr;
  ulkPageidptr.i = operationRecPtr.p->elementPage;
  tulkLocalPtr = operationRecPtr.p->elementPointer + 1;
  ndbrequire(Magic::check_ptr(operationRecPtr.p));

  if ((opbits & Operationrec::OP_STATE_MASK) == Operationrec::OP_STATE_RUNNING)
  {
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    c_page8_pool.getPtr(ulkPageidptr);
    arrGuard(tulkLocalPtr, 2048);
    /**
     * We lock the fragment to ensure that now readers can see the new
     * row version since it is both inserted into the hash index AND
     * the row has been updated, thus readers from the same transaction
     * can now see the row. Need to ensure this happens in an ordered
     * way through mutex locks.
     */
    acquire_frag_mutex_hash(fragrecptr.p, operationRecPtr);
    operationRecPtr.p->localdata = localkey;
    ndbrequire(fragrecptr.p->localkeylen == 1);
    ulkPageidptr.p->word32[tulkLocalPtr] = localkey.m_page_no;
    release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
    return;
  }//if
  ndbabort();
}//Dbacc::execACCMINUPDATE()

void
Dbacc::removerow(Uint32 opPtrI, const Local_key* key)
{
  jamEntry();
  operationRecPtr.i = opPtrI;
  ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
  Uint32 opbits = operationRecPtr.p->m_op_bits;
  fragrecptr.i = operationRecPtr.p->fragptr;

  /* Mark element disappeared */
  opbits |= Operationrec::OP_ELEMENT_DISAPPEARED;
  opbits &= ~Uint32(Operationrec::OP_COMMIT_DELETE_CHECK);

  /**
   * This function is (currently?) only used when refreshTuple()
   *   inserts a record...and later wants to remove it
   *
   * Since this should not affect row-count...we change the optype to UPDATE
   *   execACC_COMMITREQ will be called in same timeslice as this change...
   */
  opbits &= ~Uint32(Operationrec::OP_MASK);
  opbits |= ZUPDATE;

  operationRecPtr.p->m_op_bits = opbits;

#if defined(VM_TRACE) || defined(ERROR_INSERT)
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  ndbrequire(operationRecPtr.p->localdata.m_page_no == key->m_page_no);
  ndbrequire(operationRecPtr.p->localdata.m_page_idx == key->m_page_idx);
#endif
}//Dbacc::execACCMINUPDATE()

/* ******************--------------------------------------------------------------- */
/* ACC_COMMITREQ                                        COMMIT  TRANSACTION          */
/*                                                     SENDER: LQH,    LEVEL B       */
/*       INPUT:  OPERATION_REC_PTR ,                                                 */
/* ******************--------------------------------------------------------------- */
void Dbacc::execACC_COMMITREQ(Signal* signal,
                              Uint32 opPtrI,
                              Dbacc::Operationrec *opPtrP)
{
  Uint8 Toperation;
  jamEntry();
  operationRecPtr.i = opPtrI;
  operationRecPtr.p = opPtrP;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  Uint32 tmp = operationRecPtr.i;
  void* ptr = operationRecPtr.p;
#endif
  Uint32 opbits = operationRecPtr.p->m_op_bits;
  fragrecptr.i = operationRecPtr.p->fragptr;
  Toperation = opbits & Operationrec::OP_MASK;
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  ndbrequire(Magic::check_ptr(operationRecPtr.p));
  commitOperation(signal);
  ndbassert(operationRecPtr.i == tmp);
  ndbassert(operationRecPtr.p == ptr);
  operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
  if((Toperation != ZREAD) &&
     (Toperation != ZSCAN_OP))
  {
    fragrecptr.p->m_commit_count++;
  }
  return;
}//Dbacc::execACC_COMMITREQ()

/* ******************------------------------------------------------------- */
/* ACC ABORT REQ                   ABORT ALL OPERATION OF THE TRANSACTION    */
/* ******************------------------------------+                         */
/*   SENDER: LQH,    LEVEL B                                                 */
/* ******************------------------------------------------------------- */
/* ACC ABORT REQ                                         ABORT TRANSACTION   */
/* ******************------------------------------+                         */
/*   SENDER: LQH,    LEVEL B       */
void Dbacc::execACC_ABORTREQ(Signal* signal,
                             Uint32 opPtrI,
                             Dbacc::Operationrec *opPtrP,
                             Uint32 sendConf)
{
  jamEntry();
  operationRecPtr.i = opPtrI;
  operationRecPtr.p = opPtrP;
  fragrecptr.i = operationRecPtr.p->fragptr;
  Uint32 opbits = operationRecPtr.p->m_op_bits;
  Uint32 opstate = opbits & Operationrec::OP_STATE_MASK;
  ndbrequire(Magic::check_ptr(operationRecPtr.p));

  if (opbits == Operationrec::OP_EXECUTED_DIRTY_READ)
  {
    jam();
  }
  else if (opstate == Operationrec::OP_STATE_EXECUTED ||
	   opstate == Operationrec::OP_STATE_WAITING ||
	   opstate == Operationrec::OP_STATE_RUNNING)
  {
    jam();
    ndbassert(!m_is_query_block);
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    abortOperation(signal);
  }
  
  operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;

  signal->theData[0] = operationRecPtr.p->userptr;
  signal->theData[1] = 0;
  switch(sendConf){
  case 0:
    return;
  case 2:
    if (opstate != Operationrec::OP_STATE_RUNNING)
    {
      return;
    }
    [[fallthrough]];
  case 1:
    sendSignal(operationRecPtr.p->userblockref, GSN_ACC_ABORTCONF, 
	       signal, 1, JBB);
  }
  
  signal->theData[1] = RNIL;
}

/*
 * Lock or unlock tuple.
 */
void Dbacc::execACC_LOCKREQ(Signal* signal)
{
  jamEntryDebug();
  AccLockReq* sig = (AccLockReq*)signal->getDataPtrSend();
  AccLockReq reqCopy = *sig;
  AccLockReq* const req = &reqCopy;
  Uint32 lockOp = (req->requestInfo & 0xFF);
  if (lockOp == AccLockReq::LockShared ||
      lockOp == AccLockReq::LockExclusive)
  {
    jam();
    // find table
    tabptr.i = req->tableId;
    ptrCheckGuard(tabptr, ctablesize, tabrec);
    // find fragment (TUX will know it)
    if (req->fragPtrI == RNIL) {
      for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragholder); i++) {
        jam();
        if (tabptr.p->fragholder[i] == req->fragId){
	  jam();
	  req->fragPtrI = tabptr.p->fragptrholder[i];
	  break;
	}
      }
    }
    fragrecptr.i = req->fragPtrI;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    ndbrequire(req->fragId == fragrecptr.p->myfid);
    // caller must be explicit here
    ndbrequire(req->accOpPtr == RNIL);
    // seize operation to hold the lock
    bool succ = true;
    if (unlikely(req->isCopyFragScan))
    {
      jam();
      operationRecPtr.i = c_copy_frag_oprec;
      ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
      ndbrequire(operationRecPtr.p->m_op_bits == Operationrec::OP_INITIAL);
    }
    else
    {
      if (unlikely(!oprec_pool.seize(operationRecPtr)))
      {
        jam();
        succ = false;
      }
    }
    if (likely(succ))
    {
      jamDebug();
      // init as in ACCSEIZEREQ
      operationRecPtr.p->userptr = req->userPtr;
      operationRecPtr.p->userblockref = req->userRef;
      operationRecPtr.p->scanRecPtr = RNIL;
      // do read with lock via ACCKEYREQ
      Uint32 lockMode = (lockOp == AccLockReq::LockShared) ? 0 : 1;
      Uint32 opCode = ZSCAN_OP;
      {
        Uint32 accreq = 0;
        accreq = AccKeyReq::setOperation(accreq, opCode);
        accreq = AccKeyReq::setLockType(accreq, lockMode);
        accreq = AccKeyReq::setDirtyOp(accreq, false);
        accreq = AccKeyReq::setReplicaType(accreq, 0); // ?
        accreq = AccKeyReq::setTakeOver(accreq, false);
        accreq = AccKeyReq::setLockReq(accreq, true);
        AccKeyReq* keyreq = reinterpret_cast<AccKeyReq*>(&signal->theData[0]);
        keyreq->fragmentPtr = fragrecptr.i;
        keyreq->requestInfo = accreq;
        keyreq->hashValue = req->hashValue;
        keyreq->keyLen = 0;   // search local key
        keyreq->transId1 = req->transId1;
        keyreq->transId2 = req->transId2;
        keyreq->lockConnectPtr = RNIL;
        // enter local key in place of PK
        keyreq->localKey[0] = req->page_id;
        keyreq->localKey[1] = req->page_idx;
        static_assert(AccKeyReq::SignalLength_localKey == 10);
      }
      signal->setLength(AccKeyReq::SignalLength_localKey);
      execACCKEYREQ(signal,
                    operationRecPtr.i,
                    operationRecPtr.p);
        /* keyreq invalid, signal now contains return value */
      // translate the result
      if (signal->theData[0] < RNIL)
      {
        jamDebug();
        req->returnCode = AccLockReq::Success;
        req->accOpPtr = operationRecPtr.i;
      }
      else if (signal->theData[0] == RNIL)
      {
        jam();
        req->returnCode = AccLockReq::IsBlocked;
        req->accOpPtr = operationRecPtr.i;
      }
      else
      {
        ndbrequire(signal->theData[0] == (UintR)-1);
        releaseOpRec();
        req->returnCode = AccLockReq::Refused;
        req->accOpPtr = RNIL;
      }
    }
    else
    {
      jam();
      ndbrequire(req->isCopyFragScan == ZFALSE);
      req->returnCode = AccLockReq::NoFreeOp;
    }
    *sig = *req;
    return;
  }
  operationRecPtr.i = req->accOpPtr;
  ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
  if (lockOp == AccLockReq::Unlock)
  {
    jam();
    // do unlock via ACC_COMMITREQ (immediate)
    execACC_COMMITREQ(signal,
                      operationRecPtr.i,
                      operationRecPtr.p);
    releaseOpRec();
    req->returnCode = AccLockReq::Success;
    *sig = *req;
    return;
  }
  if (lockOp == AccLockReq::Abort) {
    jam();
    // do abort via ACC_ABORTREQ (immediate)
    execACC_ABORTREQ(signal,
                     operationRecPtr.i,
                     operationRecPtr.p,
                     0);
    releaseOpRec();
    req->returnCode = AccLockReq::Success;
    *sig = *req;
    return;
  }
  if (lockOp == AccLockReq::AbortWithConf) {
    jam();
    // do abort via ACC_ABORTREQ (with conf signal)
    execACC_ABORTREQ(signal,
                     operationRecPtr.i,
                     operationRecPtr.p,
                     1);
    releaseOpRec();
    req->returnCode = AccLockReq::Success;
    *sig = *req;
    return;
  }
  ndbabort();
}

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF EXECUTE OPERATION MODULE                                             */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */

/**
 * HASH TABLE MODULE
 *
 * Each partition (fragment) consist of a linear hash table in Dbacc.
 * The linear hash table can expand and shrink by one bucket at a time,
 * moving data from only one bucket.
 *
 * The operations supported are:
 *
 * [] insert one new element
 * [] delete one element
 * [] lookup one element
 * [] expand by splitting one bucket creating a new top bucket
 * [] shrink by merge top bucket data into a merge bucket
 * [] scan
 *
 * SCANS INTERACTION WITH EXPAND AND SHRINK
 *
 * Since expanding and shrinking can occur during the scan, and elements
 * move around one need to take extra care so that elements are scanned
 * exactly once.  Elements deleted or inserted during scan should be
 * scanned at most once, there reinserted data always counts as a different
 * element.
 *
 * Scans are done in one or two laps.  The first lap scans buckets from
 * bottom (bucket 0) to top.  During this lap expanding and shrinking may
 * occur.  In the second lap one rescan buckets that got merged after they
 * was scanned in lap one, and now expanding and shrinking are not allowed.
 *
 * Neither is a expand or shrink involving the currently scanned bucket
 * allowed.
 *
 * During lap one the table can be seen consisting of five kinds of buckets:
 *
 * [] unscanned, note that these have no defined scan bits, since the scan
 *    bits are left overs from earlier scans.
 * [] current, exactly one bucket
 * [] scanned, all buckets below current
 * [] expanded, these buckets have not been scanned in lap one, but may
 *    contain scanned elements.  Anyway they always have well defined scan
 *    bits also for unscanned elements.
 * [] merged and scanned, these are buckets scanned in lap one but have
 *    been merged after they got scanned, and may contain unscanned
 *    elements.  These buckets must be rescanned during lap two of scan.
 *    Note that we only keep track of a first and last bucket to rescan
 *    even if there are some buckets in between that have not been merged.
 *
 * The diagram below show the possible regions of buckets.  The names to
 * the right are the data members that describes the limits of the regions.
 *
 *  +--------------------------+
 *  | Expanded buckets.  May   | Fragmentrec::level.getTop()
 *  | contain both scanned and |
 *  | unscanned data.          |
 *  |                          |
 *  +--------------------------+
 *  | Unscanned data with      | ScanRec::startNoOfBuckets
 *  | undefined scan bits.     |
 *  |                          | ScanRec::nextBucketIndex + 1
 *  +--------------------------+
 *  | Currently scanned data.  | ScanRec::nextBucketIndex
 *  +--------------------------+
 *  | Scanned buckets.         |
 *  |                          |
 *  +--------------------------+
 *  | Merged buckets after     | ScanRec::maxBucketIndexToRescan
 *  | scan start - need rescan.|
 *  |                          | ScanRec::minBucketIndexToRescan
 *  +--------------------------+
 *  |                          |
 *  | Scanned buckets.         | 0
 *  +--------------------------+
 *
 * When scan starts, all buckets are unscanned and have undefined scan bits.
 * On start scanning of an unscanned bucket with undefined scan bits all
 * scan bits for the bucket are cleared.  ScanRec::startNoOfBuckets keeps
 * track of the last bucket with undefined scan bits, note that
 * startNoOfBuckets may decrease if table shrinks below it.
 *
 * During the second lap the buckets from minBucketIndexToRescan to
 * maxBucketIndexToRescan inclusive, are scanned, and no bucket need to have
 * its scan bits cleared prior to scan.
 *
 * SCAN AND EXPAND
 *
 * After expand, the new top bucket will always have defined scan bits.
 *
 * If the split bucket have undefined scan bits the buckets scan bits are
 * cleared before split.
 *
 * The expanded bucket may only contain scanned elements if the split
 * bucket was a scanned bucket below the current bucket.  This fact comes
 * from noting that once the split bucket are below current bucket, the
 * following expand can not have a split bucket above current bucket, since
 * next split bucket is either the next bucket, or the bottom bucket due to
 * how the linear hash table grow.  And since expand are not allowed when
 * split bucket would be the current bucket all expand bucket with scanned
 * elements must come from buckets below current bucket.
 *
 * SCAN AND SHRINK
 *
 * Shrink merge back the top bucket into the bucket it was split from in
 * the corresponding expand.  This implies that we will never merge back a
 * bucket with scanned elements into an unscanned bucket, with or without
 * defined scan bits.
 *
 * If the top bucket have undefined scan bits they are cleared before merge,
 * even if it is into another bucket with undefined scan bits.  This is to
 * ensure that an element is not inserted in a bucket that have scan bits
 * set that are not allowed in bucket, for details why see under BUCKET
 * INVARIANTS.
 *
 * Whenever top bucket have undefined scan bits one need to decrease
 * startNoOfBuckets that indicates the last bucket with undefined scan
 * bits.  If the top bucket reappear by expand it will have defined
 * scan bits which possibly indicate scan elements, these must not be
 * cleared prior scan.
 *
 * If merge destination are below current bucket, it must be added for
 * rescan.  Note that we only keep track of lowest and highest bucket
 * number to rescan even if some buckets in between are not merged and do
 * not need rescan.
 *
 * CONTAINERS
 *
 * Each bucket is a linked list of containers.  Only the first head
 * container may be empty.
 *
 * Containers are located in 8KiB pages.  Each page have 72 buffers with
 * 28 words.  Each buffer may host up to two containers.  One headed at
 * buffers lowest address, called left end, and one headed at buffers high
 * words, the right end.  The left end container grows forward towards
 * higher addresses, and the right end container grows backwards.
 *
 * Each bucket has its first container at a unique logical address, the
 * logical page number is bucket number divided by 64 with the remainder
 * index one of the first 64 left end containers on page.  A dynamic array
 * are used to map the logical page number to physical page number.
 *
 * The pages which host the head containers of buckets are called normal
 * pages.  When a container is full a new container is allocated, first it
 * looks for one of the eight left end containers that are on same page.
 * If no one is free, one look for a free right end container on same page.
 * Otherwise one look for an overflow container on an overflow page.  New
 * overflow pages are allocated if needed.
 *
 * SCAN BITS
 *
 * To keep track of which elements have been scanned several means are used.
 * Every container header have scan bits, if a scan bit is set it means that
 * all elements in that container have been scanned by the corresponding
 * scan.
 *
 * If a container is currently scanned, that is some elements are scanned
 * and some not, each element in the container have a scan bit in the scan
 * record (ScanRec::elemScanned).  The next scanned element is looked for
 * in the current container, if none found, the next container is used, and
 * then the next bucket.
 *
 * A scan may only scan one container at a time.
 *
 * BUCKETS INVARIANTS
 *
 * To be able to guarantee that only one container at a time are currently
 * scanned, there is an important invariant:
 *
 * [] No container may have a scan bit set that preceding container have
 *    not set.  That is, container are scanned in order within bucket, and
 *    no inserted element may be put in such that the invariant breaks.
 *
 * Also a condition that all operations on buckets must satisfy is:
 *
 * [] It is not allowed to insert an element with more scan bits set than
 *    the buckets head container have (unless it is for a new top bucket).
 *
 *    This is too avoid extra complexity that would arise if such an
 *    element was inserted.  A new container can not be inserted preceding
 *    the bucket head container since it has an fixed logical address.  The
 *    alternative would be to create a new bucket after the bucket head
 *    container and move every element from head container to the new
 *    container.
 *
 * How the condition is fulfilled are:
 *
 * [] Shrink, where top bucket have undefined scan bits.
 *
 *    Top buckets scan bits are first cleared prior to merge.
 *
 * [] Shrink, where destination bucket have undefined scan bits.
 *
 *    In this case top bucket must also have undefined scan bits (see SCAN
 *    AND SHRINK above) and both top and destination bucket have their scan
 *    bits cleared before merge.
 *
 * [] Shrink, where destination bucket is scanned, below current.
 *
 *    The only way the top bucket can have scanned elements is that it is
 *    expanded from a scanned bucket, below current.  Since that must be the
 *    shrink destination bucket, no element can have more scan bits set than
 *    the destination buckets head container.
 *
 * [] Expand.
 *
 *    The new top bucket is always a new bucket and head containers scan bits
 *    are taken from split source bucket.
 *
 * [] Insert.
 *
 *    A new element may be inserted in any container with free space, and it
 *    inherits the containers scan bits.  If a new container is needed it is
 *    put last with container scan bits copied from preceding container.
 *
 * [] Delete.
 *
 *    Deleting an element, replaces the deleted element with the last
 *    element with same scan bits as the deleted element.  If a container
 *    becomes empty it is unlinked, unless it is the head container which
 *    always must remain.
 *
 *    Since the first containers in a bucket are more likely to be on the
 *    same (normal) page, it is better to unlink a container towards the
 *    end of bucket.  If the deleted element is the last one in its
 *    container, but not the head container, and there are no other element
 *    in bucket with same scan bits that can replace the deleted element.
 *    It is allowed to use another element with fewer bits as replacement
 *    and clear scan bits of the container accordingly.
 *
 *    The reason the bucket head container may not have some of its scan
 *    bits cleared, is that it could later result in a need to insert back
 *    an element with more scan bits set.  The scenario for that is:
 *
 *    1) Split a merged bucket, A, into a new bucket B, moving some
 *       elements with some scan bits set.
 *
 *    2) Delete some elements in bucket A, leaving only elements with no
 *       scan bits set.
 *
 *    3) Shrink table and merge back bucket B into bucket A, if we have
 *       cleared the head container of bucket A, this would result in
 *       inserting elements with more scan bits set then bucket A head
 *       container.
 *
 */

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       MODULE:         INSERT                                                      */
/*               THE FOLLOWING SUBROUTINES ARE ONLY USED BY INSERT_ELEMENT. THIS     */
/*               ROUTINE IS THE SOLE INTERFACE TO INSERT ELEMENTS INTO THE INDEX.    */
/*               CURRENT USERS ARE INSERT REQUESTS, EXPAND CONTAINER AND SHRINK      */
/*               CONTAINER.                                                          */
/*                                                                                   */
/*               THE FOLLOWING SUBROUTINES ARE INCLUDED IN THIS MODULE:              */
/*               INSERT_ELEMENT                                                      */
/*               INSERT_CONTAINER                                                    */
/*               ADDNEWCONTAINER                                                     */
/*               GETFREELIST                                                         */
/*               INCREASELISTCONT                                                    */
/*               SEIZE_LEFTLIST                                                      */
/*               SEIZE_RIGHTLIST                                                     */
/*                                                                                   */
/*               THESE ROUTINES ARE ONLY USED BY THIS MODULE AND BY NO ONE ELSE.     */
/*               ALSO THE ROUTINES MAKE NO USE OF ROUTINES IN OTHER MODULES.         */
/*               TAKE_REC_OUT_OF_FREE_OVERPAGE AND RELEASE_OVERFLOW_REC ARE          */
/*               EXCEPTIONS TO THIS RULE.                                            */
/*                                                                                   */
/*               THE ONLY SHORT-LIVED VARIABLES USED IN OTHER PARTS OF THE BLOCK ARE */
/*               THOSE DEFINED AS INPUT AND OUTPUT IN INSERT_ELEMENT                 */
/*               SHORT-LIVED VARIABLES INCLUDE TEMPORARY VARIABLES, COMMON VARIABLES */
/*               AND POINTER VARIABLES.                                              */
/*               THE ONLY EXCEPTION TO THIS RULE IS FRAGRECPTR WHICH POINTS TO THE   */
/*               FRAGMENT RECORD. THIS IS MORE LESS STATIC ALWAYS DURING A SIGNAL    */
/*               EXECUTION.                                                          */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* INSERT_ELEMENT                                                                    */
/*       INPUT:                                                                      */
/*               IDR_PAGEPTR (POINTER TO THE ACTIVE PAGE REC)                        */
/*               TIDR_PAGEINDEX (INDEX OF THE CONTAINER)                             */
/*               TIDR_FORWARD (DIRECTION FORWARD OR BACKWARD)                        */
/*               TIDR_ELEMHEAD (HEADER OF ELEMENT TO BE INSERTED                     */
/*               CIDR_KEYS(ARRAY OF TUPLE KEYS)                                      */
/*               CLOCALKEY(ARRAY OF LOCAL KEYS).                                     */
/*               FRAGRECPTR                                                          */
/*               IDR_OPERATION_REC_PTR                                               */
/*               TIDR_KEY_LEN                                                        */
/*               conScanMask - ANY_SCANBITS or scan bits container must              */
/*                 have. Note elements inserted are never more scanned than          */
/*                 container.                                                        */
/*                                                                                   */
/*       OUTPUT:                                                                     */
/*               TIDR_PAGEINDEX (PAGE INDEX OF INSERTED ELEMENT)                     */
/*               IDR_PAGEPTR    (PAGE POINTER OF INSERTED ELEMENT)                   */
/*               TIDR_FORWARD   (CONTAINER DIRECTION OF INSERTED ELEMENT)            */
/*               NONE                                                                */
/* --------------------------------------------------------------------------------- */
void Dbacc::insertElement(const Element   elem,
                          OperationrecPtr oprecptr,
                          Page8Ptr&       pageptr,
                          Uint32&         conidx,
                          bool&           isforward,
                          Uint32&         conptr,
                          Uint16          conScanMask,
                          const bool      newBucket)
{
  Page8Ptr inrNewPageptr;
  Uint32 tidrResult;
  Uint16 scanmask;
  bool newContainer = newBucket;

  ContainerHeader containerhead;
  do {
    insertContainer(elem,
                    oprecptr,
                    pageptr,
                    conidx,
                    isforward,
                    conptr,
                    containerhead,
                    conScanMask,
                    newContainer,
                    tidrResult);
    if (tidrResult != ZFALSE)
    {
      jam();
      return;
      /* INSERTION IS DONE, OR */
      /* AN ERROR IS DETECTED  */
    }//if
    if (containerhead.getNextEnd() != 0) {
      /* THE NEXT CONTAINER IS IN THE SAME PAGE */
      conidx = containerhead.getNextIndexNumber();
      if (containerhead.getNextEnd() == ZLEFT) {
        jam();
        isforward = true;
      } else if (containerhead.getNextEnd() == ZRIGHT) {
        jam();
        isforward = false;
      } else {
        ndbabort();
        return;
      }//if
      if (!containerhead.isNextOnSamePage()) {
        jam();     /* NEXT CONTAINER IS IN AN OVERFLOW PAGE */
        pageptr.i = pageptr.p->word32[conptr + 1];
        c_page8_pool.getPtr(pageptr);
      }//if
      ndbrequire(conidx <= Container::MAX_CONTAINER_INDEX);
    } else {
      scanmask = containerhead.getScanBits();
      break;
    }//if
    // Only first container can be a new container
    newContainer = false;
  } while (1);
  Uint32 newPageindex;;
  Uint32 newBuftype;
  getfreelist(pageptr, newPageindex, newBuftype);
  bool nextOnSamePage;
  if (newPageindex == Container::NO_CONTAINER_INDEX) {
    jam();
    /* NO FREE BUFFER IS FOUND */
    if (fragrecptr.p->sparsepages.isEmpty())
    {
      jam();
      Uint32 result = allocOverflowPage();
      ndbrequire(result <= ZLIMIT_OF_ERROR);
    }//if
    {
      LocalContainerPageList sparselist(c_page8_pool, fragrecptr.p->sparsepages);
      sparselist.first(inrNewPageptr);
    }
    getfreelist(inrNewPageptr, newPageindex, newBuftype);
    ndbrequire(newPageindex != Container::NO_CONTAINER_INDEX);
    nextOnSamePage = false;
  } else {
    jam();
    inrNewPageptr = pageptr;
    nextOnSamePage = true;
  }//if
  if (newBuftype == ZLEFT)
  {
    seizeLeftlist(inrNewPageptr, newPageindex);
    isforward = true;
  }
  else if (newBuftype == ZRIGHT)
  {
    seizeRightlist(inrNewPageptr, newPageindex);
    isforward = false;
  }
  else
  {
    ndbrequire(newBuftype == ZLEFT || newBuftype == ZRIGHT);
  }
  Uint32 containerptr = getContainerPtr(newPageindex, isforward);
  ContainerHeader newcontainerhead;
  newcontainerhead.initInUse();
  Uint32 nextPtrI;
  if (containerhead.haveNext())
  {
    nextPtrI = pageptr.p->word32[conptr+1];
    newcontainerhead.setNext(containerhead.getNextEnd(),
                          containerhead.getNextIndexNumber(),
                          inrNewPageptr.i == nextPtrI);
  }
  else
  {
    nextPtrI = RNIL;
    newcontainerhead.clearNext();
  }
  inrNewPageptr.p->word32[containerptr] = newcontainerhead;
  inrNewPageptr.p->word32[containerptr + 1] = nextPtrI;
  addnewcontainer(pageptr, conptr, newPageindex,
    newBuftype, nextOnSamePage, inrNewPageptr.i);
  pageptr = inrNewPageptr;
  conidx = newPageindex;
  if (conScanMask == Operationrec::ANY_SCANBITS)
  {
    /**
     * ANY_SCANBITS indicates that this is an insert of a new element, not
     * an insert from expand or shrink.  In that case the inserted element
     * and the new container will inherit scan bits from previous container.
     * This makes the element look as scanned as possible still preserving
     * the invariant that containers and element towards the end of bucket
     * has less scan bits set than those towards the beginning.
     */
    conScanMask = scanmask;
  }
  insertContainer(elem,
                  oprecptr,
                  pageptr,
                  conidx,
                  isforward,
                  conptr,
                  containerhead,
                  conScanMask,
                  true,
                  tidrResult);
  ndbrequire(tidrResult == ZTRUE);
}//Dbacc::insertElement()

/**
 * insertContainer puts an element into a container if it has free space and
 * the requested scan bits match.
 *
 * If it is a new element inserted the requested scan bits given by
 * conScanMask can be ANY_SCANBITS or a valid set of bits.  If it is
 * ANY_SCANBITS the containers scan bits are not checked.  If it is set to
 * valid scan bits the container is a newly created empty container.
 *
 * The buckets header container may never be removed.  Nor should any scan
 * bit of it be cleared, unless for expand there the first inserted element
 * determines the bucket header containers scan bits.  newContainer indicates
 * that that current insert is part of populating a new bucket with expand.
 *
 * In case the container is empty it is either the bucket header container
 * or a new container created by caller (insertElement).
 */
void Dbacc::insertContainer(const Element          elem,
                            const OperationrecPtr  oprecptr,
                            const Page8Ptr         pageptr,
                            const Uint32           conidx,
                            const bool             isforward,
                            Uint32&                conptr,
                            ContainerHeader&       containerhead,
                            Uint16                 conScanMask,
                            const bool             newContainer,
                            Uint32&                result)
{
  Uint32 tidrContainerlen;
  Uint32 tidrConfreelen;
  Uint32 tidrNextSide;
  Uint32 tidrNextConLen;
  Uint32 tidrIndex;

  result = ZFALSE;
  /* --------------------------------------------------------------------------------- */
  /*       CALCULATE THE POINTER TO THE ELEMENT TO BE INSERTED AND THE POINTER TO THE  */
  /*       CONTAINER HEADER OF THE OTHER SIDE OF THE BUFFER.                           */
  /* --------------------------------------------------------------------------------- */
  conptr = getForwardContainerPtr(conidx);
  if (isforward) {
    jam();
    tidrNextSide = conptr + (ZBUF_SIZE - Container::HEADER_SIZE);
    arrGuard(tidrNextSide + 1, 2048);
    containerhead = pageptr.p->word32[conptr];
    tidrContainerlen = containerhead.getLength();
    tidrIndex = conptr + tidrContainerlen;
  } else {
    jam();
    tidrNextSide = conptr;
    conptr = conptr + (ZBUF_SIZE - Container::HEADER_SIZE);
    arrGuard(conptr + 1, 2048);
    containerhead = pageptr.p->word32[conptr];
    tidrContainerlen = containerhead.getLength();
    tidrIndex = (conptr - tidrContainerlen) +
                (Container::HEADER_SIZE - fragrecptr.p->elementLength);
  }//if
  const Uint16 activeScanMask = fragrecptr.p->activeScanMask;
  const Uint16 conscanmask = containerhead.getScanBits();
  if(tidrContainerlen > Container::HEADER_SIZE || !newContainer)
  {
    if (conScanMask != Operationrec::ANY_SCANBITS &&
        ((conscanmask & ~conScanMask) & activeScanMask) != 0)
    {
      /* Container have more scan bits set than requested */
      /* Continue to next container. */
      return;
    }
  }
  if (tidrContainerlen == Container::HEADER_SIZE && newContainer)
  {
    /**
     * Only the first header container in a bucket or a newly created bucket
     * in insertElement can be empty.
     *
     * Set container scan bits as requested.
     */
    ndbrequire(conScanMask != Operationrec::ANY_SCANBITS);
    containerhead.copyScanBits(conScanMask & activeScanMask);
    pageptr.p->word32[conptr] = containerhead;
  }
  if (tidrContainerlen >= (ZBUF_SIZE - fragrecptr.p->elementLength))
  {
    return;
  }//if
  tidrConfreelen = ZBUF_SIZE - tidrContainerlen;
  /* --------------------------------------------------------------------------------- */
  /*       WE CALCULATE THE TOTAL LENGTH THE CONTAINER CAN EXPAND TO                   */
  /*       THIS INCLUDES THE OTHER SIDE OF THE BUFFER IF POSSIBLE TO EXPAND THERE.     */
  /* --------------------------------------------------------------------------------- */
  if (!containerhead.isUsingBothEnds()) {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*       WE HAVE NOT EXPANDED TO THE ENTIRE BUFFER YET. WE CAN THUS READ THE OTHER   */
    /*       SIDE'S CONTAINER HEADER TO READ HIS LENGTH.                                 */
    /* --------------------------------------------------------------------------------- */
    ContainerHeader conhead(pageptr.p->word32[tidrNextSide]);
    tidrNextConLen = conhead.getLength();
    tidrConfreelen = tidrConfreelen - tidrNextConLen;
    if (tidrConfreelen > ZBUF_SIZE) {
      ndbabort();
      /* --------------------------------------------------------------------------------- */
      /*       THE BUFFERS ARE PLACED ON TOP OF EACH OTHER. THIS SHOULD NEVER OCCUR.       */
      /* --------------------------------------------------------------------------------- */
      return;
    }//if
  } else {
    jam();
    tidrNextConLen = 1;	/* INDICATE OTHER SIDE IS NOT PART OF FREE LIST */
  }//if
  if (tidrConfreelen < fragrecptr.p->elementLength) {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*       THE CONTAINER COULD NOT BE EXPANDED TO FIT THE NEW ELEMENT. WE HAVE TO      */
    /*       RETURN AND FIND A NEW CONTAINER TO INSERT IT INTO.                          */
    /* --------------------------------------------------------------------------------- */
    return;
  }//if
  tidrContainerlen = tidrContainerlen + fragrecptr.p->elementLength;
  if (tidrNextConLen == 0) {
    /* EACH SIDE OF THE BUFFER WHICH BELONG TO A FREE */
    /* LIST, HAS ZERO AS LENGTH. */
    if (tidrContainerlen > Container::UP_LIMIT) {
      ContainerHeader conthead = pageptr.p->word32[conptr];
      conthead.setUsingBothEnds();
      pageptr.p->word32[conptr] = conthead;
      if (isforward) {
        jam();
        /* REMOVE THE RIGHT SIDE OF THE BUFFER FROM THE FREE LIST */
        seizeRightlist(pageptr, conidx);
      } else {
        jam();
        /* REMOVE THE LEFT SIDE OF THE BUFFER FROM THE FREE LIST */
        seizeLeftlist(pageptr, conidx);
      }//if
    }//if
  }//if
  /* OF THE FREE CONTAINERS */
  /* --------------------------------------------------------------------------------- */
  /*       WE HAVE NOW FOUND A FREE SPOT IN THE CURRENT CONTAINER. WE INSERT THE       */
  /*       ELEMENT HERE. THE ELEMENT CONTAINS A HEADER, A LOCAL KEY AND A TUPLE KEY.   */
  /*       BEFORE INSERTING THE ELEMENT WE WILL UPDATE THE OPERATION RECORD WITH THE   */
  /*       DATA CONCERNING WHERE WE INSERTED THE ELEMENT. THIS MAKES IT EASY TO FIND   */
  /*       THIS INFORMATION WHEN WE RETURN TO UPDATE THE LOCAL KEY OR RETURN TO COMMIT */
  /*       OR ABORT THE INSERT. IF NO OPERATION RECORD EXIST IT MEANS THAT WE ARE      */
  /*       PERFORMING THIS AS A PART OF THE EXPAND OR SHRINK PROCESS.                  */
  /* --------------------------------------------------------------------------------- */
  const Uint32 elemhead = elem.getHeader();
  ContainerHeader conthead = pageptr.p->word32[conptr];
  if (oprecptr.i != RNIL)
  {
    jam();
    ndbrequire(ElementHeader::getLocked(elemhead));
    oprecptr.p->elementPage = pageptr.i;
    oprecptr.p->elementContainer = conptr;
    oprecptr.p->elementPointer = tidrIndex;
  }
  else
  {
    ndbassert(!ElementHeader::getLocked(elemhead));
  }
  /* --------------------------------------------------------------------------------- */
  /*       WE CHOOSE TO UNDO LOG INSERTS BY WRITING THE BEFORE VALUE TO THE UNDO LOG.  */
  /*       WE COULD ALSO HAVE DONE THIS BY WRITING THIS BEFORE VALUE WHEN DELETING     */
  /*       ELEMENTS. WE CHOOSE TO PUT IT HERE SINCE WE THEREBY ENSURE THAT WE ALWAYS   */
  /*       UNDO LOG ALL WRITES TO PAGE MEMORY. IT SHOULD BE EASIER TO MAINTAIN SUCH A  */
  /*       STRUCTURE. IT IS RATHER DIFFICULT TO MAINTAIN A LOGICAL STRUCTURE WHERE     */
  /*       DELETES ARE INSERTS AND INSERTS ARE PURELY DELETES.                         */
  /* --------------------------------------------------------------------------------- */
  ndbrequire(fragrecptr.p->localkeylen == 1);
  arrGuard(tidrIndex + 1, 2048);
  pageptr.p->word32[tidrIndex] = elem.getHeader();
  pageptr.p->word32[tidrIndex + 1] = elem.getData(); /* INSERTS LOCALKEY */
  conthead.setLength(tidrContainerlen);
  pageptr.p->word32[conptr] = conthead;
  result = ZTRUE;
}//Dbacc::insertContainer()

/** ---------------------------------------------------------------------------
 * Set next link of a container to reference to next container.
 *
 * @param[in]  pageptr       Pointer to page of container to modify.
 * @param[in]  conptr        Pointer within page of container to modify.
 * @param[in]  nextConidx    Index within page of next container.
 * @param[in]  nextContype   Type of next container, left or right end.
 * @param[in]  nextSamepage  True if next container is on same page as modified
 *                           container
 * @param[in]  nextPagei     Overflow page number of next container.
 * ------------------------------------------------------------------------- */
void Dbacc::addnewcontainer(Page8Ptr pageptr,
                            Uint32 conptr,
                            Uint32 nextConidx,
                            Uint32 nextContype,
                            bool nextSamepage,
                            Uint32 nextPagei) const
{
  ContainerHeader containerhead(pageptr.p->word32[conptr]);
  containerhead.setNext(nextContype, nextConidx, nextSamepage);
  pageptr.p->word32[conptr] = containerhead;
  pageptr.p->word32[conptr + 1] = nextPagei;
}//Dbacc::addnewcontainer()

/* --------------------------------------------------------------------------------- */
/* GETFREELIST                                                                       */
/*         INPUT:                                                                    */
/*               GFL_PAGEPTR (POINTER TO A PAGE RECORD).                             */
/*         OUTPUT:                                                                   */
/*                TGFL_PAGEINDEX(POINTER TO A FREE BUFFER IN THE FREEPAGE), AND      */
/*                TGFL_BUF_TYPE( TYPE OF THE FREE BUFFER).                           */
/*         DESCRIPTION: SEARCHES IN THE FREE LIST OF THE FREE BUFFER IN THE PAGE HEAD*/
/*                     (WORD32(1)),AND RETURN ADDRESS OF A FREE BUFFER OR NIL.       */
/*                     THE FREE BUFFER CAN BE A RIGHT CONTAINER OR A LEFT ONE        */
/*                     THE KIND OF THE CONTAINER IS NOTED BY TGFL_BUF_TYPE.          */
/* --------------------------------------------------------------------------------- */
void Dbacc::getfreelist(Page8Ptr pageptr, Uint32& pageindex, Uint32& buftype)
{
  const Uint32 emptylist = pageptr.p->word32[Page8::EMPTY_LIST];
  pageindex = (emptylist >> 7) & 0x7f;	/* LEFT FREE LIST */
  buftype = ZLEFT;
  if (pageindex == Container::NO_CONTAINER_INDEX) {
    jam();
    pageindex = emptylist & 0x7f;	/* RIGHT FREE LIST */
    buftype = ZRIGHT;
  }//if
  ndbrequire((pageindex <= Container::MAX_CONTAINER_INDEX) ||
             (pageindex == Container::NO_CONTAINER_INDEX));
}//Dbacc::getfreelist()

/* --------------------------------------------------------------------------------- */
/* INCREASELISTCONT                                                                  */
/*       INPUT:                                                                      */
/*               ILC_PAGEPTR     PAGE POINTER TO INCREASE NUMBER OF CONTAINERS IN    */
/*           A CONTAINER OF AN OVERFLOW PAGE (FREEPAGEPTR) IS ALLOCATED, NR OF       */
/*           ALLOCATED CONTAINER HAVE TO BE INCREASED BY ONE.                        */
/*           IF THE NUMBER OF ALLOCATED CONTAINERS IS ABOVE THE FREE LIMIT WE WILL   */
/*           REMOVE THE PAGE FROM THE FREE LIST.                                     */
/* --------------------------------------------------------------------------------- */
void Dbacc::increaselistcont(Page8Ptr ilcPageptr)
{
  ilcPageptr.p->word32[Page8::ALLOC_CONTAINERS] = ilcPageptr.p->word32[Page8::ALLOC_CONTAINERS] + 1;
  // A sparse page just got full
  if (ilcPageptr.p->word32[Page8::ALLOC_CONTAINERS] == ZFREE_LIMIT + 1) {
    // Check that it is an overflow page
    if (((ilcPageptr.p->word32[Page8::EMPTY_LIST] >> ZPOS_PAGE_TYPE_BIT) & 3) == 1)
    {
      jam();
      LocalContainerPageList sparselist(c_page8_pool, fragrecptr.p->sparsepages);
      LocalContainerPageList fulllist(c_page8_pool, fragrecptr.p->fullpages);
      sparselist.remove(ilcPageptr);
      fulllist.addLast(ilcPageptr);
    }//if
  }//if
}//Dbacc::increaselistcont()

/* --------------------------------------------------------------------------------- */
/* SEIZE_LEFTLIST                                                                    */
/*       INPUT:                                                                      */
/*               TSL_PAGEINDEX           PAGE INDEX OF CONTAINER TO SEIZE            */
/*               SL_PAGEPTR              PAGE POINTER OF CONTAINER TO SEIZE          */
/*               TSL_UPDATE_HEADER       SHOULD WE UPDATE THE CONTAINER HEADER       */
/*                                                                                   */
/*       OUTPUT:                                                                     */
/*               NONE                                                                */
/*         DESCRIPTION: THE BUFFER NOTED BY TSL_PAGEINDEX WILL BE REMOVED FROM THE   */
/*                      LIST OF LEFT FREE CONTAINER, IN THE HEADER OF THE PAGE       */
/*                      (FREEPAGEPTR). PREVIOUS AND NEXT BUFFER OF REMOVED BUFFER    */
/*                      WILL BE UPDATED.                                             */
/* --------------------------------------------------------------------------------- */
void Dbacc::seizeLeftlist(Page8Ptr slPageptr, Uint32 tslPageindex)
{
  Uint32 tsllTmp1;
  Uint32 tsllHeadIndex;
  Uint32 tsllTmp;

  tsllHeadIndex = getForwardContainerPtr(tslPageindex);
  arrGuard(tsllHeadIndex + 1, 2048);
  Uint32 tslNextfree = slPageptr.p->word32[tsllHeadIndex];
  Uint32 tslPrevfree = slPageptr.p->word32[tsllHeadIndex + 1];
  if (tslPrevfree == Container::NO_CONTAINER_INDEX) {
    jam();
    /* UPDATE FREE LIST OF LEFT CONTAINER IN PAGE HEAD */
    tsllTmp1 = slPageptr.p->word32[Page8::EMPTY_LIST];
    tsllTmp = tsllTmp1 & 0x7f;
    tsllTmp1 = (tsllTmp1 >> 14) << 14;
    tsllTmp1 = (tsllTmp1 | (tslNextfree << 7)) | tsllTmp;
    slPageptr.p->word32[Page8::EMPTY_LIST] = tsllTmp1;
  } else {
    ndbrequire(tslPrevfree <= Container::MAX_CONTAINER_INDEX);
    jam();
    tsllTmp = getForwardContainerPtr(tslPrevfree);
    slPageptr.p->word32[tsllTmp] = tslNextfree;
  }//if
  if (tslNextfree <= Container::MAX_CONTAINER_INDEX) {
    jam();
    tsllTmp = getForwardContainerPtr(tslNextfree) + 1;
    slPageptr.p->word32[tsllTmp] = tslPrevfree;
  } else {
    ndbrequire(tslNextfree == Container::NO_CONTAINER_INDEX);
    jam();
  }//if
  increaselistcont(slPageptr);
}//Dbacc::seizeLeftlist()

/* --------------------------------------------------------------------------------- */
/* SEIZE_RIGHTLIST                                                                   */
/*         DESCRIPTION: THE BUFFER NOTED BY TSL_PAGEINDEX WILL BE REMOVED FROM THE   */
/*                      LIST OF RIGHT FREE CONTAINER, IN THE HEADER OF THE PAGE      */
/*                      (SL_PAGEPTR). PREVIOUS AND NEXT BUFFER OF REMOVED BUFFER     */
/*                      WILL BE UPDATED.                                             */
/* --------------------------------------------------------------------------------- */
void Dbacc::seizeRightlist(Page8Ptr slPageptr, Uint32 tslPageindex)
{
  Uint32 tsrlHeadIndex;
  Uint32 tsrlTmp;

  tsrlHeadIndex = getBackwardContainerPtr(tslPageindex);
  arrGuard(tsrlHeadIndex + 1, 2048);
  Uint32 tslNextfree = slPageptr.p->word32[tsrlHeadIndex];
  Uint32 tslPrevfree = slPageptr.p->word32[tsrlHeadIndex + 1];
  if (tslPrevfree == Container::NO_CONTAINER_INDEX) {
    jam();
    tsrlTmp = slPageptr.p->word32[Page8::EMPTY_LIST];
    slPageptr.p->word32[Page8::EMPTY_LIST] = ((tsrlTmp >> 7) << 7) | tslNextfree;
  } else {
    ndbrequire(tslPrevfree <= Container::MAX_CONTAINER_INDEX);
    jam();
    tsrlTmp = getBackwardContainerPtr(tslPrevfree);
    slPageptr.p->word32[tsrlTmp] = tslNextfree;
  }//if
  if (tslNextfree <= Container::MAX_CONTAINER_INDEX) {
    jam();
    tsrlTmp = getBackwardContainerPtr(tslNextfree) + 1;
    slPageptr.p->word32[tsrlTmp] = tslPrevfree;
  } else {
    ndbrequire(tslNextfree == Container::NO_CONTAINER_INDEX);
    jam();
  }//if
  increaselistcont(slPageptr);
}//Dbacc::seizeRightlist()

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF INSERT_ELEMENT MODULE                                                */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       MODULE:         GET_ELEMENT                                                 */
/*               THE FOLLOWING SUBROUTINES ARE ONLY USED BY GET_ELEMENT AND          */
/*               GETDIRINDEX. THIS ROUTINE IS THE SOLE INTERFACE TO GET ELEMENTS     */
/*               FROM THE INDEX. CURRENT USERS ARE ALL REQUESTS AND EXECUTE UNDO LOG */
/*                                                                                   */
/*               THE FOLLOWING SUBROUTINES ARE INCLUDED IN THIS MODULE:              */
/*               GET_ELEMENT                                                         */
/*               GET_DIRINDEX                                                        */
/*               SEARCH_LONG_KEY                                                     */
/*                                                                                   */
/*               THESE ROUTINES ARE ONLY USED BY THIS MODULE AND BY NO ONE ELSE.     */
/*               ALSO THE ROUTINES MAKE NO USE OF ROUTINES IN OTHER MODULES.         */
/*               THE ONLY SHORT-LIVED VARIABLES USED IN OTHER PARTS OF THE BLOCK ARE */
/*               THOSE DEFINED AS INPUT AND OUTPUT IN GET_ELEMENT AND GETDIRINDEX    */
/*               SHORT-LIVED VARIABLES INCLUDE TEMPORARY VARIABLES, COMMON VARIABLES */
/*               AND POINTER VARIABLES.                                              */
/*               THE ONLY EXCEPTION TO THIS RULE IS FRAGRECPTR WHICH POINTS TO THE   */
/*               FRAGMENT RECORD. THIS IS MORE LESS STATIC ALWAYS DURING A SIGNAL    */
/*               EXECUTION.                                                          */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* GETDIRINDEX                                                                       */
/*       SUPPORT ROUTINE FOR INSERT ELEMENT, GET ELEMENT AND COMMITDELETE            */
/*         INPUT:FRAGRECPTR ( POINTER TO THE ACTIVE FRAGMENT REC)                    */
/*               OPERATION_REC_PTR  (POINTER TO THE OPERATION REC).                  */
/*                                                                                   */
/*         OUTPUT:GDI_PAGEPTR ( POINTER TO THE PAGE OF THE ELEMENT)                  */
/*                TGDI_PAGEINDEX ( INDEX OF THE ELEMENT IN THE PAGE).                */
/*                                                                                   */
/*         DESCRIPTION: CHECK THE HASH VALUE OF THE OPERATION REC AND CALCULATE THE  */
/*                     THE ADDRESS OF THE ELEMENT IN THE HASH TABLE,(GDI_PAGEPTR,    */
/*                     TGDI_PAGEINDEX) ACCORDING TO LH3.                             */
/* --------------------------------------------------------------------------------- */
Uint32 Dbacc::getPagePtr(DynArr256::Head& directory, Uint32 index)
{
  DynArr256 dir(directoryPoolPtr, directory);
  Uint32* ptr = dir.get(index);
  return *ptr;
}

bool Dbacc::setPagePtr(DynArr256::Head& directory, Uint32 index, Uint32 ptri)
{
  DynArr256 dir(directoryPoolPtr, directory);
  Uint32* ptr = dir.set(index);
  if (ptr == NULL) return false;
  *ptr = ptri;
  return true;
}

Uint32 Dbacc::unsetPagePtr(DynArr256::Head& directory, Uint32 index)
{
  DynArr256 dir(directoryPoolPtr, directory);
  Uint32* ptr = dir.get(index);
  Uint32 ptri = *ptr;
  *ptr = RNIL;
  return ptri;
}

void Dbacc::getdirindex(Page8Ptr& pageptr, Uint32& conidx)
{
  const LHBits32 hashValue = operationRecPtr.p->hashValue;
  const Uint32 address = fragrecptr.p->level.getBucketNumber(hashValue);
  conidx = fragrecptr.p->getPageIndex(address);
  pageptr.i = getPagePtr(fragrecptr.p->directory,
                         fragrecptr.p->getPageNumber(address));
  c_page8_pool.getPtr(pageptr);
}//Dbacc::getdirindex()

Uint32
Dbacc::find_key_operation(Ptr<Operationrec> opPtr, bool invalid_local_key)
{
  if (invalid_local_key)
  {
    if (m_ldm_instance_used->c_lqh->has_key_info(opPtr.p->userptr))
    {
      jam();
      return opPtr.p->userptr;
    }
  }
  do
  {
    opPtr.i = opPtr.p->nextSerialQue;
    if (opPtr.i == RNIL)
    {
      jam();
      return RNIL;
    }
    opPtr.p = m_ldm_instance_used->getOperationPtrP(opPtr.i);
    if (m_ldm_instance_used->c_lqh->has_key_info(opPtr.p->userptr))
    {
      jam();
      return opPtr.p->userptr;
    }
  } while (true);
  return RNIL;
}

Uint32
Dbacc::readTablePk(Uint32 localkey1,
                   Uint32 localkey2,
                   Uint32 eh,
                   Ptr<Operationrec> opPtr,
                   Uint32 *keys,
                   bool xfrm)
{
  int ret = -ZTUPLE_DELETED_ERROR;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  const int xfrm_multiply = (xfrm) ? MAX_XFRM_MULTIPLY : 1;
  std::memset(keys, 0x1f, (fragrecptr.p->keyLength * xfrm_multiply) << 2);
#endif
  bool invalid_local_key = true;
  if (likely(! Local_key::isInvalid(localkey1, localkey2)))
  {
    jamDebug();
    invalid_local_key = false;
    ret = c_tup->accReadPk(localkey1,
                           localkey2,
                           keys,
                           xfrm);
  }
  if (ret == (-ZTUPLE_DELETED_ERROR))
  {
    jamDebug();
    /**
     * We can come here in two cases:
     * 1) The local key hasn't been updated yet. In this case the Insert
     *    was delayed by a disk allocation. The key is found from the
     *    lock owners operation record.
     * 2) The local key is set, but the FREE flag is set. In
     *    this case accReadPk will return -TUPLE_DELETED_ERROR. This means
     *    that the INSERT was followed by a DELETE and the DELETE have been
     *    committed. There is thus no key to be found in the row and there
     *    is no copy row. Thus we're back to reading the key from the lock
     *    queue.
     *
     *    We need to find an operation record that still has the key
     *    attached to it. We will check the lock owner and all operations
     *    in the serial queue. If the local key is invalid we will find
     *    the key in the lock owner. We won't search the parallel queue
     *    since these operations have likely already released the key
     *    and also if the decision was taken to delete the record, then
     *    no operation in the parallel queue will revert that decision.
     *    However all operations in the serial queue have not yet
     *    released any key they might have. If none in the serial queue
     *    has a key attached to it, then there are either no operation
     *    there or there are only SCAN operations. Thus we can safely
     *    return not found since the tuple is going away and we can start
     *    a new tuple here.
     *
     * find_key_operation will only check lock owner if the local key is
     * invalid. This will only happen when INSERT has started, but not
     * yet arrived at the point where we called ACCMINUPDATE. This is
     * protected by the ACC mutex, thus the query thread need no extra
     * protection to check the keyInfoIVal in DBLQH since this is not
     * released before we have called ACCMINUPDATE and it is certain to
     * have been set before starting the INSERT operation in DBACC.
     *
     * When local key isn't invalid we are dealing with a DELETE operation.
     * In this case we only need to worry about any operations in the
     * serial queue. These are waiting in the queue and are currently idle
     * and can only be removed from serial queue when holding the ACC mutex.
     * keyInfoIVal is not released before the ACC operation is removed. Thus
     * it is safe to check the keyInfoIVal also for query threads from here.
     */
    ndbrequire(ElementHeader::getLocked(eh));
    Uint32 lqhOpPtr = find_key_operation(opPtr, invalid_local_key);
    if (lqhOpPtr == RNIL)
    {
      jamDebug();
      dump_lock_queue(opPtr);
      ndbrequire(opPtr.p->m_op_bits & Operationrec::OP_ELEMENT_DISAPPEARED);
      if (unlikely((opPtr.p->m_op_bits & Operationrec::OP_MASK) == ZSCAN_OP))
      {
        ndbrequire(opPtr.p->m_op_bits & Operationrec::OP_COMMIT_DELETE_CHECK);
        ndbrequire(((opPtr.p->m_op_bits & Operationrec::OP_STATE_MASK) ==
                    Operationrec::OP_STATE_RUNNING) ||
                    ((opPtr.p->m_op_bits & Operationrec::OP_STATE_MASK) ==
                    Operationrec::OP_STATE_EXECUTED));
      }
      return 0;
    }
    ret = m_ldm_instance_used->c_lqh->readPrimaryKeys(lqhOpPtr, keys, xfrm);
  }
  jamEntryDebug();
  ndbrequire(ret >= 0);
  return ret;
}

/** ---------------------------------------------------------------------------
 * Find element.
 *
 * Method scan the bucket given by hashValue from operationRecPtr and look for
 * the element with primary key given in signal.  If element found, return
 * pointer to element, if not found return only bucket information.
 *
 * @param[in]   signal         Signal containing primary key to look for.
 * @param[out]  lockOwnerPtr   Lock owner if any of found element.
 * @param[out]  bucketPageptr  Page of first container of bucket there element
                               should be.
 * @param[out]  bucketConidx   Index within page of first container of bucket
                               there element should be.
 * @param[out]  elemPageptr    Page of found element.
 * @param[out]  elemConptr     Pointer within page to container of found
                               element.
 * @param[out]  elemptr        Pointer within page to found element.
 * @return                     Returns ZTRUE if element was found.
 * ------------------------------------------------------------------------- */
Uint32
Dbacc::getElement(const AccKeyReq* signal,
                  OperationrecPtr& lockOwnerPtr,
                  Page8Ptr& bucketPageptr,
                  Uint32& bucketConidx,
                  Page8Ptr& elemPageptr,
                  Uint32& elemConptr,
                  Uint32& elemptr)
{
  Uint32 tgeElementHeader;
  Uint32 tgeElemStep = 0;
  Uint32 tgePageindex;
  Uint32 tgeNextptrtype;
  Uint32 tgeRemLen = 0;
  const Uint32 TelemLen = fragrecptr.p->elementLength;
  const Uint32* Tkeydata = signal->keyInfo; /* or localKey if keyLen == 0 */
  const Uint32 localkeylen = fragrecptr.p->localkeylen;
  Uint32 bucket_number = fragrecptr.p->level.getBucketNumber(
                          operationRecPtr.p->hashValue);
  union {
    Uint32 keys[2048];
    Uint64 keys_align;
  };
  (void)keys_align;

  getdirindex(bucketPageptr, bucketConidx);
  elemPageptr = bucketPageptr;
  tgePageindex = bucketConidx;
  /*
   * The value searched is
   * - table key for ACCKEYREQ, stored in TUP
   * - local key (1 word) for ACC_LOCKREQ and UNDO, stored in ACC
   */
  const bool searchLocalKey = operationRecPtr.p->tupkeylen == 0;

  ndbrequire(TelemLen == ZELEM_HEAD_SIZE + localkeylen);
  tgeNextptrtype = ZLEFT;

  do {
    if (tgeNextptrtype == ZLEFT)
    {
      jamDebug();
      elemConptr = getForwardContainerPtr(tgePageindex);
      elemptr = elemConptr + Container::HEADER_SIZE;
      tgeElemStep = TelemLen;
      ndbrequire(elemConptr < 2048);
      ContainerHeader conhead(elemPageptr.p->word32[elemConptr]);
      tgeRemLen = conhead.getLength();
      ndbrequire((elemConptr + tgeRemLen - 1) < 2048);
    }
    else if (tgeNextptrtype == ZRIGHT)
    {
      jamDebug();
      elemConptr = getBackwardContainerPtr(tgePageindex);
      tgeElemStep = 0 - TelemLen;
      elemptr = elemConptr - TelemLen;
      ndbrequire(elemConptr < 2048);
      ContainerHeader conhead(elemPageptr.p->word32[elemConptr]);
      tgeRemLen = conhead.getLength();
      ndbrequire((elemConptr - tgeRemLen) < 2048);
    }
    else
    {
      ndbrequire((tgeNextptrtype == ZLEFT) || (tgeNextptrtype == ZRIGHT));
    }//if
    if (tgeRemLen >= Container::HEADER_SIZE + TelemLen)
    {
      ndbrequire(tgeRemLen <= ZBUF_SIZE);
      /* ------------------------------------------------------------------- */
      // There is at least one element in this container. 
      // Check if it is the element searched for.
      /* ------------------------------------------------------------------- */
      do {
        bool possible_match;
        tgeElementHeader = elemPageptr.p->word32[elemptr];
        tgeRemLen = tgeRemLen - TelemLen;
        Local_key localkey;
        lockOwnerPtr.i = RNIL;
        lockOwnerPtr.p = NULL;
        LHBits16 reducedHashValue;
        if (ElementHeader::getLocked(tgeElementHeader))
        {
          jamDebug();
          lockOwnerPtr.i = ElementHeader::getOpPtrI(tgeElementHeader);
          /**
           * We need to get the operation record of the lock owner.
           * Since we can be the query thread we cannot access it directly
           * since we don't share the operation records with the owning
           * LDM thread. We will get the operation record from the
           * owning LDM thread.
           */
          lockOwnerPtr.p =
            m_ldm_instance_used->getOperationPtrP(lockOwnerPtr.i);
          possible_match =
            lockOwnerPtr.p->hashValue.match(operationRecPtr.p->hashValue);
          reducedHashValue = lockOwnerPtr.p->reducedHashValue;
          localkey = lockOwnerPtr.p->localdata;
        }
        else
        {
          jamDebug();
          reducedHashValue =
            ElementHeader::getReducedHashValue(tgeElementHeader);
          const Uint32 pos = elemptr + 1;
          ndbrequire(localkeylen == 1);
          localkey.m_page_no = elemPageptr.p->word32[pos];
          localkey.m_page_idx = ElementHeader::getPageIdx(tgeElementHeader);
          possible_match = true;
        }
        if (possible_match &&
            operationRecPtr.p->hashValue.match(
              fragrecptr.p->level.enlarge(reducedHashValue, bucket_number)))
        {
          jamDebug();
          jamLineDebug(Uint16(elemPageptr.i));
          jamLineDebug(Uint16(elemptr));
          bool found;
          if (! searchLocalKey) 
	  {
            const bool xfrm = false;
            Uint32 len = readTablePk(localkey.m_page_no,
                                     localkey.m_page_idx,
                                     tgeElementHeader,
                                     lockOwnerPtr,
                                     &keys[0],
                                     xfrm);
            if (unlikely(len == 0))
            {
              jamDebug();
              found = false;
            }
            else
            {
              if (fragrecptr.p->hasCharAttr)  //Need to consult charset library
              {
                jamDebug();
                const Uint32 table = fragrecptr.p->myTableId;
                found = (cmp_key(table, Tkeydata, &keys[0]) == 0);
              }
              else
              {
                jamDebug();
                found = (len == operationRecPtr.p->tupkeylen) &&
                        (memcmp(Tkeydata, &keys[0], len << 2) == 0);
              }
            }
          }
          else
          {
            jam();
            found = (localkey.m_page_no == Tkeydata[0] &&
                     Uint32(localkey.m_page_idx) == Tkeydata[1]);
          }
          if (found) 
          {
            jamDebug();
            operationRecPtr.p->localdata = localkey;
            return ZTRUE;
          }
        }
        if (tgeRemLen <= Container::HEADER_SIZE)
        {
          break;
        }
        elemptr = elemptr + tgeElemStep;
      } while (true);
    }//if
    ndbrequire(tgeRemLen == Container::HEADER_SIZE);
    ContainerHeader containerhead = elemPageptr.p->word32[elemConptr];
    tgeNextptrtype = containerhead.getNextEnd();
    if (tgeNextptrtype == 0)
    {
      jamDebug();
      return ZFALSE;	/* NO MORE CONTAINER */
    }//if
    /* NEXT CONTAINER PAGE INDEX 7 BITS */
    tgePageindex = containerhead.getNextIndexNumber();
    ndbrequire(tgePageindex <= Container::NO_CONTAINER_INDEX);
    if (!containerhead.isNextOnSamePage())
    {
      jamDebug();
      elemPageptr.i = elemPageptr.p->word32[elemConptr + 1]; /* NEXT PAGE ID */
      c_page8_pool.getPtr(elemPageptr);
    }//if
  } while (1);
  return ZFALSE;
}//Dbacc::getElement()

/**
 * report_pending_dealloc
 *
 * ACC indicates to LQH that it expects LQH to deallocate
 * the TUPle at some point after all the reported operations
 * have completed and the deallocation is allowed.
 *
 * opPtrP       Ptr to operation involved in dealloc
 * countOpPtrP  Ptr to operation tracking delete reference count
 *               (can be same as opPtrP)
 */
void
Dbacc::report_pending_dealloc(Signal* signal,
                              Operationrec* opPtrP,
                              const Operationrec* countOpPtrP)
{
  Local_key localKey = opPtrP->localdata;
  Uint32 opbits = opPtrP->m_op_bits;
  Uint32 userptr= opPtrP->userptr;
  const bool scanInd = (((opbits & Operationrec::OP_MASK) == ZSCAN_OP) ||
                        (opbits & Operationrec::OP_LOCK_REQ));

  if (! localKey.isInvalid())
  {
    if (scanInd)
    {
      jam();
      /**
       * Scan operation holding a lock on a key whose tuple
       * is being deallocated.
       * If this is the last operation to commit on the key
       * then it will notify LQH when the dealloc is
       * triggered.
       * To make that possible, we store the deleting
       * operation's userptr in the scan op record.
       */
      ndbrequire(opPtrP->m_scanOpDeleteCountOpRef == RNIL);
      opPtrP->m_scanOpDeleteCountOpRef = countOpPtrP->userptr;
      return;
    }
    ndbrequire(countOpPtrP->userptr != RNIL);

    /**
     * Inform LQH of operation involved in transaction
     * which is deallocating a tuple.
     * Also pass the LQH reference of the refcount operation
     */
    signal->theData[0] = fragrecptr.p->myfid;
    signal->theData[1] = fragrecptr.p->myTableId;
    signal->theData[2] = localKey.m_page_no;
    signal->theData[3] = localKey.m_page_idx;
    signal->theData[4] = userptr;
    signal->theData[5] = countOpPtrP->userptr;
    c_lqh->execTUP_DEALLOCREQ(signal);
    jamEntryDebug();
  }
}

/**
 * trigger_dealloc
 *
 * ACC is now done with the TUPle storage, so inform LQH
 * that it can go ahead with deallocation when it is able
 */
void
Dbacc::trigger_dealloc(Signal* signal, const Operationrec* opPtrP)
{
  Local_key localKey = opPtrP->localdata;
  Uint32 opbits = opPtrP->m_op_bits;
  Uint32 userptr= opPtrP->userptr;
  const bool scanInd =
    ((opbits & Operationrec::OP_MASK) == ZSCAN_OP) || 
    (opbits & Operationrec::OP_LOCK_REQ);
  
  if (! localKey.isInvalid())
  {
    if (scanInd)
    {
      jam();

      if (likely(opPtrP->m_scanOpDeleteCountOpRef != RNIL))
      {
        jam();
        ndbrequire((opbits & Operationrec::OP_PENDING_ABORT) == 0);

        /**
         * Operation triggering deallocation as part of commit
         * is a scan operation.
         * We must use a reference to the LQH deallocation operation
         * stored on the scan operation in commitDeleteCheck()/
         * report_pending_dealloc() to inform LQH that the
         * deallocation is triggered.
         * LQH then decides when it is safe to deallocate.
         */
        userptr = opPtrP->m_scanOpDeleteCountOpRef;
      }
      else
      {
        jam();
        ndbrequire((opbits & Operationrec::OP_PENDING_ABORT) != 0);

        /**
         * Operation triggering deallocation as part of abort
         * is a scan operation.
         *
         * We will inform LQH to deallocate immediately.
         */
        userptr = RNIL;
      }
    }
    /* Inform LQH that deallocation can go ahead */
    signal->theData[0] = fragrecptr.p->myfid;
    signal->theData[1] = fragrecptr.p->myTableId;
    signal->theData[2] = localKey.m_page_no;
    signal->theData[3] = localKey.m_page_idx;
    signal->theData[4] = userptr;
    signal->theData[5] = RNIL;
    c_lqh->execTUP_DEALLOCREQ(signal);
    jamEntryDebug();
  }
}

void Dbacc::commitdelete(Signal* signal)
{
  Page8Ptr lastPageptr;
  Page8Ptr lastPrevpageptr;
  bool lastIsforward;
  Uint32 tlastPageindex;
  Uint32 tlastElementptr;
  Uint32 tlastContainerptr;
  Uint32 tlastPrevconptr;
  Page8Ptr lastBucketPageptr;
  Uint32 lastBucketConidx;

  jam();
  trigger_dealloc(signal, operationRecPtr.p);
  
  getdirindex(lastBucketPageptr, lastBucketConidx);
  lastPageptr = lastBucketPageptr;
  tlastPageindex = lastBucketConidx;
  lastIsforward = true;
  tlastContainerptr = getForwardContainerPtr(tlastPageindex);
  arrGuard(tlastContainerptr, 2048);
  lastPrevpageptr.i = RNIL;
  ptrNull(lastPrevpageptr);
  tlastPrevconptr = 0;

  /**
   * Position last on delete container before call to getLastAndRemove.
   */
  Page8Ptr delPageptr;
  delPageptr.i = operationRecPtr.p->elementPage;
  c_page8_pool.getPtr(delPageptr);
  const Uint32 delConptr = operationRecPtr.p->elementContainer;

  while (lastPageptr.i != delPageptr.i ||
         tlastContainerptr != delConptr)
  {
    lastPrevpageptr = lastPageptr;
    tlastPrevconptr = tlastContainerptr;
    ContainerHeader lasthead(lastPageptr.p->word32[tlastContainerptr]);
    ndbrequire(lasthead.haveNext());
    if (!lasthead.isNextOnSamePage())
    {
      lastPageptr.i = lastPageptr.p->word32[tlastContainerptr + 1];
      c_page8_pool.getPtr(lastPageptr);
    }
    tlastPageindex = lasthead.getNextIndexNumber();
    lastIsforward = lasthead.getNextEnd() == ZLEFT;
    tlastContainerptr = getContainerPtr(tlastPageindex, lastIsforward);
  }

  getLastAndRemove(lastPrevpageptr,
                   tlastPrevconptr,
                   lastPageptr,
                   tlastPageindex,
                   tlastContainerptr,
                   lastIsforward,
                   tlastElementptr);

  const Uint32 delElemptr = operationRecPtr.p->elementPointer;
  /*
   * If last element is in same container as delete element, and that container
   * have scans in progress, one must make sure the last element still have the
   * same scan state, or clear if it is the one deleted.
   * If last element is not in same container as delete element, that element
   * can not have any scans in progress, in that case the container scanbits
   * should have been fewer than delete containers which is not allowed for last.
   */
  if ((lastPageptr.i == delPageptr.i) &&
      (tlastContainerptr == delConptr))
  {
    ContainerHeader conhead(delPageptr.p->word32[delConptr]);
    /**
     * If the deleted element was the only element in container
     * getLastAndRemove may have released the container already.
     * In that case header is still valid to read but it will
     * not be in use, but free.
     */
    if (conhead.isInUse() && conhead.isScanInProgress())
    {
      /**
       * Initialize scanInProgress with the active scans which have not
       * completely scanned the container.  Then check which scan actually
       * currently scan the container.
       */
      Uint16 scansInProgress =
          fragrecptr.p->activeScanMask & ~conhead.getScanBits();
      scansInProgress = delPageptr.p->checkScans(scansInProgress, delConptr);
      for(int i = 0; scansInProgress != 0; i++, scansInProgress >>= 1)
      {
        /**
         * For each scan in progress in container, move the scan bit for
         * last element to the delete elements place.  If it is the last
         * element that is deleted, the scan bit will be cleared by
         * moveScanBit.
         */
        if ((scansInProgress & 1) != 0)
        {
          ScanRecPtr scanPtr;
          scanPtr.i = fragrecptr.p->scan[i];
          ndbrequire(scanRec_pool.getValidPtr(scanPtr));
          scanPtr.p->moveScanBit(delElemptr, tlastElementptr);
        }
      }
    }
  }
  else
  {
    /**
     * The last element which is to be moved into deleted elements place
     * are in different containers.
     *
     * Since both containers have the same scan bits that implies that there
     * are no scans in progress in the last elements container, otherwise
     * the delete container should have an extra scan bit set.
     */
#if defined (VM_TRACE) || defined(ERROR_INSERT)
    ContainerHeader conhead(lastPageptr.p->word32[tlastContainerptr]);
    ndbassert(!conhead.isInUse() || !conhead.isScanInProgress());
    conhead = ContainerHeader(delPageptr.p->word32[delConptr]);
#else
    ContainerHeader conhead(delPageptr.p->word32[delConptr]);
#endif
    if (conhead.isScanInProgress())
    {
      /**
       * Initialize scanInProgress with the active scans which have not
       * completely scanned the container.  Then check which scan actually
       * currently scan the container.
       */
      Uint16 scansInProgress = fragrecptr.p->activeScanMask & ~conhead.getScanBits();
      scansInProgress = delPageptr.p->checkScans(scansInProgress, delConptr);
      for(int i = 0; scansInProgress != 0; i++, scansInProgress >>= 1)
      {
        if ((scansInProgress & 1) != 0)
        {
          ScanRecPtr scanPtr;
          scanPtr.i = fragrecptr.p->scan[i];
          ndbrequire(scanRec_pool.getValidPtr(scanPtr));
          if(scanPtr.p->isScanned(delElemptr))
          {
            scanPtr.p->clearScanned(delElemptr);
          }
        }
      }
    }
  }
  if (operationRecPtr.p->elementPage == lastPageptr.i &&
      operationRecPtr.p->elementPointer == tlastElementptr) {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*  THE LAST ELEMENT WAS THE ELEMENT TO BE DELETED. WE NEED NOT COPY IT.             */
    /*  Setting it to an invalid value only for sanity, the value should never be read.  */
    /* --------------------------------------------------------------------------------- */
    jamLineDebug(Uint16(delPageptr.i));
    jamLineDebug(Uint16(delElemptr));
    delPageptr.p->word32[delElemptr] = ElementHeader::setInvalid();
  } else {
    /* --------------------------------------------------------------------------------- */
    /*  THE DELETED ELEMENT IS NOT THE LAST. WE READ THE LAST ELEMENT AND OVERWRITE THE  */
    /*  DELETED ELEMENT.                                                                 */
    /* --------------------------------------------------------------------------------- */
#if defined(VM_TRACE) || !defined(NDEBUG) || defined(ERROR_INSERT)
    jamDebug();
    jamLineDebug(Uint16(delPageptr.i));
    jamLineDebug(Uint16(delElemptr));
    delPageptr.p->word32[delElemptr] = ElementHeader::setInvalid();
#endif
    deleteElement(delPageptr,
                  delConptr,
                  delElemptr,
                  lastPageptr,
                  tlastElementptr);
  }

  // Adjust the 'slack' for the deleted element.
  // If needed, initiate a 'shrink' of the storage structures.
  fragrecptr.p->slack += fragrecptr.p->elementLength;
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(3004) &&
      fragrecptr.p->fragmentid == 0 &&
      fragrecptr.p->level.getSize() != ERROR_INSERT_EXTRA)
  {
    jam();
    signal->theData[0] = fragrecptr.i;
    fragrecptr.p->expandOrShrinkQueued = true;
    sendSignal(reference(), GSN_SHRINKCHECK2, signal, 1, JBB);
  }
#endif
  if (fragrecptr.p->slack > fragrecptr.p->slackCheck)
  {
    /* TIME FOR JOIN BUCKETS PROCESS */
    if (fragrecptr.p->expandCounter > 0) {
      if (!fragrecptr.p->expandOrShrinkQueued)
      {
        jam();
        signal->theData[0] = fragrecptr.i;
        fragrecptr.p->expandOrShrinkQueued = true;
        sendSignal(reference(), GSN_SHRINKCHECK2, signal, 1, JBB);
      }//if
    }//if
  }//if
}//Dbacc::commitdelete()

/** --------------------------------------------------------------------------
 * Move last element over deleted element.
 *
 * And if moved element has an operation record update that with new element
 * location.
 *
 * @param[in]  delPageptr   Pointer to page of deleted element.
 * @param[in]  delConptr    Pointer within page to container of deleted element
 * @param[in]  delElemptr   Pointer within page to deleted element.
 * @param[in]  lastPageptr  Pointer to page of last element.
 * @param[in]  lastElemptr  Pointer within page to last element.
 * ------------------------------------------------------------------------- */
void Dbacc::deleteElement(Page8Ptr delPageptr,
                          Uint32 delConptr,
                          Uint32 delElemptr,
                          Page8Ptr lastPageptr,
                          Uint32 lastElemptr) const
{
  OperationrecPtr deOperationRecPtr;

  if (lastElemptr >= 2048)
    goto deleteElement_index_error1;
  {
    const Uint32 tdeElemhead = lastPageptr.p->word32[lastElemptr];
    ndbrequire(fragrecptr.p->elementLength == 2);
    ndbassert(!ElementHeader::isValid(delPageptr.p->word32[delElemptr]));
    delPageptr.p->word32[delElemptr] = lastPageptr.p->word32[lastElemptr];
    delPageptr.p->word32[delElemptr + 1] =
      lastPageptr.p->word32[lastElemptr + 1];
    if (ElementHeader::getLocked(tdeElemhead))
    {
      /* --------------------------------------------------------------------------------- */
      /* THE LAST ELEMENT IS LOCKED AND IS THUS REFERENCED BY AN OPERATION RECORD. WE NEED */
      /* TO UPDATE THE OPERATION RECORD WITH THE NEW REFERENCE TO THE ELEMENT.             */
      /* --------------------------------------------------------------------------------- */
      deOperationRecPtr.i = ElementHeader::getOpPtrI(tdeElemhead);
      ndbrequire(oprec_pool.getValidPtr(deOperationRecPtr));
      deOperationRecPtr.p->elementPage = delPageptr.i;
      deOperationRecPtr.p->elementContainer = delConptr;
      deOperationRecPtr.p->elementPointer = delElemptr;
      /*  Writing an invalid value only for sanity, the value should never be read.  */
      jamDebug();
      jamLineDebug(Uint16(lastPageptr.i));
      jamLineDebug(Uint16(lastElemptr));
      lastPageptr.p->word32[lastElemptr] = ElementHeader::setInvalid();
    }//if
    return;
  }

 deleteElement_index_error1:
  arrGuard(lastElemptr, 2048);
  return;

}//Dbacc::deleteElement()

/** ---------------------------------------------------------------------------
 * Find last element in bucket.
 *
 * Shrink container of last element, but keep element words intact.  If
 * container became empty and is not the first container in bucket, unlink it
 * from previous container.
 * 
 * @param[in]      lastPrevpageptr    Page of previous container, if any.
 * @param[in]      tlastPrevconptr    Pointer within page of previous container
 * @param[in,out]  lastPageptr        Page of first container to search, and on
 *                                    return the last container.
 * @param[in,out]  tlastPageindex     Index of container within first page to
 *                                    search, and on return the last container.
 * @param[in,out]  tlastContainerptr  Pointer within page to first container to
 *                                    search, and on return the last container.
 * @param[in,out]  lastIsforward      Direction of first container to search,
 *                                    and on return the last container.
 * @param[out]     tlastElementptr    On return the pointer within page to last
 *                                    element.
 * ------------------------------------------------------------------------ */
void Dbacc::getLastAndRemove(Page8Ptr lastPrevpageptr,
                             Uint32 tlastPrevconptr,
                             Page8Ptr& lastPageptr,
                             Uint32& tlastPageindex,
                             Uint32& tlastContainerptr,
                             bool& lastIsforward,
                             Uint32& tlastElementptr)
{
  /**
   * Should find the last container with same scanbits as the first.
   */
  ContainerHeader containerhead(lastPageptr.p->word32[tlastContainerptr]);
  Uint32 tlastContainerlen = containerhead.getLength();
  /**
   * getLastAndRemove are always called prior delete of element in first
   * container, and that can not be empty.
   */
  ndbassert(tlastContainerlen != Container::HEADER_SIZE);
  const Uint16 activeScanMask = fragrecptr.p->activeScanMask;
  const Uint16 conScanMask = containerhead.getScanBits();
  while (containerhead.getNextEnd() != 0)
  {
    jam();
    Uint32 nextIndex = containerhead.getNextIndexNumber();
    Uint32 nextEnd = containerhead.getNextEnd();
    bool nextOnSamePage = containerhead.isNextOnSamePage();
    Page8Ptr nextPage;
    if (nextOnSamePage)
    {
      nextPage = lastPageptr;
    }
    else
    {
      jam();
      nextPage.i = lastPageptr.p->word32[tlastContainerptr + 1];
      c_page8_pool.getPtr(nextPage);
    }
    const bool nextIsforward = nextEnd == ZLEFT;
    const Uint32 nextConptr = getContainerPtr(nextIndex, nextIsforward);
    const ContainerHeader nextHead(nextPage.p->word32[nextConptr]);
    const Uint16 nextScanMask = nextHead.getScanBits();
    if (((conScanMask ^ nextScanMask) & activeScanMask) != 0)
    {
      /**
       * Next container have different active scan bits,
       * current container is the last one with wanted scan bits.
       * Stop searching!
       */

      ndbassert(((nextScanMask & ~conScanMask) & activeScanMask) == 0);
      break;
    }
    lastPrevpageptr.i = lastPageptr.i;
    lastPrevpageptr.p = lastPageptr.p;
    tlastPrevconptr = tlastContainerptr;
    tlastPageindex = nextIndex;
    if (!nextOnSamePage)
    {
      lastPageptr = nextPage;
    }
    lastIsforward = nextIsforward;
    tlastContainerptr = nextConptr;
    containerhead = lastPageptr.p->word32[tlastContainerptr];
    tlastContainerlen = containerhead.getLength();
    ndbassert(tlastContainerlen >= ((Uint32)Container::HEADER_SIZE + fragrecptr.p->elementLength));
  }
  /**
   * Last container found.
   */
  tlastContainerlen = tlastContainerlen - fragrecptr.p->elementLength;
  if (lastIsforward)
  {
    jam();
    tlastElementptr = tlastContainerptr + tlastContainerlen;
  }
  else
  {
    jam();
    tlastElementptr = (tlastContainerptr + (Container::HEADER_SIZE -
                                            fragrecptr.p->elementLength)) -
                       tlastContainerlen;
  }//if
  if (containerhead.isUsingBothEnds()) {
    /* --------------------------------------------------------------------------------- */
    /*       WE HAVE OWNERSHIP OF BOTH PARTS OF THE CONTAINER ENDS.                      */
    /* --------------------------------------------------------------------------------- */
    if (tlastContainerlen < Container::DOWN_LIMIT) {
      /* --------------------------------------------------------------------------------- */
      /*       WE HAVE DECREASED THE SIZE BELOW THE DOWN LIMIT, WE MUST GIVE UP THE OTHER  */
      /*       SIDE OF THE BUFFER.                                                         */
      /* --------------------------------------------------------------------------------- */
      containerhead.clearUsingBothEnds();
      if (lastIsforward)
      {
        jam();
        Uint32 relconptr = tlastContainerptr +
                           (ZBUF_SIZE - Container::HEADER_SIZE);
        releaseRightlist(lastPageptr, tlastPageindex, relconptr);
      } else {
        jam();
        Uint32 relconptr = tlastContainerptr -
                           (ZBUF_SIZE - Container::HEADER_SIZE);
        releaseLeftlist(lastPageptr, tlastPageindex, relconptr);
      }//if
    }//if
  }//if
  if (tlastContainerlen <= Container::HEADER_SIZE)
  {
    ndbrequire(tlastContainerlen == Container::HEADER_SIZE);
    if (lastPrevpageptr.i != RNIL)
    {
      jam();
      /* --------------------------------------------------------------------------------- */
      /*  THE LAST CONTAINER IS EMPTY AND IS NOT THE FIRST CONTAINER WHICH IS NOT REMOVED. */
      /*  DELETE THE LAST CONTAINER AND UPDATE THE PREVIOUS CONTAINER. ALSO PUT THIS       */
      /*  CONTAINER IN FREE CONTAINER LIST OF THE PAGE.                                    */
      /* --------------------------------------------------------------------------------- */
      ndbrequire(tlastPrevconptr < 2048);
      ContainerHeader prevConhead(lastPrevpageptr.p->word32[tlastPrevconptr]);
      ndbrequire(containerhead.isInUse());
      if (!containerhead.haveNext())
      {
         Uint32 tglrTmp = prevConhead.clearNext();
         lastPrevpageptr.p->word32[tlastPrevconptr] = tglrTmp;
      }
      else
      {
        Uint32 nextPagei = (containerhead.isNextOnSamePage()
                            ? lastPageptr.i
                            : lastPageptr.p->word32[tlastContainerptr+1]);
        Uint32 tglrTmp = prevConhead.setNext(containerhead.getNextEnd(),
                                             containerhead.getNextIndexNumber(),
                                             (nextPagei == lastPrevpageptr.i));
        lastPrevpageptr.p->word32[tlastPrevconptr] = tglrTmp;
        lastPrevpageptr.p->word32[tlastPrevconptr+1] = nextPagei;
      }
      /**
       * Any scans currently scanning the last container must be evicted from
       * container since it is about to be deleted.  Scans will look for next
       * unscanned container at next call to getScanElement.
       */
      if (containerhead.isScanInProgress())
      {
        Uint16 scansInProgress =
            fragrecptr.p->activeScanMask & ~containerhead.getScanBits();
        scansInProgress = lastPageptr.p->checkScans(scansInProgress,
                                                    tlastContainerptr);
        Uint16 scanbit = 1;
        for(int i = 0 ;
            scansInProgress != 0 ;
            i++, scansInProgress>>=1, scanbit<<=1)
        {
          if ((scansInProgress & 1) != 0)
          {
            ScanRecPtr scanPtr;
            scanPtr.i = fragrecptr.p->scan[i];
            ndbrequire(scanRec_pool.getValidPtr(scanPtr));
            scanPtr.p->leaveContainer(lastPageptr.i, tlastContainerptr);
            lastPageptr.p->clearScanContainer(scanbit, tlastContainerptr);
          }
        }
        /**
         * All scans in progress for container are now canceled.
         * No need to call clearScanInProgress for container header since
         * container is about to be released anyway.
         */
      }
      if (lastIsforward)
      {
        jam();
        releaseLeftlist(lastPageptr, tlastPageindex, tlastContainerptr);
      }
      else
      {
        jam();
        releaseRightlist(lastPageptr, tlastPageindex, tlastContainerptr);
      }//if
      return;
    }//if
  }//if
  containerhead.setLength(tlastContainerlen);
  arrGuard(tlastContainerptr, 2048);
  lastPageptr.p->word32[tlastContainerptr] = containerhead;
}//Dbacc::getLastAndRemove()

/* --------------------------------------------------------------------------------- */
/* RELEASE_LEFTLIST                                                                  */
/*       INPUT:                                                                      */
/*               RL_PAGEPTR              PAGE POINTER OF CONTAINER TO BE RELEASED    */
/*               TRL_PAGEINDEX           PAGE INDEX OF CONTAINER TO BE RELEASED      */
/*               TURL_INDEX              INDEX OF CONTAINER TO BE RELEASED           */
/*               TRL_REL_CON             TRUE IF CONTAINER RELEASED OTHERWISE ONLY   */
/*                                       A PART IS RELEASED.                         */
/*                                                                                   */
/*       OUTPUT:                                                                     */
/*               NONE                                                                */
/*                                                                                   */
/*          THE FREE LIST OF LEFT FREE BUFFER IN THE PAGE WILL BE UPDATE             */
/*     TULL_INDEX IS INDEX TO THE FIRST WORD IN THE LEFT SIDE OF THE BUFFER          */
/* --------------------------------------------------------------------------------- */
void Dbacc::releaseLeftlist(Page8Ptr pageptr, Uint32 conidx, Uint32 conptr)
{
  Uint32 tullTmp;
  Uint32 tullTmp1;

  arrGuard(conptr + 1, 2048);
  pageptr.p->word32[conptr + 1] = Container::NO_CONTAINER_INDEX;
  tullTmp1 = (pageptr.p->word32[Page8::EMPTY_LIST] >> 7) & 0x7f;
  arrGuard(conptr, 2048);
  pageptr.p->word32[conptr] = tullTmp1;
  if (tullTmp1 <= Container::MAX_CONTAINER_INDEX) {
    jam();
    tullTmp1 = getForwardContainerPtr(tullTmp1) + 1;
    /* UPDATES PREV POINTER IN THE NEXT FREE */
    pageptr.p->word32[tullTmp1] = conidx;
  } else {
    ndbrequire(tullTmp1 == Container::NO_CONTAINER_INDEX);
  }//if
  tullTmp = pageptr.p->word32[Page8::EMPTY_LIST];
  tullTmp = (((tullTmp >> 14) << 14) | (conidx << 7)) | (tullTmp & 0x7f);
  pageptr.p->word32[Page8::EMPTY_LIST] = tullTmp;
  pageptr.p->word32[Page8::ALLOC_CONTAINERS] =
    pageptr.p->word32[Page8::ALLOC_CONTAINERS] - 1;
  ndbrequire(pageptr.p->word32[Page8::ALLOC_CONTAINERS] <= ZNIL);
  if (((pageptr.p->word32[Page8::EMPTY_LIST] >> ZPOS_PAGE_TYPE_BIT) & 3) == 1) {
    jam();
    c_page8_pool.getPtrForce(pageptr);
    checkoverfreelist(pageptr);
  }//if
}//Dbacc::releaseLeftlist()

/* --------------------------------------------------------------------------------- */
/* RELEASE_RIGHTLIST                                                                 */
/*       INPUT:                                                                      */
/*               RL_PAGEPTR              PAGE POINTER OF CONTAINER TO BE RELEASED    */
/*               TRL_PAGEINDEX           PAGE INDEX OF CONTAINER TO BE RELEASED      */
/*               TURL_INDEX              INDEX OF CONTAINER TO BE RELEASED           */
/*               TRL_REL_CON             TRUE IF CONTAINER RELEASED OTHERWISE ONLY   */
/*                                       A PART IS RELEASED.                         */
/*                                                                                   */
/*       OUTPUT:                                                                     */
/*               NONE                                                                */
/*                                                                                   */
/*         THE FREE LIST OF RIGHT FREE BUFFER IN THE PAGE WILL BE UPDATE.            */
/*         TURL_INDEX IS INDEX TO THE FIRST WORD IN THE RIGHT SIDE OF                */
/*         THE BUFFER, WHICH IS THE LAST WORD IN THE BUFFER.                         */
/* --------------------------------------------------------------------------------- */
void Dbacc::releaseRightlist(Page8Ptr pageptr, Uint32 conidx, Uint32 conptr)
{
  Uint32 turlTmp1;
  Uint32 turlTmp;

  arrGuard(conptr + 1, 2048);
  pageptr.p->word32[conptr + 1] = Container::NO_CONTAINER_INDEX;
  turlTmp1 = pageptr.p->word32[Page8::EMPTY_LIST] & 0x7f;
  arrGuard(conptr, 2048);
  pageptr.p->word32[conptr] = turlTmp1;
  if (turlTmp1 <= Container::MAX_CONTAINER_INDEX) {
    jam();
    turlTmp = getBackwardContainerPtr(turlTmp1) + 1;
    /* UPDATES PREV POINTER IN THE NEXT FREE */
    pageptr.p->word32[turlTmp] = conidx;
  } else {
    ndbrequire(turlTmp1 == Container::NO_CONTAINER_INDEX);
  }//if
  turlTmp = pageptr.p->word32[Page8::EMPTY_LIST];
  pageptr.p->word32[Page8::EMPTY_LIST] = ((turlTmp >> 7) << 7) | conidx;
  pageptr.p->word32[Page8::ALLOC_CONTAINERS] =
    pageptr.p->word32[Page8::ALLOC_CONTAINERS] - 1;
  ndbrequire(pageptr.p->word32[Page8::ALLOC_CONTAINERS] <= ZNIL);
  if (((pageptr.p->word32[Page8::EMPTY_LIST] >> ZPOS_PAGE_TYPE_BIT) & 3) == 1) {
    jam();
    checkoverfreelist(pageptr);
  }//if
}//Dbacc::releaseRightlist()

/* --------------------------------------------------------------------------------- */
/* CHECKOVERFREELIST                                                                 */
/*        INPUT: COL_PAGEPTR, POINTER OF AN OVERFLOW PAGE RECORD.                    */
/*        DESCRIPTION: CHECKS IF THE PAGE HAVE TO PUT IN FREE LIST OF OVER FLOW      */
/*                     PAGES. WHEN IT HAVE TO, AN OVERFLOW REC PTR WILL BE ALLOCATED */
/*                     TO KEEP NFORMATION  ABOUT THE PAGE.                           */
/* --------------------------------------------------------------------------------- */
void Dbacc::checkoverfreelist(Page8Ptr colPageptr)
{
  Uint32 tcolTmp;

// always an overflow page
  tcolTmp = colPageptr.p->word32[Page8::ALLOC_CONTAINERS];
  if (tcolTmp == 0) // Just got empty
  {
    jam();
    releaseOverpage(colPageptr);
  }
  else if (tcolTmp == ZFREE_LIMIT) // Just got sparse
  {
    jam();
    LocalContainerPageList fulllist(c_page8_pool, fragrecptr.p->fullpages);
    LocalContainerPageList sparselist(c_page8_pool, fragrecptr.p->sparsepages);
    fulllist.remove(colPageptr);
    sparselist.addFirst(colPageptr);
  }//if
}//Dbacc::checkoverfreelist()

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/*       END OF DELETE MODULE                                                */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/*       COMMIT AND ABORT MODULE                                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */


/**
 * mark_pending_abort
 *
 * Called when aborting an operation, to mark any dependent operations
 * as pendingAbort.
 * This is useful for handling ABORT and PREPARE concurrency when there
 * are multiple operations on the same row.
 *
 * Dependencies
 *   Within a transaction : 
 *     Later modify operations depend on earlier
 *       modify operations.
 *     Later READ operations may or may not depend
 *       on earlier modify operations
 *       - READs have no state at TUP
 *       - READs may READ older (unaborted) row states
 *       Since we do not know, we abort.
 *     Later operations do not depend on earlier
 *       READ operations
 *   Between transactions : 
 *     There are no abort dependencies
 */
void
Dbacc::mark_pending_abort(OperationrecPtr abortingOp, Uint32 nextParallelOp)
{
  jam();
  const Uint32 abortingOpBits = abortingOp.p->m_op_bits;
  const Uint32 opType = abortingOpBits & Operationrec::OP_MASK;

  /* Only relevant when aborting modifying operations */
  if (opType == ZREAD ||
      opType == ZSCAN_OP)
  {
    jam();
    return;
  }

  if ((abortingOpBits & Operationrec::OP_PENDING_ABORT) != 0)
  {
    jam();
    /**
     * Aborting Op already PENDING_ABORT therefore followers also
     * already PENDING_ABORT
     */
    return;
  }

  ndbassert(abortingOpBits & Operationrec::OP_LOCK_MODE);
  ndbassert(opType == ZINSERT ||
            opType == ZUPDATE ||
            opType == ZDELETE); /* Don't expect WRITE */

  OperationrecPtr follower;
  follower.i = nextParallelOp;
  while (follower.i != RNIL)
  {
    ndbrequire(oprec_pool.getValidPtr(follower));
    if (likely(follower.p->is_same_trans(abortingOp.p)))
    {
      jam();
      if ((follower.p->m_op_bits & Operationrec::OP_PENDING_ABORT) != 0)
      {
        jam();
        /* Found a later op in PENDING_ABORT state - done */
        break;
      }

      follower.p->m_op_bits |= Operationrec::OP_PENDING_ABORT;
    }
    else
    {
      /* Follower is not same trans - unexpected as we hold EX lock */
      dump_lock_queue(follower);
      ndbabort();
    }
    follower.i = follower.p->nextParallelQue;
  }
}


/**
 * checkOpPendingAbort
 *
 * Method called by LQH to check that an op has not 
 * been marked as pending abort by the abort of
 * some other operation
 */
bool
Dbacc::checkOpPendingAbort(Uint32 accConnectPtr) const
{
  OperationrecPtr opPtr;
  opPtr.i = accConnectPtr;
  ndbrequire(oprec_pool.getValidPtr(opPtr));
  
  return ((opPtr.p->m_op_bits & 
           Operationrec::OP_PENDING_ABORT) != 0);
}

/* ------------------------------------------------------------------------- */
/* ABORT_OPERATION                                                           */
/*DESCRIPTION: AN OPERATION RECORD CAN BE IN A LOCK QUEUE OF AN ELEMENT OR   */
/*OWNS THE LOCK. BY THIS SUBROUTINE THE LOCK STATE OF THE OPERATION WILL     */
/*BE CHECKED. THE OPERATION RECORD WILL BE REMOVED FROM THE QUEUE IF IT      */
/*BELONGED TO ANY ONE, OTHERWISE THE ELEMENT HEAD WILL BE UPDATED.           */
/* ------------------------------------------------------------------------- */

/**
 * 
 * P0 - P1 - P2 - P3
 * S0
 * S1
 * S2
 */
void
Dbacc::abortParallelQueueOperation(Signal* signal, OperationrecPtr opPtr)
{
  jam();
  OperationrecPtr nextP;
  OperationrecPtr prevP;
  OperationrecPtr loPtr;

  Uint32 opbits = opPtr.p->m_op_bits;
  Uint32 opstate = opbits & Operationrec::OP_STATE_MASK;
  nextP.i = opPtr.p->nextParallelQue;
  prevP.i = opPtr.p->prevParallelQue;
  loPtr.i = opPtr.p->m_lock_owner_ptr_i;

  ndbassert(! (opbits & Operationrec::OP_LOCK_OWNER));
  ndbassert(opbits & Operationrec::OP_RUN_QUEUE);

  ndbrequire(oprec_pool.getValidPtr(prevP));
  ndbassert(prevP.p->nextParallelQue == opPtr.i);
  prevP.p->nextParallelQue = nextP.i;
  
  if (nextP.i != RNIL)
  {
    ndbrequire(oprec_pool.getValidPtr(nextP));
    ndbassert(nextP.p->prevParallelQue == opPtr.i);
    nextP.p->prevParallelQue = prevP.i;
  }
  else if (prevP.i != loPtr.i)
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(loPtr));
    ndbassert(loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
    ndbassert(loPtr.p->m_lo_last_parallel_op_ptr_i == opPtr.i);
    loPtr.p->m_lo_last_parallel_op_ptr_i = prevP.i;
    prevP.p->m_lock_owner_ptr_i = loPtr.i;
    
    /**
     * Abort P3...check start next
     */
    startNext(signal, prevP);
    validate_lock_queue(prevP);
    return;
  }
  else
  {
    jam();
    /**
     * P0 - P1
     *
     * Abort P1, check start next
     */
    ndbassert(prevP.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
    prevP.p->m_lo_last_parallel_op_ptr_i = RNIL;
    startNext(signal, prevP);
    validate_lock_queue(prevP);
    return;
  }

  /**
   * This op is not at the end of the parallel queue, so 
   * mark pending aborts there as necessary
   */
  mark_pending_abort(opPtr, nextP.i);
         
  /**
   * Abort P1/P2
   */
  if (opbits & Operationrec::OP_LOCK_MODE)
  {
    Uint32 nextbits = nextP.p->m_op_bits;
    while ((nextbits & Operationrec::OP_LOCK_MODE) == 0)
    {
      ndbassert(nextbits & Operationrec::OP_ACC_LOCK_MODE);
      nextbits &= ~(Uint32)Operationrec::OP_ACC_LOCK_MODE;
      nextP.p->m_op_bits = nextbits;
      
      if (nextP.p->nextParallelQue != RNIL)
      {
	nextP.i = nextP.p->nextParallelQue;
        ndbrequire(oprec_pool.getValidPtr(nextP));
	nextbits = nextP.p->m_op_bits;
      }
      else
      {
	break;
      }
    }
  }

  /**
   * Abort P1, P2
   */
  if (opstate == Operationrec::OP_STATE_RUNNING)
  {
    jam();
    startNext(signal, prevP);
    validate_lock_queue(prevP);
    return;
  }
  
  ndbassert(opstate == Operationrec::OP_STATE_EXECUTED ||
	    opstate == Operationrec::OP_STATE_WAITING);
  
  /**
   * Scan to last of run queue
   */
  while (nextP.p->nextParallelQue != RNIL)
  {
    jam();
    nextP.i = nextP.p->nextParallelQue;
    ndbrequire(oprec_pool.getValidPtr(nextP));
  }

#if defined(VM_TRACE) || defined(ERROR_INSERT)
  loPtr.i = nextP.p->m_lock_owner_ptr_i;
  ndbrequire(oprec_pool.getValidPtr(loPtr));
  ndbassert(loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
  ndbassert(loPtr.p->m_lo_last_parallel_op_ptr_i == nextP.i);
#endif

  startNext(signal, nextP);
  validate_lock_queue(nextP);
  
  return;
}

void 
Dbacc::abortSerieQueueOperation(Signal* signal, OperationrecPtr opPtr)
{
  jam();
  OperationrecPtr prevS, nextS;
  OperationrecPtr prevP, nextP;
  OperationrecPtr loPtr;

  Uint32 opbits = opPtr.p->m_op_bits;

  prevS.i = opPtr.p->prevSerialQue;
  nextS.i = opPtr.p->nextSerialQue;

  prevP.i = opPtr.p->prevParallelQue;
  nextP.i = opPtr.p->nextParallelQue;

  ndbassert((opbits & Operationrec::OP_LOCK_OWNER) == 0);
  ndbassert((opbits & Operationrec::OP_RUN_QUEUE) == 0);

  {
    FragmentrecPtr frp;
    frp.i = opPtr.p->fragptr;
    ptrCheckGuard(frp, cfragmentsize, fragmentrec);

    frp.p->m_lockStats.wait_fail((opbits & 
                                  Operationrec::OP_LOCK_MODE) 
                                 != ZREADLOCK,
                                 opPtr.p->m_lockTime,
                                 getHighResTimer());
  }
  
  if (prevP.i != RNIL)
  {
    /**
     * We're not list head...
     */
    ndbrequire(oprec_pool.getValidPtr(prevP));
    ndbassert(prevP.p->nextParallelQue == opPtr.i);
    prevP.p->nextParallelQue = nextP.i;

    if (nextP.i != RNIL)
    {
      ndbrequire(oprec_pool.getValidPtr(nextP));
      ndbassert(nextP.p->prevParallelQue == opPtr.i);
      ndbassert((nextP.p->m_op_bits & Operationrec::OP_STATE_MASK) == 
		Operationrec::OP_STATE_WAITING);
      nextP.p->prevParallelQue = prevP.i;
      
      if ((prevP.p->m_op_bits & Operationrec::OP_ACC_LOCK_MODE) == 0 &&
	  opbits & Operationrec::OP_LOCK_MODE)
      {
	/**
	 * Scan right in parallel queue to fix OP_ACC_LOCK_MODE
	 */
	while ((nextP.p->m_op_bits & Operationrec::OP_LOCK_MODE) == 0)
	{
	  ndbassert(nextP.p->m_op_bits & Operationrec::OP_ACC_LOCK_MODE);
	  nextP.p->m_op_bits &= ~(Uint32)Operationrec::OP_ACC_LOCK_MODE;
	  nextP.i = nextP.p->nextParallelQue;
	  if (nextP.i == RNIL)
	    break;
          ndbrequire(oprec_pool.getValidPtr(nextP));
	}
      }
    }
    validate_lock_queue(prevP);
    return;
  }
  else
  {
    /**
     * We're a list head
     */
    ndbrequire(oprec_pool.getValidPtr(prevS));
    ndbassert(prevS.p->nextSerialQue == opPtr.i);
    
    if (nextP.i != RNIL)
    {
      /**
       * Promote nextP to list head
       */
      ndbrequire(oprec_pool.getValidPtr(nextP));
      ndbassert(nextP.p->prevParallelQue == opPtr.i);
      prevS.p->nextSerialQue = nextP.i;
      nextP.p->prevParallelQue = RNIL;
      nextP.p->nextSerialQue = nextS.i;
      if (nextS.i != RNIL)
      {
	jam();
        ndbrequire(oprec_pool.getValidPtr(nextS));
	ndbassert(nextS.p->prevSerialQue == opPtr.i);
	nextS.p->prevSerialQue = nextP.i;
	validate_lock_queue(prevS);
	return;
      }
      else
      {
	// nextS is RNIL, i.e we're last in serial queue...
	// we must update lockOwner.m_lo_last_serial_op_ptr_i
	loPtr = prevS;
	while ((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0)
	{
	  loPtr.i = loPtr.p->prevSerialQue;
          ndbrequire(oprec_pool.getValidPtr(loPtr));
	}
	ndbassert(loPtr.p->m_lo_last_serial_op_ptr_i == opPtr.i);
	loPtr.p->m_lo_last_serial_op_ptr_i = nextP.i;
	validate_lock_queue(loPtr);
	return;
      }
    }
    
    if (nextS.i == RNIL)
    {
      /**
       * Abort S2
       */

      // nextS is RNIL, i.e we're last in serial queue...
      // and we have no parallel queue, 
      // we must update lockOwner.m_lo_last_serial_op_ptr_i
      prevS.p->nextSerialQue = RNIL;
      
      loPtr = prevS;
      while ((loPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) == 0)
      {
	loPtr.i = loPtr.p->prevSerialQue;
        ndbrequire(oprec_pool.getValidPtr(loPtr));
      }
      ndbassert(loPtr.p->m_lo_last_serial_op_ptr_i == opPtr.i);
      if (prevS.i != loPtr.i)
      {
	jam();
	loPtr.p->m_lo_last_serial_op_ptr_i = prevS.i;
      }
      else
      {
	loPtr.p->m_lo_last_serial_op_ptr_i = RNIL;
      }
      validate_lock_queue(loPtr);
    }
    else if (nextP.i == RNIL)
    {
      ndbrequire(oprec_pool.getValidPtr(nextS));
      ndbassert(nextS.p->prevSerialQue == opPtr.i);
      prevS.p->nextSerialQue = nextS.i;
      nextS.p->prevSerialQue = prevS.i;
      
      if (prevS.p->m_op_bits & Operationrec::OP_LOCK_OWNER)
      {
	/**
	 * Abort S0
	 */
	OperationrecPtr lastOp;
	lastOp.i = prevS.p->m_lo_last_parallel_op_ptr_i;
	if (lastOp.i != RNIL)
	{
	  jam();
          ndbrequire(oprec_pool.getValidPtr(lastOp));
	  ndbassert(lastOp.p->m_lock_owner_ptr_i == prevS.i);
	}
	else
	{
	  jam();
	  lastOp = prevS;
	}
	startNext(signal, lastOp);
	validate_lock_queue(lastOp);
      }
      else
      {
	validate_lock_queue(prevS);
      }
    }
  }
}


void Dbacc::abortOperation(Signal* signal)
{
  Uint32 opbits = operationRecPtr.p->m_op_bits;

  validate_lock_queue(operationRecPtr);

  if (opbits & Operationrec::OP_LOCK_OWNER) 
  {
    /**
     * We only need to protect changes when the lock owner aborts or
     * commits, this is to ensure that the state of the operation
     * that is linked to the hash index doesn't change while a query
     * thread is reading it. This could cause the query thread to
     * consider a row deleted which isn't and vice versa.
     */
    acquire_frag_mutex_hash(fragrecptr.p, operationRecPtr);
    fragrecptr.p->lockCount--;
    opbits &= ~(Uint32)Operationrec::OP_LOCK_OWNER;
    if (opbits & Operationrec::OP_INSERT_IS_DONE)
    { 
      jam();
      opbits |= Operationrec::OP_ELEMENT_DISAPPEARED;
    }//if
    operationRecPtr.p->m_op_bits = opbits;
    const bool queue = (operationRecPtr.p->nextParallelQue != RNIL ||
			operationRecPtr.p->nextSerialQue != RNIL);
    
    if (queue)
    {
      jam();
      mark_pending_abort(operationRecPtr, operationRecPtr.p->nextParallelQue);
      release_lockowner(signal, operationRecPtr, false);
    } 
    else 
    {
      /* -------------------------------------------------------------------
       * WE ARE OWNER OF THE LOCK AND NO OTHER OPERATIONS ARE QUEUED. 
       * IF INSERT OR STANDBY WE DELETE THE ELEMENT OTHERWISE WE REMOVE 
       * THE LOCK FROM THE ELEMENT.
       * ------------------------------------------------------------------ */
      if ((opbits & Operationrec::OP_ELEMENT_DISAPPEARED) == 0)
      {
        jam();
	Page8Ptr aboPageidptr;
	Uint32 taboElementptr;
	Uint32 tmp2Olq;

        taboElementptr = operationRecPtr.p->elementPointer;
        aboPageidptr.i = operationRecPtr.p->elementPage;
        ndbassert(!operationRecPtr.p->localdata.isInvalid());
        tmp2Olq = ElementHeader::setUnlocked(
                      operationRecPtr.p->localdata.m_page_idx,
                      operationRecPtr.p->reducedHashValue);
        c_page8_pool.getPtr(aboPageidptr);
        arrGuard(taboElementptr, 2048);
        aboPageidptr.p->word32[taboElementptr] = tmp2Olq;
        release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
        return;
      } 
      else 
      {
        jam();
        commitdelete(signal);
      }//if
    }//if
    release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
  }
  else if (opbits & Operationrec::OP_RUN_QUEUE)
  {
    abortParallelQueueOperation(signal, operationRecPtr);
  }
  else
  {
    abortSerieQueueOperation(signal, operationRecPtr);
  }
}

void
Dbacc::commitDeleteCheck(Signal* signal)
{
  OperationrecPtr opPtr;
  OperationrecPtr lastOpPtr;
  OperationrecPtr deleteOpPtr;
  Uint32 elementDeleted = 0;
  bool deleteCheckOngoing = true;
  LHBits32 hashValue;
  lastOpPtr = operationRecPtr;
  opPtr.i = operationRecPtr.p->nextParallelQue;
  while (opPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(opPtr));
    lastOpPtr = opPtr;
    opPtr.i = opPtr.p->nextParallelQue;
  }//while
  deleteOpPtr = lastOpPtr;
  do {
    Uint32 opbits = deleteOpPtr.p->m_op_bits;
    Uint32 op = opbits & Operationrec::OP_MASK;
    if (op == ZDELETE) {
      jam();
      /* -------------------------------------------------------------------
       * IF THE CURRENT OPERATION TO BE COMMITTED IS A DELETE OPERATION DUE TO
       * A SCAN-TAKEOVER THE ACTUAL DELETE WILL BE PERFORMED BY THE PREVIOUS 
       * OPERATION (SCAN) IN THE PARALLEL QUEUE WHICH OWNS THE LOCK.
       * THE PROBLEM IS THAT THE SCAN OPERATION DOES NOT HAVE A HASH VALUE 
       * ASSIGNED TO IT SO WE COPY IT FROM THIS OPERATION.
       *
       * WE ASSUME THAT THIS SOLUTION WILL WORK BECAUSE THE ONLY WAY A 
       * SCAN CAN PERFORM A DELETE IS BY BEING FOLLOWED BY A NORMAL 
       * DELETE-OPERATION THAT HAS A HASH VALUE.
       * ----------------------------------------------------------------- */
      hashValue = deleteOpPtr.p->hashValue;
      elementDeleted = Operationrec::OP_ELEMENT_DISAPPEARED;
      deleteCheckOngoing = false;
    } else if (op == ZREAD || op == ZSCAN_OP) {
      /* -------------------------------------------------------------------
       * We are trying to find out whether the commit will in the end delete 
       * the tuple. Normally the delete will be the last operation in the 
       * list of operations on this. It is however possible to issue reads 
       * and scans in the same savepoint as the delete operation was issued 
       * and these can end up after the delete in the list of operations 
       * in the parallel queue. Thus if we discover a read or a scan 
       * we have to continue scanning the list looking for a delete operation.
       */
      deleteOpPtr.i = deleteOpPtr.p->prevParallelQue;
      if (opbits & Operationrec::OP_LOCK_OWNER) {
        jam();
        deleteCheckOngoing = false;
      } else {
        jam();
        ndbrequire(oprec_pool.getValidPtr(deleteOpPtr));
      }//if
    } else {
      jam();
      /* ------------------------------------------------------------------ */
      /* Finding an UPDATE or INSERT before finding a DELETE 
       * means we cannot be deleting as the end result of this transaction.
       */
      deleteCheckOngoing = false;
    }//if
  } while (deleteCheckOngoing);
  opPtr = lastOpPtr;
  do {
    jam();
    opPtr.p->m_op_bits |= Operationrec::OP_COMMIT_DELETE_CHECK;
    if (elementDeleted) {
      jam();
      /* All pending dealloc operations are marked and reported to LQH */
      opPtr.p->m_op_bits |= elementDeleted;
      opPtr.p->hashValue = hashValue;
      report_pending_dealloc(signal, opPtr.p, deleteOpPtr.p);
    }//if
    opPtr.i = opPtr.p->prevParallelQue;
    if (opPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) {
      jam();
      break;
    }//if
    ndbrequire(oprec_pool.getValidPtr(opPtr));
  } while (true);
}//Dbacc::commitDeleteCheck()

/* ------------------------------------------------------------------------- */
/* COMMIT_OPERATION                                                          */
/* INPUT: OPERATION_REC_PTR, POINTER TO AN OPERATION RECORD                  */
/* DESCRIPTION: THE OPERATION RECORD WILL BE TAKE OUT OF ANY LOCK QUEUE.     */
/*         IF IT OWNS THE ELEMENT LOCK. HEAD OF THE ELEMENT WILL BE UPDATED. */
/* ------------------------------------------------------------------------- */
void Dbacc::commitOperation(Signal* signal)
{
  validate_lock_queue(operationRecPtr);

  Uint32 opbits = operationRecPtr.p->m_op_bits;
  Uint32 op = opbits & Operationrec::OP_MASK;
  ndbrequire((opbits & Operationrec::OP_STATE_MASK) == Operationrec::OP_STATE_EXECUTED);
  ndbrequire(((opbits & Operationrec::OP_PENDING_ABORT) == 0) ||
             (op == ZSCAN_OP) || (op == ZREAD)); // Scan commits to unlock/abort

  if ((opbits & Operationrec::OP_COMMIT_DELETE_CHECK) == 0 &&
      (op != ZREAD && op != ZSCAN_OP))
  {
    jam();
    /*  This method is used to check whether the end result of the transaction
        will be to delete the tuple. In this case all operation will be marked
        with elementIsDisappeared = true to ensure that the last operation
        committed will remove the tuple. We only run this once per transaction
        (commitDeleteCheckFlag = true if performed earlier) and we don't
        execute this code when committing a scan operation since committing
        a scan operation only means that the scan is continuing and the scan
        lock is released.
    */
    commitDeleteCheck(signal);
    opbits = operationRecPtr.p->m_op_bits;
  }//if

  ndbassert(opbits & Operationrec::OP_RUN_QUEUE);
  
  if (opbits & Operationrec::OP_LOCK_OWNER) 
  {
    jam();
    acquire_frag_mutex_hash(fragrecptr.p, operationRecPtr);
    fragrecptr.p->lockCount--;
    opbits &= ~(Uint32)Operationrec::OP_LOCK_OWNER;
    operationRecPtr.p->m_op_bits = opbits;
    
    const bool queue = (operationRecPtr.p->nextParallelQue != RNIL ||
			operationRecPtr.p->nextSerialQue != RNIL);
    
    if (!queue && (opbits & Operationrec::OP_ELEMENT_DISAPPEARED) == 0) 
    {
      jam();
      /* 
       * This is the normal path through the commit for operations owning the
       * lock without any queues and not a delete operation.
       */
      Page8Ptr coPageidptr;
      Uint32 tcoElementptr;
      Uint32 tmp2Olq;
      
      coPageidptr.i = operationRecPtr.p->elementPage;
      tcoElementptr = operationRecPtr.p->elementPointer;
      ndbassert(!operationRecPtr.p->localdata.isInvalid());
      tmp2Olq = ElementHeader::setUnlocked(
                    operationRecPtr.p->localdata.m_page_idx,
                    operationRecPtr.p->reducedHashValue);
      c_page8_pool.getPtr(coPageidptr);
      arrGuard(tcoElementptr, 2048);
      coPageidptr.p->word32[tcoElementptr] = tmp2Olq;
      release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
      return;
    }
    else if (queue)
    {
      jam();
      /*
       * The case when there is a queue lined up.
       * Release the lock and pass it to the next operation lined up.
       */
      release_lockowner(signal, operationRecPtr, true);
      release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
      return;
    } 
    else 
    {
      jam();
      /*
       * No queue and elementIsDisappeared is true. 
       * We perform the actual delete operation.
       */
      commitdelete(signal);
      release_frag_mutex_hash(fragrecptr.p, operationRecPtr);
      return;
    }//if
  } 
  else 
  {
    /**
     * THE OPERATION DOES NOT OWN THE LOCK. IT MUST BE IN A LOCK QUEUE OF THE
     * ELEMENT.
     */
    jam();
    OperationrecPtr prev, next, lockOwner;
    prev.i = operationRecPtr.p->prevParallelQue;
    next.i = operationRecPtr.p->nextParallelQue;
    lockOwner.i = operationRecPtr.p->m_lock_owner_ptr_i;
    ndbrequire(oprec_pool.getValidPtr(prev));
    
    prev.p->nextParallelQue = next.i;
    if (next.i != RNIL) 
    {
      jam();
      ndbrequire(oprec_pool.getValidPtr(next));
      next.p->prevParallelQue = prev.i;
    }
    else if (prev.p->m_op_bits & Operationrec::OP_LOCK_OWNER)
    {
      jam();
      ndbassert(lockOwner.i == prev.i);
      prev.p->m_lo_last_parallel_op_ptr_i = RNIL;
      next = prev;
    }
    else
    {
      jam();
      /**
       * Last operation in parallel queue
       */
      ndbassert(prev.i != lockOwner.i);
      ndbrequire(oprec_pool.getValidPtr(lockOwner));
      ndbassert(lockOwner.p->m_op_bits & Operationrec::OP_LOCK_OWNER);
      lockOwner.p->m_lo_last_parallel_op_ptr_i = prev.i;
      prev.p->m_lock_owner_ptr_i = lockOwner.i;
      next = prev;
    }
    
    /**
     * Check possible lock upgrade
     */
    if(opbits & Operationrec::OP_ACC_LOCK_MODE)
    {
      jam();

      /**
       * Not lock owner...committing a exclusive operation...
       *
       * e.g
       *   T1(R) T1(X)
       *   T2(R/X)
       *
       *   If T1(X) commits T2(R/X) is not supposed to run
       *     as T1(R) should also commit
       *
       * e.g
       *   T1(R) T1(X) T1*(R)
       *   T2(R/X)
       *
       *   If T1*(R) commits T2(R/X) is not supposed to run
       *     as T1(R),T2(x) should also commit
       */
      validate_lock_queue(prev);
      return;
    }

    /**
     * We committed a shared lock
     *   Check if we can start next...
     */
    while(next.p->nextParallelQue != RNIL)
    {
      jam();
      next.i = next.p->nextParallelQue;
      ndbrequire(oprec_pool.getValidPtr(next));
      
      if ((next.p->m_op_bits & Operationrec::OP_STATE_MASK) != 
	  Operationrec::OP_STATE_EXECUTED)
      {
	jam();
	return;
      }
    }
    
    startNext(signal, next);
    
    validate_lock_queue(prev);
  }
}//Dbacc::commitOperation()

void 
Dbacc::release_lockowner(Signal* signal, OperationrecPtr opPtr, bool commit)
{
  OperationrecPtr nextP;
  OperationrecPtr nextS;
  OperationrecPtr newOwner;
  OperationrecPtr lastP;
  
  Uint32 opbits = opPtr.p->m_op_bits;
  nextP.i = opPtr.p->nextParallelQue;
  nextS.i = opPtr.p->nextSerialQue;
  lastP.i = opPtr.p->m_lo_last_parallel_op_ptr_i;
  Uint32 lastS = opPtr.p->m_lo_last_serial_op_ptr_i;

  ndbassert(lastP.i != RNIL || lastS != RNIL);
  ndbassert(nextP.i != RNIL || nextS.i != RNIL);

  enum {
    NOTHING,
    CHECK_LOCK_UPGRADE,
    START_NEW
  } action = NOTHING;

  if (nextP.i != RNIL)
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(nextP));
    newOwner = nextP;

    if (lastP.i == newOwner.i)
    {
      newOwner.p->m_lo_last_parallel_op_ptr_i = RNIL;
      lastP = nextP;
    }
    else
    {
      ndbrequire(oprec_pool.getValidPtr(lastP));
      newOwner.p->m_lo_last_parallel_op_ptr_i = lastP.i;
      lastP.p->m_lock_owner_ptr_i = newOwner.i;
    }
    
    newOwner.p->m_lo_last_serial_op_ptr_i = lastS;
    newOwner.p->nextSerialQue = nextS.i;
    
    if (nextS.i != RNIL)
    {
      jam();
      ndbrequire(oprec_pool.getValidPtr(nextS));
      ndbassert(nextS.p->prevSerialQue == opPtr.i);
      nextS.p->prevSerialQue = newOwner.i;
    }
    
    if (commit)
    {
      if ((opbits & Operationrec::OP_ACC_LOCK_MODE) == ZREADLOCK)
      {
	jam();
	/**
	 * Lock owner...committing a shared operation...
	 * this can be a lock upgrade
	 *
	 * e.g
	 *   T1(R) T2(R)
	 *   T2(X)
	 *
	 *   If T1(R) commits T2(X) is supposed to run
	 *
	 * e.g
	 *   T1(X) T1(R)
	 *   T2(R)
	 *
	 *   If T1(X) commits, then T1(R) _should_ commit before T2(R) is
	 *     allowed to proceed
	 */
	action = CHECK_LOCK_UPGRADE;
      }
      else
      {
	jam();
	newOwner.p->m_op_bits |= Operationrec::OP_LOCK_MODE;
      }
    }
    else
    {
      /**
       * Aborting an operation can *always* lead to lock upgrade
       */
      action = CHECK_LOCK_UPGRADE;
      Uint32 opstate = opbits & Operationrec::OP_STATE_MASK;
      if (opstate != Operationrec::OP_STATE_EXECUTED)
      {
	ndbassert(opstate == Operationrec::OP_STATE_RUNNING);
	if (opbits & Operationrec::OP_ELEMENT_DISAPPEARED)
	{
	  jam();
          trigger_dealloc(signal, opPtr.p);
	  newOwner.p->localdata.setInvalid();
	}
	else
	{
	  jam();
	  newOwner.p->localdata = opPtr.p->localdata;
	}
	action = START_NEW;
      }
      
      /**
       * Update ACC_LOCK_MODE
       */
      if (opbits & Operationrec::OP_LOCK_MODE)
      {
	Uint32 nextbits = nextP.p->m_op_bits;
	while ((nextbits & Operationrec::OP_LOCK_MODE) == 0)
	{
	  ndbassert(nextbits & Operationrec::OP_ACC_LOCK_MODE);
	  nextbits &= ~(Uint32)Operationrec::OP_ACC_LOCK_MODE;
	  nextP.p->m_op_bits = nextbits;
	  
	  if (nextP.p->nextParallelQue != RNIL)
	  {
	    nextP.i = nextP.p->nextParallelQue;
            ndbrequire(oprec_pool.getValidPtr(nextP));
	    nextbits = nextP.p->m_op_bits;
	  }
	  else
	  {
	    break;
	  }
	}
      }
    }
  }
  else
  {
    jam();
    ndbrequire(oprec_pool.getValidPtr(nextS));
    newOwner = nextS;
    
    newOwner.p->m_op_bits |= Operationrec::OP_RUN_QUEUE;
    
    if (opbits & Operationrec::OP_ELEMENT_DISAPPEARED)
    {
      trigger_dealloc(signal, opPtr.p);
      newOwner.p->localdata.setInvalid();
    }
    else
    {
      jam();
      newOwner.p->localdata = opPtr.p->localdata;
    }
    
    lastP = newOwner;
    while (lastP.p->nextParallelQue != RNIL)
    {
      lastP.i = lastP.p->nextParallelQue;
      ndbrequire(oprec_pool.getValidPtr(lastP));
      lastP.p->m_op_bits |= Operationrec::OP_RUN_QUEUE;
    }
    
    if (newOwner.i != lastP.i)
    {
      jam();
      newOwner.p->m_lo_last_parallel_op_ptr_i = lastP.i;
    }
    else
    {
      jam();
      newOwner.p->m_lo_last_parallel_op_ptr_i = RNIL;
    }

    if (newOwner.i != lastS)
    {
      jam();
      newOwner.p->m_lo_last_serial_op_ptr_i = lastS;
    }
    else
    {
      jam();
      newOwner.p->m_lo_last_serial_op_ptr_i = RNIL;
    }
    
    action = START_NEW;
  }
  
  fragrecptr.p->lockCount++;
  newOwner.p->m_op_bits |= Operationrec::OP_LOCK_OWNER;
  
  /**
   * Copy op info, and store op in element
   *
   */
  {
    newOwner.p->elementPage = opPtr.p->elementPage;
    newOwner.p->elementPointer = opPtr.p->elementPointer;
    newOwner.p->elementContainer = opPtr.p->elementContainer;
    newOwner.p->reducedHashValue = opPtr.p->reducedHashValue;
    newOwner.p->m_op_bits |= (opbits & Operationrec::OP_ELEMENT_DISAPPEARED);
    if (opbits & Operationrec::OP_ELEMENT_DISAPPEARED)
    {
      /* ------------------------------------------------------------------- */
      // If the elementIsDisappeared is set then we know that the 
      // hashValue is also set since it always originates from a 
      // committing abort or a aborting insert. 
      // Scans do not initialise the hashValue and must have this 
      // value initialised if they are
      // to successfully commit the delete.
      /* ------------------------------------------------------------------- */
      jam();
      newOwner.p->hashValue = opPtr.p->hashValue;
    }//if

    Page8Ptr pagePtr;
    pagePtr.i = newOwner.p->elementPage;
    c_page8_pool.getPtr(pagePtr);
    const Uint32 tmp = ElementHeader::setLocked(newOwner.i);
    arrGuard(newOwner.p->elementPointer, 2048);
    pagePtr.p->word32[newOwner.p->elementPointer] = tmp;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    /**
     * Invalidate page number in elements second word for test in initScanOp
     */
    if (newOwner.p->localdata.isInvalid())
    {
      pagePtr.p->word32[newOwner.p->elementPointer + 1] =
        newOwner.p->localdata.m_page_no;
    }
    else
    {
      ndbrequire(newOwner.p->localdata.m_page_no ==
                   pagePtr.p->word32[newOwner.p->elementPointer+1]);
    }
#endif
  }
  
  switch(action){
  case NOTHING:
    validate_lock_queue(newOwner);
    return;
  case START_NEW:
    startNew(signal, newOwner);
    validate_lock_queue(newOwner);
    return;
  case CHECK_LOCK_UPGRADE:
    startNext(signal, lastP);
    validate_lock_queue(lastP);
    break;
  }
  
}

void
Dbacc::startNew(Signal* signal, OperationrecPtr newOwner)
{
  OperationrecPtr save = operationRecPtr;
  operationRecPtr = newOwner;
  
  Uint32 opbits = newOwner.p->m_op_bits;
  Uint32 op = opbits & Operationrec::OP_MASK;
  ndbassert((opbits & Operationrec::OP_STATE_MASK) ==
             Operationrec::OP_STATE_WAITING);
  ndbassert(opbits & Operationrec::OP_LOCK_OWNER);
  const bool deleted = opbits & Operationrec::OP_ELEMENT_DISAPPEARED;
  Uint32 errCode = 0;

  opbits &= opbits & ~(Uint32)Operationrec::OP_STATE_MASK;
  opbits |= Operationrec::OP_STATE_RUNNING;
  
  if (op == ZSCAN_OP && (opbits & Operationrec::OP_LOCK_REQ) == 0)
    goto scan;

  /* Waiting op now runnable... */
  {
    FragmentrecPtr frp;
    frp.i = newOwner.p->fragptr;
    ptrCheckGuard(frp, cfragmentsize, fragmentrec);
    frp.p->m_lockStats.wait_ok((opbits & Operationrec::OP_LOCK_MODE) 
                               != ZREADLOCK,
                               operationRecPtr.p->m_lockTime,
                               getHighResTimer());
  }

  if (deleted)
  {
    jam();
    if (op != ZINSERT && op != ZWRITE)
    {
      errCode = ZREAD_ERROR;
      goto ref;
    }
    
    opbits &= ~(Uint32)Operationrec::OP_MASK;
    opbits &= ~(Uint32)Operationrec::OP_ELEMENT_DISAPPEARED;
    opbits |= (op = ZINSERT);
    opbits |= Operationrec::OP_INSERT_IS_DONE;
    goto conf;
  }
  else if (op == ZINSERT)
  {
    jam();
    errCode = ZWRITE_ERROR;
    goto ref;
  }
  else if (op == ZWRITE)
  {
    jam();
    opbits &= ~(Uint32)Operationrec::OP_MASK;
    opbits |= (op = ZUPDATE);
    goto conf;
  }

conf:
  newOwner.p->m_op_bits = opbits;

  sendAcckeyconf(signal);
  sendSignal(newOwner.p->userblockref, GSN_ACCKEYCONF, 
	     signal, 6, JBB);

  operationRecPtr = save;
  return;
  
scan:
  jam();
  newOwner.p->m_op_bits = opbits;
  
  takeOutScanLockQueue(newOwner.p->scanRecPtr);
  putReadyScanQueue(newOwner.p->scanRecPtr);

  operationRecPtr = save;
  return;
  
ref:
  newOwner.p->m_op_bits = opbits;
  
  signal->theData[0] = newOwner.p->userptr;
  signal->theData[1] = errCode;
  sendSignal(newOwner.p->userblockref, GSN_ACCKEYREF, signal, 
	     2, JBB);
  
  operationRecPtr = save;
  return;
}

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF COMMIT AND ABORT MODULE                                              */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* ALLOC_OVERFLOW_PAGE                                                               */
/*          DESCRIPTION:                                                             */
/* --------------------------------------------------------------------------------- */
bool
Dbacc::get_lock_information(Dbacc **acc_block, Dblqh** lqh_block)
{
  bool lock_flag = false;
  if (m_is_query_block)
  {
    Uint32 instanceNo = c_lqh->m_current_ldm_instance;
    ndbrequire(instanceNo != 0);
    *acc_block = (Dbacc*) globalData.getBlock(DBACC, instanceNo);
    *lqh_block = (Dblqh*) globalData.getBlock(DBLQH, instanceNo);
    ndbrequire(!(*lqh_block)->is_restore_phase_done());
    lock_flag = true;
  }
  else
  {
    (*acc_block) = this;
    (*lqh_block) = c_lqh;
    if (!c_lqh->is_restore_phase_done() &&
        (globalData.ndbMtRecoverThreads +
         globalData.ndbMtQueryThreads) > 0)
    {
      lock_flag = true;
    }
  }
  return lock_flag;
}

Uint32
Dbacc::seizePage_lock(Page8Ptr& spPageptr, int sub_page_id)
{
  Dblqh *lqh_block;
  Dbacc *acc_block;
  bool lock_flag = get_lock_information(&acc_block, &lqh_block);
  if (lock_flag)
  {
    NdbMutex_Lock(lqh_block->m_lock_acc_page_mutex);
  }
  Uint32 result = acc_block->seizePage(spPageptr,
                                       Page32Lists::ANY_SUB_PAGE,
                                       c_allow_use_of_spare_pages,
                                       fragrecptr,
                                       jamBuffer());
  if (lock_flag)
  {
    NdbMutex_Unlock(lqh_block->m_lock_acc_page_mutex);
  }
  return result;
}

Uint32 Dbacc::allocOverflowPage()
{
  Page8Ptr spPageptr;
  Uint32 result = seizePage_lock(spPageptr, Page32Lists::ANY_SUB_PAGE);
  if (result > ZLIMIT_OF_ERROR)
  {
    return result;
  }
  {
    LocalContainerPageList sparselist(c_page8_pool, fragrecptr.p->sparsepages);
    sparselist.addLast(spPageptr);
  }
  initOverpage(spPageptr);
  return 0;
}//Dbacc::allocOverflowPage()

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       EXPAND/SHRINK MODULE                                                        */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/*EXPANDCHECK                                        EXPAND BUCKET ORD               */
/* SENDER: ACC,    LEVEL B         */
/*   INPUT:   FRAGRECPTR, POINTS TO A FRAGMENT RECORD.                               */
/*   DESCRIPTION: A BUCKET OF A FRAGMENT PAGE WILL BE EXPAND INTO TWO BUCKETS        */
/*                                 ACCORDING TO LH3.                                 */
/* ******************--------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/* EXPANDCHECK                                        EXPAND BUCKET ORD              */
/* ******************------------------------------+                                 */
/* SENDER: ACC,    LEVEL B         */
/* A BUCKET OF THE FRAGMENT WILL   */
/* BE EXPANDED ACCORDING TO LH3,   */
/* AND COMMIT TRANSACTION PROCESS  */
/* WILL BE CONTINUED */
Uint32 Dbacc::checkScanExpand(Uint32 splitBucket)
{
  Uint32 Ti;
  Uint32 TreturnCode = 0;
  Uint32 TPageIndex;
  Uint32 TDirInd;
  Uint32 TSplit;
  Uint32 TreleaseScanBucket;
  Page8Ptr TPageptr;
  ScanRecPtr TscanPtr;
  Uint16 releaseScanMask = 0;

  TSplit = splitBucket;
  for (Ti = 0; Ti < MAX_PARALLEL_SCANS_PER_FRAG; Ti++)
  {
    if (fragrecptr.p->scan[Ti] != RNIL)
    {
      //-------------------------------------------------------------
      // A scan is ongoing on this particular local fragment. We have
      // to check its current state.
      //-------------------------------------------------------------
      TscanPtr.i = fragrecptr.p->scan[Ti];
      ndbrequire(scanRec_pool.getValidPtr(TscanPtr));
      if (TscanPtr.p->activeLocalFrag == fragrecptr.i) {
        if (TscanPtr.p->scanBucketState ==  ScanRec::FIRST_LAP) {
          if (TSplit == TscanPtr.p->nextBucketIndex) {
            jam();
	    //-------------------------------------------------------------
	    // We are currently scanning this bucket. We cannot split it
	    // simultaneously with the scan. We have to pass this offer for
	    // splitting the bucket.
	    //-------------------------------------------------------------
            TreturnCode = 1;
            return TreturnCode;
          }
          else if (TSplit > TscanPtr.p->nextBucketIndex)
          {
            jam();
            ndbassert(TSplit <= TscanPtr.p->startNoOfBuckets);
            if (TSplit <= TscanPtr.p->startNoOfBuckets)
            {
	      //-------------------------------------------------------------
	      // This bucket has not yet been scanned. We must reset the scanned
	      // bit indicator for this scan on this bucket.
	      //-------------------------------------------------------------
              releaseScanMask |= TscanPtr.p->scanMask;
            }
          }
          else
          {
            jam();
          }//if
        } else if (TscanPtr.p->scanBucketState ==  ScanRec::SECOND_LAP) {
          jam();
	  //-------------------------------------------------------------
	  // We are performing a second lap to handle buckets that was
	  // merged during the first lap of scanning. During this second
	  // lap we do not allow any splits or merges.
	  //-------------------------------------------------------------
          TreturnCode = 1;
          return TreturnCode;
        } else {
          ndbrequire(TscanPtr.p->scanBucketState ==  ScanRec::SCAN_COMPLETED);
          jam();
	  //-------------------------------------------------------------
	  // The scan is completed and we can thus go ahead and perform
	  // the split.
	  //-------------------------------------------------------------
        }//if
      }//if
    }//if
  }//for
  TreleaseScanBucket = TSplit;
  TPageIndex = fragrecptr.p->getPageIndex(TreleaseScanBucket);
  TDirInd = fragrecptr.p->getPageNumber(TreleaseScanBucket);
  TPageptr.i = getPagePtr(fragrecptr.p->directory, TDirInd);
  c_page8_pool.getPtr(TPageptr);
  releaseScanBucket(TPageptr, TPageIndex, releaseScanMask);
  return TreturnCode;
}//Dbacc::checkScanExpand()

void Dbacc::execEXPANDCHECK2(Signal* signal)
{
  jamEntry();

  if(refToBlock(signal->getSendersBlockRef()) == getDBLQH())
  {
    jam();
    return;
  }

  fragrecptr.i = signal->theData[0];
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  fragrecptr.p->expandOrShrinkQueued = false;
#ifdef ERROR_INSERT
  bool force_expand_shrink = false;
  if (ERROR_INSERTED(3004) && fragrecptr.p->fragmentid == 0)
  {
    if (fragrecptr.p->level.getSize() > ERROR_INSERT_EXTRA)
    {
      execSHRINKCHECK2(signal);
      return;
    }
    else if (fragrecptr.p->level.getSize() == ERROR_INSERT_EXTRA)
    {
      return;
    }
    force_expand_shrink = true;
  }
  if (!force_expand_shrink && fragrecptr.p->slack > 0)
#else
  if (fragrecptr.p->slack > 0)
#endif
  {
    jam();
    /* IT MEANS THAT IF SLACK > ZERO */
    /*--------------------------------------------------------------*/
    /*       THE SLACK HAS IMPROVED AND IS NOW ACCEPTABLE AND WE    */
    /*       CAN FORGET ABOUT THE EXPAND PROCESS.                   */
    /*--------------------------------------------------------------*/
    if (ERROR_INSERTED(3002))
      debug_lh_vars("SLK");
    if (fragrecptr.p->dirRangeFull == ZTRUE) {
      jam();
      fragrecptr.p->dirRangeFull = ZFALSE;
    }
    return;
  }//if
  if (fragrecptr.p->level.isFull())
  {
    jam();
    /*
     * The level structure does not allow more buckets.
     * Do not expand.
     */
    return;
  }
  if (fragrecptr.p->sparsepages.isEmpty())
  {
    jam();
    Uint32 result = allocOverflowPage();
    if (result > ZLIMIT_OF_ERROR) {
      jam();
      /*--------------------------------------------------------------*/
      /* WE COULD NOT ALLOCATE ANY OVERFLOW PAGE. THUS WE HAVE TO STOP*/
      /* THE EXPAND SINCE WE CANNOT GUARANTEE ITS COMPLETION.         */
      /*--------------------------------------------------------------*/
      return;
    }//if
  }//if

  Uint32 splitBucket;
  Uint32 receiveBucket;

  bool doSplit = fragrecptr.p->level.getSplitBucket(splitBucket, receiveBucket);

  // Check that split bucket is not currently scanned
  if (doSplit && checkScanExpand(splitBucket) == 1) {
    jam();
    /*--------------------------------------------------------------*/
    // A scan state was inconsistent with performing an expand
    // operation.
    /*--------------------------------------------------------------*/
    return;
  }//if
  c_tup->prepare_tab_pointers_acc(fragrecptr.p->myTableId,
                                  fragrecptr.p->myfid);
  acquire_frag_mutex_bucket(fragrecptr.p, splitBucket);
  /*--------------------------------------------------------------------------*/
  /*       WE START BY FINDING THE PAGE, THE PAGE INDEX AND THE PAGE DIRECTORY*/
  /*       OF THE NEW BUCKET WHICH SHALL RECEIVE THE ELEMENT WHICH HAVE A 1 IN*/
  /*       THE NEXT HASH BIT. THIS BIT IS USED IN THE SPLIT MECHANISM TO      */
  /*       DECIDE WHICH ELEMENT GOES WHERE.                                   */
  /*--------------------------------------------------------------------------*/

  Uint32 expDirInd = fragrecptr.p->getPageNumber(receiveBucket);
  Page8Ptr expPageptr;
  if (fragrecptr.p->getPageIndex(receiveBucket) == 0)
  { // Need new bucket
    expPageptr.i = RNIL;
  }
  else
  {
    expPageptr.i = getPagePtr(fragrecptr.p->directory, expDirInd);
    ndbassert(expPageptr.i != RNIL);
  }
  if (expPageptr.i == RNIL) {
    jam();
    Uint32 result = seizePage_lock(expPageptr, Page32Lists::ANY_SUB_PAGE);
    if (result > ZLIMIT_OF_ERROR) {
      jam();
      release_frag_mutex_bucket(fragrecptr.p, splitBucket);
      return;
    }//if
    if (!setPagePtr(fragrecptr.p->directory, expDirInd, expPageptr.i))
    {
      jam();
      releasePage_lock(expPageptr);
      //result = ZDIR_RANGE_FULL_ERROR;
      release_frag_mutex_bucket(fragrecptr.p, splitBucket);
      return;
    }
    initPage(expPageptr, expDirInd);
  } else {
    c_page8_pool.getPtr(expPageptr);
  }//if

  /**
   * Allow use of extra index memory (m_free_pct) during expand
   * even after node have become started.
   * Reset to false in endofexpLab().
   */
  c_allow_use_of_spare_pages = true;

  fragrecptr.p->expReceivePageptr = expPageptr.i;
  fragrecptr.p->expReceiveIndex = fragrecptr.p->getPageIndex(receiveBucket);
  /*--------------------------------------------------------------------------*/
  /*       THE NEXT ACTION IS TO FIND THE PAGE, THE PAGE INDEX AND THE PAGE   */
  /*       DIRECTORY OF THE BUCKET TO BE SPLIT.                               */
  /*--------------------------------------------------------------------------*/
  Page8Ptr pageptr;
  Uint32 conidx = fragrecptr.p->getPageIndex(splitBucket);
  expDirInd = fragrecptr.p->getPageNumber(splitBucket);
  pageptr.i = getPagePtr(fragrecptr.p->directory, expDirInd);
  ndbassert(pageptr.i != RNIL);
  fragrecptr.p->expSenderIndex = conidx;
  fragrecptr.p->expSenderPageptr = pageptr.i;
  if (pageptr.i == RNIL) {
    jam();
    endofexpLab(signal);	/* EMPTY BUCKET */
    release_frag_mutex_bucket(fragrecptr.p, splitBucket);
    return;
  }//if
  fragrecptr.p->expReceiveIsforward = true;
  c_page8_pool.getPtr(pageptr);
  expandcontainer(pageptr, conidx);
  endofexpLab(signal);
  release_frag_mutex_bucket(fragrecptr.p, splitBucket);
  return;
}//Dbacc::execEXPANDCHECK2()
  
void Dbacc::endofexpLab(Signal* signal)
{
  c_allow_use_of_spare_pages = false;
  fragrecptr.p->slack += fragrecptr.p->maxloadfactor;
  fragrecptr.p->expandCounter++;
  fragrecptr.p->level.expand();
  Uint32 noOfBuckets = fragrecptr.p->level.getSize();
  Uint32 Thysteres = fragrecptr.p->maxloadfactor - fragrecptr.p->minloadfactor;
  fragrecptr.p->slackCheck = Int64(noOfBuckets) * Thysteres;
#ifdef ERROR_INSERT
  bool force_expand_shrink = false;
  if (ERROR_INSERTED(3004) &&
      fragrecptr.p->fragmentid == 0 &&
      fragrecptr.p->level.getSize() != ERROR_INSERT_EXTRA)
  {
    force_expand_shrink = true;
  }
  if ((force_expand_shrink || fragrecptr.p->slack < 0) &&
      !fragrecptr.p->level.isFull())
#else
  if (fragrecptr.p->slack < 0 && !fragrecptr.p->level.isFull())
#endif
  {
    jam();
    /* IT MEANS THAT IF SLACK < ZERO */
    /* --------------------------------------------------------------------------------- */
    /*       IT IS STILL NECESSARY TO EXPAND THE FRAGMENT EVEN MORE. START IT FROM HERE  */
    /*       WITHOUT WAITING FOR NEXT COMMIT ON THE FRAGMENT.                            */
    /* --------------------------------------------------------------------------------- */
    signal->theData[0] = fragrecptr.i;
    fragrecptr.p->expandOrShrinkQueued = true;
    sendSignal(reference(), GSN_EXPANDCHECK2, signal, 1, JBB);
  }//if
  return;
}//Dbacc::endofexpLab()

void Dbacc::execDEBUG_SIG(Signal* signal) 
{
  jamEntry();

  progError(__LINE__, NDBD_EXIT_SR_UNDOLOG);
  return;
}//Dbacc::execDEBUG_SIG()

LHBits32 Dbacc::getElementHash(OperationrecPtr& oprec)
{
  jam();
  ndbassert(!oprec.isNull());

  // Only calculate hash value if operation does not already have a complete hash value
  if (oprec.p->hashValue.valid_bits() < fragrecptr.p->MAX_HASH_VALUE_BITS)
  {
    jam();
    union {
      Uint32 keys[2048 * MAX_XFRM_MULTIPLY];
      Uint64 keys_align;
    };
    (void)keys_align;
    Local_key localkey;
    localkey = oprec.p->localdata;
    const bool xfrm = fragrecptr.p->hasCharAttr;
    Uint32 len = readTablePk(localkey.m_page_no,
                              localkey.m_page_idx,
                              ElementHeader::setLocked(oprec.i),
                              oprec,
                              &keys[0],
                              xfrm);
    if (len > 0)
    {
      /**
       * Return of len == 0 can only happen when the element is ready to be
       * deleted and no new operations is linked to the element, thus the
       * element will be removed soon since it will always return 0 for
       * all operations and as soon as the operations in the lock queue
       * have completed the element will be gone. Thus no issue if the
       * element is in the wrong place in the hash since it won't be found
       * by anyone even if in the right place.
       */
      oprec.p->hashValue = LHBits32(md5_hash((Uint64*)&keys[0], len));
    }
  }
  return oprec.p->hashValue;
}

LHBits32 Dbacc::getElementHash(Uint32 const* elemptr)
{
  jam();
  assert(ElementHeader::getUnlocked(*elemptr));

  union {
    Uint32 keys[2048 * MAX_XFRM_MULTIPLY];
    Uint64 keys_align;
  };
  (void)keys_align;
  Uint32 elemhead = *elemptr;
  Local_key localkey;
  elemptr += 1;
  ndbrequire(fragrecptr.p->localkeylen == 1);
  localkey.m_page_no = *elemptr;
  localkey.m_page_idx = ElementHeader::getPageIdx(elemhead);
  OperationrecPtr oprec;
  oprec.i = RNIL;
  const bool xfrm = fragrecptr.p->hasCharAttr;
  Uint32 len = readTablePk(localkey.m_page_no,
                           localkey.m_page_idx,
                           elemhead,
                           oprec,
                           &keys[0],
                           xfrm);
  if (len > 0)
  {
    jam();
    return LHBits32(md5_hash((Uint64*)&keys[0], len));
  }
  else
  { // Return an invalid hash value if no data
    jam();
    ndbabort(); // TODO RONM, see if this ever happens
    return LHBits32();
  }
}

LHBits32 Dbacc::getElementHash(Uint32 const* elemptr, OperationrecPtr& oprec)
{
  jam();

  if (!oprec.isNull())
  {
    jam();
    return getElementHash(oprec);
  }

  Uint32 elemhead = *elemptr;
  if (ElementHeader::getUnlocked(elemhead))
  {
    jam();
    return getElementHash(elemptr);
  }
  else
  {
    jam();
    oprec.i = ElementHeader::getOpPtrI(elemhead);
    ndbrequire(oprec_pool.getValidPtr(oprec));
    return getElementHash(oprec);
  }
}

/* --------------------------------------------------------------------------------- */
/* EXPANDCONTAINER                                                                   */
/*        INPUT: EXC_PAGEPTR (POINTER TO THE ACTIVE PAGE RECORD)                     */
/*               CEXC_PAGEINDEX (INDEX OF THE BUCKET).                               */
/*                                                                                   */
/*        DESCRIPTION: THE HASH VALUE OF ALL ELEMENTS IN THE CONTAINER WILL BE       */
/*                  CHECKED. SOME OF THIS ELEMENTS HAVE TO MOVE TO THE NEW CONTAINER */
/* --------------------------------------------------------------------------------- */
void Dbacc::expandcontainer(Page8Ptr pageptr, Uint32 conidx)
{
  ContainerHeader containerhead;
  LHBits32 texcHashvalue;
  Uint32 tidrContainerptr;
  Uint32 tidrElemhead;

  Page8Ptr lastPageptr;
  Page8Ptr lastPrevpageptr;
  bool lastIsforward;
  Uint32 tlastPageindex;
  Uint32 tlastElementptr;
  Uint32 tlastContainerptr;
  Uint32 tlastPrevconptr;

  Uint32 elemptr;
  Uint32 prevPageptr = RNIL;
  Uint32 prevConptr = 0;
  bool isforward = true;
  Uint32 elemStep;
  const Uint32 elemLen = fragrecptr.p->elementLength;
  OperationrecPtr oprecptr;
  bool newBucket = true;
 EXP_CONTAINER_LOOP:
  Uint32 conptr = getContainerPtr(conidx, isforward);
  if (isforward)
  {
    jam();
    elemptr = conptr + Container::HEADER_SIZE;
    elemStep = elemLen;
  }
  else
  {
    jam();
    elemStep = 0 - elemLen;
    elemptr = conptr + elemStep;
  }
  arrGuard(conptr, 2048);
  containerhead = pageptr.p->word32[conptr];
  const Uint32 conlen = containerhead.getLength();
  Uint32 cexcMovedLen = Container::HEADER_SIZE;
  if (conlen <= Container::HEADER_SIZE) {
    ndbrequire(conlen >= Container::HEADER_SIZE);
    jam();
    goto NEXT_ELEMENT;
  }//if
 NEXT_ELEMENT_LOOP:
  oprecptr.i = RNIL;
  ptrNull(oprecptr);
  /* --------------------------------------------------------------------------------- */
  /*       CEXC_PAGEINDEX         PAGE INDEX OF CURRENT CONTAINER BEING EXAMINED.      */
  /*       CEXC_CONTAINERPTR      INDEX OF CURRENT CONTAINER BEING EXAMINED.           */
  /*       CEXC_ELEMENTPTR        INDEX OF CURRENT ELEMENT BEING EXAMINED.             */
  /*       EXC_PAGEPTR            PAGE WHERE CURRENT ELEMENT RESIDES.                  */
  /*       CEXC_PREVPAGEPTR        PAGE OF PREVIOUS CONTAINER.                         */
  /*       CEXC_PREVCONPTR        INDEX OF PREVIOUS CONTAINER                          */
  /*       CEXC_FORWARD           DIRECTION OF CURRENT CONTAINER                       */
  /* --------------------------------------------------------------------------------- */
  arrGuard(elemptr, 2048);
  tidrElemhead = pageptr.p->word32[elemptr];
  bool move;
  if (ElementHeader::getLocked(tidrElemhead))
  {
    jam();
    oprecptr.i = ElementHeader::getOpPtrI(tidrElemhead);
    ndbrequire(oprec_pool.getValidPtr(oprecptr));
    ndbassert(oprecptr.p->reducedHashValue.valid_bits() >= 1);
    move = oprecptr.p->reducedHashValue.get_bit(1);
    oprecptr.p->reducedHashValue.shift_out();
    const LHBits16 reducedHashValue = oprecptr.p->reducedHashValue;
    if (!fragrecptr.p->enough_valid_bits(reducedHashValue))
    {
      jam();
      oprecptr.p->reducedHashValue =
        fragrecptr.p->level.reduceForSplit(getElementHash(oprecptr));
    }
  }
  else
  {
    jam();
    LHBits16 reducedHashValue = ElementHeader::getReducedHashValue(tidrElemhead);
    ndbassert(reducedHashValue.valid_bits() >= 1);
    move = reducedHashValue.get_bit(1);
    reducedHashValue.shift_out();
    if (!fragrecptr.p->enough_valid_bits(reducedHashValue))
    {
      jam();
      const Uint32* elemwordptr = &pageptr.p->word32[elemptr];
      const LHBits32 hashValue = getElementHash(elemwordptr);
      reducedHashValue =
        fragrecptr.p->level.reduceForSplit(hashValue);
    }
    tidrElemhead = ElementHeader::setReducedHashValue(tidrElemhead, reducedHashValue);
  }
  if (!move)
  {
    jam();
    if (ElementHeader::getUnlocked(tidrElemhead))
      pageptr.p->word32[elemptr] = tidrElemhead;
    /* --------------------------------------------------------------------------------- */
    /*       THIS ELEMENT IS NOT TO BE MOVED. WE CALCULATE THE WHEREABOUTS OF THE NEXT   */
    /*       ELEMENT AND PROCEED WITH THAT OR END THE SEARCH IF THERE ARE NO MORE        */
    /*       ELEMENTS IN THIS CONTAINER.                                                 */
    /* --------------------------------------------------------------------------------- */
    goto NEXT_ELEMENT;
  }//if
  /* --------------------------------------------------------------------------------- */
  /*       THE HASH BIT WAS SET AND WE SHALL MOVE THIS ELEMENT TO THE NEW BUCKET.      */
  /*       WE START BY READING THE ELEMENT TO BE ABLE TO INSERT IT INTO THE NEW BUCKET.*/
  /*       THEN WE INSERT THE ELEMENT INTO THE NEW BUCKET. THE NEXT STEP IS TO DELETE  */
  /*       THE ELEMENT FROM THIS BUCKET. THIS IS PERFORMED BY REPLACING IT WITH THE    */
  /*       LAST ELEMENT IN THE BUCKET. IF THIS ELEMENT IS TO BE MOVED WE MOVE IT AND   */
  /*       GET THE LAST ELEMENT AGAIN UNTIL WE EITHER FIND ONE THAT STAYS OR THIS      */
  /*       ELEMENT IS THE LAST ELEMENT.                                                */
  /* --------------------------------------------------------------------------------- */
  {
    ndbrequire(fragrecptr.p->localkeylen == 1);
    const Uint32 localkey = pageptr.p->word32[elemptr + 1];
#if defined(VM_TRACE) || !defined(NDEBUG) || defined(ERROR_INSERT)
    jamDebug();
    jamLineDebug(Uint16(pageptr.i));
    jamLineDebug(Uint16(elemptr));
    pageptr.p->word32[elemptr] = ElementHeader::setInvalid();
#endif
    Uint32 tidrPageindex = fragrecptr.p->expReceiveIndex;
    Page8Ptr idrPageptr;
    idrPageptr.i = fragrecptr.p->expReceivePageptr;
    c_page8_pool.getPtr(idrPageptr);
    bool tidrIsforward = fragrecptr.p->expReceiveIsforward;
    insertElement(Element(tidrElemhead, localkey),
                  oprecptr,
                  idrPageptr,
                  tidrPageindex,
                  tidrIsforward,
                  tidrContainerptr,
                  containerhead.getScanBits(),
                  newBucket);
    fragrecptr.p->expReceiveIndex = tidrPageindex;
    fragrecptr.p->expReceivePageptr = idrPageptr.i;
    fragrecptr.p->expReceiveIsforward = tidrIsforward;
    newBucket = false;
  }
 REMOVE_LAST_LOOP:
  jam();
  lastPageptr.i = pageptr.i;
  lastPageptr.p = pageptr.p;
  tlastContainerptr = conptr;
  lastPrevpageptr.i = prevPageptr;
  c_page8_pool.getPtrForce(lastPrevpageptr);
  tlastPrevconptr = prevConptr;
  arrGuard(tlastContainerptr, 2048);
  lastIsforward = isforward;
  tlastPageindex = conidx;
  getLastAndRemove(lastPrevpageptr,
                   tlastPrevconptr,
                   lastPageptr,
                   tlastPageindex,
                   tlastContainerptr,
                   lastIsforward,
                   tlastElementptr);
  if (pageptr.i == lastPageptr.i)
  {
    if (elemptr == tlastElementptr)
    {
      jam();
      /* --------------------------------------------------------------------------------- */
      /*       THE CURRENT ELEMENT WAS ALSO THE LAST ELEMENT.                              */
      /* --------------------------------------------------------------------------------- */
      return;
    }//if
  }//if
  /* --------------------------------------------------------------------------------- */
  /*       THE CURRENT ELEMENT WAS NOT THE LAST ELEMENT. IF THE LAST ELEMENT SHOULD    */
  /*       STAY WE COPY IT TO THE POSITION OF THE CURRENT ELEMENT, OTHERWISE WE INSERT */
  /*       INTO THE NEW BUCKET, REMOVE IT AND TRY WITH THE NEW LAST ELEMENT.           */
  /* --------------------------------------------------------------------------------- */
  oprecptr.i = RNIL;
  ptrNull(oprecptr);
  arrGuard(tlastElementptr, 2048);
  tidrElemhead = lastPageptr.p->word32[tlastElementptr];
  if (ElementHeader::getLocked(tidrElemhead))
  {
    jam();
    oprecptr.i = ElementHeader::getOpPtrI(tidrElemhead);
    ndbrequire(oprec_pool.getValidPtr(oprecptr));
    ndbassert(oprecptr.p->reducedHashValue.valid_bits() >= 1);
    move = oprecptr.p->reducedHashValue.get_bit(1);
    oprecptr.p->reducedHashValue.shift_out();
    if (!fragrecptr.p->enough_valid_bits(oprecptr.p->reducedHashValue))
    {
      jam();
      oprecptr.p->reducedHashValue =
        fragrecptr.p->level.reduceForSplit(getElementHash(oprecptr));
    }
  }
  else
  {
    jam();
    LHBits16 reducedHashValue = ElementHeader::getReducedHashValue(tidrElemhead);
    ndbassert(reducedHashValue.valid_bits() > 0);
    move = reducedHashValue.get_bit(1);
    reducedHashValue.shift_out();
    if (!fragrecptr.p->enough_valid_bits(reducedHashValue))
    {
      jam();
      const Uint32* elemwordptr = &lastPageptr.p->word32[tlastElementptr];
      const LHBits32 hashValue = getElementHash(elemwordptr);
      reducedHashValue =
        fragrecptr.p->level.reduceForSplit(hashValue);
    }
    tidrElemhead = ElementHeader::setReducedHashValue(tidrElemhead, reducedHashValue);
  }
  if (!move)
  {
    jam();
    if (ElementHeader::getUnlocked(tidrElemhead))
      lastPageptr.p->word32[tlastElementptr] = tidrElemhead;
    /* --------------------------------------------------------------------------------- */
    /*       THE LAST ELEMENT IS NOT TO BE MOVED. WE COPY IT TO THE CURRENT ELEMENT.     */
    /* --------------------------------------------------------------------------------- */
    const Page8Ptr delPageptr = pageptr;
    const Uint32 delConptr = conptr;
    const Uint32 delElemptr = elemptr;
    deleteElement(delPageptr,
                  delConptr,
                  delElemptr,
                  lastPageptr,
                  tlastElementptr);
  }
  else
  {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*       THE LAST ELEMENT IS ALSO TO BE MOVED.                                       */
    /* --------------------------------------------------------------------------------- */
    {
      ndbrequire(fragrecptr.p->localkeylen == 1);
      const Uint32 localkey = lastPageptr.p->word32[tlastElementptr + 1];
      Uint32 tidrPageindex = fragrecptr.p->expReceiveIndex;
      Page8Ptr idrPageptr;
      idrPageptr.i = fragrecptr.p->expReceivePageptr;
      c_page8_pool.getPtr(idrPageptr);
      bool tidrIsforward = fragrecptr.p->expReceiveIsforward;
      insertElement(Element(tidrElemhead, localkey),
                    oprecptr,
                    idrPageptr,
                    tidrPageindex,
                    tidrIsforward,
                    tidrContainerptr,
                    containerhead.getScanBits(),
                    newBucket);
      fragrecptr.p->expReceiveIndex = tidrPageindex;
      fragrecptr.p->expReceivePageptr = idrPageptr.i;
      fragrecptr.p->expReceiveIsforward = tidrIsforward;
      newBucket = false;
    }
    goto REMOVE_LAST_LOOP;
  }//if
 NEXT_ELEMENT:
  arrGuard(conptr, 2048);
  containerhead = pageptr.p->word32[conptr];
  cexcMovedLen = cexcMovedLen + fragrecptr.p->elementLength;
  if (containerhead.getLength() > cexcMovedLen) {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*       WE HAVE NOT YET MOVED THE COMPLETE CONTAINER. WE PROCEED WITH THE NEXT      */
    /*       ELEMENT IN THE CONTAINER. IT IS IMPORTANT TO READ THE CONTAINER LENGTH      */
    /*       FROM THE CONTAINER HEADER SINCE IT MIGHT CHANGE BY REMOVING THE LAST        */
    /*       ELEMENT IN THE BUCKET.                                                      */
    /* --------------------------------------------------------------------------------- */
    elemptr = elemptr + elemStep;
    goto NEXT_ELEMENT_LOOP;
  }//if
  if (containerhead.getNextEnd() != 0) {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*       WE PROCEED TO THE NEXT CONTAINER IN THE BUCKET.                             */
    /* --------------------------------------------------------------------------------- */
    prevPageptr = pageptr.i;
    prevConptr = conptr;
    nextcontainerinfo(pageptr,
                      conptr,
                      containerhead,
                      conidx,
                      isforward);
    goto EXP_CONTAINER_LOOP;
  }//if
}//Dbacc::expandcontainer()

/* ******************--------------------------------------------------------------- */
/* SHRINKCHECK                                        JOIN BUCKET ORD                */
/*                                                   SENDER: ACC,    LEVEL B         */
/*   INPUT:   FRAGRECPTR, POINTS TO A FRAGMENT RECORD.                               */
/*   DESCRIPTION: TWO BUCKET OF A FRAGMENT PAGE WILL BE JOINED TOGETHER              */
/*                                 ACCORDING TO LH3.                                 */
/* ******************--------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/* SHRINKCHECK                                            JOIN BUCKET ORD            */
/* ******************------------------------------+                                 */
/*   SENDER: ACC,    LEVEL B       */
/* TWO BUCKETS OF THE FRAGMENT     */
/* WILL BE JOINED ACCORDING TO LH3 */
/* AND COMMIT TRANSACTION PROCESS  */
/* WILL BE CONTINUED */
Uint32 Dbacc::checkScanShrink(Uint32 sourceBucket, Uint32 destBucket)
{
  Uint32 Ti;
  Uint32 TreturnCode = 0;
  Uint32 TPageIndex;
  Uint32 TDirInd;
  Uint32 TmergeDest;
  Uint32 TmergeSource;
  Uint32 TreleaseScanBucket;
  Uint32 TreleaseInd = 0;
  enum Actions { ExtendRescan, ReduceUndefined };
  Bitmask<1> actions[MAX_PARALLEL_SCANS_PER_FRAG];
  Uint16 releaseDestScanMask = 0;
  Uint16 releaseSourceScanMask = 0;
  Page8Ptr TPageptr;
  ScanRecPtr scanPtr;

  TmergeDest = destBucket;
  TmergeSource = sourceBucket;
  for (Ti = 0; Ti < MAX_PARALLEL_SCANS_PER_FRAG; Ti++)
  {
    actions[Ti].clear();
    if (fragrecptr.p->scan[Ti] != RNIL) {
      scanPtr.i = fragrecptr.p->scan[Ti];
      ndbrequire(scanRec_pool.getValidPtr(scanPtr));
      if (scanPtr.p->activeLocalFrag == fragrecptr.i) {
	//-------------------------------------------------------------
	// A scan is ongoing on this particular local fragment. We have
	// to check its current state.
	//-------------------------------------------------------------
        if (scanPtr.p->scanBucketState ==  ScanRec::FIRST_LAP) {
          jam();
          if ((TmergeDest == scanPtr.p->nextBucketIndex) ||
              (TmergeSource == scanPtr.p->nextBucketIndex)) {
            jam();
	    //-------------------------------------------------------------
	    // We are currently scanning one of the buckets involved in the
	    // merge. We cannot merge while simultaneously performing a scan.
	    // We have to pass this offer for merging the buckets.
	    //-------------------------------------------------------------
            TreturnCode = 1;
            return TreturnCode;
          }
          else if (TmergeDest < scanPtr.p->nextBucketIndex)
          {
            jam();
            /**
             * Merge bucket into scanned bucket.  Mark for rescan.
             */
            actions[Ti].set(ExtendRescan);
            if (TmergeSource == scanPtr.p->startNoOfBuckets)
            {
              /**
               * Merge unscanned bucket with undefined scan bits into scanned
               * bucket.  Source buckets scan bits must be cleared.
               */
              actions[Ti].set(ReduceUndefined);
              releaseSourceScanMask |= scanPtr.p->scanMask;
            }
            TreleaseInd = 1;
          }//if
          else
          {
            /**
             * Merge unscanned bucket with undefined scan bits into unscanned
             * bucket with undefined scan bits.
             */
            if (TmergeSource == scanPtr.p->startNoOfBuckets)
            {
              actions[Ti].set(ReduceUndefined);
              releaseSourceScanMask |= scanPtr.p->scanMask;
              TreleaseInd = 1;
            }
            if (TmergeDest <= scanPtr.p->startNoOfBuckets)
            {
              jam();
              // Destination bucket is not scanned by scan
              releaseDestScanMask |= scanPtr.p->scanMask;
            }
          }
        }
        else if (scanPtr.p->scanBucketState ==  ScanRec::SECOND_LAP)
        {
          jam();
	  //-------------------------------------------------------------
	  // We are performing a second lap to handle buckets that was
	  // merged during the first lap of scanning. During this second
	  // lap we do not allow any splits or merges.
	  //-------------------------------------------------------------
          TreturnCode = 1;
          return TreturnCode;
        } else if (scanPtr.p->scanBucketState ==  ScanRec::SCAN_COMPLETED) {
          jam();
	  //-------------------------------------------------------------
	  // The scan is completed and we can thus go ahead and perform
	  // the split.
	  //-------------------------------------------------------------
          releaseDestScanMask |= scanPtr.p->scanMask;
          releaseSourceScanMask |= scanPtr.p->scanMask;
        } else {
          jam();
          sendSystemerror(__LINE__);
          return TreturnCode;
        }//if
      }//if
    }//if
  }//for

  TreleaseScanBucket = TmergeSource;
  TPageIndex = fragrecptr.p->getPageIndex(TreleaseScanBucket);
  TDirInd = fragrecptr.p->getPageNumber(TreleaseScanBucket);
  TPageptr.i = getPagePtr(fragrecptr.p->directory, TDirInd);
  c_page8_pool.getPtr(TPageptr);
  releaseScanBucket(TPageptr, TPageIndex, releaseSourceScanMask);

  TreleaseScanBucket = TmergeDest;
  TPageIndex = fragrecptr.p->getPageIndex(TreleaseScanBucket);
  TDirInd = fragrecptr.p->getPageNumber(TreleaseScanBucket);
  TPageptr.i = getPagePtr(fragrecptr.p->directory, TDirInd);
  c_page8_pool.getPtr(TPageptr);
  releaseScanBucket(TPageptr, TPageIndex, releaseDestScanMask);

  if (TreleaseInd == 1) {
    jam();
    for (Ti = 0; Ti < MAX_PARALLEL_SCANS_PER_FRAG; Ti++) {
      if (!actions[Ti].isclear())
      {
        jam();
        scanPtr.i = fragrecptr.p->scan[Ti];
        ndbrequire(scanRec_pool.getValidPtr(scanPtr));
        if (actions[Ti].get(ReduceUndefined))
        {
          scanPtr.p->startNoOfBuckets --;
        }
        if (actions[Ti].get(ExtendRescan))
        {
          if (TmergeDest < scanPtr.p->minBucketIndexToRescan)
          {
            jam();
	    //-------------------------------------------------------------
	    // We have to keep track of the starting bucket to Rescan in the
	    // second lap.
	    //-------------------------------------------------------------
            scanPtr.p->minBucketIndexToRescan = TmergeDest;
          }//if
          if (TmergeDest > scanPtr.p->maxBucketIndexToRescan)
          {
            jam();
	    //-------------------------------------------------------------
	    // We have to keep track of the ending bucket to Rescan in the
	    // second lap.
	    //-------------------------------------------------------------
            scanPtr.p->maxBucketIndexToRescan = TmergeDest;
          }//if
        }
      }//if
    }//for
  }//if
  return TreturnCode;
}//Dbacc::checkScanShrink()

void Dbacc::execSHRINKCHECK2(Signal* signal) 
{
  jamEntry();
  fragrecptr.i = signal->theData[0];
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  fragrecptr.p->expandOrShrinkQueued = false;
#ifdef ERROR_INSERT
  bool force_expand_shrink = false;
  if (ERROR_INSERTED(3004) && fragrecptr.p->fragmentid == 0)
  {
    if (fragrecptr.p->level.getSize() < ERROR_INSERT_EXTRA)
    {
      execEXPANDCHECK2(signal);
      return;
    }
    else if (fragrecptr.p->level.getSize() == ERROR_INSERT_EXTRA)
    {
      return;
    }
    force_expand_shrink = true;
  }
  if (!force_expand_shrink &&
      fragrecptr.p->slack <= fragrecptr.p->slackCheck)
#else
  if (fragrecptr.p->slack <= fragrecptr.p->slackCheck)
#endif
  {
    jam();
    /* TIME FOR JOIN BUCKETS PROCESS */
    /*--------------------------------------------------------------*/
    /*       NO LONGER NECESSARY TO SHRINK THE FRAGMENT.            */
    /*--------------------------------------------------------------*/
    return;
  }//if
#ifdef ERROR_INSERT
  if (!force_expand_shrink && fragrecptr.p->slack < 0)
#else
  if (fragrecptr.p->slack < 0)
#endif
  {
    jam();
    /*--------------------------------------------------------------*/
    /* THE SLACK IS NEGATIVE, IN THIS CASE WE WILL NOT NEED ANY     */
    /* SHRINK.                                                      */
    /*--------------------------------------------------------------*/
    return;
  }//if
  if (fragrecptr.p->level.isEmpty())
  {
    jam();
    /* no need to shrink empty hash table */
    return;
  }
  if (fragrecptr.p->sparsepages.isEmpty())
  {
    jam();
    Uint32 result = allocOverflowPage();
    if (result > ZLIMIT_OF_ERROR) {
      jam();
      return;
    }//if
  }//if
  if (!pages.haveFreePage8(Page32Lists::ANY_SUB_PAGE))
  {
    jam();
    return;
  }

  // Since expandCounter guards more shrinks than expands and
  // all fragments starts with a full page of buckets
  ndbassert(fragrecptr.p->getPageNumber(fragrecptr.p->level.getTop()) > 0);

  Uint32 mergeSourceBucket;
  Uint32 mergeDestBucket;
  bool doMerge = fragrecptr.p->level.getMergeBuckets(mergeSourceBucket, mergeDestBucket);

  ndbassert(doMerge); // Merge always needed since we never shrink below one page of buckets

  /* check that neither of source or destination bucket are currently scanned */
  if (doMerge && checkScanShrink(mergeSourceBucket, mergeDestBucket) == 1) {
    jam();
    /*--------------------------------------------------------------*/
    // A scan state was inconsistent with performing a shrink
    // operation.
    /*--------------------------------------------------------------*/
    return;
  }//if

  acquire_frag_mutex_bucket(fragrecptr.p, mergeDestBucket);
  /**
   * Allow use of extra index memory (m_free_pct) during shrink
   * even after node have become started.
   * Reset to false in endofshrinkbucketLab().
   */
  c_allow_use_of_spare_pages = true;

  if (ERROR_INSERTED(3002))
    debug_lh_vars("SHR");
  if (fragrecptr.p->dirRangeFull == ZTRUE) {
    jam();
    fragrecptr.p->dirRangeFull = ZFALSE;
  }

  shrink_adjust_reduced_hash_value(mergeDestBucket);

  /*--------------------------------------------------------------------------*/
  /*       WE START BY FINDING THE NECESSARY INFORMATION OF THE BUCKET TO BE  */
  /*       REMOVED WHICH WILL SEND ITS ELEMENTS TO THE RECEIVING BUCKET.      */
  /*--------------------------------------------------------------------------*/
  Uint32 cexcPageindex = fragrecptr.p->getPageIndex(mergeSourceBucket);
  Uint32 expDirInd = fragrecptr.p->getPageNumber(mergeSourceBucket);
  Page8Ptr pageptr;
  pageptr.i = getPagePtr(fragrecptr.p->directory, expDirInd);
  fragrecptr.p->expSenderIndex = cexcPageindex;
  fragrecptr.p->expSenderPageptr = pageptr.i;
  fragrecptr.p->expSenderDirIndex = expDirInd;
  /*--------------------------------------------------------------------------*/
  /*       WE NOW PROCEED BY FINDING THE NECESSARY INFORMATION ABOUT THE      */
  /*       RECEIVING BUCKET.                                                  */
  /*--------------------------------------------------------------------------*/
  expDirInd = fragrecptr.p->getPageNumber(mergeDestBucket);
  fragrecptr.p->expReceivePageptr = getPagePtr(fragrecptr.p->directory, expDirInd);
  fragrecptr.p->expReceiveIndex = fragrecptr.p->getPageIndex(mergeDestBucket);
  fragrecptr.p->expReceiveIsforward = true;
  if (pageptr.i == RNIL)
  {
    jam();
    endofshrinkbucketLab(signal);	/* EMPTY BUCKET */
    release_frag_mutex_bucket(fragrecptr.p, mergeDestBucket);
    return;
  }//if
  /*--------------------------------------------------------------------------*/
  /*       INITIALISE THE VARIABLES FOR THE SHRINK PROCESS.                   */
  /*--------------------------------------------------------------------------*/
  c_page8_pool.getPtr(pageptr);
  bool isforward = true;
  Uint32 conptr = getForwardContainerPtr(cexcPageindex);
  arrGuard(conptr, 2048);
  ContainerHeader containerhead = pageptr.p->word32[conptr];
  Uint32 conlen = containerhead.getLength();
  if (conlen <= Container::HEADER_SIZE) {
    ndbrequire(conlen == Container::HEADER_SIZE);
  } else {
    jam();
    shrinkcontainer(pageptr, conptr, isforward, conlen);
  }//if
  /*--------------------------------------------------------------------------*/
  /*       THIS CONTAINER IS NOT YET EMPTY AND WE REMOVE ALL THE ELEMENTS.    */
  /*--------------------------------------------------------------------------*/
  if (containerhead.isUsingBothEnds()) {
    jam();
    Uint32 relconptr = conptr + (ZBUF_SIZE - Container::HEADER_SIZE);
    releaseRightlist(pageptr, cexcPageindex, relconptr);
  }//if
  ContainerHeader conthead;
  conthead.initInUse();
  arrGuard(conptr, 2048);
  pageptr.p->word32[conptr] = conthead;
  if (containerhead.getNextEnd() == 0) {
    jam();
    endofshrinkbucketLab(signal);
    release_frag_mutex_bucket(fragrecptr.p, mergeDestBucket);
    return;
  }//if
  nextcontainerinfo(pageptr,
                    conptr,
                    containerhead,
                    cexcPageindex,
                    isforward);
  do {
    conptr = getContainerPtr(cexcPageindex, isforward);
    arrGuard(conptr, 2048);
    containerhead = pageptr.p->word32[conptr];
    conlen = containerhead.getLength();
    ndbrequire(conlen > Container::HEADER_SIZE);
    /*--------------------------------------------------------------------------*/
    /*       THIS CONTAINER IS NOT YET EMPTY AND WE REMOVE ALL THE ELEMENTS.    */
    /*--------------------------------------------------------------------------*/
    shrinkcontainer(pageptr, conptr, isforward, conlen);
    const Uint32 prevPageptr = pageptr.i;
    const Uint32 cexcPrevpageindex = cexcPageindex;
    const Uint32 cexcPrevisforward = isforward;
    if (containerhead.getNextEnd() != 0) {
      jam();
      /*--------------------------------------------------------------------------*/
      /*       WE MUST CALL THE NEXT CONTAINER INFO ROUTINE BEFORE WE RELEASE THE */
      /*       CONTAINER SINCE THE RELEASE WILL OVERWRITE THE NEXT POINTER.       */
      /*--------------------------------------------------------------------------*/
      nextcontainerinfo(pageptr,
                        conptr,
                        containerhead,
                        cexcPageindex,
                        isforward);
    }//if
    Page8Ptr rlPageptr;
    rlPageptr.i = prevPageptr;
    c_page8_pool.getPtr(rlPageptr);
    ndbassert(!containerhead.isScanInProgress());
    if (cexcPrevisforward)
    {
      jam();
      if (containerhead.isUsingBothEnds()) {
        jam();
        Uint32 relconptr = conptr + (ZBUF_SIZE - Container::HEADER_SIZE);
        releaseRightlist(rlPageptr, cexcPrevpageindex, relconptr);
      }//if
      ndbrequire(ContainerHeader(rlPageptr.p->word32[conptr]).isInUse());
      releaseLeftlist(rlPageptr, cexcPrevpageindex, conptr);
    }
    else
    {
      jam();
      if (containerhead.isUsingBothEnds()) {
        jam();
        Uint32 relconptr = conptr - (ZBUF_SIZE - Container::HEADER_SIZE);
        releaseLeftlist(rlPageptr, cexcPrevpageindex, relconptr);
      }//if
      ndbrequire(ContainerHeader(rlPageptr.p->word32[conptr]).isInUse());
      releaseRightlist(rlPageptr, cexcPrevpageindex, conptr);
    }//if
  } while (containerhead.getNextEnd() != 0);
  endofshrinkbucketLab(signal);
  release_frag_mutex_bucket(fragrecptr.p, mergeDestBucket);
  return;
}//Dbacc::execSHRINKCHECK2()

void Dbacc::endofshrinkbucketLab(Signal* signal)
{
  c_allow_use_of_spare_pages = false;
  fragrecptr.p->level.shrink();
  fragrecptr.p->expandCounter--;
  fragrecptr.p->slack -= fragrecptr.p->maxloadfactor;
  if (fragrecptr.p->expSenderIndex == 0) {
    jam();
    if (fragrecptr.p->expSenderPageptr != RNIL) {
      jam();
      Page8Ptr rpPageptr;
      rpPageptr.i = fragrecptr.p->expSenderPageptr;
      c_page8_pool.getPtr(rpPageptr);
      releasePage_lock(rpPageptr);
      unsetPagePtr(fragrecptr.p->directory, fragrecptr.p->expSenderDirIndex);
    }//if
    if ((fragrecptr.p->getPageNumber(fragrecptr.p->level.getSize()) & 0xff) == 0) {
      jam();
      DynArr256 dir(directoryPoolPtr, fragrecptr.p->directory);
      DynArr256::ReleaseIterator iter;
      Uint32 relcode;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
      Uint32 count = 0;
#endif
      dir.init(iter);
      while ((relcode = dir.trim(fragrecptr.p->expSenderDirIndex, iter)) != 0)
      {
#if defined(VM_TRACE) || defined(ERROR_INSERT)
        count++;
        ndbrequire(count <= 256);
#endif
      }
    }//if
  }//if
#ifdef ERROR_INSERT
  bool force_expand_shrink = false;
  if (ERROR_INSERTED(3004) &&
      fragrecptr.p->fragmentid == 0 &&
      fragrecptr.p->level.getSize() != ERROR_INSERT_EXTRA)
  {
    force_expand_shrink = true;
  }
  if (force_expand_shrink || fragrecptr.p->slack > 0)
#else
  if (fragrecptr.p->slack > 0)
#endif
  {
    jam();
    /*--------------------------------------------------------------*/
    /* THE SLACK IS POSITIVE, IN THIS CASE WE WILL CHECK WHETHER    */
    /* WE WILL CONTINUE PERFORM ANOTHER SHRINK.                     */
    /*--------------------------------------------------------------*/
    Uint32 noOfBuckets = fragrecptr.p->level.getSize();
    Uint32 Thysteresis = fragrecptr.p->maxloadfactor - fragrecptr.p->minloadfactor;
    fragrecptr.p->slackCheck = Int64(noOfBuckets) * Thysteresis;
#ifdef ERROR_INSERT
    if (force_expand_shrink || fragrecptr.p->slack > Thysteresis)
#else
    if (fragrecptr.p->slack > Thysteresis)
#endif
    {
      /*--------------------------------------------------------------*/
      /*       IT IS STILL NECESSARY TO SHRINK THE FRAGMENT MORE. THIS*/
      /*       CAN HAPPEN WHEN A NUMBER OF SHRINKS GET REJECTED       */
      /*       DURING A LOCAL CHECKPOINT. WE START A NEW SHRINK       */
      /*       IMMEDIATELY FROM HERE WITHOUT WAITING FOR A COMMIT TO  */
      /*       START IT.                                              */
      /*--------------------------------------------------------------*/
      if (fragrecptr.p->expandCounter > 0) {
        jam();
	/*--------------------------------------------------------------*/
	/*       IT IS VERY IMPORTANT TO NOT TRY TO SHRINK MORE THAN    */
	/*       WAS EXPANDED. IF MAXP IS SET TO A VALUE BELOW 63 THEN  */
	/*       WE WILL LOSE RECORDS SINCE GETDIRINDEX CANNOT HANDLE   */
	/*       SHRINKING BELOW 2^K - 1 (NOW 63). THIS WAS A BUG THAT  */
	/*       WAS REMOVED 2000-05-12.                                */
	/*--------------------------------------------------------------*/
        signal->theData[0] = fragrecptr.i;
        ndbrequire(!fragrecptr.p->expandOrShrinkQueued);
        fragrecptr.p->expandOrShrinkQueued = true;
        sendSignal(reference(), GSN_SHRINKCHECK2, signal, 1, JBB);
      }//if
    }//if
  }//if
  ndbrequire(fragrecptr.p->getPageNumber(fragrecptr.p->level.getSize()) > 0);
  return;
}//Dbacc::endofshrinkbucketLab()

/* --------------------------------------------------------------------------------- */
/* SHRINKCONTAINER                                                                   */
/*        INPUT: EXC_PAGEPTR (POINTER TO THE ACTIVE PAGE RECORD)                     */
/*               CEXC_CONTAINERLEN (LENGTH OF THE CONTAINER).                        */
/*               CEXC_CONTAINERPTR (ARRAY INDEX OF THE CONTAINER).                   */
/*               CEXC_FORWARD (CONTAINER FORWARD (+1) OR BACKWARD (-1))              */
/*                                                                                   */
/*        DESCRIPTION: SCAN ALL ELEMENTS IN DESTINATION BUCKET BEFORE MERGE          */
/*               AND ADJUST THE STORED REDUCED HASH VALUE (SHIFT IN ZERO).           */
/* --------------------------------------------------------------------------------- */
void
Dbacc::shrink_adjust_reduced_hash_value(Uint32 bucket_number)
{
  /*
   * Note: function are a copy paste from getElement() with modified inner loop
   * instead of finding a specific element, scan through all and modify.
   */
  Uint32 tgeElementHeader;
  Uint32 tgeElemStep;
  Uint32 tgePageindex;
  Uint32 tgeNextptrtype;
  Uint32 tgeContainerptr;
  Uint32 tgeElementptr;
  Uint32 tgeRemLen;
  const Uint32 TelemLen = fragrecptr.p->elementLength;
  const Uint32 localkeylen = fragrecptr.p->localkeylen;

  tgePageindex = fragrecptr.p->getPageIndex(bucket_number);
  Page8Ptr gePageptr;
  gePageptr.i = getPagePtr(fragrecptr.p->directory, fragrecptr.p->getPageNumber(bucket_number));
  c_page8_pool.getPtr(gePageptr);

  ndbrequire(TelemLen == ZELEM_HEAD_SIZE + localkeylen);
  tgeNextptrtype = ZLEFT;

  /* Loop through all containers in a bucket */
  do {
    if (tgeNextptrtype == ZLEFT)
    {
      jam();
      tgeContainerptr = getForwardContainerPtr(tgePageindex);
      tgeElementptr = tgeContainerptr + Container::HEADER_SIZE;
      tgeElemStep = TelemLen;
      ndbrequire(tgeContainerptr < 2048);
      tgeRemLen = ContainerHeader(gePageptr.p->word32[tgeContainerptr]).getLength();
      ndbrequire((tgeContainerptr + tgeRemLen - 1) < 2048);
    }
    else if (tgeNextptrtype == ZRIGHT)
    {
      jam();
      tgeContainerptr = getBackwardContainerPtr(tgePageindex);
      tgeElementptr = tgeContainerptr - TelemLen;
      tgeElemStep = 0 - TelemLen;
      ndbrequire(tgeContainerptr < 2048);
      tgeRemLen = ContainerHeader(gePageptr.p->word32[tgeContainerptr]).getLength();
      ndbrequire((tgeContainerptr - tgeRemLen) < 2048);
    }
    else
    {
      jam();
      jamLine(tgeNextptrtype);
      ndbabort();
    }//if
    if (tgeRemLen >= Container::HEADER_SIZE + TelemLen)
    {
      ndbrequire(tgeRemLen <= ZBUF_SIZE);
      /* ------------------------------------------------------------------- */
      /* Loop through all elements in a container */
      do
      {
        tgeElementHeader = gePageptr.p->word32[tgeElementptr];
        tgeRemLen = tgeRemLen - TelemLen;
        /*
         * Adjust the stored reduced hash value for element, shifting in a zero
         */
        if (ElementHeader::getLocked(tgeElementHeader))
        {
          jam();
          OperationrecPtr oprec;
          oprec.i = ElementHeader::getOpPtrI(tgeElementHeader);
          ndbrequire(oprec_pool.getValidPtr(oprec));
          oprec.p->reducedHashValue.shift_in(false);
        }
        else
        {
          jam();
          LHBits16 reducedHashValue = ElementHeader::getReducedHashValue(tgeElementHeader);
          reducedHashValue.shift_in(false);
          tgeElementHeader = ElementHeader::setReducedHashValue(tgeElementHeader, reducedHashValue);
          gePageptr.p->word32[tgeElementptr] = tgeElementHeader;
        }
        if (tgeRemLen <= Container::HEADER_SIZE)
        {
          break;
        }
        tgeElementptr = tgeElementptr + tgeElemStep;
      } while (true);
    }//if
    ndbrequire(tgeRemLen == Container::HEADER_SIZE);
    ContainerHeader containerhead = gePageptr.p->word32[tgeContainerptr];
    ndbassert((containerhead.getScanBits() & ~fragrecptr.p->activeScanMask) == 0);
    tgeNextptrtype = containerhead.getNextEnd();
    if (tgeNextptrtype == 0)
    {
      jam();
      return;	/* NO MORE CONTAINER */
    }//if
    tgePageindex = containerhead.getNextIndexNumber();	/* NEXT CONTAINER PAGE INDEX 7 BITS */
    ndbrequire((tgePageindex <= Container::MAX_CONTAINER_INDEX) || (tgePageindex == Container::NO_CONTAINER_INDEX));
    if (!containerhead.isNextOnSamePage())
    {
      jam();
      gePageptr.i = gePageptr.p->word32[tgeContainerptr + 1];  /* NEXT PAGE I */
      c_page8_pool.getPtr(gePageptr);
    }//if
  } while (1);

  return;
}//Dbacc::shrink_adjust_reduced_hash_value()

void Dbacc::shrinkcontainer(Page8Ptr pageptr,
                            Uint32 conptr,
                            bool isforward,
                            Uint32 conlen)
{
  Uint32 tshrElementptr;
  Uint32 tshrRemLen;
  Uint32 tidrContainerptr;
  Uint32 tidrElemhead;
  const Uint32 elemLen = fragrecptr.p->elementLength;
  Uint32 elemStep;
  OperationrecPtr oprecptr;
  tshrRemLen = conlen - Container::HEADER_SIZE;
  if (isforward)
  {
    jam();
    tshrElementptr = conptr + Container::HEADER_SIZE;
    elemStep = elemLen;
  }
  else
  {
    jam();
    elemStep = 0 - elemLen;
    tshrElementptr = conptr + elemStep;
  }//if
 SHR_LOOP:
  oprecptr.i = RNIL;
  ptrNull(oprecptr);
  /* --------------------------------------------------------------------------------- */
  /*       THE CODE BELOW IS ALL USED TO PREPARE FOR THE CALL TO INSERT_ELEMENT AND    */
  /*       HANDLE THE RESULT FROM INSERT_ELEMENT. INSERT_ELEMENT INSERTS THE ELEMENT   */
  /*       INTO ANOTHER BUCKET.                                                        */
  /* --------------------------------------------------------------------------------- */
  arrGuard(tshrElementptr, 2048);
  tidrElemhead = pageptr.p->word32[tshrElementptr];
  if (ElementHeader::getLocked(tidrElemhead)) {
    jam();
    /* --------------------------------------------------------------------------------- */
    /*       IF THE ELEMENT IS LOCKED WE MUST UPDATE THE ELEMENT INFO IN THE OPERATION   */
    /*       RECORD OWNING THE LOCK. WE DO THIS BY READING THE OPERATION RECORD POINTER  */
    /*       FROM THE ELEMENT HEADER.                                                    */
    /* --------------------------------------------------------------------------------- */
    oprecptr.i = ElementHeader::getOpPtrI(tidrElemhead);
    ndbrequire(oprec_pool.getValidPtr(oprecptr));
    oprecptr.p->reducedHashValue.shift_in(true);
  }//if
  else
  {
    LHBits16 reducedHashValue = ElementHeader::getReducedHashValue(tidrElemhead);
    reducedHashValue.shift_in(true);
    tidrElemhead = ElementHeader::setReducedHashValue(tidrElemhead, reducedHashValue);
  }
  {
    ndbrequire(fragrecptr.p->localkeylen == 1);
    const Uint32 localkey = pageptr.p->word32[tshrElementptr + 1];
    Uint32 tidrPageindex = fragrecptr.p->expReceiveIndex;
    Page8Ptr idrPageptr;
    idrPageptr.i = fragrecptr.p->expReceivePageptr;
    c_page8_pool.getPtr(idrPageptr);
    bool tidrIsforward = fragrecptr.p->expReceiveIsforward;
    insertElement(Element(tidrElemhead, localkey),
                  oprecptr,
                  idrPageptr,
                  tidrPageindex,
                  tidrIsforward,
                  tidrContainerptr,
                  ContainerHeader(pageptr.p->word32[conptr]).getScanBits(),
                  false);
    /* --------------------------------------------------------------- */
    /*       TAKE CARE OF RESULT FROM INSERT_ELEMENT.                  */
    /* --------------------------------------------------------------- */
    fragrecptr.p->expReceiveIndex = tidrPageindex;
    fragrecptr.p->expReceivePageptr = idrPageptr.i;
    fragrecptr.p->expReceiveIsforward = tidrIsforward;
  }
  if (tshrRemLen < elemLen) {
    jam();
    sendSystemerror(__LINE__);
  }//if
  tshrRemLen = tshrRemLen - elemLen;
  if (tshrRemLen != 0) {
    jam();
    tshrElementptr += elemStep;
    goto SHR_LOOP;
  }//if
}//Dbacc::shrinkcontainer()

void Dbacc::initFragAdd(Signal* signal,
                        FragmentrecPtr regFragPtr) const
{
  const AccFragReq * const req = (AccFragReq*)&signal->theData[0];  
  Uint32 minLoadFactor = (req->minLoadFactor * ZBUF_SIZE) / 100;
  Uint32 maxLoadFactor = (req->maxLoadFactor * ZBUF_SIZE) / 100;
  if (ERROR_INSERTED(3003)) // use small LoadFactors to force sparse hash table
  {
    jam();
    minLoadFactor = 1;
    maxLoadFactor = 2; 
  }
  if (minLoadFactor >= maxLoadFactor) {
    jam();
    minLoadFactor = maxLoadFactor - 1;
  }//if
  regFragPtr.p->fragState = ACTIVEFRAG;
  // NOTE: next line must match calculation in Dblqh::execLQHFRAGREQ
  regFragPtr.p->myfid = req->fragId;
  regFragPtr.p->myTableId = req->tableId;
  ndbrequire(req->kValue == 6);
  ndbrequire(req->kValue == regFragPtr.p->k);
  regFragPtr.p->expandCounter = 0;

  /**
   * Only allow shrink during SR
   *   - to make sure we don't run out of pages during REDO log execution
   *
   * Is later restored to 0 by LQH at end of REDO log execution
   */
  regFragPtr.p->expandOrShrinkQueued = false;
  regFragPtr.p->level.setSize(1 << req->kValue);
  regFragPtr.p->minloadfactor = minLoadFactor;
  regFragPtr.p->maxloadfactor = maxLoadFactor;
  regFragPtr.p->slack = Int64(regFragPtr.p->level.getSize()) * maxLoadFactor;
  regFragPtr.p->localkeylen = req->localKeyLen;
  regFragPtr.p->nodetype = (req->reqInfo >> 4) & 0x3;
  regFragPtr.p->keyLength = req->keyLength;
  ndbrequire(req->keyLength != 0);
  ndbrequire(regFragPtr.p->elementLength ==
             ZELEM_HEAD_SIZE + regFragPtr.p->localkeylen);
  Uint32 Tmp1 = regFragPtr.p->level.getSize();
  Uint32 Tmp2 = regFragPtr.p->maxloadfactor - regFragPtr.p->minloadfactor;
  regFragPtr.p->slackCheck = Int64(Tmp1) * Tmp2;
  regFragPtr.p->mytabptr = req->tableId;
  regFragPtr.p->roothashcheck = req->kValue + req->lhFragBits;
  regFragPtr.p->m_commit_count = 0; // stable results
  for (Uint32 i = 0; i < MAX_PARALLEL_SCANS_PER_FRAG; i++) {
    regFragPtr.p->scan[i] = RNIL;
  }//for
  
  Uint32 hasCharAttr = g_key_descriptor_pool.getPtr(req->tableId)->hasCharAttr;
  regFragPtr.p->hasCharAttr = hasCharAttr;
  for (Uint32 i = 0; i < NUM_ACC_FRAGMENT_MUTEXES; i++)
  {
    NdbMutex_Init(&regFragPtr.p->acc_frag_mutex[i]);
  }
}//Dbacc::initFragAdd()

void Dbacc::initFragGeneral(FragmentrecPtr regFragPtr)const
{
  new (&regFragPtr.p->directory) DynArr256::Head();

  regFragPtr.p->lockCount = 0;
  regFragPtr.p->hasCharAttr = ZFALSE;
  regFragPtr.p->dirRangeFull = ZFALSE;
  regFragPtr.p->fragState = FREEFRAG;

  regFragPtr.p->sparsepages.init();
  regFragPtr.p->fullpages.init();
  regFragPtr.p->m_noOfAllocatedPages = 0;
  regFragPtr.p->activeScanMask = 0;

  regFragPtr.p->m_lockStats.init();
}//Dbacc::initFragGeneral()


void Dbacc::execACC_SCANREQ(Signal* signal) //Direct Executed
{
  jamEntry();
  AccScanReq * req = (AccScanReq*)&signal->theData[0];
  Uint32 userptr = req->senderData;
  BlockReference userblockref = req->senderRef;
  tabptr.i = req->tableId;
  Uint32 fid = req->fragmentNo;
  Uint32 scanFlag = req->requestInfo;
  Uint32 scanTrid1 = req->transId1;
  Uint32 scanTrid2 = req->transId2;
  
  ptrCheckGuard(tabptr, ctablesize, tabrec);
  ndbrequire(getfragmentrec(fragrecptr, fid));
  
  Uint32 i;
  for (i = 0; i < MAX_PARALLEL_SCANS_PER_FRAG; i++) {
    jam();
    if (fragrecptr.p->scan[i] == RNIL) {
      jam();
      break;
    }
  }
  ndbrequire(i != MAX_PARALLEL_SCANS_PER_FRAG);
  if (unlikely(!scanRec_pool.seize(scanPtr)))
  {
    signal->theData[8] = AccScanRef::AccNoFreeScanOp;
    return;
  }

  fragrecptr.p->scan[i] = scanPtr.i;
  scanPtr.p->scanBucketState =  ScanRec::FIRST_LAP;
  scanPtr.p->scanLockMode = AccScanReq::getLockMode(scanFlag);
  scanPtr.p->scanReadCommittedFlag = AccScanReq::getReadCommittedFlag(scanFlag);
  /* TWELVE BITS OF THE ELEMENT HEAD ARE SCAN */
  /* CHECK BITS. THE MASK NOTES WHICH BIT IS */
  /* ALLOCATED FOR THE ACTIVE SCAN */
  scanPtr.p->scanMask = 1 << i;
  scanPtr.p->scanUserptr = userptr;
  scanPtr.p->scanUserblockref = userblockref;
  scanPtr.p->scanTrid1 = scanTrid1;
  scanPtr.p->scanTrid2 = scanTrid2;
  scanPtr.p->scanState = ScanRec::WAIT_NEXT;
  scanPtr.p->scan_lastSeen = __LINE__;
  initScanFragmentPart();

  /* ************************ */
  /*  ACC_SCANCONF            */
  /* ************************ */
  signal->theData[0] = scanPtr.p->scanUserptr;
  signal->theData[1] = scanPtr.i;
  signal->theData[2] = 1; /* NR OF LOCAL FRAGMENT */
  signal->theData[3] = fragrecptr.p->fragmentid;
  signal->theData[4] = RNIL;
  signal->theData[7] = AccScanConf::ZNOT_EMPTY_FRAGMENT;
  signal->theData[8] = 0; /* Success */
  /**
   * Return with signal->theData[8] == 0 indicates ACC_SCANCONF
   * return signal.
   */
  return;
}//Dbacc::execACC_SCANREQ()

/* ******************--------------------------------------------------------------- */
/*  NEXT_SCANREQ                                       REQUEST FOR NEXT ELEMENT OF   */
/* ******************------------------------------+   A FRAGMENT.                   */
/*   SENDER: LQH,    LEVEL B       */
void Dbacc::execNEXT_SCANREQ(Signal* signal) 
{
  Uint32 tscanNextFlag;
  jamEntryDebug();
  scanPtr.i = signal->theData[0];
  ndbrequire(scanRec_pool.getUncheckedPtrRW(scanPtr));
  operationRecPtr.i = signal->theData[1];
  tscanNextFlag = signal->theData[2];
  /* ------------------------------------------ */
  /* 1 = ZCOPY_NEXT  GET NEXT ELEMENT           */
  /* 2 = ZCOPY_NEXT_COMMIT COMMIT THE           */
  /* ACTIVE ELEMENT AND GET THE NEXT ONE        */
  /* 3 = ZCOPY_COMMIT COMMIT THE ACTIVE ELEMENT */
  /* 4 = ZCOPY_REPEAT GET THE ACTIVE ELEMENT    */
  /* 5 = ZCOPY_ABORT RELOCK THE ACTIVE ELEMENT  */
  /* 6 = ZCOPY_CLOSE THE SCAN PROCESS IS READY  */
  /* ------------------------------------------ */
  ndbrequire(scanPtr.p->scanState == ScanRec::WAIT_NEXT);
  ndbrequire(Magic::check_ptr(scanPtr.p));

  switch (tscanNextFlag) {
  case NextScanReq::ZSCAN_NEXT:
    jam();
    /*empty*/;
    break;
  case NextScanReq::ZSCAN_NEXT_COMMIT:
  case NextScanReq::ZSCAN_COMMIT:
    jam();
    /* --------------------------------------------------------------------- */
    /* COMMIT ACTIVE OPERATION. 
     * SEND NEXT SCAN ELEMENT IF IT IS ZCOPY_NEXT_COMMIT.
     * --------------------------------------------------------------------- */
    ndbrequire(oprec_pool.getUncheckedPtrRW(operationRecPtr));
    fragrecptr.i = operationRecPtr.p->fragptr;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    ndbrequire(Magic::check_ptr(operationRecPtr.p));
    if (!scanPtr.p->scanReadCommittedFlag) {
      commitOperation(signal);
    }//if
    operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
    takeOutActiveScanOp();
    releaseOpRec();
    scanPtr.p->scanOpsAllocated--;
    if (tscanNextFlag == NextScanReq::ZSCAN_COMMIT) {
      jam();
      signal->theData[0] = 0; /* Success */
      /**
       * signal->theData[0] = 0 indicates NEXT_SCANCONF return
       * signal for NextScanReq::ZSCAN_COMMIT
       */
      return;
    }//if
    break;
  case NextScanReq::ZSCAN_CLOSE:
    jam();
    fragrecptr.i = scanPtr.p->activeLocalFrag;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    ndbassert(fragrecptr.p->activeScanMask & scanPtr.p->scanMask);
    /* ---------------------------------------------------------------------
     * THE SCAN PROCESS IS FINISHED. RELOCK ALL LOCKED EL. 
     * RELEASE ALL INVOLVED REC.
     * ------------------------------------------------------------------- */
    releaseScanLab(signal);
    return;
  default:
    ndbabort();
  }//switch
  scanPtr.p->scan_lastSeen = __LINE__;
  signal->theData[0] = scanPtr.i;
  signal->theData[1] = AccCheckScan::ZNOT_CHECK_LCP_STOP;
  execACC_CHECK_SCAN(signal);
  return;
}//Dbacc::execNEXT_SCANREQ()

void Dbacc::checkNextBucketLab(Signal* signal)
{
  Page8Ptr nsPageptr;
  Page8Ptr gnsPageidptr;
  Page8Ptr tnsPageidptr;
  Uint32 tnsElementptr;
  Uint32 tnsContainerptr;
  Uint32 tnsIsLocked;
  Uint32 tnsCopyDir;

  tnsCopyDir = fragrecptr.p->getPageNumber(scanPtr.p->nextBucketIndex);
  tnsPageidptr.i = getPagePtr(fragrecptr.p->directory, tnsCopyDir);
  c_page8_pool.getPtr(tnsPageidptr);
  gnsPageidptr.i = tnsPageidptr.i;
  gnsPageidptr.p = tnsPageidptr.p;
  Uint32 conidx = fragrecptr.p->getPageIndex(scanPtr.p->nextBucketIndex);
  Page8Ptr pageptr;
  pageptr.i = gnsPageidptr.i;
  pageptr.p = gnsPageidptr.p;
  Uint32 conptr;
  bool isforward;
  Uint32 elemptr;
  Uint32 islocked;
  if (!getScanElement(pageptr, conidx, conptr, isforward, elemptr, islocked))
  {
    scanPtr.p->nextBucketIndex++;
    if (scanPtr.p->scanBucketState ==  ScanRec::SECOND_LAP)
    {
      if (scanPtr.p->nextBucketIndex > scanPtr.p->maxBucketIndexToRescan)
      {
	/* ---------------------------------------------------------------- */
	// We have finished the rescan phase. 
	// We are ready to proceed with the next fragment part.
	/* ---------------------------------------------------------------- */
        jam();
        checkNextFragmentLab(signal);
        return;
      }//if
    }
    else if (scanPtr.p->scanBucketState ==  ScanRec::FIRST_LAP)
    {
      if (fragrecptr.p->level.getTop() < scanPtr.p->nextBucketIndex)
      {
	/* ---------------------------------------------------------------- */
	// All buckets have been scanned a first time.
	/* ---------------------------------------------------------------- */
        if (scanPtr.p->minBucketIndexToRescan == 0xFFFFFFFF)
        {
          jam();
	  /* -------------------------------------------------------------- */
	  // We have not had any merges behind the scan. 
	  // Thus it is not necessary to perform any rescan any buckets 
	  // and we can proceed immediately with the next fragment part.
	  /* --------------------------------------------------------------- */
          checkNextFragmentLab(signal);
          return;
        }
        else
        {
          jam();
	  /**
	   * Some buckets are in the need of rescanning due to merges that have
           * moved records from in front of the scan to behind the scan. During
           * the merges we kept track of which buckets that need a rescan.
           * We start with the minimum and end with maximum.
           */
          scanPtr.p->nextBucketIndex = scanPtr.p->minBucketIndexToRescan;
	  scanPtr.p->scanBucketState =  ScanRec::SECOND_LAP;
          if (scanPtr.p->maxBucketIndexToRescan > fragrecptr.p->level.getTop())
          {
            jam();
	    /**
	     * If we have had so many merges that the maximum is bigger than
             * the number of buckets then we will simply satisfy ourselves with
             * scanning to the end. This can only happen after bringing down
             * the total of buckets to less than half and the minimum should
	     * be 0 otherwise there is some problem.
             */
            if (scanPtr.p->minBucketIndexToRescan != 0)
            {
              jam();
              sendSystemerror(__LINE__);
              return;
            }//if
            scanPtr.p->maxBucketIndexToRescan = fragrecptr.p->level.getTop();
          }//if
        }//if
      }//if
    }//if
    if ((scanPtr.p->scanBucketState ==  ScanRec::FIRST_LAP) &&
        (scanPtr.p->nextBucketIndex <= scanPtr.p->startNoOfBuckets))
    {
      /**
       * We will only reset the scan indicator on the buckets that existed at
       * the start of the scan. The others will be handled by the split and
       * merge code.
       */
      Uint32 conidx = fragrecptr.p->getPageIndex(scanPtr.p->nextBucketIndex);
      if (conidx == 0)
      {
        jam();
        Uint32 pagei = fragrecptr.p->getPageNumber(scanPtr.p->nextBucketIndex);
        gnsPageidptr.i = getPagePtr(fragrecptr.p->directory, pagei);
        c_page8_pool.getPtr(gnsPageidptr);
      }//if
      ndbassert(!scanPtr.p->isInContainer());
      releaseScanBucket(gnsPageidptr, conidx, scanPtr.p->scanMask);
    }//if
    releaseFreeOpRec();
    scanPtr.p->scan_lastSeen = __LINE__;
    BlockReference ref = scanPtr.p->scanUserblockref;
    signal->theData[0] = scanPtr.p->scanUserptr;
    signal->theData[1] = GSN_ACC_CHECK_SCAN;
    signal->theData[2] = AccCheckScan::ZCHECK_LCP_STOP;
    sendSignal(ref, GSN_ACC_CHECK_SCAN, signal, 3, JBB);
    return;
  }//if
  /* ----------------------------------------------------------------------- */
  /*	AN ELEMENT WHICH HAVE NOT BEEN SCANNED WAS FOUND. WE WILL PREPARE IT */
  /*	TO BE SENT TO THE LQH BLOCK FOR FURTHER PROCESSING.                  */
  /*    WE ASSUME THERE ARE OPERATION RECORDS AVAILABLE SINCE LQH SHOULD HAVE*/
  /*    GUARANTEED THAT THROUGH EARLY BOOKING.                               */
  /* ----------------------------------------------------------------------- */
  tnsIsLocked = islocked;
  tnsElementptr = elemptr;
  tnsContainerptr = conptr;
  nsPageptr.i = pageptr.i;
  nsPageptr.p = pageptr.p;
  ndbrequire(cfreeopRec != RNIL);
  operationRecPtr.i = cfreeopRec;
  cfreeopRec = RNIL;
  ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
  initScanOpRec(nsPageptr, tnsContainerptr, tnsElementptr);
 
  if (!tnsIsLocked){
    if (!scanPtr.p->scanReadCommittedFlag) {
      jam();
      /* Immediate lock grant as element unlocked */
      fragrecptr.p->m_lockStats.
        req_start_imm_ok(scanPtr.p->scanLockMode != ZREADLOCK,
                         operationRecPtr.p->m_lockTime,
                         getHighResTimer());
      
      setlock(nsPageptr, tnsElementptr);
      fragrecptr.p->lockCount++;
      operationRecPtr.p->m_op_bits |=
        Operationrec::OP_LOCK_OWNER |
        Operationrec::OP_STATE_RUNNING | Operationrec::OP_RUN_QUEUE;
    }//if
  } else {
    arrGuard(tnsElementptr, 2048);
    queOperPtr.i = 
      ElementHeader::getOpPtrI(nsPageptr.p->word32[tnsElementptr]);
    ndbrequire(oprec_pool.getValidPtr(queOperPtr));
    if (queOperPtr.p->m_op_bits & Operationrec::OP_ELEMENT_DISAPPEARED ||
	queOperPtr.p->localdata.isInvalid())
    {
      jam();
      /* ------------------------------------------------------------------ */
      // If the lock owner indicates the element is disappeared then 
      // we will not report this tuple. We will continue with the next tuple.
      /* ------------------------------------------------------------------ */
      /* FC : Is this correct, shouldn't we wait for lock holder commit? */
      operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
      releaseOpRec();
      scanPtr.p->scanOpsAllocated--;
      scanPtr.p->scan_lastSeen = __LINE__;
      BlockReference ref = scanPtr.p->scanUserblockref;
      signal->theData[0] = scanPtr.p->scanUserptr;
      signal->theData[1] = GSN_ACC_CHECK_SCAN;
      signal->theData[2] = AccCheckScan::ZCHECK_LCP_STOP;
      sendSignal(ref, GSN_ACC_CHECK_SCAN, signal, 3, JBB);
      return;
    }//if
    if (!scanPtr.p->scanReadCommittedFlag) {
      Uint32 return_result;
      if (scanPtr.p->scanLockMode == ZREADLOCK) {
        jam();
        return_result = placeReadInLockQueue(queOperPtr);
      } else {
        jam();
        return_result = placeWriteInLockQueue(queOperPtr);
      }//if
      if (return_result == ZSERIAL_QUEUE) {
	/* -----------------------------------------------------------------
	 * WE PLACED THE OPERATION INTO A SERIAL QUEUE AND THUS WE HAVE TO 
	 * WAIT FOR THE LOCK TO BE RELEASED. WE CONTINUE WITH THE NEXT ELEMENT
	 * ----------------------------------------------------------------- */
        fragrecptr.p->
          m_lockStats.req_start(scanPtr.p->scanLockMode != ZREADLOCK,
                                operationRecPtr.p->m_lockTime,
                                getHighResTimer());
        putOpScanLockQue();	/* PUT THE OP IN A QUE IN THE SCAN REC */
        scanPtr.p->scan_lastSeen = __LINE__;
        BlockReference ref = scanPtr.p->scanUserblockref;
        signal->theData[0] = scanPtr.p->scanUserptr;
        signal->theData[1] = GSN_ACC_CHECK_SCAN;
        signal->theData[2] = AccCheckScan::ZCHECK_LCP_STOP;
        sendSignal(ref, GSN_ACC_CHECK_SCAN, signal, 3, JBB);
        return;
      } else if (return_result != ZPARALLEL_QUEUE) {
        jam();
	/* ----------------------------------------------------------------- */
	// The tuple is either not committed yet or a delete in 
	// the same transaction (not possible here since we are a scan). 
	// Thus we simply continue with the next tuple.
	/* ----------------------------------------------------------------- */
	operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
        releaseOpRec();
	scanPtr.p->scanOpsAllocated--;
        scanPtr.p->scan_lastSeen = __LINE__;
        BlockReference ref = scanPtr.p->scanUserblockref;
        signal->theData[0] = scanPtr.p->scanUserptr;
        signal->theData[1] = GSN_ACC_CHECK_SCAN;
        signal->theData[2] = AccCheckScan::ZCHECK_LCP_STOP;
        sendSignal(ref, GSN_ACC_CHECK_SCAN, signal, 3, JBB);
        return;
      }//if
      ndbassert(return_result == ZPARALLEL_QUEUE);
      /* We got into the parallel queue - immediate grant */
      fragrecptr.p->m_lockStats.
        req_start_imm_ok(scanPtr.p->scanLockMode != ZREADLOCK,
                         operationRecPtr.p->m_lockTime,
                         getHighResTimer());
    }//if
  }//if
  /* ----------------------------------------------------------------------- */
  // Committed read proceed without caring for locks immediately 
  // down here except when the tuple was deleted permanently 
  // and no new operation has inserted it again.
  /* ----------------------------------------------------------------------- */
  scanPtr.p->scan_lastSeen = __LINE__;
  putActiveScanOp();
  sendNextScanConf(signal);
  return;
}//Dbacc::checkNextBucketLab()


void Dbacc::checkNextFragmentLab(Signal* signal)
{
  scanPtr.p->scanBucketState =  ScanRec::SCAN_COMPLETED;
  // The scan is completed. ACC_CHECK_SCAN will perform all the necessary 
  // checks to see
  // what the next step is.
  releaseFreeOpRec();
  signal->theData[0] = scanPtr.i;
  signal->theData[1] = AccCheckScan::ZCHECK_LCP_STOP;
  execACC_CHECK_SCAN(signal);
  return;
}//Dbacc::checkNextFragmentLab()

void Dbacc::initScanFragmentPart()
{
  Page8Ptr cnfPageidptr;
  /* ----------------------------------------------------------------------- */
  // Set the active fragment part.
  // Set the current bucket scanned to the first.
  // Start with the first lap.
  // Remember the number of buckets at start of the scan.
  // Set the minimum and maximum to values that will always be smaller and 
  //    larger than.
  // Reset the scan indicator on the first bucket.
  /* ----------------------------------------------------------------------- */
  scanPtr.p->activeLocalFrag = fragrecptr.i;
  scanPtr.p->nextBucketIndex = 0;	/* INDEX OF SCAN BUCKET */
  ndbassert(!scanPtr.p->isInContainer());
  scanPtr.p->scanBucketState = ScanRec::FIRST_LAP;
  scanPtr.p->startNoOfBuckets = fragrecptr.p->level.getTop();
  scanPtr.p->minBucketIndexToRescan = 0xFFFFFFFF;
  scanPtr.p->maxBucketIndexToRescan = 0;
  cnfPageidptr.i = getPagePtr(fragrecptr.p->directory, 0);
  c_page8_pool.getPtr(cnfPageidptr);
  const Uint32 conidx = fragrecptr.p->getPageIndex(scanPtr.p->nextBucketIndex);
  ndbassert(!(fragrecptr.p->activeScanMask & scanPtr.p->scanMask));
  ndbassert(!scanPtr.p->isInContainer());
  releaseScanBucket(cnfPageidptr, conidx, scanPtr.p->scanMask);
  fragrecptr.p->activeScanMask |= scanPtr.p->scanMask;
}//Dbacc::initScanFragmentPart()

/* -------------------------------------------------------------------------
 * FLAG = 6 = ZCOPY_CLOSE THE SCAN PROCESS IS READY OR ABORTED. 
 * ALL OPERATION IN THE ACTIVE OR WAIT QUEUE ARE RELEASED, 
 * SCAN FLAG OF ROOT FRAG IS RESET AND THE SCAN RECORD IS RELEASED.
 * ------------------------------------------------------------------------ */
void Dbacc::releaseScanLab(Signal* signal)
{
  releaseAndCommitActiveOps(signal);
  releaseAndCommitQueuedOps(signal);
  releaseAndAbortLockedOps(signal);

  fragrecptr.i = scanPtr.p->activeLocalFrag;
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  ndbassert(fragrecptr.p->activeScanMask & scanPtr.p->scanMask);

  /**
   * Dont leave partial scanned bucket as partial scanned.
   * Elements scanbits must match containers scanbits.
   */
  if ((scanPtr.p->scanBucketState ==  ScanRec::FIRST_LAP &&
       scanPtr.p->nextBucketIndex <= fragrecptr.p->level.getTop()) ||
      (scanPtr.p->scanBucketState ==  ScanRec::SECOND_LAP &&
       scanPtr.p->nextBucketIndex <= scanPtr.p->maxBucketIndexToRescan))
  {
    jam();
    Uint32 conidx = fragrecptr.p->getPageIndex(scanPtr.p->nextBucketIndex);
    Uint32 pagei = fragrecptr.p->getPageNumber(scanPtr.p->nextBucketIndex);
    Page8Ptr pageptr;
    pageptr.i = getPagePtr(fragrecptr.p->directory, pagei);
    c_page8_pool.getPtr(pageptr);

    Uint32 inPageI;
    Uint32 inConptr;
    if(scanPtr.p->getContainer(inPageI, inConptr))
    {
      Page8Ptr page;
      page.i = inPageI;
      c_page8_pool.getPtr(page);
      ContainerHeader conhead(page.p->word32[inConptr]);
      scanPtr.p->leaveContainer(inPageI, inConptr);
      page.p->clearScanContainer(scanPtr.p->scanMask, inConptr);
      if (!page.p->checkScanContainer(inConptr))
      {
        conhead.clearScanInProgress();
        page.p->word32[inConptr] = Uint32(conhead);
      }
    }
    releaseScanBucket(pageptr, conidx, scanPtr.p->scanMask);
  }

  for (Uint32 i = 0; i < MAX_PARALLEL_SCANS_PER_FRAG; i++) {
    jam();
    if (fragrecptr.p->scan[i] == scanPtr.i)
    {
      jam();
      fragrecptr.p->scan[i] = RNIL;
    }//if
  }//for
  // Stops the heartbeat
  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scanPtr.p->scanUserptr;
  conf->accOperationPtr = RNIL;
  conf->fragId = RNIL;
  fragrecptr.p->activeScanMask &= ~scanPtr.p->scanMask;
  releaseScanRec();
  signal->setLength(NextScanConf::SignalLengthNoTuple);
  c_lqh->exec_next_scan_conf(signal);
  return;
}//Dbacc::releaseScanLab()


void Dbacc::releaseAndCommitActiveOps(Signal* signal)
{
  OperationrecPtr trsoOperPtr;
  operationRecPtr.i = scanPtr.p->scanFirstActiveOp;
  while (operationRecPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
    trsoOperPtr.i = operationRecPtr.p->nextOp;
    fragrecptr.i = operationRecPtr.p->fragptr;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    if (!scanPtr.p->scanReadCommittedFlag) {
      jam();
      if ((operationRecPtr.p->m_op_bits & Operationrec::OP_STATE_MASK) ==
	  Operationrec::OP_STATE_EXECUTED)
      {
	commitOperation(signal);
      }
      else
      {
	abortOperation(signal);
      }
    }//if
    operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
    takeOutActiveScanOp();
    releaseOpRec();
    scanPtr.p->scanOpsAllocated--;
    operationRecPtr.i = trsoOperPtr.i;
  }//if
}//Dbacc::releaseAndCommitActiveOps()


void Dbacc::releaseAndCommitQueuedOps(Signal* signal)
{
  OperationrecPtr trsoOperPtr;
  operationRecPtr.i = scanPtr.p->scanFirstQueuedOp;
  while (operationRecPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
    trsoOperPtr.i = operationRecPtr.p->nextOp;
    fragrecptr.i = operationRecPtr.p->fragptr;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    if (!scanPtr.p->scanReadCommittedFlag) {
      jam();
      if ((operationRecPtr.p->m_op_bits & Operationrec::OP_STATE_MASK) ==
	  Operationrec::OP_STATE_EXECUTED)
      {
	commitOperation(signal);
      }
      else
      {
	abortOperation(signal);
      }
    }//if
    operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
    takeOutReadyScanQueue();
    releaseOpRec();
    scanPtr.p->scanOpsAllocated--;
    operationRecPtr.i = trsoOperPtr.i;
  }//if
}//Dbacc::releaseAndCommitQueuedOps()

void Dbacc::releaseAndAbortLockedOps(Signal* signal) {

  OperationrecPtr trsoOperPtr;
  operationRecPtr.i = scanPtr.p->scanFirstLockedOp;
  while (operationRecPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
    trsoOperPtr.i = operationRecPtr.p->nextOp;
    fragrecptr.i = operationRecPtr.p->fragptr;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
    if (!scanPtr.p->scanReadCommittedFlag) {
      jam();
      abortOperation(signal);
    }//if
    takeOutScanLockQueue(scanPtr.i);
    operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
    releaseOpRec();
    scanPtr.p->scanOpsAllocated--;
    operationRecPtr.i = trsoOperPtr.i;
  }//if
}//Dbacc::releaseAndAbortLockedOps()

/* 3.18.3  ACC_CHECK_SCAN */
/* ******************--------------------------------------------------------------- */
/* ACC_CHECK_SCAN                                                                    */
/*          ENTER ACC_CHECK_SCAN WITH                                                */
/*                    SCAN_PTR                                                       */
/* ******************--------------------------------------------------------------- */
/* ******************--------------------------------------------------------------- */
/* ACC_CHECK_SCAN                                                                    */
/* ******************------------------------------+                                 */
void Dbacc::execACC_CHECK_SCAN(Signal* signal) 
{
  Uint32 TcheckLcpStop;
  jamEntryDebug();
  scanPtr.i = signal->theData[0];
  ndbrequire(scanRec_pool.getUncheckedPtrRW(scanPtr));
  TcheckLcpStop = signal->theData[1];
  Uint32 firstQueuedOp = scanPtr.p->scanFirstQueuedOp;
  ndbrequire(Magic::check_ptr(scanPtr.p));
  while (firstQueuedOp != RNIL)
  {
    jamDebug();
    //---------------------------------------------------------------------
    // An operation has been released from the lock queue. 
    // We are in the parallel queue of this tuple. We are 
    // ready to report the tuple now.
    //------------------------------------------------------------------------
    operationRecPtr.i = scanPtr.p->scanFirstQueuedOp;
    ndbrequire(oprec_pool.getValidPtr(operationRecPtr));
    takeOutReadyScanQueue();
    fragrecptr.i = operationRecPtr.p->fragptr;
    ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);

    /* Scan op that had to wait for a lock is now runnable */
    fragrecptr.p->m_lockStats.wait_ok(scanPtr.p->scanLockMode != ZREADLOCK,
                                      operationRecPtr.p->m_lockTime,
                                      getHighResTimer());

    if (operationRecPtr.p->m_op_bits & Operationrec::OP_ELEMENT_DISAPPEARED) 
    {
      jam();
      /**
       * Despite aborting, this is an 'ok' wait.
       * This op is waking up to find the entity it locked has gone.
       * As a 'QueuedOp', we are in the parallel queue of the element, so 
       * at the abort below we don't double-count abort as a failure.
       */
      abortOperation(signal);
      operationRecPtr.p->m_op_bits = Operationrec::OP_INITIAL;
      releaseOpRec();
      scanPtr.p->scanOpsAllocated--;
      firstQueuedOp = scanPtr.p->scanFirstQueuedOp;
      continue;
    }//if
    scanPtr.p->scan_lastSeen = __LINE__;
    putActiveScanOp();
    sendNextScanConf(signal);
    return;
  }//while


  if ((scanPtr.p->scanBucketState == ScanRec::SCAN_COMPLETED) &&
      (scanPtr.p->scanLockHeld == 0)) {
    jam();
    //----------------------------------------------------------------------------
    // The scan is now completed and there are no more locks outstanding. Thus we
    // we will report the scan as completed to LQH.
    //----------------------------------------------------------------------------
    scanPtr.p->scan_lastSeen = __LINE__;
    releaseFreeOpRec();
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scanPtr.p->scanUserptr;
    conf->accOperationPtr = RNIL;
    conf->fragId = RNIL;
    signal->setLength(NextScanConf::SignalLengthNoTuple);
    c_lqh->exec_next_scan_conf(signal);
    return;
  }//if
  if (TcheckLcpStop == AccCheckScan::ZCHECK_LCP_STOP) {
  //---------------------------------------------------------------------------
  // To ensure that the block of the fragment occurring at the start of a local
  // checkpoint is not held for too long we insert a release and reacquiring of
  // that lock here. This is performed in LQH. If we are blocked or if we have
  // requested a sleep then we will receive RNIL in the returning signal word.
  //---------------------------------------------------------------------------
    signal->theData[0] = scanPtr.p->scanUserptr;
    signal->theData[1] =
      (((scanPtr.p->scanLockHeld >= ZSCAN_MAX_LOCK) ||
        (scanPtr.p->scanBucketState ==  ScanRec::SCAN_COMPLETED)) ?
       CheckLcpStop::ZSCAN_RESOURCE_WAIT:
       CheckLcpStop::ZSCAN_RUNNABLE);

    c_lqh->execCHECK_LCP_STOP(signal);
    jamEntryDebug();
    if (signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK) {
      jamDebug();
      scanPtr.p->scan_lastSeen = __LINE__;
      /* WE ARE ENTERING A REAL-TIME BREAK FOR A SCAN HERE */
      return;
    }//if
  }//if
  /**
   * If we have more than max locks held OR
   * scan is completed AND at least one lock held
   *  - Inform LQH about this condition
   * Also when no free operation records to handle lock
   * operations.
   */
  if (cfreeopRec == RNIL)
  {
    OperationrecPtr opPtr;
    if (oprec_pool.seize(opPtr))
    {
      jam();
      cfreeopRec = opPtr.i;
    }
    else
    {
      signal->theData[0] = scanPtr.p->scanUserptr;
      signal->theData[1] = CheckLcpStop::ZSCAN_RESOURCE_WAIT_STOPPABLE;
      c_lqh->execCHECK_LCP_STOP(signal);
      if (signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK)
      {
        jamEntryDebug();
        scanPtr.p->scan_lastSeen = __LINE__;
        /* WE ARE ENTERING A REAL-TIME BREAK FOR A SCAN HERE */
        return;
      }
      jamEntryDebug();
      ndbrequire(signal->theData[0] == CheckLcpStop::ZABORT_SCAN);
      /*
       * Fall through, cfreeOpRec == RNIL will lead to NEXT_SCANCONF
       * CHECK_LCP_STOP has already prepared LQH by setting complete
       * status to true.
       */
    }
  }
  if ((scanPtr.p->scanLockHeld >= ZSCAN_MAX_LOCK) ||
      (cfreeopRec == RNIL) ||
      ((scanPtr.p->scanBucketState == ScanRec::SCAN_COMPLETED) &&
       (scanPtr.p->scanLockHeld > 0))) {
    jam();
    scanPtr.p->scan_lastSeen = __LINE__;
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scanPtr.p->scanUserptr;
    conf->accOperationPtr = RNIL;
    conf->fragId = 512; // MASV
    /* WE ARE ENTERING A REAL-TIME BREAK FOR A SCAN HERE */
    sendSignal(scanPtr.p->scanUserblockref,
               GSN_NEXT_SCANCONF,
               signal,
               NextScanConf::SignalLengthNoTuple,
               JBB);
    return;
  }
  if (scanPtr.p->scanBucketState == ScanRec::SCAN_COMPLETED) {
    jam();
    releaseFreeOpRec();
    signal->theData[0] = scanPtr.i;
    signal->theData[1] = AccCheckScan::ZCHECK_LCP_STOP;
    execACC_CHECK_SCAN(signal);
    return;
  }//if

  fragrecptr.i = scanPtr.p->activeLocalFrag;
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  ndbassert(fragrecptr.p->activeScanMask & scanPtr.p->scanMask);
  checkNextBucketLab(signal);
  return;
}//Dbacc::execACC_CHECK_SCAN()

/* ******************---------------------------------------------------- */
/* ACC_TO_REQ                                       PERFORM A TAKE OVER   */
/* ******************-------------------+                                 */
/*   SENDER: LQH,    LEVEL B       */
void Dbacc::execACC_TO_REQ(Signal* signal) 
{
  OperationrecPtr tatrOpPtr;

  jamEntry();
  tatrOpPtr.i = signal->theData[1];     /*  OPER PTR OF ACC                */
  ndbrequire(oprec_pool.getValidPtr(tatrOpPtr));

  /* Only scan locks can be taken over */
  if ((tatrOpPtr.p->m_op_bits & Operationrec::OP_MASK) == ZSCAN_OP)
  {
    if (signal->theData[2] == tatrOpPtr.p->transId1 &&
        signal->theData[3] == tatrOpPtr.p->transId2)
    {
      /* If lock is from same transaction as take over, lock can
       * be taken over several times.
       *
       * This occurs for example in this scenario:
       *
       * create table t (x int primary key, y int);
       * insert into t (x, y) values (1, 0);
       * begin;
       * # Scan and lock rows in t, update using take over operation.
       * update t set y = 1;
       * # The second update on same row, will take over the same lock as previous update
       * update t set y = 2;
       * commit;
       */
      return;
    }
    else if (tatrOpPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER &&
             tatrOpPtr.p->nextParallelQue == RNIL)
    {
      /* If lock is taken over from other transaction it must be
       * the only one in the parallel queue.  Otherwise one could
       * end up with mixing operations from different transaction
       * in a parallel queue.
       */
      tatrOpPtr.p->transId1 = signal->theData[2];
      tatrOpPtr.p->transId2 = signal->theData[3];
      validate_lock_queue(tatrOpPtr);
      return;
    }
  }
  jam();
  signal->theData[0] = Uint32(-1);
  signal->theData[1] = ZTO_OP_STATE_ERROR;
  return;
}//Dbacc::execACC_TO_REQ()

/** ---------------------------------------------------------------------------
 * Get next unscanned element in fragment.
 *
 * @param[in,out]  pageptr    Page of first container to scan, on return
 *                            container for found element.
 * @param[in,out]  conidx     Index within page for first container to scan, on
 *                            return container for found element.
 * @param[out]     conptr     Pointer within page of first container to scan,
 *                            on return container for found element.
 * @param[in,out]  isforward  Direction of first container to scan, on return
 *                            the direction of container for found element.
 * @param[out]     elemptr    Pointer within page of next element in scan.
 * @param[out]     islocked   Indicates if element is locked.
 * @return                    Return true if an unscanned element was found.
 * ------------------------------------------------------------------------- */
bool Dbacc::getScanElement(Page8Ptr& pageptr,
                           Uint32& conidx,
                           Uint32& conptr,
                           bool& isforward,
                           Uint32& elemptr,
                           Uint32& islocked) const
{
  /* Input is always the bucket header container */
  isforward = true;
  /* Check if scan is already active in a container */
  Uint32 inPageI;
  Uint32 inConptr;
  if (scanPtr.p->getContainer(inPageI, inConptr))
  {
    // TODO: in VM_TRACE double check container is in bucket!
    pageptr.i = inPageI;
    c_page8_pool.getPtr(pageptr);
    conptr = inConptr;
    ContainerHeader conhead(pageptr.p->word32[conptr]);
    ndbassert(conhead.isScanInProgress());
    ndbassert((conhead.getScanBits() & scanPtr.p->scanMask)==0);
    getContainerIndex(conptr, conidx, isforward);
  }
  else // if first bucket is not in scan nor scanned , start it
  {
    Uint32 conptr = getContainerPtr(conidx, isforward);
    ContainerHeader containerhead(pageptr.p->word32[conptr]);
    if (!(containerhead.getScanBits() & scanPtr.p->scanMask))
    {
      if(!containerhead.isScanInProgress())
      {
        containerhead.setScanInProgress();
        pageptr.p->word32[conptr] = containerhead;
      }
      scanPtr.p->enterContainer(pageptr.i, conptr);
      pageptr.p->setScanContainer(scanPtr.p->scanMask, conptr);
    }
  }
 NEXTSEARCH_SCAN_LOOP:
  conptr = getContainerPtr(conidx, isforward);
  ContainerHeader containerhead(pageptr.p->word32[conptr]);
  Uint32 conlen = containerhead.getLength();
  if (containerhead.getScanBits() & scanPtr.p->scanMask)
  { // Already scanned, go to next.
    ndbassert(!pageptr.p->checkScans(scanPtr.p->scanMask, conptr));
  }
  else
  {
    ndbassert(containerhead.isScanInProgress());
    if (searchScanContainer(pageptr,
                            conptr,
                            isforward,
                            conlen,
                            elemptr,
                            islocked))
    {
      jam();
      return true;
    }//if
  }
  if ((containerhead.getScanBits() & scanPtr.p->scanMask) == 0)
  {
    containerhead.setScanBits(scanPtr.p->scanMask);
    scanPtr.p->leaveContainer(pageptr.i, conptr);
    pageptr.p->clearScanContainer(scanPtr.p->scanMask, conptr);
    if (!pageptr.p->checkScanContainer(conptr))
    {
      containerhead.clearScanInProgress();
    }
    pageptr.p->word32[conptr] = Uint32(containerhead);
  }
  if (containerhead.haveNext())
  {
    jam();
    nextcontainerinfo(pageptr, conptr, containerhead, conidx, isforward);
    conptr=getContainerPtr(conidx,isforward);
    containerhead=pageptr.p->word32[conptr];
    if ((containerhead.getScanBits() & scanPtr.p->scanMask) == 0)
    {
      if(!containerhead.isScanInProgress())
      {
        containerhead.setScanInProgress();
      }
      pageptr.p->word32[conptr] = Uint32(containerhead);
      scanPtr.p->enterContainer(pageptr.i, conptr);
      pageptr.p->setScanContainer(scanPtr.p->scanMask, conptr);
    } // else already scanned, get next
    goto NEXTSEARCH_SCAN_LOOP;
  }//if
  pageptr.p->word32[conptr] = Uint32(containerhead);
  return false;
}//Dbacc::getScanElement()

/* --------------------------------------------------------------------------------- */
/*  INIT_SCAN_OP_REC                                                                 */
/* --------------------------------------------------------------------------------- */
void Dbacc::initScanOpRec(Page8Ptr pageptr,
                          Uint32 conptr,
                          Uint32 elemptr) const
{
  Uint32 tisoLocalPtr;
  Uint32 localkeylen = fragrecptr.p->localkeylen;

  scanPtr.p->scanOpsAllocated++;

  Uint32 opbits = 0;
  opbits |= ZSCAN_OP;
  opbits |= scanPtr.p->scanLockMode ? (Uint32) Operationrec::OP_LOCK_MODE : 0;
  opbits |= scanPtr.p->scanLockMode ? (Uint32) Operationrec::OP_ACC_LOCK_MODE : 0;
  opbits |= (scanPtr.p->scanReadCommittedFlag ? 
             (Uint32) Operationrec::OP_EXECUTED_DIRTY_READ : 0);
  opbits |= Operationrec::OP_COMMIT_DELETE_CHECK;
  operationRecPtr.p->userptr = RNIL;
  operationRecPtr.p->scanRecPtr = scanPtr.i;
  operationRecPtr.p->fid = fragrecptr.p->myfid;
  operationRecPtr.p->fragptr = fragrecptr.i;
  operationRecPtr.p->nextParallelQue = RNIL;
  operationRecPtr.p->prevParallelQue = RNIL;
  operationRecPtr.p->nextSerialQue = RNIL;
  operationRecPtr.p->prevSerialQue = RNIL;
  operationRecPtr.p->transId1 = scanPtr.p->scanTrid1;
  operationRecPtr.p->transId2 = scanPtr.p->scanTrid2;
  operationRecPtr.p->elementContainer = conptr;
  operationRecPtr.p->elementPointer = elemptr;
  operationRecPtr.p->elementPage = pageptr.i;
  operationRecPtr.p->m_op_bits = opbits;
  tisoLocalPtr = elemptr + 1;

  arrGuard(tisoLocalPtr, 2048);
  if(ElementHeader::getUnlocked(pageptr.p->word32[elemptr]))
  {
    Local_key key;
    key.m_page_no = pageptr.p->word32[tisoLocalPtr];
    key.m_page_idx = ElementHeader::getPageIdx(pageptr.p->word32[elemptr]);
    operationRecPtr.p->localdata = key;
  }
  else
  {
    OperationrecPtr oprec;
    oprec.i = ElementHeader::getOpPtrI(pageptr.p->word32[elemptr]);
    ndbrequire(oprec_pool.getValidPtr(oprec));
    ndbassert(oprec.p->localdata.m_page_no == pageptr.p->word32[tisoLocalPtr]);
    operationRecPtr.p->localdata = oprec.p->localdata;
  }
  tisoLocalPtr = tisoLocalPtr + 1;
  ndbrequire(localkeylen == 1)
  operationRecPtr.p->hashValue.clear();
  operationRecPtr.p->tupkeylen = fragrecptr.p->keyLength;
  operationRecPtr.p->m_scanOpDeleteCountOpRef = RNIL;
  NdbTick_Invalidate(&operationRecPtr.p->m_lockTime);
}//Dbacc::initScanOpRec()

/* ----------------------------------------------------------------------------
 * Get information of next container.
 *
 * @param[in,out] pageptr          Page of current container, and on return to
 *                                 next container.
 * @param[in]     conptr           Pointer within page to current container.
 * @param[in]     containerheader  Header of current container.
 * @param[out]    nextConidx       Index within page to next container.
 * @param[out]    nextIsforward    Direction of next container.
 * ------------------------------------------------------------------------- */
void Dbacc::nextcontainerinfo(Page8Ptr& pageptr,
                              Uint32 conptr,
                              ContainerHeader containerhead,
                              Uint32& nextConidx,
                              bool& nextIsforward) const
{
  /* THE NEXT CONTAINER IS IN THE SAME PAGE */
  nextConidx = containerhead.getNextIndexNumber();
  if (containerhead.getNextEnd() == ZLEFT)
  {
    jam();
    nextIsforward = true;
  }
  else if (containerhead.getNextEnd() == ZRIGHT)
  {
    jam();
    nextIsforward = false;
  }
  else
  {
    ndbrequire(containerhead.getNextEnd() == ZLEFT ||
               containerhead.getNextEnd() == ZRIGHT);
  }
  if (!containerhead.isNextOnSamePage())
  {
    jam();
    /* NEXT CONTAINER IS IN AN OVERFLOW PAGE */
    arrGuard(conptr + 1, 2048);
    pageptr.i = pageptr.p->word32[conptr + 1];
    c_page8_pool.getPtr(pageptr);
  }//if
}//Dbacc::nextcontainerinfo()

/* --------------------------------------------------------------------------------- */
/* PUT_ACTIVE_SCAN_OP                                                                */
/* --------------------------------------------------------------------------------- */
void Dbacc::putActiveScanOp() const
{
  OperationrecPtr pasOperationRecPtr;
  pasOperationRecPtr.i = scanPtr.p->scanFirstActiveOp;
  if (pasOperationRecPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(pasOperationRecPtr));
    pasOperationRecPtr.p->prevOp = operationRecPtr.i;
  }//if
  operationRecPtr.p->nextOp = pasOperationRecPtr.i;
  operationRecPtr.p->prevOp = RNIL;
  scanPtr.p->scanFirstActiveOp = operationRecPtr.i;
}//Dbacc::putActiveScanOp()

/**
 * putOpScanLockQueue
 *
 * Description: Put an operation in the doubly linked 
 * lock list on a scan record. The list is used to 
 * keep track of which operations belonging
 * to the scan are put in serial lock list of another 
 * operation
 *
 * @note Use takeOutScanLockQueue to remove an operation
 *       from the list
 *
 */
void Dbacc::putOpScanLockQue() const
{
  OperationrecPtr pslOperationRecPtr;
  ScanRec theScanRec;
  theScanRec = *scanPtr.p;

  pslOperationRecPtr.i = scanPtr.p->scanLastLockedOp;
  operationRecPtr.p->prevOp = pslOperationRecPtr.i;
  operationRecPtr.p->nextOp = RNIL;
  if (pslOperationRecPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(pslOperationRecPtr));
    pslOperationRecPtr.p->nextOp = operationRecPtr.i;
  } else {
    jam();
    scanPtr.p->scanFirstLockedOp = operationRecPtr.i;
  }//if
  scanPtr.p->scanLastLockedOp = operationRecPtr.i;
  scanPtr.p->scanLockHeld++;
  scanPtr.p->scanLockCount++;

}//Dbacc::putOpScanLockQue()

/* --------------------------------------------------------------------------------- */
/* PUT_READY_SCAN_QUEUE                                                              */
/* --------------------------------------------------------------------------------- */
void Dbacc::putReadyScanQueue(Uint32 scanRecIndex) const
{
  OperationrecPtr prsOperationRecPtr;
  ScanRecPtr TscanPtr;

  TscanPtr.i = scanRecIndex;
  ndbrequire(scanRec_pool.getValidPtr(TscanPtr));

  prsOperationRecPtr.i = TscanPtr.p->scanLastQueuedOp;
  operationRecPtr.p->prevOp = prsOperationRecPtr.i;
  operationRecPtr.p->nextOp = RNIL;
  TscanPtr.p->scanLastQueuedOp = operationRecPtr.i;
  if (prsOperationRecPtr.i != RNIL) {
    jam();
    ndbrequire(oprec_pool.getValidPtr(prsOperationRecPtr));
    prsOperationRecPtr.p->nextOp = operationRecPtr.i;
  } else {
    jam();
    TscanPtr.p->scanFirstQueuedOp = operationRecPtr.i;
  }//if
}//Dbacc::putReadyScanQueue()

/** ---------------------------------------------------------------------------
 * Reset scan bit for all elements within a bucket.
 *
 * Which scan bit are determined by scanPtr.
 *
 * @param[in]  pageptr  Page of first container of bucket
 * @param[in]  conidx   Index within page to first container of bucket
 * @param[in]  scanMask Scan bit mask for scan bits that should be cleared
 * ------------------------------------------------------------------------- */
void Dbacc::releaseScanBucket(Page8Ptr pageptr,
                              Uint32 conidx,
                              Uint16 scanMask) const
{
  scanMask |= (~fragrecptr.p->activeScanMask &
               ((1 << MAX_PARALLEL_SCANS_PER_FRAG) - 1));
  bool isforward = true;
 NEXTRELEASESCANLOOP:
  Uint32 conptr = getContainerPtr(conidx, isforward);
  ContainerHeader containerhead(pageptr.p->word32[conptr]);
  Uint32 conlen = containerhead.getLength();
  const Uint16 isScanned = containerhead.getScanBits() & scanMask;
  releaseScanContainer(pageptr, conptr, isforward, conlen, scanMask, isScanned);
  if (isScanned)
  {
    containerhead.clearScanBits(isScanned);
    pageptr.p->word32[conptr] = Uint32(containerhead);
  }
  if (containerhead.getNextEnd() != 0) {
    jam();
    nextcontainerinfo(pageptr, conptr, containerhead, conidx, isforward);
    goto NEXTRELEASESCANLOOP;
  }//if
}//Dbacc::releaseScanBucket()

/** --------------------------------------------------------------------------
 * Reset scan bit of the element for each element in a container.
 * Which scan bit are determined by scanPtr.
 *
 * @param[in]  pageptr  Pointer to page holding container.
 * @param[in]  conptr   Pointer within page to container.
 * @param[in]  isforward  Container growing direction.
 * @param[in]  conlen   Containers current size.
 * @param[in]  scanMask   Scan bits that should be cleared if set
 * @param[in]  allScanned All elements should have this bits set (debug)
 * ------------------------------------------------------------------------- */
void Dbacc::releaseScanContainer(const Page8Ptr pageptr,
                                 const Uint32 conptr,
                                 const bool isforward,
                                 const Uint32 conlen,
                                 const Uint16 scanMask,
                                 const Uint16 allScanned) const
{
  OperationrecPtr rscOperPtr;
  Uint32 trscElemStep;
  Uint32 trscElementptr;
  Uint32 trscElemlens;
  Uint32 trscElemlen;

  if (conlen < 4) {
    if (conlen != Container::HEADER_SIZE) {
      jam();
      sendSystemerror(__LINE__);
    }//if
    return;	/* 2 IS THE MINIMUM SIZE OF THE ELEMENT */
  }//if
  trscElemlens = conlen - Container::HEADER_SIZE;
  trscElemlen = fragrecptr.p->elementLength;
  if (isforward)
  {
    jam();
    trscElementptr = conptr + Container::HEADER_SIZE;
    trscElemStep = trscElemlen;
  }
  else
  {
    jam();
    trscElementptr = conptr - trscElemlen;
    trscElemStep = 0 - trscElemlen;
  }//if
  if (trscElemlens % trscElemlen != 0)
  {
    jam();
    sendSystemerror(__LINE__);
  }//if
}//Dbacc::releaseScanContainer()

/* --------------------------------------------------------------------------------- */
/* RELEASE_SCAN_REC                                                                  */
/* --------------------------------------------------------------------------------- */
void Dbacc::releaseScanRec()
{  
  // Check that all ops this scan has allocated have been 
  // released
  ndbrequire(scanPtr.p->scanOpsAllocated==0);

  // Check that all locks this scan might have acquired
  // have been properly released
  ndbrequire(scanPtr.p->scanLockHeld == 0);
  ndbrequire(scanPtr.p->scanFirstLockedOp == RNIL);
  ndbrequire(scanPtr.p->scanLastLockedOp == RNIL);

  // Check that all active operations have been 
  // properly released
  ndbrequire(scanPtr.p->scanFirstActiveOp == RNIL);

  // Check that all queued operations have been 
  // properly released
  ndbrequire(scanPtr.p->scanFirstQueuedOp == RNIL);
  ndbrequire(scanPtr.p->scanLastQueuedOp == RNIL);

  // Put scan record in free list
  scanRec_pool.release(scanPtr);
  checkPoolShrinkNeed(DBACC_SCAN_RECORD_TRANSIENT_POOL_INDEX,
                      scanRec_pool);
}//Dbacc::releaseScanRec()

/* --------------------------------------------------------------------------------- */
/*  SEARCH_SCAN_CONTAINER                                                            */
/*       INPUT:           TSSC_CONTAINERLEN                                          */
/*                        TSSC_CONTAINERPTR                                          */
/*                        TSSC_ISFORWARD                                             */
/*                        SSC_PAGEIDPTR                                              */
/*                        SCAN_PTR                                                   */
/*       OUTPUT:          TSSC_IS_LOCKED                                             */
/*                                                                                   */
/*            DESCRIPTION: SEARCH IN A CONTAINER TO FIND THE NEXT SCAN ELEMENT.      */
/*                    TO DO THIS THE SCAN BIT OF THE ELEMENT HEADER IS CHECKED. IF   */
/*                    THIS BIT IS ZERO, IT IS SET TO ONE AND THE ELEMENT IS RETURNED.*/
/* --------------------------------------------------------------------------------- */
bool Dbacc::searchScanContainer(Page8Ptr pageptr,
                                Uint32 conptr,
                                bool isforward,
                                Uint32 conlen,
                                Uint32& elemptr,
                                Uint32& islocked) const
{
  OperationrecPtr operPtr;
  Uint32 elemlens;
  Uint32 elemlen;
  Uint32 elemStep;
  Uint32 Telemptr;
  Uint32 Tislocked;

#ifdef VM_TRACE
  ContainerHeader chead(pageptr.p->word32[conptr]);
  ndbassert((chead.getScanBits()&scanPtr.p->scanMask)==0);
  ndbassert(chead.isScanInProgress());
  ndbassert(scanPtr.p->isInContainer());
  {
    Uint32 pagei; Uint32 cptr;
    ndbassert(scanPtr.p->getContainer(pagei, cptr));
    ndbassert(pageptr.i==pagei);
    ndbassert(conptr==cptr);
  }
#endif

  if (conlen < 4) {
    jam();
    return false;	/* 2 IS THE MINIMUM SIZE OF THE ELEMENT */
  }//if
  elemlens = conlen - Container::HEADER_SIZE;
  elemlen = fragrecptr.p->elementLength;
  /* LENGTH OF THE ELEMENT */
  if (isforward)
  {
    jam();
    Telemptr = conptr + Container::HEADER_SIZE;
    elemStep = elemlen;
  }
  else
  {
    jam();
    Telemptr = conptr - elemlen;
    elemStep = 0 - elemlen;
  }//if
 SCANELEMENTLOOP001:
  arrGuard(Telemptr, 2048);
  const Uint32 eh = pageptr.p->word32[Telemptr];
  bool found=false;
  if (!scanPtr.p->isScanned(Telemptr))
  {
    found=true;
    scanPtr.p->setScanned(Telemptr);
  }
  Tislocked = ElementHeader::getLocked(eh);
  if (found)
  {
    elemptr = Telemptr;
    islocked = Tislocked;
    return true;
  }
  ndbassert(!found);
  /* THE ELEMENT IS ALREADY SENT. */
  /* SEARCH FOR NEXT ONE */
  elemlens = elemlens - elemlen;
  if (elemlens > 1) {
    jam();
    Telemptr = Telemptr + elemStep;
    goto SCANELEMENTLOOP001;
  }//if
  return false;
}//Dbacc::searchScanContainer()

/* --------------------------------------------------------------------------------- */
/*  SEND THE RESPONSE NEXT_SCANCONF AND POSSIBLE KEYINFO SIGNALS AS WELL.            */
/* --------------------------------------------------------------------------------- */
void Dbacc::sendNextScanConf(Signal* signal)
{
  const Local_key localKey = operationRecPtr.p->localdata;

  c_tup->prepare_scanTUPKEYREQ(localKey.m_page_no, localKey.m_page_idx);

  const Uint32 scanUserPtr = scanPtr.p->scanUserptr;
  const Uint32 opPtrI = operationRecPtr.i;
  const Uint32 fid = operationRecPtr.p->fid;
  /** ---------------------------------------------------------------------
   * LQH WILL NOT HAVE ANY USE OF THE TUPLE KEY LENGTH IN THIS CASE AND 
   * SO WE DO NOT PROVIDE IT. IN THIS CASE THESE VALUES ARE UNDEFINED. 
   * ---------------------------------------------------------------------- */
  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scanUserPtr;
  conf->accOperationPtr = opPtrI;
  conf->fragId = fid;
  conf->localKey[0] = localKey.m_page_no;
  conf->localKey[1] = localKey.m_page_idx;
  signal->setLength(NextScanConf::SignalLengthNoGCI);
  c_lqh->exec_next_scan_conf(signal);
}//Dbacc::sendNextScanConf()

/** ---------------------------------------------------------------------------
 * Sets lock on an element.
 *
 * Information about the element is copied from element head into operation
 * record.  A pointer to operation record are inserted in element header
 * instead.
 *
 * @param[in]  pageptr  Pointer to page holding element.
 * @param[in]  elemptr  Pointer within page to element.
 * ------------------------------------------------------------------------- */
void Dbacc::setlock(Page8Ptr pageptr, Uint32 elemptr) const
{
  Uint32 tselTmp1;

  arrGuard(elemptr, 2048);
  tselTmp1 = pageptr.p->word32[elemptr];
  operationRecPtr.p->reducedHashValue = ElementHeader::getReducedHashValue(tselTmp1);

  tselTmp1 = ElementHeader::setLocked(operationRecPtr.i);
  pageptr.p->word32[elemptr] = tselTmp1;
}//Dbacc::setlock()

/* --------------------------------------------------------------------------------- */
/*  TAKE_OUT_ACTIVE_SCAN_OP                                                          */
/*         DESCRIPTION: AN ACTIVE SCAN OPERATION IS BELOGED TO AN ACTIVE LIST OF THE */
/*                      SCAN RECORD. BY THIS SUBRUTIN THE LIST IS UPDATED.           */
/* --------------------------------------------------------------------------------- */
void Dbacc::takeOutActiveScanOp() const
{
  OperationrecPtr tasOperationRecPtr;

  if (operationRecPtr.p->prevOp != RNIL) {
    jam();
    tasOperationRecPtr.i = operationRecPtr.p->prevOp;
    ndbrequire(oprec_pool.getValidPtr(tasOperationRecPtr));
    tasOperationRecPtr.p->nextOp = operationRecPtr.p->nextOp;
  } else {
    jam();
    scanPtr.p->scanFirstActiveOp = operationRecPtr.p->nextOp;
  }//if
  if (operationRecPtr.p->nextOp != RNIL) {
    jam();
    tasOperationRecPtr.i = operationRecPtr.p->nextOp;
    ndbrequire(oprec_pool.getValidPtr(tasOperationRecPtr));
    tasOperationRecPtr.p->prevOp = operationRecPtr.p->prevOp;
  }//if
}//Dbacc::takeOutActiveScanOp()

/**
 * takeOutScanLockQueue
 *
 * Description: Take out an operation from the doubly linked 
 * lock list on a scan record.
 *
 * @note Use putOpScanLockQue to insert a operation in 
 *       the list
 *
 */
void Dbacc::takeOutScanLockQueue(Uint32 scanRecIndex) const
{
  OperationrecPtr tslOperationRecPtr;
  ScanRecPtr TscanPtr;

  TscanPtr.i = scanRecIndex;
  ndbrequire(scanRec_pool.getValidPtr(TscanPtr));

  if (operationRecPtr.p->prevOp != RNIL) {
    jam();
    tslOperationRecPtr.i = operationRecPtr.p->prevOp;
    ndbrequire(oprec_pool.getValidPtr(tslOperationRecPtr));
    tslOperationRecPtr.p->nextOp = operationRecPtr.p->nextOp;
  } else {
    jam();
    // Check that first are pointing at operation to take out
    ndbrequire(TscanPtr.p->scanFirstLockedOp==operationRecPtr.i);
    TscanPtr.p->scanFirstLockedOp = operationRecPtr.p->nextOp;
  }//if
  if (operationRecPtr.p->nextOp != RNIL) {
    jam();
    tslOperationRecPtr.i = operationRecPtr.p->nextOp;
    ndbrequire(oprec_pool.getValidPtr(tslOperationRecPtr));
    tslOperationRecPtr.p->prevOp = operationRecPtr.p->prevOp;
  } else {
    jam();
    // Check that last are pointing at operation to take out
    ndbrequire(TscanPtr.p->scanLastLockedOp==operationRecPtr.i);
    TscanPtr.p->scanLastLockedOp = operationRecPtr.p->prevOp;
  }//if
  TscanPtr.p->scanLockHeld--;
}//Dbacc::takeOutScanLockQueue()

/* --------------------------------------------------------------------------------- */
/* TAKE_OUT_READY_SCAN_QUEUE                                                         */
/* --------------------------------------------------------------------------------- */
void Dbacc::takeOutReadyScanQueue() const
{
  OperationrecPtr trsOperationRecPtr;

  if (operationRecPtr.p->prevOp != RNIL) {
    jam();
    trsOperationRecPtr.i = operationRecPtr.p->prevOp;
    ndbrequire(oprec_pool.getValidPtr(trsOperationRecPtr));
    trsOperationRecPtr.p->nextOp = operationRecPtr.p->nextOp;
  } else {
    jam();
    scanPtr.p->scanFirstQueuedOp = operationRecPtr.p->nextOp;
  }//if
  if (operationRecPtr.p->nextOp != RNIL) {
    jam();
    trsOperationRecPtr.i = operationRecPtr.p->nextOp;
    ndbrequire(oprec_pool.getValidPtr(trsOperationRecPtr));
    trsOperationRecPtr.p->prevOp = operationRecPtr.p->prevOp;
  } else {
    jam();
    scanPtr.p->scanLastQueuedOp = operationRecPtr.p->nextOp;
  }//if
}//Dbacc::takeOutReadyScanQueue()

/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/*                                                                                   */
/*       END OF SCAN MODULE                                                          */
/*                                                                                   */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */

bool Dbacc::getfragmentrec(FragmentrecPtr& rootPtr, Uint32 fid)
{
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragholder); i++) {
    jam();
    if (tabptr.p->fragholder[i] == fid) {
      jam();
      fragrecptr.i = tabptr.p->fragptrholder[i];
      ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
      return true;
    }//if
  }//for
  return false;
}//Dbacc::getrootfragmentrec()

/* --------------------------------------------------------------------------------- */
/* INIT_OVERPAGE                                                                     */
/*         INPUT. IOP_PAGEPTR, POINTER TO AN OVERFLOW PAGE RECORD                    */
/*         DESCRIPTION: CONTAINERS AND FREE LISTS OF THE PAGE, GET INITIALE VALUE    */
/*         ACCORDING TO LH3 AND PAGE STRUCTOR DESCRIPTION OF NDBACC BLOCK            */
/* --------------------------------------------------------------------------------- */
void Dbacc::initOverpage(Page8Ptr iopPageptr)
{
  Page32* p32 = reinterpret_cast<Page32*>(iopPageptr.p - (iopPageptr.i % 4));
  ndbrequire(p32->magic == Page32::MAGIC);
  Uint32 tiopPrevFree;
  Uint32 tiopNextFree;

  // Clear page, but keep page list entries
  // Setting word32[ALLOC_CONTAINERS] and word32[CHECK_SUM] to zero is essential
  Uint32 nextPage = iopPageptr.p->word32[Page8::NEXT_PAGE];
  Uint32 prevPage = iopPageptr.p->word32[Page8::PREV_PAGE];
  std::memset(iopPageptr.p->word32 + Page8::P32_WORD_COUNT,
              0,
              sizeof(iopPageptr.p->word32) - Page8::P32_WORD_COUNT * sizeof(Uint32));
  iopPageptr.p->word32[Page8::NEXT_PAGE] = nextPage;
  iopPageptr.p->word32[Page8::PREV_PAGE] = prevPage;

  iopPageptr.p->word32[Page8::EMPTY_LIST] = (1 << ZPOS_PAGE_TYPE_BIT);
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE PREVIOUS PART OF DOUBLY LINKED LIST FOR LEFT CONTAINERS.         */
  /* --------------------------------------------------------------------------------- */
  Uint32 iopIndex = ZHEAD_SIZE + 1;
  iopPageptr.p->word32[iopIndex] = Container::NO_CONTAINER_INDEX;
  for (tiopPrevFree = 0; tiopPrevFree <= Container::MAX_CONTAINER_INDEX - 1; tiopPrevFree++) {
    iopIndex = iopIndex + ZBUF_SIZE;
    iopPageptr.p->word32[iopIndex] = tiopPrevFree;
  }//for
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE NEXT PART OF DOUBLY LINKED LIST FOR LEFT CONTAINERS.             */
  /* --------------------------------------------------------------------------------- */
  iopIndex = ZHEAD_SIZE;
  for (tiopNextFree = 1; tiopNextFree <= Container::MAX_CONTAINER_INDEX; tiopNextFree++) {
    iopPageptr.p->word32[iopIndex] = tiopNextFree;
    iopIndex = iopIndex + ZBUF_SIZE;
  }//for
  iopPageptr.p->word32[iopIndex] = Container::NO_CONTAINER_INDEX;	/* LEFT_LIST IS UPDATED */
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE PREVIOUS PART OF DOUBLY LINKED LIST FOR RIGHT CONTAINERS.        */
  /* --------------------------------------------------------------------------------- */
  iopIndex = (ZBUF_SIZE + ZHEAD_SIZE) - 1;
  iopPageptr.p->word32[iopIndex] = Container::NO_CONTAINER_INDEX;
  for (tiopPrevFree = 0; tiopPrevFree <= Container::MAX_CONTAINER_INDEX - 1; tiopPrevFree++) {
    iopIndex = iopIndex + ZBUF_SIZE;
    iopPageptr.p->word32[iopIndex] = tiopPrevFree;
  }//for
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE NEXT PART OF DOUBLY LINKED LIST FOR RIGHT CONTAINERS.            */
  /* --------------------------------------------------------------------------------- */
  iopIndex = (ZBUF_SIZE + ZHEAD_SIZE) - 2;
  for (tiopNextFree = 1; tiopNextFree <= Container::MAX_CONTAINER_INDEX; tiopNextFree++) {
    iopPageptr.p->word32[iopIndex] = tiopNextFree;
    iopIndex = iopIndex + ZBUF_SIZE;
  }//for
  iopPageptr.p->word32[iopIndex] = Container::NO_CONTAINER_INDEX;	/* RIGHT_LIST IS UPDATED */
}//Dbacc::initOverpage()

/* --------------------------------------------------------------------------------- */
/* INIT_PAGE                                                                         */
/*         INPUT. INP_PAGEPTR, POINTER TO A PAGE RECORD                              */
/*         DESCRIPTION: CONTAINERS AND FREE LISTS OF THE PAGE, GET INITIALE VALUE    */
/*         ACCORDING TO LH3 AND PAGE STRUCTOR DISACRIPTION OF NDBACC BLOCK           */
/* --------------------------------------------------------------------------------- */
void Dbacc::initPage(Page8Ptr inpPageptr, Uint32 tipPageId)
{
  Uint32 tinpIndex;
  Uint32 tinpTmp;
  Uint32 tinpPrevFree;
  Uint32 tinpNextFree;

  Page32* p32 = reinterpret_cast<Page32*>(inpPageptr.p - (inpPageptr.i % 4));
  ndbrequire(p32->magic == Page32::MAGIC);
  for (Uint32 i = Page8::P32_WORD_COUNT; i <= 2047; i++)
  {
    // Do not clear page list
    if (i == Page8::NEXT_PAGE) continue;
    if (i == Page8::PREV_PAGE) continue;

    inpPageptr.p->word32[i] = 0;
  }//for
  /* --------------------------------------------------------------------------------- */
  /*       SET PAGE ID FOR USE OF CHECKPOINTER.                                        */
  /*       PREPARE CONTAINER HEADERS INDICATING EMPTY CONTAINERS WITHOUT NEXT.         */
  /* --------------------------------------------------------------------------------- */
  inpPageptr.p->word32[Page8::PAGE_ID] = tipPageId;
  ContainerHeader tinpTmp1;
  tinpTmp1.initInUse();
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE ZNO_CONTAINERS PREDEFINED HEADERS ON LEFT SIZE.                  */
  /* --------------------------------------------------------------------------------- */
  tinpIndex = ZHEAD_SIZE;
  for (tinpTmp = 0; tinpTmp <= ZNO_CONTAINERS - 1; tinpTmp++) {
    inpPageptr.p->word32[tinpIndex] = tinpTmp1;
    tinpIndex = tinpIndex + ZBUF_SIZE;
  }//for
  /* WORD32(Page8::EMPTY_LIST) DATA STRUCTURE:*/
  /*--------------------------------------- */
  /*| PAGE TYPE|LEFT FREE|RIGHT FREE        */
  /*|     1    |  LIST   |  LIST            */
  /*|    BIT   | 7 BITS  | 7 BITS           */
  /*--------------------------------------- */
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE FIRST POINTER TO DOUBLY LINKED LIST OF FREE CONTAINERS.          */
  /*       INITIALISE LEFT FREE LIST TO 64 AND RIGHT FREE LIST TO ZERO.                */
  /*       ALSO INITIALISE PAGE TYPE TO NOT OVERFLOW PAGE.                             */
  /* --------------------------------------------------------------------------------- */
  tinpTmp = (ZNO_CONTAINERS << 7);
  inpPageptr.p->word32[Page8::EMPTY_LIST] = tinpTmp;
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE PREVIOUS PART OF DOUBLY LINKED LIST FOR RIGHT CONTAINERS.        */
  /* --------------------------------------------------------------------------------- */
  tinpIndex = (ZHEAD_SIZE + ZBUF_SIZE) - 1;
  inpPageptr.p->word32[tinpIndex] = Container::NO_CONTAINER_INDEX;
  for (tinpPrevFree = 0; tinpPrevFree <= Container::MAX_CONTAINER_INDEX - 1; tinpPrevFree++) {
    tinpIndex = tinpIndex + ZBUF_SIZE;
    inpPageptr.p->word32[tinpIndex] = tinpPrevFree;
  }//for
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE NEXT PART OF DOUBLY LINKED LIST FOR RIGHT CONTAINERS.            */
  /* --------------------------------------------------------------------------------- */
  tinpIndex = (ZHEAD_SIZE + ZBUF_SIZE) - 2;
  for (tinpNextFree = 1; tinpNextFree <= Container::MAX_CONTAINER_INDEX; tinpNextFree++) {
    inpPageptr.p->word32[tinpIndex] = tinpNextFree;
    tinpIndex = tinpIndex + ZBUF_SIZE;
  }//for
  inpPageptr.p->word32[tinpIndex] = Container::NO_CONTAINER_INDEX;
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE PREVIOUS PART OF DOUBLY LINKED LIST FOR LEFT CONTAINERS.         */
  /*       THE FIRST ZNO_CONTAINERS ARE NOT PUT INTO FREE LIST SINCE THEY ARE          */
  /*       PREDEFINED AS OCCUPIED.                                                     */
  /* --------------------------------------------------------------------------------- */
  tinpIndex = (ZNO_CONTAINERS * ZBUF_SIZE) + ZHEAD_SIZE;
  for (tinpNextFree = ZNO_CONTAINERS + 1; tinpNextFree <= Container::MAX_CONTAINER_INDEX; tinpNextFree++) {
    inpPageptr.p->word32[tinpIndex] = tinpNextFree;
    tinpIndex = tinpIndex + ZBUF_SIZE;
  }//for
  inpPageptr.p->word32[tinpIndex] = Container::NO_CONTAINER_INDEX;
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE NEXT PART OF DOUBLY LINKED LIST FOR LEFT CONTAINERS.             */
  /*       THE FIRST ZNO_CONTAINERS ARE NOT PUT INTO FREE LIST SINCE THEY ARE          */
  /*       PREDEFINED AS OCCUPIED.                                                     */
  /* --------------------------------------------------------------------------------- */
  tinpIndex = ((ZNO_CONTAINERS * ZBUF_SIZE) + ZHEAD_SIZE) + 1;
  inpPageptr.p->word32[tinpIndex] = Container::NO_CONTAINER_INDEX;
  for (tinpPrevFree = ZNO_CONTAINERS; tinpPrevFree <= Container::MAX_CONTAINER_INDEX - 1; tinpPrevFree++) {
    tinpIndex = tinpIndex + ZBUF_SIZE;
    inpPageptr.p->word32[tinpIndex] = tinpPrevFree;
  }//for
  /* --------------------------------------------------------------------------------- */
  /*       INITIALISE HEADER POSITIONS NOT CURRENTLY USED AND ENSURE USE OF OVERFLOW   */
  /*       RECORD POINTER ON THIS PAGE LEADS TO ERROR.                                 */
  /* --------------------------------------------------------------------------------- */
  inpPageptr.p->word32[Page8::CHECKSUM] = 0;
  inpPageptr.p->word32[Page8::ALLOC_CONTAINERS] = 0;
}//Dbacc::initPage()

/* --------------------------------------------------------------------------------- */
/* RELEASE OP RECORD                                                                 */
/*         PUT A FREE OPERATION IN A FREE LIST OF THE OPERATIONS                     */
/* --------------------------------------------------------------------------------- */
void Dbacc::releaseOpRec()
{
  ndbrequire(operationRecPtr.p->m_op_bits == Operationrec::OP_INITIAL);
  if (likely(operationRecPtr.i != c_copy_frag_oprec))
  {
    oprec_pool.release(operationRecPtr);
    checkPoolShrinkNeed(DBACC_OPERATION_RECORD_TRANSIENT_POOL_INDEX,
                        oprec_pool);
  }
  else
  {
    /**
     * We initialise object by releasing it and seizing it again.
     * This will call both the destructor code and the constructor code
     * to ensure the operation object is properly initialised before used
     * again.
     * Since this is the very first object seized it will get the first
     * reserved slot and since no one has a chance to come in between AND
     * we only have this single free reserved slot since all others are
     * allocated and managed by LQH. Therefore we can be sure to get back
     * to the same record again.
     */
    oprec_pool.release(operationRecPtr);
    ndbrequire(oprec_pool.seize(operationRecPtr));
    ndbrequire(operationRecPtr.i == c_copy_frag_oprec);
  }
}

void Dbacc::releaseFreeOpRec()
{
  if (cfreeopRec != RNIL)
  {
    OperationrecPtr opPtr;
    opPtr.i = cfreeopRec;
    cfreeopRec = RNIL;
    ndbrequire(oprec_pool.getValidPtr(opPtr));
    ndbrequire(opPtr.p->m_op_bits == Operationrec::OP_INITIAL);
    oprec_pool.release(opPtr);
    checkPoolShrinkNeed(DBACC_OPERATION_RECORD_TRANSIENT_POOL_INDEX,
                        oprec_pool);
  }
}

/* --------------------------------------------------------------------------------- */
/* RELEASE_OVERPAGE                                                                  */
/* --------------------------------------------------------------------------------- */
void Dbacc::releaseOverpage(Page8Ptr ropPageptr)
{
  jam();
  {
    LocalContainerPageList sparselist(c_page8_pool, fragrecptr.p->sparsepages);
    sparselist.remove(ropPageptr);
  }
  jam();
  releasePage_lock(ropPageptr);
}//Dbacc::releaseOverpage()


/* ------------------------------------------------------------------------- */
/* RELEASE_PAGE                                                              */
/* ------------------------------------------------------------------------- */
void Dbacc::releasePage_lock(Page8Ptr rpPageptr)
{
  Dblqh *lqh_block;
  Dbacc *acc_block;
  bool lock_flag = get_lock_information(&acc_block, &lqh_block);
  if (lock_flag)
  {
    NdbMutex_Lock(lqh_block->m_lock_acc_page_mutex);
  }
  acc_block->releasePage(rpPageptr, fragrecptr, jamBuffer());
  if (lock_flag)
  {
    NdbMutex_Unlock(lqh_block->m_lock_acc_page_mutex);
  }
}

void Dbacc::releasePage(Page8Ptr rpPageptr,
                        FragmentrecPtr fragPtr,
                        EmulatedJamBuffer *jamBuf)
{
  thrjam(jamBuf);
  ndbrequire(!m_is_in_query_thread);
  pages.releasePage8(c_page_pool, rpPageptr);
  cnoOfAllocatedPages--;
  fragPtr.p->m_noOfAllocatedPages--;

  Page32Ptr page32ptr;
  pages.dropLastPage32(c_page_pool, page32ptr, 5);
  if (page32ptr.i != RNIL)
  {
    g_acc_pages_used[instance()]--;
    ndbassert(cpageCount >= 4);
    cpageCount -= 4; // 8KiB pages per 32KiB page
    m_ctx.m_mm.release_page(RT_DBACC_PAGE, page32ptr.i);
  }

  ndbassert(pages.getCount() ==
            cfreepages.getCount() + cnoOfAllocatedPages);
  ndbassert(pages.getCount() <= cpageCount);
}//Dbacc::releasePage()

bool Dbacc::validatePageCount() const
{
  jam();
  FragmentrecPtr regFragPtr;
  Uint32 pageCount = 0;
  for (regFragPtr.i = 0; regFragPtr.i < cfragmentsize; regFragPtr.i++)
  {
    ptrAss(regFragPtr, fragmentrec);
    pageCount += regFragPtr.p->m_noOfAllocatedPages;
  }
  return pageCount==cnoOfAllocatedPages;
}//Dbacc::validatePageCount()


Uint64 Dbacc::getLinHashByteSize(Uint32 fragId) const
{
  ndbassert(validatePageCount());
  FragmentrecPtr fragPtr(NULL, fragId);
  ptrCheck(fragPtr, cfragmentsize, fragmentrec);
  if (unlikely(fragPtr.p == NULL))
  {
    jam();
    ndbassert(false);
    return 0;
  }
  else
  {
    jam();
    ndbassert(fragPtr.p->fragState == ACTIVEFRAG);
    return fragPtr.p->m_noOfAllocatedPages * static_cast<Uint64>(sizeof(Page8));
  }
}

/* --------------------------------------------------------------------------------- */
/* SEIZE    FRAGREC                                                                  */
/* --------------------------------------------------------------------------------- */
void Dbacc::seizeFragrec()
{
  RSS_OP_ALLOC(cnoOfFreeFragrec);
  fragrecptr.i = cfirstfreefrag;
  ptrCheckGuard(fragrecptr, cfragmentsize, fragmentrec);
  cfirstfreefrag = fragrecptr.p->nextfreefrag;
  fragrecptr.p->nextfreefrag = RNIL;
}//Dbacc::seizeFragrec()

/** 
 * A ZPAGESIZE_ERROR has occurred, out of index pages
 * Print some debug info if debug compiled
 */
void Dbacc::zpagesize_error(const char* where){
  ACC_DEBUG(where << endl
	<< "  ZPAGESIZE_ERROR" << endl
        << "  cfreepages.getCount()=" << cfreepages.getCount() << endl
	<< "  cnoOfAllocatedPages="<<cnoOfAllocatedPages);
}


/* ------------------------------------------------------------------------- */
/* SEIZE_PAGE                                                                */
/* ------------------------------------------------------------------------- */
Uint32 Dbacc::seizePage(Page8Ptr& spPageptr,
                        int sub_page_id,
                        bool allow_use_of_spare_pages,
                        FragmentrecPtr fragPtr,
                        EmulatedJamBuffer *jamBuf)
{
  thrjam(jamBuf);
  pages.seizePage8(c_page_pool, spPageptr, sub_page_id);
  if (spPageptr.i == RNIL)
  {
    thrjam(jamBuf);
    /**
     * Need to allocate a new 32KiB page
     */
    Page32Ptr ptr;
    void * p = m_ctx.m_mm.alloc_page(RT_DBACC_PAGE,
                                     &ptr.i,
                                     Ndbd_mem_manager::NDB_ZONE_LE_30);
    if (p == NULL && allow_use_of_spare_pages)
    {
      thrjam(jamBuf);
      p = m_ctx.m_mm.alloc_spare_page(RT_DBACC_PAGE,
                                      &ptr.i,
                                      Ndbd_mem_manager::NDB_ZONE_LE_30);
    }
    if (p == NULL)
    {
      thrjam(jamBuf);
      zpagesize_error("Dbacc::seizePage");
      return Uint32(ZPAGESIZE_ERROR);
    }
    ptr.p = static_cast<Page32*>(p);

    g_acc_pages_used[instance()]++;
    cpageCount += 4; // 8KiB pages per 32KiB page
    pages.addPage32(c_page_pool, ptr);
    pages.seizePage8(c_page_pool, spPageptr, sub_page_id);
    ndbrequire(spPageptr.i != RNIL);
    ndbassert(spPageptr.p == &ptr.p->page8[spPageptr.i % 4]);
    ndbassert((spPageptr.i >> 2) == ptr.i);
  }
  cnoOfAllocatedPages++;
  ndbassert(pages.getCount() == cfreepages.getCount() + cnoOfAllocatedPages);
  ndbassert(pages.getCount() <= cpageCount);
  fragPtr.p->m_noOfAllocatedPages++;

  if (cnoOfAllocatedPages > cnoOfAllocatedPagesMax)
  {
    cnoOfAllocatedPagesMax = cnoOfAllocatedPages;
  }
  return Uint32(0);
}//Dbacc::seizePage()

/* --------------------------------------------------------------------------------- */
/* SEND_SYSTEMERROR                                                                  */
/* --------------------------------------------------------------------------------- */
void Dbacc::sendSystemerror(int line)const
{
  progError(line, NDBD_EXIT_PRGERR);
}//Dbacc::sendSystemerror()

void Dbacc::execDBINFO_SCANREQ(Signal *signal)
{
  jamEntry();
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));

  Ndbinfo::Ratelimit rl;

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    jam();
    const DynArr256Pool::Info pmpInfo = directoryPool.getInfo();

    Ndbinfo::pool_entry pools[] =
    {
      { "ACC Operation Record",
        oprec_pool.getUsed(),
        oprec_pool.getSize(),
        oprec_pool.getEntrySize(),
        oprec_pool.getUsedHi(),
        { 0, 0, 0, 0},
        RT_DBACC_OPERATION},
      { "ACC Scan Record",
        scanRec_pool.getUsed(),
        scanRec_pool.getSize(),
        scanRec_pool.getEntrySize(),
        scanRec_pool.getUsedHi(),
        { 0, 0, 0, 0},
        RT_DBACC_SCAN},
      { "Index memory",
        cnoOfAllocatedPages,
        cpageCount,
        sizeof(Page8),
        cnoOfAllocatedPagesMax,
        { CFG_DB_INDEX_MEM,0,0,0 },
        RG_DATAMEM},
      { "L2PMap pages",
        pmpInfo.pg_count,
        0,                  /* No real limit */
        pmpInfo.pg_byte_sz,
        /*
          No HWM for this row as it would be a fixed fraction of "Data memory"
          and therefore of limited interest.
        */
        0,
        { 0, 0, 0},
        RG_DATAMEM},
      { "L2PMap nodes",
        pmpInfo.inuse_nodes,
        pmpInfo.pg_count * pmpInfo.nodes_per_page, // Max within current pages.
        pmpInfo.node_byte_sz,
        /*
          No HWM for this row as it would be a fixed fraction of "Data memory"
          and therefore of limited interest.
        */
        0,
        { 0, 0, 0 },
        RT_DBACC_DIRECTORY},
      { NULL, 0,0,0,0,{ 0,0,0,0 }, 0}
    };

    static const size_t num_config_params =
      sizeof(pools[0].config_params)/sizeof(pools[0].config_params[0]);
    const Uint32 numPools = NDB_ARRAY_SIZE(pools);
    Uint32 pool = cursor->data[0];
    ndbrequire(pool < numPools);
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);

      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      row.write_uint32(GET_RG(pools[pool].record_type));
      row.write_uint32(GET_TID(pools[pool].record_type));
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  case Ndbinfo::FRAG_LOCKS_TABLEID:
  {
    Uint32 tableid = cursor->data[0];
    
    for (;tableid < ctablesize; tableid++)
    {
      TabrecPtr tabPtr;
      tabPtr.i = tableid;
      ptrAss(tabPtr, tabrec);
      if (tabPtr.p->fragholder[0] != RNIL)
      {
        jam();
        // Loop over all fragments for this table.
        for (Uint32 f = 0; f < NDB_ARRAY_SIZE(tabPtr.p->fragholder); f++)
        {
          if (tabPtr.p->fragholder[f] != RNIL)
          {
            jam();
            FragmentrecPtr frp;
            frp.i = tabPtr.p->fragptrholder[f];
            ptrCheckGuard(frp, cfragmentsize, fragmentrec);
            
            const Fragmentrec::LockStats& ls = frp.p->m_lockStats;
            
            Ndbinfo::Row row(signal, req);
            row.write_uint32(getOwnNodeId());
            row.write_uint32(instance());
            row.write_uint32(tableid);
            row.write_uint32(tabPtr.p->fragholder[f]);

            row.write_uint64(ls.m_ex_req_count);
            row.write_uint64(ls.m_ex_imm_ok_count);
            row.write_uint64(ls.m_ex_wait_ok_count);
            row.write_uint64(ls.m_ex_wait_fail_count);
            
            row.write_uint64(ls.m_sh_req_count);
            row.write_uint64(ls.m_sh_imm_ok_count);
            row.write_uint64(ls.m_sh_wait_ok_count);
            row.write_uint64(ls.m_sh_wait_fail_count);

            row.write_uint64(ls.m_wait_ok_millis);
            row.write_uint64(ls.m_wait_fail_millis);

            ndbinfo_send_row(signal, req, row, rl);
          }
        }
      }

      /*
        If a break is needed, break on a table boundary, 
        as we use the table id as a cursor.
      */
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, tableid + 1);
        return;
      }
    }
    break;
  }
  case Ndbinfo::ACC_OPERATIONS_TABLEID:
  {
    jam();
    /* Take a break periodically when scanning records */
    Uint32 maxToCheck = 100;
    NDB_TICKS now = getHighResTimer();
    OperationrecPtr opRecPtr;
    Uint32 i = cursor->data[0];
    do
    {
      if (rl.need_break(req) || maxToCheck == 0)
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, i);
        return;
      }
      bool found = getNextOpRec(i, opRecPtr, 10);
      /**
       * ACC holds lock requests/operations in a 2D queue 
       * structure.
       * The lock owning operation is directly linked from the
       * PK hash element.  Only one operation is the 'owner'
       * at any one time.
       * 
       * The lock owning operation may have other operations
       * concurrently holding the lock, for example other
       * operations in the same transaction, or, for shared
       * reads, in other transactions.
       * These operations are in the 'parallel' queue of the
       * lock owning operation, linked from its 
       * nextParallelQue member.
       *
       * Non-compatible lock requests must wait until some/
       * all of the current lock holder(s) have released the
       * lock before they can run.  They are held in the
       * 'serial' queue, lined from the lockOwner's 
       * nextSerialQue member.
       * 
       * Note also : Only one operation per row can 'run' 
       * in LDM at any one time, but this serialisation 
       * is not considered as locking overhead.
       *
       * Note also : These queue members are part of overlays
       * and are not always guaranteed to be valid, m_op_bits
       * often must be consulted too.
       */
      if (found &&
          opRecPtr.p->m_op_bits != Operationrec::OP_INITIAL)
      {
        jam();

        FragmentrecPtr fp;
        fp.i = opRecPtr.p->fragptr;
        ptrCheckGuard(fp, cfragmentsize, fragmentrec);

        const Uint32 tableId = fp.p->myTableId;
        const Uint32 fragId = fp.p->myfid;
        const Uint64 rowId = 
          Uint64(opRecPtr.p->localdata.m_page_no) << 32 |
          Uint64(opRecPtr.p->localdata.m_page_idx);
        /* Send as separate attrs, as in cluster_operations */
        const Uint32 transId0 = opRecPtr.p->transId1;
        const Uint32 transId1 = opRecPtr.p->transId2;
        const Uint32 prevSerialQue = opRecPtr.p->prevSerialQue;
        const Uint32 nextSerialQue = opRecPtr.p->nextSerialQue;
        const Uint32 prevParallelQue = opRecPtr.p->prevParallelQue;
        const Uint32 nextParallelQue = opRecPtr.p->nextParallelQue;
        const Uint32 flags = opRecPtr.p->m_op_bits;
        /* Ignore Uint32 overflow at ~ 50 days */
        const Uint32 durationMillis = 
          (Uint32) NdbTick_Elapsed(opRecPtr.p->m_lockTime,
                                   now).milliSec();
        const Uint32 userPtr = opRecPtr.p->userptr;

        /* Live operation */
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(instance());
        row.write_uint32(tableId);
        row.write_uint32(fragId);
        row.write_uint64(rowId);
        row.write_uint32(transId0);
        row.write_uint32(transId1);
        row.write_uint32(opRecPtr.i);
        row.write_uint32(flags);
        row.write_uint32(prevSerialQue);
        row.write_uint32(nextSerialQue);
        row.write_uint32(prevParallelQue);
        row.write_uint32(nextParallelQue);
        row.write_uint32(durationMillis);
        row.write_uint32(userPtr);

        ndbinfo_send_row(signal, req, row, rl);
      }
      maxToCheck--;
      if (i == RNIL)
      {
        /* No more rows left to scan */
        ndbinfo_send_scan_conf(signal, req, rl);
        return;
      }
    } while (true);

    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

bool
Dbacc::getNextScanRec(Uint32 &next, ScanRecPtr &loc_scanptr)
{
  Uint32 found = 0;
  Uint32 loop_count = 0;

  while (found == 0 && next != RNIL && loop_count < 10)
  {
    found = scanRec_pool.getUncheckedPtrs(&next, &loc_scanptr, 1);
    if (found > 0 &&
        !Magic::check_ptr(loc_scanptr.p))
      found = 0;
    loop_count++;
  }
  return (found > 0);
}

bool
Dbacc::getNextOpRec(Uint32 &next,
                    OperationrecPtr &loc_opptr,
                    Uint32 max_loops)
{
  Uint32 found = 0;
  Uint32 loop_count = 0;
  while (found == 0 && next != RNIL &&
         (max_loops == 0 || loop_count < max_loops))
  {
    found = oprec_pool.getUncheckedPtrs(&next, &loc_opptr, 1);
    if (found > 0 &&
        !Magic::check_ptr(loc_opptr.p))
      found = 0;
    loop_count++;
  }
  return (found > 0);
}

void
Dbacc::execDUMP_STATE_ORD(Signal* signal)
{
  DumpStateOrd * const dumpState = (DumpStateOrd *)&signal->theData[0];
  if (dumpState->args[0] == DumpStateOrd::AccDumpOneScanRec)
  {
    ScanRecPtr scanPtr;
    Uint32 recordNo = RNIL;
    if (signal->length() == 2)
    {
      jam();
      recordNo = dumpState->args[1];
    }
    else
    {
      jam();
      return;
    }
    scanPtr.i = recordNo;
    if (!scanRec_pool.getValidPtr(scanPtr))
    {
      jam();
      return;
    }
    jam();
    
    g_eventLogger->info("Dbacc::ScanRec[%d]: state=%d, transid(0x%x, 0x%x)",
	      scanPtr.i, scanPtr.p->scanState,scanPtr.p->scanTrid1,
	      scanPtr.p->scanTrid2);
    g_eventLogger->info("activeLocalFrag=%d, nextBucketIndex=%d",
	      scanPtr.p->activeLocalFrag,
	      scanPtr.p->nextBucketIndex);
    g_eventLogger->info("firstActOp=%d firstLockedOp=%d",
	      scanPtr.p->scanFirstActiveOp,
	      scanPtr.p->scanFirstLockedOp);
    g_eventLogger->info("scanLastLockedOp=%d firstQOp=%d lastQOp=%d",
	      scanPtr.p->scanLastLockedOp,
	      scanPtr.p->scanFirstQueuedOp,
	      scanPtr.p->scanLastQueuedOp);
    g_eventLogger->info("scanUserP=%d, startNoBuck=%d,"
                        " minBucketIndexToRescan=%d",
	      scanPtr.p->scanUserptr,
	      scanPtr.p->startNoOfBuckets,
	      scanPtr.p->minBucketIndexToRescan);
    g_eventLogger->info("maxBucketIndexToRescan=%d, scan_lastSeen = %d, ",
	      scanPtr.p->maxBucketIndexToRescan,
              scanPtr.p->scan_lastSeen);
    g_eventLogger->info("scanBucketState=%d, scanLockHeld=%d, userBlockRef=%d",
	      scanPtr.p->scanBucketState,
	      scanPtr.p->scanLockHeld,
	      scanPtr.p->scanUserblockref);
    g_eventLogger->info("scanMask=%d scanLockMode=%d, scanLockCount=%d",
	      scanPtr.p->scanMask,
	      scanPtr.p->scanLockMode,
              scanPtr.p->scanLockCount);
    return;
  }

  // Dump all ScanRec(ords)
  if (dumpState->args[0] == DumpStateOrd::AccDumpAllScanRec ||
      dumpState->args[0] == DumpStateOrd::AccDumpAllActiveScanRec)
  {
    Uint32 recordNo = 0;
    if (signal->length() == 1)
      infoEvent("ACC: Dump all active ScanRec");
    else if (signal->length() == 2)
      recordNo = dumpState->args[1];
    else
      return;
    ScanRecPtr loc_scanptr;
    bool found = getNextScanRec(recordNo, loc_scanptr);
    if (found)
    {
      dumpState->args[0] = DumpStateOrd::AccDumpOneScanRec;
      dumpState->args[1] = loc_scanptr.i;
      execDUMP_STATE_ORD(signal);
    }
    if (recordNo != RNIL)
    {
      dumpState->args[0] = DumpStateOrd::AccDumpAllScanRec;
      dumpState->args[1] = recordNo;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
    return;
  }

  if(dumpState->args[0] == DumpStateOrd::EnableUndoDelayDataWrite){
    ndbout << "Dbacc:: delay write of datapages for table = " 
	   << dumpState->args[1]<< endl;
    SET_ERROR_INSERT_VALUE(3000);
    return;
  }

  if(dumpState->args[0] == DumpStateOrd::AccDumpOneOperationRec){
    Uint32 recordNo = RNIL;
    if (signal->length() == 2)
      recordNo = dumpState->args[1];
    else 
      return;

    OperationrecPtr tmpOpPtr;
    tmpOpPtr.i = recordNo;
    if (!oprec_pool.getValidPtr(tmpOpPtr))
      return;
    
    infoEvent("Dbacc::operationrec[%d]: transid(0x%x, 0x%x)",
	      tmpOpPtr.i, tmpOpPtr.p->transId1,
	      tmpOpPtr.p->transId2);
    infoEvent("elementPage=%d, elementPointer=%d ",
	      tmpOpPtr.p->elementPage, 
	      tmpOpPtr.p->elementPointer);
    infoEvent("fid=%d, fragptr=%d ",
              tmpOpPtr.p->fid, tmpOpPtr.p->fragptr);
    infoEvent("hashValue=%d", tmpOpPtr.p->hashValue.pack());
    infoEvent("nextOp=%d, nextParallelQue=%d ",
	      tmpOpPtr.p->nextOp, tmpOpPtr.p->nextParallelQue);
    infoEvent("nextSerialQue=%d, prevOp=%d ",
	      tmpOpPtr.p->nextSerialQue, 
	      tmpOpPtr.p->prevOp);
    infoEvent("prevParallelQue=%d, prevSerialQue=%d, scanRecPtr=%d",
	      tmpOpPtr.p->prevParallelQue,
	      tmpOpPtr.p->prevSerialQue, tmpOpPtr.p->scanRecPtr);
    infoEvent("m_op_bits=0x%x, reducedHashValue=%x ",
              tmpOpPtr.p->m_op_bits, tmpOpPtr.p->reducedHashValue.pack());
    return;
  }

#ifdef ERROR_INSERT
  if(dumpState->args[0] == DumpStateOrd::AccDumpNumOpRecs)
  {
    Uint32 freeOpRecs = oprec_pool.getUsed();
    infoEvent("Dbacc::OperationRecords: free=%d",	      
	      freeOpRecs);

    return;
  }
#endif

  if (dumpState->args[0] == DumpStateOrd::AccDumpOneOpRecLocal)
  {
    if (signal->length() != 2)
    {
      return;
    }

    OperationrecPtr opPtr;
    opPtr.i = dumpState->args[1];
    ndbrequire(oprec_pool.getValidPtr(opPtr));

    {
      char buff[200];
      StaticBuffOutputStream buffStream(buff, sizeof(buff));
      NdbOut buffOut(buffStream);

      buffOut << opPtr;

      g_eventLogger->info("ACC %u : %s",
                          instance(),
                          buff);
    }

    return;
  }

  if (dumpState->args[0] == DumpStateOrd::AccDumpOpPrecedingLocks)
  {
    jam();
    if (signal->length() != 2)
    {
      return;
    }

    OperationrecPtr startOpPtr;
    OperationrecPtr currOpPtr;
    startOpPtr.i = dumpState->args[1];
    ndbrequire(oprec_pool.getValidPtr(startOpPtr));

    currOpPtr = startOpPtr;

    /* Dump start op */
    signal->theData[0] = DumpStateOrd::AccDumpOneOpRecLocal;
    signal->theData[1] = startOpPtr.i;
    execDUMP_STATE_ORD(signal);

    if (getPrecedingOperation(currOpPtr))
    {
      jam();

      do
      {
        /* Dump dependent op */
        signal->theData[1] = currOpPtr.i;
        execDUMP_STATE_ORD(signal);
      } while (getPrecedingOperation(currOpPtr));
    }
  }


#if 0
  if (type == 100) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 2;
    req->secondaryTableId = RNIL;
    req->userPtr = 2;
    req->userRef = DBDICT_REF;
    sendSignal(reference(), GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 101) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 4;
    req->secondaryTableId = 5;
    req->userPtr = 4;
    req->userRef = DBDICT_REF;
    sendSignal(reference(), GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 102) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 6;
    req->secondaryTableId = 8;
    req->userPtr = 6;
    req->userRef = DBDICT_REF;
    sendSignal(reference(), GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 103) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 2;
    req->secondaryTableId = RNIL;
    req->userPtr = 2;
    req->userRef = DBDICT_REF;
    sendSignal(reference(), GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
  if (type == 104) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 4;
    req->secondaryTableId = 5;
    req->userPtr = 4;
    req->userRef = DBDICT_REF;
    sendSignal(reference(), GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
  if (type == 105) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 6;
    req->secondaryTableId = 8;
    req->userPtr = 6;
    req->userRef = DBDICT_REF;
    sendSignal(reference(), GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
#endif

  if (signal->theData[0] == DumpStateOrd::SchemaResourceSnapshot)
  {
    RSS_OP_SNAPSHOT_SAVE(cnoOfFreeFragrec);
    return;
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceCheckLeak)
  {
    RSS_OP_SNAPSHOT_CHECK(cnoOfFreeFragrec);
    return;
  }
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  if (signal->theData[0] == DumpStateOrd::AccSetTransientPoolMaxSize)
  {
    jam();
    if (signal->getLength() < 3)
      return;
    const Uint32 pool_index = signal->theData[1];
    const Uint32 new_size = signal->theData[2];
    if (pool_index >= c_transient_pool_count)
      return;
    c_transient_pools[pool_index]->setMaxSize(new_size);
    return;
  }
  if (signal->theData[0] == DumpStateOrd::AccResetTransientPoolMaxSize)
  {
    jam();
    if(signal->getLength() < 2)
      return;
    const Uint32 pool_index = signal->theData[1];
    if (pool_index >= c_transient_pool_count)
      return;
    c_transient_pools[pool_index]->resetMaxSize();
    return;
  }
#endif
}//Dbacc::execDUMP_STATE_ORD()

Uint32
Dbacc::getL2PMapAllocBytes(Uint32 fragId) const
{
  jam();
  FragmentrecPtr fragPtr(NULL, fragId);
  ptrCheckGuard(fragPtr, cfragmentsize, fragmentrec);
  return fragPtr.p->directory.getByteSize();
}

#ifdef VM_TRACE
void
Dbacc::debug_lh_vars(const char* where)const
{
  Uint32 b = fragrecptr.p->level.getTop();
  Uint32 di = fragrecptr.p->getPageNumber(b);
  Uint32 ri = di >> 8;
  ndbout
    << "DBACC: " << where << ":"
    << " frag:" << fragrecptr.p->myTableId
    << "/" << fragrecptr.p->myfid
    << " slack:" << fragrecptr.p->slack
    << "/" << fragrecptr.p->slackCheck
    << " top:" << fragrecptr.p->level.getTop()
    << " di:" << di
    << " ri:" << ri
    << " full:" << fragrecptr.p->dirRangeFull
    << "\n";
}
#endif

/**
 * getPrecedingOperation
 *
 * Used to iterate the lock queues on a row, based
 * on an arbitrary starting position.
 *
 * Given an opPtr we :
 *  1.  Check it is on a lock queue, or return RNIL
 *  2.  Return a pointer to a preceding operation in terms
 *      of lock ownership order, or RNIL
 */
bool
Dbacc::getPrecedingOperation(OperationrecPtr& opPtr) const
{
  ndbrequire(oprec_pool.getValidPtr(opPtr));

  if ((opPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) != 0)
  {
    /* owner, nothing precedes */
    ndbrequire((opPtr.p->m_op_bits & Operationrec::OP_RUN_QUEUE) != 0);
    opPtr.i = RNIL;
    //ndbout_c("OWNER");
  }
  else
  {
    /* !owner, anything preceding? */
    if (opPtr.p->prevParallelQue != RNIL)
    {
      /* Traverse parallel first */
      opPtr.i = opPtr.p->prevParallelQue;
      //ndbout_c("PREV PARALLEL");
      ndbrequire(oprec_pool.getValidPtr(opPtr));
    }
    else if (opPtr.p->prevSerialQue != RNIL)
    {
      /* Traverse serial */
      opPtr.i = opPtr.p->prevSerialQue;
      //ndbout_c("PREV SERIAL");
      ndbrequire(oprec_pool.getValidPtr(opPtr));

      /* Do we have a parallel queue here? */
      if (opPtr.p->nextParallelQue != RNIL)
      {
        /* AFAIK, only the first serial entry can have parallel ops */
        ndbrequire((opPtr.p->m_op_bits & Operationrec::OP_LOCK_OWNER) !=0);

        /* Jump to end of parallel queue */
        OperationrecPtr lo = opPtr;
        opPtr.i = opPtr.p->m_lo_last_parallel_op_ptr_i;
        ndbrequire(oprec_pool.getValidPtr(opPtr));
        //ndbout_c("PREV SERIAL HAS PARALLEL QUEUE, JUMP TO END");

        /* Check end of parallel queue refs start */
        ndbrequire(opPtr.p->m_lock_owner_ptr_i == lo.i);
      }
    }
    else
    {
      /* !owner, nothing precedes - not locked */
      //ndbout_c("NOTHING PRECEDES");
    }
  }

  return (opPtr.i != RNIL);
}

/**
 * Implementation of Dbacc::Page32Lists
 */

void Dbacc::Page32Lists::addPage32(Page32_pool& pool, Page32Ptr p)
{
  const Uint8 list_id = 0; // List of 32KiB pages with all 8KiB pages free
  LocalPage32_list list(pool, lists[list_id]);
  list.addFirst(p);
  nonempty_lists |= (1 << list_id);
  p.p->list_id = list_id;
  p.p->magic = Page32::MAGIC;
}

void Dbacc::Page32Lists::dropLastPage32(Page32_pool& pool,
                                         Page32Ptr& p,
                                         Uint32 keep)
{
  if (lists[0].getCount() <= keep)
  {
    p.i = RNIL;
    p.p = NULL;
    return;
  }
  LocalPage32_list list(pool, lists[0]);
  list.last(p);
  dropPage32(pool, p);
}

void Dbacc::Page32Lists::dropPage32(Page32_pool& pool, Page32Ptr p)
{
  require(p.p->magic == Page32::MAGIC);
  require(p.p->list_id == 0);
  p.p->magic = ~Page32::MAGIC;
  const Uint8 list_id = 0; // The list of pages with all its four 8KiB pages free
  LocalPage32_list list(pool, lists[list_id]);
  list.remove(p);
  if (list.isEmpty())
  {
    nonempty_lists &= ~(1 << list_id);
  }
}

void Dbacc::Page32Lists::seizePage8(Page32_pool& pool,
                                    Page8Ptr& /* out */ p8,
                                    int sub_page_id)
{
  Uint16 list_id_set;
  Uint8 sub_page_id_set;
  if (sub_page_id == LEAST_COMMON_SUB_PAGE)
  { // Find out least common sub_page_ids
    Uint32 min_sub_page_count = UINT32_MAX;
    for (int i = 0; i < 4; i++)
    {
      if (sub_page_id_count[i] < min_sub_page_count)
      {
        min_sub_page_count = sub_page_id_count[i];
      }
    }
    list_id_set = 0;
    sub_page_id_set = 0;
    for (int i = 0; i < 4; i++)
    {
      if (sub_page_id_count[i] == min_sub_page_count)
      {
        list_id_set |= sub_page_id_to_list_id_set(sub_page_id);
        sub_page_id_set |= (1 << i);
      }
    }
  }
  else
  {
    list_id_set = sub_page_id_to_list_id_set(sub_page_id);
    if (sub_page_id < 0)
    {
      sub_page_id_set = 0xf;
    }
    else
    {
      sub_page_id_set = 1 << sub_page_id;
    }
  }
  list_id_set &= nonempty_lists;
  if (list_id_set == 0)
  {
    p8.i = RNIL;
    p8.p = NULL;
    return;
  }
  Uint8 list_id = least_free_list(list_id_set);
  Uint8 list_sub_page_id_set = list_id_to_sub_page_id_set(list_id);
  if (sub_page_id < 0)
  {
    Uint32 set = sub_page_id_set & list_sub_page_id_set;
    require(set != 0);
    sub_page_id = BitmaskImpl::fls(set);
  }
  list_sub_page_id_set ^= (1 << sub_page_id);
  Uint8 new_list_id = sub_page_id_set_to_list_id(list_sub_page_id_set);

  LocalPage32_list old_list(pool, lists[list_id]);
  LocalPage32_list new_list(pool, lists[new_list_id]);

  Page32Ptr p;
  old_list.removeFirst(p);
  if (old_list.isEmpty())
  {
    nonempty_lists &= ~(1 << list_id);
  }
  require(p.p->magic == Page32::MAGIC);
  require(p.p->list_id == list_id);
  new_list.addFirst(p);
  nonempty_lists |= (1 << new_list_id);
  p.p->list_id = new_list_id;
  p8.i = (p.i << 2) | sub_page_id;
  p8.p = &p.p->page8[sub_page_id];
  sub_page_id_count[sub_page_id] ++;
}

void Dbacc::Page32Lists::releasePage8(Page32_pool& pool, Page8Ptr p8)
{
  int sub_page_id = p8.i & 3;
  Page32Ptr p;
  p.i = p8.i >> 2;
  p.p = reinterpret_cast<Page32*>(p8.p-sub_page_id);

  Uint8 list_id = p.p->list_id;
  Uint8 sub_page_id_set = list_id_to_sub_page_id_set(list_id);
  sub_page_id_set ^= (1 << sub_page_id);
  Uint8 new_list_id = sub_page_id_set_to_list_id(sub_page_id_set);

  LocalPage32_list old_list(pool, lists[list_id]);
  LocalPage32_list new_list(pool, lists[new_list_id]);

  old_list.remove(p);
  if (old_list.isEmpty())
  {
    nonempty_lists &= ~(1 << list_id);
  }
  require(p.p->magic == Page32::MAGIC);
  require(p.p->list_id == list_id);
  new_list.addFirst(p);
  nonempty_lists |= (1 << new_list_id);
  p.p->list_id = new_list_id;
  sub_page_id_count[sub_page_id] --;
}

void
Dbacc::sendPoolShrink(const Uint32 pool_index)
{
  const bool need_send = c_transient_pools_shrinking.get(pool_index) == 0;
  c_transient_pools_shrinking.set(pool_index);
  if (need_send)
  {
    Signal25 signal[1] = {};
    signal->theData[0] = ZACC_SHRINK_TRANSIENT_POOLS;
    signal->theData[1] = pool_index;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}

void
Dbacc::shrinkTransientPools(Uint32 pool_index)
{
  ndbrequire(pool_index < c_transient_pool_count);
  ndbrequire(c_transient_pools_shrinking.get(pool_index));
  if (c_transient_pools[pool_index]->rearrange_free_list_and_shrink(1))
  {
    sendPoolShrink(pool_index);
  }
  else
  {
    c_transient_pools_shrinking.clear(pool_index);
  }
}
