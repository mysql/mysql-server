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

#define DBTUX_META_CPP
#include "Dbtux.hpp"

/*
 * Create index.
 *
 * For historical reasons it looks like we are adding random fragments
 * and attributes to existing index.  In fact all fragments must be
 * created at one time and they have identical attributes.
 */

void
Dbtux::execTUXFRAGREQ(Signal* signal)
{
  jamEntry();
  const TuxFragReq reqCopy = *(const TuxFragReq*)signal->getDataPtr();
  const TuxFragReq* const req = &reqCopy;
  IndexPtr indexPtr;
  indexPtr.i = RNIL;
  FragOpPtr fragOpPtr;
  fragOpPtr.i = RNIL;
  TuxFragRef::ErrorCode errorCode = TuxFragRef::NoError;
  do {
    // get the index record
    if (req->tableId >= c_indexPool.getSize()) {
      jam();
      errorCode = TuxFragRef::InvalidRequest;
      break;
    }
    c_indexPool.getPtr(indexPtr, req->tableId);
    if (indexPtr.p->m_state != Index::NotDefined &&
        indexPtr.p->m_state != Index::Defining) {
      jam();
      errorCode = TuxFragRef::InvalidRequest;
      indexPtr.i = RNIL;        // leave alone
      break;
    }
    // get new operation record
    c_fragOpPool.seize(fragOpPtr);
    ndbrequire(fragOpPtr.i != RNIL);
    new (fragOpPtr.p) FragOp();
    fragOpPtr.p->m_userPtr = req->userPtr;
    fragOpPtr.p->m_userRef = req->userRef;
    fragOpPtr.p->m_indexId = req->tableId;
    fragOpPtr.p->m_fragId = req->fragId;
    fragOpPtr.p->m_fragNo = indexPtr.p->m_numFrags;
    fragOpPtr.p->m_numAttrsRecvd = 0;
    // check if index has place for more fragments
    ndbrequire(indexPtr.p->m_numFrags < MaxIndexFragments);
    // seize new fragment record
    FragPtr fragPtr;
    c_fragPool.seize(fragPtr);
    if (fragPtr.i == RNIL) {
      jam();
      errorCode = TuxFragRef::NoFreeFragment;
      break;
    }
    new (fragPtr.p) Frag(c_scanOpPool);
    fragPtr.p->m_tableId = req->primaryTableId;
    fragPtr.p->m_indexId = req->tableId;
    fragPtr.p->m_fragId = req->fragId;
    fragPtr.p->m_numAttrs = req->noOfAttr;
    fragPtr.p->m_storeNullKey = true;  // not yet configurable
    fragPtr.p->m_tupIndexFragPtrI = req->tupIndexFragPtrI;
    fragPtr.p->m_tupTableFragPtrI[0] = req->tupTableFragPtrI[0];
    fragPtr.p->m_tupTableFragPtrI[1] = req->tupTableFragPtrI[1];
    fragPtr.p->m_accTableFragPtrI[0] = req->accTableFragPtrI[0];
    fragPtr.p->m_accTableFragPtrI[1] = req->accTableFragPtrI[1];
    // add the fragment to the index
    indexPtr.p->m_fragId[indexPtr.p->m_numFrags] = req->fragId;
    indexPtr.p->m_fragPtrI[indexPtr.p->m_numFrags] = fragPtr.i;
    indexPtr.p->m_numFrags++;
    // save under operation
    fragOpPtr.p->m_fragPtrI = fragPtr.i;
    // prepare to receive attributes
    if (fragOpPtr.p->m_fragNo == 0) {
      jam();
      // receiving first fragment
      ndbrequire(
          indexPtr.p->m_state == Index::NotDefined &&
          DictTabInfo::isOrderedIndex(req->tableType) &&
          req->noOfAttr > 0 &&
          req->noOfAttr <= MaxIndexAttributes &&
          indexPtr.p->m_descPage == RNIL);
      indexPtr.p->m_state = Index::Defining;
      indexPtr.p->m_tableType = (DictTabInfo::TableType)req->tableType;
      indexPtr.p->m_tableId = req->primaryTableId;
      indexPtr.p->m_numAttrs = req->noOfAttr;
      indexPtr.p->m_storeNullKey = true;  // not yet configurable
      // allocate attribute descriptors
      if (! allocDescEnt(indexPtr)) {
        jam();
        errorCode = TuxFragRef::NoFreeAttributes;
        break;
      }
    } else {
      // receiving subsequent fragment
      jam();
      ndbrequire(
          indexPtr.p->m_state == Index::Defining &&
          indexPtr.p->m_tableType == (DictTabInfo::TableType)req->tableType &&
          indexPtr.p->m_tableId == req->primaryTableId &&
          indexPtr.p->m_numAttrs == req->noOfAttr);
    }
    // copy metadata address to each fragment
    fragPtr.p->m_descPage = indexPtr.p->m_descPage;
    fragPtr.p->m_descOff = indexPtr.p->m_descOff;
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      debugOut << "Add frag " << fragPtr.i << " " << *fragPtr.p << endl;
    }
#endif
    // success
    TuxFragConf* const conf = (TuxFragConf*)signal->getDataPtrSend();
    conf->userPtr = req->userPtr;
    conf->tuxConnectPtr = fragOpPtr.i;
    conf->fragPtr = fragPtr.i;
    conf->fragId = fragPtr.p->m_fragId;
    sendSignal(req->userRef, GSN_TUXFRAGCONF,
        signal, TuxFragConf::SignalLength, JBB);
    return;
  } while (0);
  // error
  TuxFragRef* const ref = (TuxFragRef*)signal->getDataPtrSend();
  ref->userPtr = req->userPtr;
  ref->errorCode = errorCode;
  sendSignal(req->userRef, GSN_TUXFRAGREF,
      signal, TuxFragRef::SignalLength, JBB);
  if (fragOpPtr.i != RNIL)
    c_fragOpPool.release(fragOpPtr);
  if (indexPtr.i != RNIL)
    dropIndex(signal, indexPtr, 0, 0);
}

