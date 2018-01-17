/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUX_MAINT_CPP
#include "Dbtux.hpp"

#define JAM_FILE_ID 369


/*
 * Maintain index.
 */

void
Dbtux::execTUX_MAINT_REQ(Signal* signal)
{
  jamEntryDebug();
  TuxMaintReq* const sig = (TuxMaintReq*)signal->getDataPtrSend();
  // ignore requests from redo log
  IndexPtr indexPtr;
  c_indexPool.getPtr(indexPtr, sig->indexId);

  if (unlikely(! (indexPtr.p->m_state == Index::Online ||
                  indexPtr.p->m_state == Index::Building)))
  {
    jam();
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      TupLoc tupLoc(sig->pageId, sig->pageIndex);
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
  // get the index
  ndbrequire(indexPtr.p->m_tableId == req->tableId);
  // get base fragment id and extra bits
  const Uint32 fragId = req->fragId;
  // get the fragment
  FragPtr fragPtr;
  findFrag(jamBuffer(), *indexPtr.p, fragId, fragPtr);
  ndbrequire(fragPtr.i != RNIL);
  Frag& frag = *fragPtr.p;
  // set up search entry
  TreeEnt ent;
  ent.m_tupLoc = TupLoc(req->pageId, req->pageIndex);
  ent.m_tupVersion = req->tupVersion;
  // set up and read search key
  KeyData searchKey(indexPtr.p->m_keySpec, false, 0);
  searchKey.set_buf(c_ctx.c_searchKey, MaxAttrDataSize << 2);
  readKeyAttrs(c_ctx, frag, ent, searchKey, indexPtr.p->m_numAttrs);
  if (unlikely(! indexPtr.p->m_storeNullKey) &&
      searchKey.get_null_cnt() == indexPtr.p->m_numAttrs) {
    jam();
    return;
  }
#ifdef VM_TRACE
  if (debugFlags & DebugMaint) {
    const Uint32 opFlag = req->opInfo >> 8;
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
  bool ok;
  switch (opCode) {
  case TuxMaintReq::OpAdd:
    jamDebug();
    ok = searchToAdd(c_ctx, frag, searchKey, ent, treePos);
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      debugOut << treePos << (! ok ? " - error" : "") << endl;
    }
#endif
    if (! ok) {
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
      req->errorCode = allocNode(c_ctx, node);
      if (req->errorCode != 0) {
        jam();
        break;
      }
      frag.m_freeLoc = node.m_loc;
      ndbrequire(frag.m_freeLoc != NullTupLoc);
    }
    treeAdd(c_ctx, frag, treePos, ent);
    frag.m_entryCount++;
    frag.m_entryBytes += searchKey.get_data_len();
    frag.m_entryOps++;
    break;
  case TuxMaintReq::OpRemove:
    jam();
    ok = searchToRemove(c_ctx, frag, searchKey, ent, treePos);
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      debugOut << treePos << (! ok ? " - error" : "") << endl;
    }
#endif
    if (! ok) {
      jam();
      // there is no "Building" state so this will have to do
      if (indexPtr.p->m_state == Index::Online) {
        jam();
        req->errorCode = TuxMaintReq::SearchError;
      }
      break;
    }
    treeRemove(frag, treePos);
    ndbrequire(frag.m_entryCount != 0);
    frag.m_entryCount--;
    frag.m_entryBytes -= searchKey.get_data_len();
    frag.m_entryOps++;
    break;
  default:
    ndbabort();
  }
#ifdef VM_TRACE
  if (debugFlags & DebugTree) {
    printTree(signal, frag, debugOut);
  }
#endif
  // copy back
  *sig = *req;

  //ndbrequire(c_keyAttrs[0] == c_keyAttrs[1]);
  //ndbrequire(c_sqlCmp[0] == c_sqlCmp[1]);
  //ndbrequire(c_searchKey[0] == c_searchKey[1]);
  //ndbrequire(c_entryKey[0] == c_entryKey[1]);
  //ndbrequire(c_dataBuffer[0] == c_dataBuffer[1]);
}
