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

#define DBTUX_MAINT_CPP
#include "Dbtux.hpp"

/*
 * Maintain index.
 */

void
Dbtux::execTUX_MAINT_REQ(Signal* signal)
{
  jamEntry();
  TuxMaintReq* const sig = (TuxMaintReq*)signal->getDataPtrSend();
  // ignore requests from redo log
  if (c_internalStartPhase < 6 &&
      c_typeOfStart != NodeState::ST_NODE_RESTART &&
      c_typeOfStart != NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      TupLoc tupLoc(sig->pageId, sig->pageOffset);
      debugOut << "opInfo=" << hex << sig->opInfo;
      debugOut << " tableId=" << dec << sig->tableId;
      debugOut << " indexId=" << dec << sig->indexId;
      debugOut << " fragId=" << dec << sig->fragId;
      debugOut << " tupLoc=" << tupLoc;
      debugOut << " tupVersion=" << dec << sig->tupVersion;
      debugOut << " -- ignored at ISP=" << dec << c_internalStartPhase;
      debugOut << " TOS=" << dec << c_typeOfStart;
      debugOut << endl;
    }
#endif
    sig->errorCode = 0;
    return;
  }
  TuxMaintReq reqCopy = *sig;
  TuxMaintReq* const req = &reqCopy;
  const Uint32 opCode = req->opInfo & 0xFF;
  const Uint32 opFlag = req->opInfo >> 8;
  // get the index
  IndexPtr indexPtr;
  c_indexPool.getPtr(indexPtr, req->indexId);
  ndbrequire(indexPtr.p->m_tableId == req->tableId);
  // get base fragment id and extra bits
  const Uint32 fragId = req->fragId & ~1;
  const Uint32 fragBit = req->fragId & 1;
  
  // get the fragment
  FragPtr fragPtr;
  fragPtr.i = RNIL;
  for (unsigned i = 0; i < indexPtr.p->m_numFrags; i++) {
    jam();
    if (indexPtr.p->m_fragId[i] == fragId) {
      jam();
      c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
      break;
    }
  }

  ndbrequire(fragPtr.i != RNIL);
  Frag& frag = *fragPtr.p;
  // set up index keys for this operation
  setKeyAttrs(frag);
  // set up search entry
  TreeEnt ent;
  ent.m_tupLoc = TupLoc(req->pageId, req->pageOffset);
  ent.m_tupVersion = req->tupVersion;
  ent.m_fragBit = fragBit;
  // read search key
  readKeyAttrs(frag, ent, 0, c_searchKey);
  if (! frag.m_storeNullKey) {
    // check if all keys are null
    const unsigned numAttrs = frag.m_numAttrs;
    bool allNull = true;
    for (unsigned i = 0; i < numAttrs; i++) {
      if (c_searchKey[i] != 0) {
        jam();
        allNull = false;
        break;
      }
    }
    if (allNull) {
      jam();
      req->errorCode = 0;
      *sig = *req;
      return;
    }
  }
#ifdef VM_TRACE
  if (debugFlags & DebugMaint) {
    debugOut << "opCode=" << dec << opCode;
    debugOut << " opFlag=" << dec << opFlag;
    debugOut << " tableId=" << dec << req->tableId;
    debugOut << " indexId=" << dec << req->indexId;
    debugOut << " fragId=" << dec << req->fragId;
    debugOut << " entry=" << ent;
    debugOut << endl;
  }
#endif
  // do the operation
  req->errorCode = 0;
  TreePos treePos;
  switch (opCode) {
  case TuxMaintReq::OpAdd:
    jam();
    searchToAdd(frag, c_searchKey, ent, treePos);
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      debugOut << treePos << (treePos.m_match ? " - error" : "") << endl;
    }
#endif
    if (treePos.m_match) {
      jam();
      // there is no "Building" state so this will have to do
      if (indexPtr.p->m_state == Index::Online) {
        jam();
        req->errorCode = TuxMaintReq::SearchError;
      }
      break;
    }
    /*
     * At most one new node is inserted in the operation.  Pre-allocate
     * it so that the operation cannot fail.
     */
    if (frag.m_freeLoc == NullTupLoc) {
      jam();
      NodeHandle node(frag);
      req->errorCode = allocNode(signal, node);
      if (req->errorCode != 0) {
        jam();
        break;
      }
      // link to freelist
      node.setLink(0, frag.m_freeLoc);
      frag.m_freeLoc = node.m_loc;
      ndbrequire(frag.m_freeLoc != NullTupLoc);
    }
    treeAdd(frag, treePos, ent);
    break;
  case TuxMaintReq::OpRemove:
    jam();
    searchToRemove(frag, c_searchKey, ent, treePos);
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      debugOut << treePos << (! treePos.m_match ? " - error" : "") << endl;
    }
#endif
    if (! treePos.m_match) {
      jam();
      // there is no "Building" state so this will have to do
      if (indexPtr.p->m_state == Index::Online) {
        jam();
        req->errorCode = TuxMaintReq::SearchError;
      }
      break;
    }
    treeRemove(frag, treePos);
    break;
  default:
    ndbrequire(false);
    break;
  }
#ifdef VM_TRACE
  if (debugFlags & DebugTree) {
    printTree(signal, frag, debugOut);
  }
#endif
  // copy back
  *sig = *req;
}