void
Dbtux::execTUX_ADD_ATTRREQ(Signal* signal)
{
  jamEntry();
  const TuxAddAttrReq reqCopy = *(const TuxAddAttrReq*)signal->getDataPtr();
  const TuxAddAttrReq* const req = &reqCopy;
  // get the records
  FragOpPtr fragOpPtr;
  IndexPtr indexPtr;
  FragPtr fragPtr;
  c_fragOpPool.getPtr(fragOpPtr, req->tuxConnectPtr);
  c_indexPool.getPtr(indexPtr, fragOpPtr.p->m_indexId);
  c_fragPool.getPtr(fragPtr, fragOpPtr.p->m_fragPtrI);
  TuxAddAttrRef::ErrorCode errorCode = TuxAddAttrRef::NoError;
  do {
    // expected attribute id
    const unsigned attrId = fragOpPtr.p->m_numAttrsRecvd++;
    ndbrequire(
        indexPtr.p->m_state == Index::Defining &&
        attrId < indexPtr.p->m_numAttrs &&
        attrId == req->attrId);
    // define the attribute
    DescEnt& descEnt = getDescEnt(indexPtr.p->m_descPage, indexPtr.p->m_descOff);
    DescAttr& descAttr = descEnt.m_descAttr[attrId];
    descAttr.m_attrDesc = req->attrDescriptor;
    descAttr.m_primaryAttrId = req->primaryAttrId;
    descAttr.m_typeId = req->extTypeInfo & 0xFF;
    descAttr.m_charset = (req->extTypeInfo >> 16);
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      debugOut << "Add frag " << fragPtr.i << " attr " << attrId << " " << descAttr << endl;
    }
#endif
    // check that type is valid and has a binary comparison method
    const NdbSqlUtil::Type& type = NdbSqlUtil::getTypeBinary(descAttr.m_typeId);
    if (type.m_typeId == NdbSqlUtil::Type::Undefined ||
        type.m_cmp == 0) {
      jam();
      errorCode = TuxAddAttrRef::InvalidAttributeType;
      break;
    }
#ifdef dbtux_uses_charset
    if (descAttr.m_charset != 0) {
      CHARSET_INFO *cs = get_charset(descAttr.m_charset, MYF(0));
      // here use the non-binary type
      if (! NdbSqlUtil::usable_in_ordered_index(descAttr.m_typeId, cs)) {
        jam();
        errorCode = TuxAddAttrRef::InvalidCharset;
        break;
      }
    }
#endif
    if (indexPtr.p->m_numAttrs == fragOpPtr.p->m_numAttrsRecvd) {
      jam();
      // initialize tree header
      TreeHead& tree = fragPtr.p->m_tree;
      new (&tree) TreeHead();
      // make these configurable later
      tree.m_nodeSize = MAX_TTREE_NODE_SIZE;
      tree.m_prefSize = MAX_TTREE_PREF_SIZE;
      const unsigned maxSlack = MAX_TTREE_NODE_SLACK;
      // size up to and including first 2 entries
      const unsigned pref = tree.getSize(AccPref);
      if (! (pref <= tree.m_nodeSize)) {
        jam();
        errorCode = TuxAddAttrRef::InvalidNodeSize;
        break;
      }
      const unsigned slots = (tree.m_nodeSize - pref) / TreeEntSize;
      // leave out work space entry
      tree.m_maxOccup = 2 + slots - 1;
      // min occupancy of interior node must be at least 2
      if (! (2 + maxSlack <= tree.m_maxOccup)) {
        jam();
        errorCode = TuxAddAttrRef::InvalidNodeSize;
        break;
      }
      tree.m_minOccup = tree.m_maxOccup - maxSlack;
      // root node does not exist (also set by ctor)
      tree.m_root = NullTupLoc;
#ifdef VM_TRACE
      if (debugFlags & DebugMeta) {
        if (fragOpPtr.p->m_fragNo == 0) {
          debugOut << "Index id=" << indexPtr.i;
          debugOut << " nodeSize=" << tree.m_nodeSize;
          debugOut << " headSize=" << NodeHeadSize;
          debugOut << " prefSize=" << tree.m_prefSize;
          debugOut << " entrySize=" << TreeEntSize;
          debugOut << " minOccup=" << tree.m_minOccup;
          debugOut << " maxOccup=" << tree.m_maxOccup;
          debugOut << endl;
        }
      }
#endif
      // fragment is defined
      c_fragOpPool.release(fragOpPtr);
    }
    // success
    TuxAddAttrConf* conf = (TuxAddAttrConf*)signal->getDataPtrSend();
    conf->userPtr = fragOpPtr.p->m_userPtr;
    sendSignal(fragOpPtr.p->m_userRef, GSN_TUX_ADD_ATTRCONF,
        signal, TuxAddAttrConf::SignalLength, JBB);
    return;
  } while (0);
  // error
  TuxAddAttrRef* ref = (TuxAddAttrRef*)signal->getDataPtrSend();
  ref->userPtr = fragOpPtr.p->m_userPtr;
  ref->errorCode = errorCode;
  sendSignal(fragOpPtr.p->m_userRef, GSN_TUX_ADD_ATTRREF,
      signal, TuxAddAttrRef::SignalLength, JBB);
  c_fragOpPool.release(fragOpPtr);
  dropIndex(signal, indexPtr, 0, 0);
}

