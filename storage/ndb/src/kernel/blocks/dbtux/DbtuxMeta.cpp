/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUX_META_CPP
#include "Dbtux.hpp"
#include <my_sys.h>

/*
 * Create index.
 *
 * For historical reasons it looks like we are adding random fragments
 * and attributes to existing index.  In fact all fragments must be
 * created at one time and they have identical attributes.
 *
 * But history changes?
 * Now index will be created using the sequence
 *   CREATE_TAB_REQ
 *     TUP_ADD_ATTR_REQ +
 *
 * Followed by 0-N
 *   TUXFRAGREQ
 */

#include <signaldata/CreateTab.hpp>
#include <signaldata/LqhFrag.hpp>

void
Dbtux::execCREATE_TAB_REQ(Signal* signal)
{
  jamEntry();
  CreateTabReq copy = *(CreateTabReq*)signal->getDataPtr();
  CreateTabReq* req = &copy;

  IndexPtr indexPtr;
  indexPtr.i = RNIL;
  FragOpPtr fragOpPtr;
  fragOpPtr.i = RNIL;
  Uint32 errorCode = 0;

  do {
    // get the index record
    if (req->tableId >= c_indexPool.getSize()) {
      jam();
      errorCode = TuxFragRef::InvalidRequest;
      break;
    }
    c_indexPool.getPtr(indexPtr, req->tableId);
    if (indexPtr.p->m_state != Index::NotDefined)
    {
      jam();
      errorCode = TuxFragRef::InvalidRequest;
      indexPtr.i = RNIL;        // leave alone
      break;
    }

    // get new operation record
    c_fragOpPool.seize(fragOpPtr);
    ndbrequire(fragOpPtr.i != RNIL);
    new (fragOpPtr.p) FragOp();
    fragOpPtr.p->m_userPtr = req->senderData;
    fragOpPtr.p->m_userRef = req->senderRef;
    fragOpPtr.p->m_indexId = req->tableId;
    fragOpPtr.p->m_fragId = RNIL;
    fragOpPtr.p->m_fragNo = RNIL;
    fragOpPtr.p->m_numAttrsRecvd = 0;
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      debugOut << "Seize frag op " << fragOpPtr.i << " " << *fragOpPtr.p << endl;
    }
#endif
    // check if index has place for more fragments
    ndbrequire(indexPtr.p->m_state == Index::NotDefined &&
               DictTabInfo::isOrderedIndex(req->tableType) &&
               req->noOfAttributes > 0 &&
               req->noOfAttributes <= MaxIndexAttributes &&
               indexPtr.p->m_descPage == RNIL);

    indexPtr.p->m_state = Index::Defining;
    indexPtr.p->m_tableType = (DictTabInfo::TableType)req->tableType;
    indexPtr.p->m_tableId = req->primaryTableId;
    indexPtr.p->m_numAttrs = req->noOfAttributes;
    indexPtr.p->m_storeNullKey = true;  // not yet configurable
    // allocate attribute descriptors
    if (! allocDescEnt(indexPtr)) {
      jam();
      errorCode = TuxFragRef::NoFreeAttributes;
      break;
    }

    // error inserts
    if ((ERROR_INSERTED(12001) && fragOpPtr.p->m_fragNo == 0) ||
        (ERROR_INSERTED(12002) && fragOpPtr.p->m_fragNo == 1)) {
      jam();
      errorCode = (TuxFragRef::ErrorCode)1;
      CLEAR_ERROR_INSERT_VALUE;
      break;
    }
    // success
    CreateTabConf* conf = (CreateTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    conf->tuxConnectPtr = fragOpPtr.i;
    sendSignal(req->senderRef, GSN_CREATE_TAB_CONF,
               signal, CreateTabConf::SignalLength, JBB);
    return;
  } while (0);
  // error

  CreateTabRef* const ref = (CreateTabRef*)signal->getDataPtrSend();
  ref->senderData = req->senderData;
  ref->errorCode = errorCode;
  sendSignal(req->senderRef, GSN_CREATE_TAB_REF,
             signal, CreateTabRef::SignalLength, JBB);

  if (indexPtr.i != RNIL) {
    jam();
    // let DICT drop the unfinished index
  }

  if (fragOpPtr.i != RNIL)
  {
    jam();
    c_fragOpPool.release(fragOpPtr);
  }
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
  c_fragOpPool.getPtr(fragOpPtr, req->tuxConnectPtr);
  c_indexPool.getPtr(indexPtr, fragOpPtr.p->m_indexId);
  TuxAddAttrRef::ErrorCode errorCode = TuxAddAttrRef::NoError;
  do {
    // expected attribute id
    const unsigned attrId = fragOpPtr.p->m_numAttrsRecvd++;
    ndbrequire(
        indexPtr.p->m_state == Index::Defining &&
        attrId < indexPtr.p->m_numAttrs &&
        attrId == req->attrId);
    const Uint32 ad = req->attrDescriptor;
    const Uint32 typeId = AttributeDescriptor::getType(ad);
    const Uint32 sizeInBytes = AttributeDescriptor::getSizeInBytes(ad);
    const Uint32 nullable = AttributeDescriptor::getNullable(ad);
    const Uint32 csNumber = req->extTypeInfo >> 16;
    const Uint32 primaryAttrId = req->primaryAttrId;

    DescHead& descHead = getDescHead(*indexPtr.p);
    // add type to spec
    KeySpec& keySpec = indexPtr.p->m_keySpec;
    KeyType keyType(typeId, sizeInBytes, nullable, csNumber);
    if (keySpec.add(keyType) == -1) {
      jam();
      errorCode = TuxAddAttrRef::InvalidAttributeType;
      break;
    }
    // add primary attr to read keys array
    AttributeHeader* keyAttrs = getKeyAttrs(descHead);
    AttributeHeader& keyAttr = keyAttrs[attrId];
    new (&keyAttr) AttributeHeader(primaryAttrId, sizeInBytes);
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      debugOut << "attr " << attrId << " " << keyType << endl;
    }
#endif
    if (csNumber != 0) {
      unsigned err;
      CHARSET_INFO *cs = all_charsets[csNumber];
      ndbrequire(cs != 0);
      if ((err = NdbSqlUtil::check_column_for_ordered_index(typeId, cs))) {
        jam();
        errorCode = (TuxAddAttrRef::ErrorCode) err;
        break;
      }
    }
    const bool lastAttr = (indexPtr.p->m_numAttrs == fragOpPtr.p->m_numAttrsRecvd);
    if ((ERROR_INSERTED(12003) && attrId == 0) ||
        (ERROR_INSERTED(12004) && lastAttr))
    {
      errorCode = (TuxAddAttrRef::ErrorCode)1;
      CLEAR_ERROR_INSERT_VALUE;
      break;
    }
    if (lastAttr) {
      // compute min prefix
      const KeySpec& keySpec = indexPtr.p->m_keySpec;
      unsigned attrs = 0;
      unsigned bytes = keySpec.get_nullmask_len(false);
      unsigned maxAttrs = indexPtr.p->m_numAttrs;
#ifdef VM_TRACE
      {
        const char* p = NdbEnv_GetEnv("MAX_TTREE_PREF_ATTRS", (char*)0, 0);
        if (p != 0 && p[0] != 0 && maxAttrs > (unsigned)atoi(p))
          maxAttrs = atoi(p);
      }
#endif
      while (attrs < maxAttrs) {
        const KeyType& keyType = keySpec.get_type(attrs);
        const unsigned newbytes = bytes + keyType.get_byte_size();
        if (newbytes > (MAX_TTREE_PREF_SIZE << 2))
          break;
        attrs++;
        bytes = newbytes;
      }
      if (attrs == 0)
        bytes = 0;
      indexPtr.p->m_prefAttrs = attrs;
      indexPtr.p->m_prefBytes = bytes;
      // fragment is defined
#ifdef VM_TRACE
      if (debugFlags & DebugMeta) {
        debugOut << "Release frag op " << fragOpPtr.i << " " << *fragOpPtr.p << endl;
      }
#endif
      c_fragOpPool.release(fragOpPtr);
    }
    // success
    TuxAddAttrConf* conf = (TuxAddAttrConf*)signal->getDataPtrSend();
    conf->userPtr = fragOpPtr.p->m_userPtr;
    conf->lastAttr = lastAttr;
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
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      debugOut << "Release on attr error frag op " << fragOpPtr.i << " " << *fragOpPtr.p << endl;
    }
