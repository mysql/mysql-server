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

#define DBTUX_GEN_CPP
#include "Dbtux.hpp"
#include <signaldata/TuxContinueB.hpp>
#include <signaldata/TuxContinueB.hpp>

Dbtux::Dbtux(const Configuration& conf) :
  SimulatedBlock(DBTUX, conf),
  c_tup(0),
  c_descPageList(RNIL),
#ifdef VM_TRACE
  debugFile(0),
  debugOut(*new NullOutputStream()),
  // until ndb_mgm supports dump
#ifdef DBTUX_DEBUG_TREE
  debugFlags(DebugTree),
#else
  debugFlags(0),
#endif
#endif
  c_internalStartPhase(0),
  c_typeOfStart(NodeState::ST_ILLEGAL_TYPE),
  c_dataBuffer(0)
{
  BLOCK_CONSTRUCTOR(Dbtux);
  // verify size assumptions (also when release-compiled)
  ndbrequire(
      (sizeof(TreeEnt) & 0x3) == 0 &&
      (sizeof(TreeNode) & 0x3) == 0 &&
      (sizeof(DescHead) & 0x3) == 0 &&
      (sizeof(DescAttr) & 0x3) == 0
  );
  /*
   * DbtuxGen.cpp
   */
  addRecSignal(GSN_CONTINUEB, &Dbtux::execCONTINUEB);
  addRecSignal(GSN_STTOR, &Dbtux::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbtux::execREAD_CONFIG_REQ, true);
  /*
   * DbtuxMeta.cpp
   */
  addRecSignal(GSN_TUXFRAGREQ, &Dbtux::execTUXFRAGREQ);
  addRecSignal(GSN_TUX_ADD_ATTRREQ, &Dbtux::execTUX_ADD_ATTRREQ);
  addRecSignal(GSN_ALTER_INDX_REQ, &Dbtux::execALTER_INDX_REQ);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbtux::execDROP_TAB_REQ);
  /*
   * DbtuxMaint.cpp
   */
  addRecSignal(GSN_TUX_MAINT_REQ, &Dbtux::execTUX_MAINT_REQ);
  /*
   * DbtuxScan.cpp
   */
  addRecSignal(GSN_ACC_SCANREQ, &Dbtux::execACC_SCANREQ);
  addRecSignal(GSN_TUX_BOUND_INFO, &Dbtux::execTUX_BOUND_INFO);
  addRecSignal(GSN_NEXT_SCANREQ, &Dbtux::execNEXT_SCANREQ);
  addRecSignal(GSN_ACC_CHECK_SCAN, &Dbtux::execACC_CHECK_SCAN);
  addRecSignal(GSN_ACCKEYCONF, &Dbtux::execACCKEYCONF);
  addRecSignal(GSN_ACCKEYREF, &Dbtux::execACCKEYREF);
  addRecSignal(GSN_ACC_ABORTCONF, &Dbtux::execACC_ABORTCONF);
  /*
   * DbtuxDebug.cpp
   */
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbtux::execDUMP_STATE_ORD);
}

Dbtux::~Dbtux()
{
}

void
Dbtux::execCONTINUEB(Signal* signal)
{
  jamEntry();
  const Uint32* data = signal->getDataPtr();
  switch (data[0]) {
  case TuxContinueB::DropIndex:
    {
      IndexPtr indexPtr;
      c_indexPool.getPtr(indexPtr, data[1]);
      dropIndex(signal, indexPtr, data[2], data[3]);
    }
    break;
  default:
    ndbrequire(false);
    break;
  }
}

/*
 * STTOR is sent to one block at a time.  In NDBCNTR it triggers
 * NDB_STTOR to the "old" blocks.  STTOR carries start phase (SP) and
 * NDB_STTOR carries internal start phase (ISP).
 *
 *      SP      ISP     activities
 *      1       none
 *      2       1       
 *      3       2       recover metadata, activate indexes
 *      4       3       recover data
 *      5       4-6     
 *      6       skip    
 *      7       skip    
 *      8       7       build non-logged indexes on SR
 *
 * DBTUX catches type of start (IS, SR, NR, INR) at SP 3 and updates
 * internal start phase at SP 7.  These are used to prevent index
 * maintenance operations caused by redo log at SR.
 */
void
Dbtux::execSTTOR(Signal* signal)
{
  jamEntry();
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 1:
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    c_tup = (Dbtup*)globalData.getBlock(DBTUP);
    ndbrequire(c_tup != 0);
    break;
  case 3:
    jam();
    c_typeOfStart = signal->theData[7];
    break;
  case 7:
    c_internalStartPhase = 6;
  default:
    jam();
    break;
  }
  signal->theData[0] = 0;       // garbage
  signal->theData[1] = 0;       // garbage
  signal->theData[2] = 0;       // garbage
  signal->theData[3] = 1;
  signal->theData[4] = 3;       // for c_typeOfStart
  signal->theData[5] = 7;       // for c_internalStartPhase
  signal->theData[6] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}

