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

Dbtux::Dbtux(const Configuration& conf) :
  SimulatedBlock(DBTUX, conf),
  c_descPageList(RNIL),
#ifdef VM_TRACE
  debugFile(0),
  debugOut(*new NullOutputStream()),
  debugFlags(0),
#endif
  c_internalStartPhase(0),
  c_typeOfStart(NodeState::ST_ILLEGAL_TYPE),
  c_keyBuffer(0)
{
  BLOCK_CONSTRUCTOR(Dbtux);
  // verify size assumptions (also when release-compiled)
  ndbrequire(
      (sizeof(DescHead) & 0x3) == 0 &&
      (sizeof(DescAttr) & 0x3) == 0 &&
      (sizeof(TreeEnt) & 0x3) == 0 &&
      (sizeof(TreeNode) & 0x3) == 0
  );
  /*
   * DbtuxGen.cpp
   */
  addRecSignal(GSN_CONTINUEB, &Dbtux::execCONTINUEB);
  addRecSignal(GSN_STTOR, &Dbtux::execSTTOR);
  addRecSignal(GSN_SIZEALT_REP, &Dbtux::execSIZEALT_REP);
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
Dbtux::execSIZEALT_REP(Signal* signal)
{
  jamEntry();
  const Uint32* data = signal->getDataPtr();
  BlockReference sender = data[TuxSizeAltReq::IND_BLOCK_REF];
  const Uint32 nIndex = data[TuxSizeAltReq::IND_INDEX];
  const Uint32 nFragment = data[TuxSizeAltReq::IND_FRAGMENT];
  const Uint32 nAttribute = data[TuxSizeAltReq::IND_ATTRIBUTE];
  const Uint32 nDescPage = (nIndex + nAttribute + DescPageSize - 1) / DescPageSize;
  const Uint32 nScanOp = data[TuxSizeAltReq::IND_SCAN];
  const Uint32 nScanBoundWords = nScanOp * ScanBoundSegmentSize * 4;
  // allocate records
  c_indexPool.setSize(nIndex);
  c_fragPool.setSize(nFragment);
  c_descPagePool.setSize(nDescPage);
  c_fragOpPool.setSize(MaxIndexFragments);
  c_nodeHandlePool.setSize(MaxNodeHandles);
  c_scanOpPool.setSize(nScanOp);
  c_scanBoundPool.setSize(nScanBoundWords);
  /*
   * Index id is physical array index.  We seize and initialize all
   * index records now.  This assumes ArrayPool is an array.
   */
  IndexPtr indexPtr;
  while (1) {
    jam();
    c_indexPool.seize(indexPtr);
    if (indexPtr.i == RNIL) {
      jam();
      break;
    }
    new (indexPtr.p) Index();
  }
  // allocate buffers
  c_keyBuffer = (Uint32*)allocRecord("c_keyBuffer", sizeof(Uint64), (MaxAttrDataSize + 1) >> 1);
  // ack
  sendSignal(sender, GSN_SIZEALT_ACK, signal, 1, JBB);
}

// utils

void
Dbtux::copyAttrs(Data dst, ConstData src, CopyPar& copyPar)
{
  CopyPar c = copyPar;
  c.m_numitems = 0;
  c.m_numwords = 0;
  while (c.m_numitems < c.m_items) {
    jam();
    if (c.m_headers) {
      unsigned i = 0;
      while (i < AttributeHeaderSize) {
        if (c.m_numwords >= c.m_maxwords) {
          copyPar = c;
          return;
        }
        dst[c.m_numwords++] = src[i++];
      }
    }
    unsigned size = src.ah().getDataSize();
    src += AttributeHeaderSize;
    unsigned i = 0;
    while (i < size) {
      if (c.m_numwords >= c.m_maxwords) {
        copyPar = c;
        return;
      }
      dst[c.m_numwords++] = src[i++];
    }
    src += size;
    c.m_numitems++;
  }
  copyPar = c;
}

BLOCK_FUNCTIONS(Dbtux);