#endif
  // let DICT drop the unfinished index
}

void
Dbtux::execTUXFRAGREQ(Signal* signal)
{
  jamEntry();

  if (signal->theData[0] == (Uint32)-1) {
    jam();
    abortAddFragOp(signal);
    return;
  }

  const TuxFragReq reqCopy = *(const TuxFragReq*)signal->getDataPtr();
  const TuxFragReq* const req = &reqCopy;
  IndexPtr indexPtr;
  indexPtr.i = RNIL;
  TuxFragRef::ErrorCode errorCode = TuxFragRef::NoError;
  do {
    // get the index record
    if (req->tableId >= c_indexPool.getSize()) {
      jam();
      errorCode = TuxFragRef::InvalidRequest;
      break;
    }
    c_indexPool.getPtr(indexPtr, req->tableId);
    if (false && indexPtr.p->m_state != Index::Defining) {
      jam();
      errorCode = TuxFragRef::InvalidRequest;
      indexPtr.i = RNIL;        // leave alone
      break;
    }

    // check if index has place for more fragments
    ndbrequire(indexPtr.p->m_numFrags < MaxIndexFragments);
    // seize new fragment record
    if (ERROR_INSERTED(12008))
    {
      CLEAR_ERROR_INSERT_VALUE;
      errorCode = TuxFragRef::InvalidRequest;
      break;
    }

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
    fragPtr.p->m_tupIndexFragPtrI = req->tupIndexFragPtrI;
    fragPtr.p->m_tupTableFragPtrI = req->tupTableFragPtrI;
    fragPtr.p->m_accTableFragPtrI = req->accTableFragPtrI;
    // add the fragment to the index
    Uint32 fragNo = indexPtr.p->m_numFrags;
    indexPtr.p->m_fragId[indexPtr.p->m_numFrags] = req->fragId;
    indexPtr.p->m_fragPtrI[indexPtr.p->m_numFrags] = fragPtr.i;
    indexPtr.p->m_numFrags++;
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      debugOut << "Add frag " << fragPtr.i << " " << *fragPtr.p << endl;
    }
#endif
    // error inserts
    if ((ERROR_INSERTED(12001) && fragNo == 0) ||
        (ERROR_INSERTED(12002) && fragNo == 1)) {
      jam();
      errorCode = (TuxFragRef::ErrorCode)1;
      CLEAR_ERROR_INSERT_VALUE;
      break;
    }

    // initialize tree header
    TreeHead& tree = fragPtr.p->m_tree;
    new (&tree) TreeHead();
    // make these configurable later
    tree.m_nodeSize = MAX_TTREE_NODE_SIZE;
    tree.m_prefSize = (indexPtr.p->m_prefBytes + 3) / 4;
    const unsigned maxSlack = MAX_TTREE_NODE_SLACK;
    // size of header and min prefix
    const unsigned fixedSize = NodeHeadSize + tree.m_prefSize;
    if (! (fixedSize <= tree.m_nodeSize)) {
      jam();
      errorCode = (TuxFragRef::ErrorCode)TuxAddAttrRef::InvalidNodeSize;
      break;
    }
    const unsigned slots = (tree.m_nodeSize - fixedSize) / TreeEntSize;
    tree.m_maxOccup = slots;
    // min occupancy of interior node must be at least 2
    if (! (2 + maxSlack <= tree.m_maxOccup)) {
      jam();
      errorCode = (TuxFragRef::ErrorCode)TuxAddAttrRef::InvalidNodeSize;
      break;
    }
    tree.m_minOccup = tree.m_maxOccup - maxSlack;
    // root node does not exist (also set by ctor)
    tree.m_root = NullTupLoc;
#ifdef VM_TRACE
    if (debugFlags & DebugMeta) {
      if (fragNo == 0) {
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

    // success
    TuxFragConf* const conf = (TuxFragConf*)signal->getDataPtrSend();
    conf->userPtr = req->userPtr;
    conf->tuxConnectPtr = RNIL;
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

  if (indexPtr.i != RNIL) {
    jam();
    // let DICT drop the unfinished index
  }
}

/*
 * LQH aborts on-going create index operation.
 */
void
Dbtux::abortAddFragOp(Signal* signal)
{
  FragOpPtr fragOpPtr;
  IndexPtr indexPtr;
  c_fragOpPool.getPtr(fragOpPtr, signal->theData[1]);
  c_indexPool.getPtr(indexPtr, fragOpPtr.p->m_indexId);
#ifdef VM_TRACE
  if (debugFlags & DebugMeta) {
    debugOut << "Release on abort frag op " << fragOpPtr.i << " " << *fragOpPtr.p << endl;
  }
#endif
  c_fragOpPool.release(fragOpPtr);
  // let DICT drop the unfinished index
}

/*
 * Set index online.  Currently at system restart this arrives before
 * build and is therefore not correct.
 */
void
Dbtux::execALTER_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const AlterIndxImplReq reqCopy = *(const AlterIndxImplReq*)signal->getDataPtr();
  const AlterIndxImplReq* const req = &reqCopy;

  IndexPtr indexPtr;
  c_indexPool.getPtr(indexPtr, req->indexId);

  //Uint32 save = indexPtr.p->m_state;
  if (! (refToBlock(req->senderRef) == DBDICT) &&
      ! (isNdbMt() && refToMain(req->senderRef) == DBTUX && 
         refToInstance(req->senderRef) == 0))
  {
    /**
     * DICT has a really distorted view of the world...
     *   ignore it :(
     */
    jam();
    switch(req->requestType){
    case AlterIndxImplReq::AlterIndexOffline:
      jam();
      /*
       * This happens at failed index build, and before dropping an
       * Online index.  It causes scans to terminate.
       */
      indexPtr.p->m_state = Index::Dropping;
      break;
    case AlterIndxImplReq::AlterIndexBuilding:
      jam();
      indexPtr.p->m_state = Index::Building;
      break;
    default:
      jam(); // fall-through
    case AlterIndxImplReq::AlterIndexOnline:
      jam();
      indexPtr.p->m_state = Index::Online;
      break;
    }
  }
  
  // success
  AlterIndxImplConf* const conf = (AlterIndxImplConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = req->senderData;
  if (req->senderRef != 0)
  {
    /**
     * TUP cheats and does execute direct
     *   setting UserRef to 0
     */
    jam();
    sendSignal(req->senderRef, GSN_ALTER_INDX_IMPL_CONF,
               signal, AlterIndxImplConf::SignalLength, JBB);
  }
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
  /*
   * Index state should be Defining or Dropping but in 7.0 it can also
   * be NotDefined (due to double call).  The Index record is always
   * consistent regardless of state so there is no state assert here.
   */
  // drop fragments
  while (indexPtr.p->m_numFrags > 0) {
    jam();
    Uint32 i = --indexPtr.p->m_numFrags;
    FragPtr fragPtr;
    c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
    /*
     * Verify that LQH has terminated scans.  (If not, then drop order
     * must change from TUP,TUX to TUX,TUP and we must wait for scans).
     */
    ScanOpPtr scanPtr;
    bool b = fragPtr.p->m_scanList.first(scanPtr);
    ndbrequire(!b);
    c_fragPool.release(fragPtr);
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
  const Uint32 size = getDescSize(*indexPtr.p);
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
  DescHead& descHead = *(DescHead*)&pagePtr.p->m_data[indexPtr.p->m_descOff];
  descHead.m_indexId = indexPtr.i;
  descHead.m_numAttrs = indexPtr.p->m_numAttrs;
  descHead.m_magic = DescHead::Magic;
  KeySpec& keySpec = indexPtr.p->m_keySpec;
  KeyType* keyTypes = getKeyTypes(descHead);
  keySpec.set_buf(keyTypes, indexPtr.p->m_numAttrs);
  return true;
}

void
Dbtux::freeDescEnt(IndexPtr indexPtr)
{
  DescPagePtr pagePtr;
  c_descPagePool.getPtr(pagePtr, indexPtr.p->m_descPage);
  Uint32* const data = pagePtr.p->m_data;
  const Uint32 size = getDescSize(*indexPtr.p);
  Uint32 off = indexPtr.p->m_descOff;
  // move the gap to the free area at the top
  while (off + size < DescPageSize - pagePtr.p->m_numFree) {
    jam();
    // next entry to move over the gap
    DescHead& descHead2 = *(DescHead*)&data[off + size];
    Uint32 indexId2 = descHead2.m_indexId;
    Index& index2 = *c_indexPool.getPtr(indexId2);
    Uint32 size2 = getDescSize(index2);
    ndbrequire(
        index2.m_descPage == pagePtr.i &&
        index2.m_descOff == off + size &&
        index2.m_numAttrs == descHead2.m_numAttrs);
    // move the entry (overlapping copy if size < size2)
    Uint32 i;
    for (i = 0; i < size2; i++) {
      jam();
      data[off + i] = data[off + size + i];
    }
    off += size2;
    // adjust page offset in index
    index2.m_descOff -= size;
    {
      // move KeySpec pointer
      DescHead& descHead2 = getDescHead(index2);
      KeyType* keyType2 = getKeyTypes(descHead2);
      index2.m_keySpec.set_buf(keyType2);
      ndbrequire(index2.m_keySpec.validate() == 0);
     }
  }
  ndbrequire(off + size == DescPageSize - pagePtr.p->m_numFree);
  pagePtr.p->m_numFree += size;
}

void
Dbtux::execDROP_FRAG_REQ(Signal* signal)
{
  DropFragReq copy = *(DropFragReq*)signal->getDataPtr();
  DropFragReq *req = &copy;

  IndexPtr indexPtr;
  c_indexPool.getPtr(indexPtr, req->tableId);
  Uint32 i = 0;
  for (i = 0; i < indexPtr.p->m_numFrags; i++)
  {
    jam();
    if (indexPtr.p->m_fragId[i] == req->fragId)
    {
      jam();
      FragPtr fragPtr;
      c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
      c_fragPool.release(fragPtr);

      for (i++; i < indexPtr.p->m_numFrags; i++)
      {
        jam();
        indexPtr.p->m_fragPtrI[i-1] = indexPtr.p->m_fragPtrI[i];
        indexPtr.p->m_fragId[i-1] = indexPtr.p->m_fragId[i];
      }
      indexPtr.p->m_numFrags--;
      break;
    }
  }


  // reply to sender
  DropFragConf* const conf = (DropFragConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = req->senderData;
  conf->tableId = req->tableId;
  sendSignal(req->senderRef, GSN_DROP_FRAG_CONF,
             signal, DropFragConf::SignalLength, JBB);
}