void
Dbtux::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();
 
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  Uint32 nIndex;
  Uint32 nFragment;
  Uint32 nAttribute;
  Uint32 nScanOp; 

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_INDEX, &nIndex));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_FRAGMENT, &nFragment));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_ATTRIBUTE, &nAttribute));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_SCAN_OP, &nScanOp));

  const Uint32 nDescPage = (nIndex + nAttribute + DescPageSize - 1) / DescPageSize;
  const Uint32 nScanBoundWords = nScanOp * ScanBoundSegmentSize * 4;
  
  c_indexPool.setSize(nIndex);
  c_fragPool.setSize(nFragment);
  c_descPagePool.setSize(nDescPage);
  c_fragOpPool.setSize(MaxIndexFragments);
  c_scanOpPool.setSize(nScanOp);
  c_scanBoundPool.setSize(nScanBoundWords);
  /*
   * Index id is physical array index.  We seize and initialize all
   * index records now.
   */
  IndexPtr indexPtr;
  while (1) {
    jam();
    refresh_watch_dog();
    c_indexPool.seize(indexPtr);
    if (indexPtr.i == RNIL) {
      jam();
      break;
    }
    new (indexPtr.p) Index();
  }
  // allocate buffers
  c_keyAttrs = (Uint32*)allocRecord("c_keyAttrs", sizeof(Uint32), MaxIndexAttributes);
  c_searchKey = (TableData)allocRecord("c_searchKey", sizeof(Uint32*), MaxIndexAttributes);
  c_entryKey = (TableData)allocRecord("c_entryKey", sizeof(Uint32*), MaxIndexAttributes);
  c_dataBuffer = (Uint32*)allocRecord("c_dataBuffer", sizeof(Uint64), (MaxAttrDataSize + 1) >> 1);
  // ack
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

// utils

void
Dbtux::setKeyAttrs(const Frag& frag)
{
  Data keyAttrs = c_keyAttrs;   // global
  const unsigned numAttrs = frag.m_numAttrs;
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  for (unsigned i = 0; i < numAttrs; i++) {
    const DescAttr& descAttr = descEnt.m_descAttr[i];
    Uint32 size = AttributeDescriptor::getSizeInWords(descAttr.m_attrDesc);
    // set attr id and fixed size
    keyAttrs.ah() = AttributeHeader(descAttr.m_primaryAttrId, size);
    keyAttrs += 1;
  }
}

void
Dbtux::readKeyAttrs(const Frag& frag, TreeEnt ent, unsigned start, TableData keyData)
{
  ConstData keyAttrs = c_keyAttrs; // global
  const Uint32 tableFragPtrI = frag.m_tupTableFragPtrI[ent.m_fragBit];
  const TupLoc tupLoc = ent.m_tupLoc;
  const Uint32 tupVersion = ent.m_tupVersion;
  ndbrequire(start < frag.m_numAttrs);
  const unsigned numAttrs = frag.m_numAttrs - start;
  // start applies to both keys and output data
  keyAttrs += start;
  keyData += start;
  c_tup->tuxReadAttrs(tableFragPtrI, tupLoc.m_pageId, tupLoc.m_pageOffset, tupVersion, numAttrs, keyAttrs, keyData);
  jamEntry();
}

void
Dbtux::readTablePk(const Frag& frag, TreeEnt ent, unsigned& pkSize, Data pkData)
{
  const Uint32 tableFragPtrI = frag.m_tupTableFragPtrI[ent.m_fragBit];
  const TupLoc tupLoc = ent.m_tupLoc;
  Uint32 size = 0;
  c_tup->tuxReadKeys(tableFragPtrI, tupLoc.m_pageId, tupLoc.m_pageOffset, &size, pkData);
  ndbrequire(size != 0);
  pkSize = size;
}

/*
 * Input is pointers to table attributes.  Output is array of attribute
 * data with headers.  Copies whatever fits.
 */
void
Dbtux::copyAttrs(const Frag& frag, TableData data1, Data data2, unsigned maxlen2)
{
  ConstData keyAttrs = c_keyAttrs; // global
  const unsigned numAttrs = frag.m_numAttrs;
  unsigned len2 = maxlen2;
  for (unsigned n = 0; n < numAttrs; n++) {
    jam();
    const unsigned attrId = keyAttrs.ah().getAttributeId();
    const unsigned dataSize = keyAttrs.ah().getDataSize();
    const Uint32* const p1 = *data1;
    if (p1 != 0) {
      if (len2 == 0)
        return;
      data2.ah() = AttributeHeader(attrId, dataSize);
      data2 += 1;
      len2 -= 1;
      unsigned n = dataSize;
      for (unsigned i = 0; i < dataSize; i++) {
        if (len2 == 0)
          return;
        *data2 = p1[i];
        data2 += 1;
        len2 -= 1;
      }
    } else {
      if (len2 == 0)
        return;
      data2.ah() = AttributeHeader(attrId, 0);
      data2.ah().setNULL();
      data2 += 1;
      len2 -= 1;
    }
    keyAttrs += 1;
    data1 += 1;
  }
#ifdef VM_TRACE
  memset(data2, DataFillByte, len2 << 2);
#endif
}

BLOCK_FUNCTIONS(Dbtux);