/*
 * Set index online.  Currently at system restart this arrives before
 * build and is therefore not correct.
 */
void
Dbtux::execALTER_INDX_REQ(Signal* signal)
{
  jamEntry();
  const AlterIndxReq reqCopy = *(const AlterIndxReq*)signal->getDataPtr();
  const AlterIndxReq* const req = &reqCopy;
  // set index online after build
  IndexPtr indexPtr;
  c_indexPool.getPtr(indexPtr, req->getIndexId());
  indexPtr.p->m_state = Index::Online;
#ifdef VM_TRACE
  if (debugFlags & DebugMeta) {
    debugOut << "Online index " << indexPtr.i << " " << *indexPtr.p << endl;
  }
#endif
  // success
  AlterIndxConf* const conf = (AlterIndxConf*)signal->getDataPtrSend();
  conf->setUserRef(reference());
  conf->setConnectionPtr(req->getConnectionPtr());
  conf->setRequestType(req->getRequestType());
  conf->setTableId(req->getTableId());
  conf->setIndexId(req->getIndexId());
  conf->setIndexVersion(req->getIndexVersion());
  sendSignal(req->getUserRef(), GSN_ALTER_INDX_CONF,
      signal, AlterIndxConf::SignalLength, JBB);
}

/*
 * Drop index.
 *
 * Uses same DROP_TAB_REQ signal as normal tables.
 */

void
Dbtux::execDROP_TAB_REQ(Signal* signal)
{
  jamEntry();
  const DropTabReq reqCopy = *(const DropTabReq*)signal->getDataPtr();
  const DropTabReq* const req = &reqCopy;
  IndexPtr indexPtr;

  Uint32 tableId = req->tableId;
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  if (tableId >= c_indexPool.getSize()) {
    jam();
    // reply to sender
    DropTabConf* const conf = (DropTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->tableId = tableId;
    sendSignal(senderRef, GSN_DROP_TAB_CONF,
	       signal, DropTabConf::SignalLength, JBB);
    return;
  }
  
  c_indexPool.getPtr(indexPtr, req->tableId);
  // drop works regardless of index state
#ifdef VM_TRACE
  if (debugFlags & DebugMeta) {
    debugOut << "Drop index " << indexPtr.i << " " << *indexPtr.p << endl;
  }
#endif
  ndbrequire(req->senderRef != 0);
  dropIndex(signal, indexPtr, req->senderRef, req->senderData);
}

void
Dbtux::dropIndex(Signal* signal, IndexPtr indexPtr, Uint32 senderRef, Uint32 senderData)
{
  jam();
  indexPtr.p->m_state = Index::Dropping;
  // drop one fragment at a time
  if (indexPtr.p->m_numFrags > 0) {
    jam();
    unsigned i = --indexPtr.p->m_numFrags;
    FragPtr fragPtr;
    c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
    c_fragPool.release(fragPtr);
    // the real time break is not used for anything currently
    signal->theData[0] = TuxContinueB::DropIndex;
    signal->theData[1] = indexPtr.i;
    signal->theData[2] = senderRef;
    signal->theData[3] = senderData;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  // drop attributes
  if (indexPtr.p->m_descPage != RNIL) {
    jam();
    freeDescEnt(indexPtr);
    indexPtr.p->m_descPage = RNIL;
  }
  if (senderRef != 0) {
    jam();
    // reply to sender
    DropTabConf* const conf = (DropTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->tableId = indexPtr.i;
    sendSignal(senderRef, GSN_DROP_TAB_CONF,
        signal, DropTabConf::SignalLength, JBB);
  }
  new (indexPtr.p) Index();
}

/*
 * Subroutines.
 */

bool
Dbtux::allocDescEnt(IndexPtr indexPtr)
{
  jam();
  const unsigned size = DescHeadSize + indexPtr.p->m_numAttrs * DescAttrSize;
  DescPagePtr pagePtr;
  pagePtr.i = c_descPageList;
  while (pagePtr.i != RNIL) {
    jam();
    c_descPagePool.getPtr(pagePtr);
    if (pagePtr.p->m_numFree >= size) {
      jam();
      break;
    }
    pagePtr.i = pagePtr.p->m_nextPage;
  }
  if (pagePtr.i == RNIL) {
    jam();
    if (! c_descPagePool.seize(pagePtr)) {
      jam();
      return false;
    }
    new (pagePtr.p) DescPage();
    // add in front of list
    pagePtr.p->m_nextPage = c_descPageList;
    c_descPageList = pagePtr.i;
    pagePtr.p->m_numFree = DescPageSize;
  }
  ndbrequire(pagePtr.p->m_numFree >= size);
  indexPtr.p->m_descPage = pagePtr.i;
  indexPtr.p->m_descOff = DescPageSize - pagePtr.p->m_numFree;
  pagePtr.p->m_numFree -= size;
  DescEnt& descEnt = getDescEnt(indexPtr.p->m_descPage, indexPtr.p->m_descOff);
  descEnt.m_descHead.m_indexId = indexPtr.i;
  descEnt.m_descHead.pad1 = 0;
  return true;
}

void
Dbtux::freeDescEnt(IndexPtr indexPtr)
{
  DescPagePtr pagePtr;
  c_descPagePool.getPtr(pagePtr, indexPtr.p->m_descPage);
  Uint32* const data = pagePtr.p->m_data;
  const unsigned size = DescHeadSize + indexPtr.p->m_numAttrs * DescAttrSize;
  unsigned off = indexPtr.p->m_descOff;
  // move the gap to the free area at the top
  while (off + size < DescPageSize - pagePtr.p->m_numFree) {
    jam();
    // next entry to move over the gap
    DescEnt& descEnt2 = *(DescEnt*)&data[off + size];
    Uint32 indexId2 = descEnt2.m_descHead.m_indexId;
    Index& index2 = *c_indexPool.getPtr(indexId2);
    unsigned size2 = DescHeadSize + index2.m_numAttrs * DescAttrSize;
    ndbrequire(
        index2.m_descPage == pagePtr.i &&
        index2.m_descOff == off + size);
    // move the entry (overlapping copy if size < size2)
    unsigned i;
    for (i = 0; i < size2; i++) {
      jam();
      data[off + i] = data[off + size + i];
    }
    off += size2;
    // adjust page offset in index and all fragments
    index2.m_descOff -= size;
    for (i = 0; i < index2.m_numFrags; i++) {
      jam();
      Frag& frag2 = *c_fragPool.getPtr(index2.m_fragPtrI[i]);
      frag2.m_descOff -= size;
      ndbrequire(
          frag2.m_descPage == index2.m_descPage &&
          frag2.m_descOff == index2.m_descOff);
    }
  }
  ndbrequire(off + size == DescPageSize - pagePtr.p->m_numFree);
  pagePtr.p->m_numFree += size;
}
