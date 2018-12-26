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




/*****************************************************************************
Name:          Ndb.cpp
******************************************************************************/

#include <ndb_global.h>

#include "API.hpp"
#include <md5_hash.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>
#include <ndb_limits.h>
#include <NdbEnv.h>
#include <BaseString.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbTick.h>

/****************************************************************************
void doConnect();

Connect to any node which has no connection at the moment.
****************************************************************************/
NdbTransaction* Ndb::doConnect(Uint32 tConNode, Uint32 instance)
{
  Uint32        tNode;
  Uint32        tAnyAlive = 0;
  int TretCode= 0;

  DBUG_ENTER("Ndb::doConnect");

  if (tConNode != 0) {
    TretCode = NDB_connect(tConNode, instance);
    if ((TretCode == 1) || (TretCode == 2)) {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
      DBUG_RETURN(getConnectedNdbTransaction(tConNode, instance));
    } else if (TretCode < 0) {
      DBUG_RETURN(NULL);
    } else if (TretCode != 0) {
      tAnyAlive = 1;
    }//if
  }//if
//****************************************************************************
// We will connect to any node. Make sure that we have connections to all
// nodes.
//****************************************************************************
  Uint32 anyInstance = 0;
  if (theImpl->m_optimized_node_selection)
  {
    Ndb_cluster_connection_node_iter &node_iter= 
      theImpl->m_node_iter;
    theImpl->m_ndb_cluster_connection.init_get_next_node(node_iter);
    while ((tNode= theImpl->m_ndb_cluster_connection.get_next_node(node_iter)))
    {
      TretCode= NDB_connect(tNode, anyInstance);
      if ((TretCode == 1) ||
	  (TretCode == 2))
      {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
	DBUG_RETURN(getConnectedNdbTransaction(tNode, anyInstance));
      } else if (TretCode < 0) {
        DBUG_RETURN(NULL);
      } else if (TretCode != 0) {
	tAnyAlive= 1;
      }//if
      DBUG_PRINT("info",("tried node %d, TretCode %d, error code %d, %s",
			 tNode, TretCode, getNdbError().code,
			 getNdbError().message));
    }
  }
  else // just do a regular round robin
  {
    Uint32 tNoOfDbNodes= theImpl->theNoOfDBnodes;
    Uint32 &theCurrentConnectIndex= theImpl->theCurrentConnectIndex;
    UintR Tcount = 0;
    do {
      theCurrentConnectIndex++;
      if (theCurrentConnectIndex >= tNoOfDbNodes)
	theCurrentConnectIndex = 0;

      Tcount++;
      tNode= theImpl->theDBnodes[theCurrentConnectIndex];
      TretCode= NDB_connect(tNode, anyInstance);
      if ((TretCode == 1) ||
	  (TretCode == 2))
      {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
	DBUG_RETURN(getConnectedNdbTransaction(tNode, anyInstance));
      } else if (TretCode < 0) {
        DBUG_RETURN(NULL);
      } else if (TretCode != 0) {
	tAnyAlive= 1;
      }//if
      DBUG_PRINT("info",("tried node %d TretCode %d", tNode, TretCode));
    } while (Tcount < tNoOfDbNodes);
  }
//****************************************************************************
// We were unable to find a free connection. If no node alive we will report
// error code for cluster failure otherwise connection failure.
//****************************************************************************
  if (tAnyAlive == 1) {
#ifdef VM_TRACE
    ndbout << "TretCode = " << TretCode << endl;
#endif
    theError.code = 4006;
  } else {
    if (theImpl->m_transporter_facade->is_cluster_completely_unavailable())
    {
      theError.code = 4009;
    }
    else
    {
      theError.code = 4035;
    }
  }//if
  DBUG_RETURN(NULL);
}

int 
Ndb::NDB_connect(Uint32 tNode, Uint32 instance)
{
//****************************************************************************
// We will perform seize of a transaction record in DBTC in the specified node.
//***************************************************************************
  
  int	         tReturnCode;

  DBUG_ENTER("Ndb::NDB_connect");

  {
    if (theImpl->get_node_stopping(tNode))
    {
      DBUG_RETURN(0);
    }
  }

  NdbTransaction * tConArray = theConnectionArray[tNode];
  if (instance != 0 && tConArray != 0)
  {
    NdbTransaction* prev = 0;
    NdbTransaction* curr = tConArray;
    while (curr)
    {
      if (refToInstance(curr->m_tcRef) == instance)
      {
        if (prev != 0)
        {
          prev->theNext = curr->theNext;
          if (!curr->theNext)
            theConnectionArrayLast[tNode] = prev;
          curr->theNext = tConArray;
          theConnectionArray[tNode] = curr;
        }
        else
        {
          assert(curr == tConArray);
        }
        DBUG_RETURN(2);
      }
      prev = curr;
      curr = curr->theNext;
    }
  }
  else if (tConArray != NULL)
  {
    DBUG_RETURN(2);
  }

  NdbTransaction * tNdbCon = getNdbCon();	// Get free connection object.
  if (tNdbCon == NULL) {
    DBUG_RETURN(4);
  }//if
  NdbApiSignal*	tSignal = getSignal();		// Get signal object
  if (tSignal == NULL) {
    releaseNdbCon(tNdbCon);
    DBUG_RETURN(4);
  }//if
  if (tSignal->setSignal(GSN_TCSEIZEREQ, DBTC) == -1) {
    releaseNdbCon(tNdbCon);
    releaseSignal(tSignal);
    DBUG_RETURN(4);
  }//if
  tSignal->setData(tNdbCon->ptr2int(), 1);
//************************************************
// Set connection pointer as NdbTransaction object
//************************************************
  tSignal->setData(theMyRef, 2);	// Set my block reference
  tSignal->setData(instance, 3);        // Set requested instance
  tNdbCon->Status(NdbTransaction::Connecting); // Set status to connecting
  tNdbCon->theDBnode = tNode;
  Uint32 nodeSequence;
  tReturnCode= sendRecSignal(tNode, WAIT_TC_SEIZE, tSignal,
                             0, &nodeSequence);
  releaseSignal(tSignal); 
  if ((tReturnCode == 0) && (tNdbCon->Status() == NdbTransaction::Connected)) {
    //************************************************
    // Send and receive was successful
    //************************************************
    tNdbCon->setConnectedNodeId(tNode, nodeSequence);
    
    tNdbCon->setMyBlockReference(theMyRef);
    prependConnectionArray(tNdbCon, tNode);
    DBUG_RETURN(1);
  } else {
//****************************************************************************
// Unsuccessful connect is indicated by 3.
//****************************************************************************
    DBUG_PRINT("info",
	       ("unsuccessful connect tReturnCode %d, tNdbCon->Status() %d",
		tReturnCode, tNdbCon->Status()));
    releaseNdbCon(tNdbCon);
    if (theError.code == 299 || // single user mode
        theError.code == 281 )  // cluster shutdown in progress
    {
      // no need to retry with other node
      DBUG_RETURN(-1);
    }

    /**
     * If node was dead, report 0...
     *
     * Btw, the sendRecSignal-method should taken out and shot
     */
    switch(tReturnCode){
    case -2:
    case -3:
      DBUG_RETURN(0);
    }

    DBUG_RETURN(3);
  }//if
}//Ndb::NDB_connect()

NdbTransaction *
Ndb::getConnectedNdbTransaction(Uint32 nodeId, Uint32 instance){
  NdbTransaction* next = theConnectionArray[nodeId];
  if (instance != 0)
  {
    NdbTransaction * prev = 0;
    while (next)
    {
      if (refToInstance(next->m_tcRef) == instance)
      {
        if (prev != 0)
        {
          assert(false); // Should have been moved in NDB_connect
          prev->theNext = next->theNext;
          if (!next->theNext)
            theConnectionArrayLast[nodeId] = prev;
          goto found_middle;
        }
        else
        {
          assert(next == theConnectionArray[nodeId]);
          goto found_first;
        }
      }
      prev = next;
      next = next->theNext;
    }
    assert(false); // !!
    return 0;
  }
found_first:
  removeConnectionArray(next, nodeId);
found_middle:
  next->theNext = NULL;

  return next;
}//Ndb::getConnectedNdbTransaction()

/*****************************************************************************
void doDisconnect();

Remark:        Disconnect all connections to the database. 
*****************************************************************************/
void 
Ndb::doDisconnect()
{
  DBUG_ENTER("Ndb::doDisconnect");
  NdbTransaction* tNdbCon;
  CHECK_STATUS_MACRO_VOID;

  /**
   * Clean up active NdbTransactions by releasing all NdbOperations,
   * ScanOperations, and NdbQuery owned by it. Release of
   * Scan- and QueryOperations will also close any open cursors
   * still remaining. Thus, any 'buddy transactions' connected to
   * such scan operations, will also be closed, *and removed* from
   * theTransactionList.
   */
  tNdbCon = theTransactionList;
  while (tNdbCon != NULL) {
    tNdbCon->releaseOperations();
    tNdbCon->releaseLockHandles();
    tNdbCon = tNdbCon->theNext;
  }//while

  /**
   * Disconnect and release all NdbTransactions in,
   * the now cleaned up, theTransactionList.
   */
  tNdbCon = theTransactionList;
  while (tNdbCon != NULL) {
    NdbTransaction* tmpNdbCon = tNdbCon;
    tNdbCon = tNdbCon->theNext;
    releaseConnectToNdb(tmpNdbCon);
  }//while

  /**
   * Transactions in theConnectionArray[] are idle, and thus in a 
   * known 'clean' state already. Disconnect and release right away.
   */
  Uint32 tNoOfDbNodes = theImpl->theNoOfDBnodes;
  Uint8 *theDBnodes= theImpl->theDBnodes;
  DBUG_PRINT("info", ("theNoOfDBnodes=%d", tNoOfDbNodes));
  UintR i;
  for (i = 0; i < tNoOfDbNodes; i++) {
    Uint32 tNode = theDBnodes[i];
    tNdbCon = theConnectionArray[tNode];
    while (tNdbCon != NULL) {
      NdbTransaction* tmpNdbCon = tNdbCon;
      tNdbCon = tNdbCon->theNext;
      releaseConnectToNdb(tmpNdbCon);
    }//while
  }//for
  DBUG_VOID_RETURN;
}//Ndb::doDisconnect()

/*****************************************************************************
int waitUntilReady(int timeout);

Return Value:   Returns 0 if the Ndb is ready within timeout seconds.
                Returns -1 otherwise.
Remark:         Waits until a node has status != 0
*****************************************************************************/ 
int
Ndb::waitUntilReady(int timeout)
{
  DBUG_ENTER("Ndb::waitUntilReady");
  int secondsCounter = 0;
  int milliCounter = 0;

  if (theInitState != Initialised) {
    // Ndb::init is not called
    theError.code = 4256;
    DBUG_RETURN(-1);
  }

  while (theNode == 0) {
    if (secondsCounter >= timeout)
    {
      theError.code = 4269;
      DBUG_RETURN(-1);
    }
    NdbSleep_MilliSleep(100);
    milliCounter += 100;
    if (milliCounter >= 1000) {
      secondsCounter++;
      milliCounter = 0;
    }//if
  }

  if (theImpl->m_ndb_cluster_connection.wait_until_ready
      (timeout-secondsCounter,30) < 0)
  {
    if (theImpl->m_transporter_facade->is_cluster_completely_unavailable())
    {
      theError.code = 4009;
    }
    else
    {
      theError.code = 4035;
    }
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/*****************************************************************************
NdbTransaction* computeHash()

Return Value:   Returns 0 for success, NDBAPI error code otherwise
Remark:         Computes the distribution hash value for a row with the
                supplied distribtion key values.
                Only relevant for natively partitioned tables.
*****************************************************************************/ 
int
Ndb::computeHash(Uint32 *retval,
                 const NdbDictionary::Table *table,
                 const struct Key_part_ptr * keyData, 
                 void* buf, Uint32 bufLen)
{
  Uint32 j = 0;
  Uint32 sumlen = 0; // Needed len
  const NdbTableImpl* impl = &NdbTableImpl::getImpl(*table);
  const NdbColumnImpl* const * cols = impl->m_columns.getBase();
  Uint32 len;
  unsigned char *pos, *bufEnd;
  void* malloced_buf = NULL;

  Uint32 colcnt = impl->m_columns.size();
  Uint32 parts = impl->m_noOfDistributionKeys;

  if (unlikely(impl->m_fragmentType == NdbDictionary::Object::UserDefined))
  {
    /* Calculating native hash on keys in user defined 
     * partitioned table is probably part of a bug
     */
    goto euserdeftable;
  }

  if (parts == 0)
  {
    parts = impl->m_noOfKeys;
  }

  for (Uint32 i = 0; i<parts; i++)
  {
    if (unlikely(keyData[i].ptr == 0))
      goto enullptr;
  }

  if (unlikely(keyData[parts].ptr != 0))
    goto emissingnullptr;

  const NdbColumnImpl* partcols[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
  for (Uint32 i = 0; i<colcnt && j < parts; i++)
  {
    if (cols[i]->m_distributionKey)
    {
      // wl3717_todo
      // char allowed now as dist key so this case should be tested
      partcols[j++] = cols[i];
    }
  }
  DBUG_ASSERT(j == parts);

  for (Uint32 i = 0; i<parts; i++)
  {
    Uint32 lb, len;
    if (unlikely(!NdbSqlUtil::get_var_length(partcols[i]->m_type, 
					     keyData[i].ptr, 
					     keyData[i].len,
					     lb, len)))
      goto emalformedkey;

    if (unlikely(keyData[i].len < (lb + len)))
      goto elentosmall;
    
    Uint32 maxlen = (partcols[i]->m_attrSize * partcols[i]->m_arraySize);

    if (unlikely(lb == 0 && keyData[i].len != maxlen))
      goto emalformedkey;
    
    if (partcols[i]->m_cs != NULL)
      len = NdbSqlUtil::strnxfrm_hash_len(partcols[i]->m_cs, (maxlen - lb));

    len = (lb + len + 3) & ~(Uint32)3;
    sumlen += len;
  }

  if (!buf)
  {
    bufLen = sumlen;
    bufLen += sizeof(Uint64); /* add space for potential alignment */
    buf = malloc(bufLen);
    if (unlikely(buf == 0))
      return 4000;
    malloced_buf = buf; /* Remember to free */
    assert(bufLen > sumlen);
  }

  {
    /* Get 64-bit aligned ptr required for hashing */
    assert(bufLen != 0);
    UintPtr org = UintPtr(buf);
    UintPtr use = (org + 7) & ~(UintPtr)7;

    buf = (void*)use;
    bufLen -= Uint32(use - org);

    if (unlikely(sumlen > bufLen))
      goto ebuftosmall;
  }

  pos= (unsigned char*) buf;
  bufEnd = pos + bufLen;

  for (Uint32 i = 0; i<parts; i++)
  {
    Uint32 lb, len;
    NdbSqlUtil::get_var_length(partcols[i]->m_type, 
			       keyData[i].ptr, keyData[i].len, lb, len);
    CHARSET_INFO* cs;
    if ((cs = partcols[i]->m_cs))
    {
      const Uint32 maxlen = (partcols[i]->m_attrSize * partcols[i]->m_arraySize) - lb;
      int n = NdbSqlUtil::strnxfrm_hash(cs,
                                   pos, bufEnd-pos, 
                                   ((uchar*)keyData[i].ptr)+lb, len, maxlen);

      if (unlikely(n == -1))
	goto emalformedstring;
      
      while ((n & 3) != 0) 
      {
	pos[n++] = 0;
      }
      pos += n;
    }
    else
    {
      len += lb;
      memcpy(pos, keyData[i].ptr, len);
      while (len & 3)
      {
	* (pos + len++) = 0;
      }
      pos += len;
    }
  }
  len = Uint32(UintPtr(pos) - UintPtr(buf));
  assert((len & 3) == 0);

  Uint32 values[4];
  md5_hash(values, (const Uint64*)buf, len >> 2);
  
  if (retval)
  {
    * retval = values[1];
  }
  
  if (malloced_buf)
    free(malloced_buf);
  
  return 0;
  
euserdeftable:
  return 4544;
  
enullptr:
  return 4316;
  
emissingnullptr:
  return 4276;

elentosmall:
  return 4277;

ebuftosmall:
  return 4278;

emalformedstring:
  if (malloced_buf)
    free(malloced_buf);
  
  return 4279;
  
emalformedkey:
  return 4280;
}

int
Ndb::computeHash(Uint32 *retval,
                 const NdbRecord *keyRec,
                 const char *keyData, 
                 void* buf, Uint32 bufLen)
{
  Uint32 len;
  unsigned char *pos, *bufEnd;
  void* malloced_buf = NULL;

  Uint32 parts = keyRec->distkey_index_length;

  if (unlikely(keyRec->flags & NdbRecord::RecHasUserDefinedPartitioning))
  {
    /* Calculating native hash on keys in user defined 
     * partitioned table is probably part of a bug
     */
    goto euserdeftable;
  }

  if (!buf)
  {
    /* We malloc buf here.  Don't have a handy 'Max distr key size'
     * variable, so let's use the key length, which must include
     * the Distr key.
     */
    bufLen = keyRec->m_keyLenInWords << 2;
    bufLen += sizeof(Uint64); /* add space for potential alignment */
    buf = malloc(bufLen);
    if (unlikely(buf == 0))
      return 4000;
    malloced_buf = buf; /* Remember to free */
  }

  {
    /* Get 64-bit aligned address as required for hashing */
    assert(bufLen != 0);
    UintPtr org = UintPtr(buf);
    UintPtr use = (org + 7) & ~(UintPtr)7;

    buf = (void*)use;
    bufLen -= Uint32(use - org);
  }

  pos= (unsigned char*) buf;
  bufEnd = pos + bufLen;

  for (Uint32 i = 0; i < parts; i++)
  {
    const struct NdbRecord::Attr &keyAttr =
      keyRec->columns[keyRec->distkey_indexes[i]];

    Uint32 len;
    Uint32 maxlen = keyAttr.maxSize;
    unsigned char *src= (unsigned char*)keyData + keyAttr.offset;

    if (keyAttr.flags & NdbRecord::IsVar1ByteLen)
    {
      if (keyAttr.flags & NdbRecord::IsMysqldShrinkVarchar)
      {
        len = uint2korr(src);
        src += 2;
      }
      else
      {
        len = *src;
        src += 1;
      }
      maxlen -= 1;
    }
    else if (keyAttr.flags & NdbRecord::IsVar2ByteLen)
    {
      len = uint2korr(src);
      src += 2;
      maxlen -= 2;
    }
    else
    {
      len = maxlen;
    }

    const CHARSET_INFO* cs = keyAttr.charset_info;
    if (cs)
    {      
      const int n = NdbSqlUtil::strnxfrm_hash(cs,
                                         pos, bufEnd-pos,
                                         src, len, maxlen);
      if (unlikely(n == -1))
        goto emalformedstring;
      len = n;
    }
    else
    {
      if (keyAttr.flags & NdbRecord::IsVar1ByteLen)
      {
        *pos= (unsigned char)len;
        memcpy(pos+1, src, len);
        len += 1;
      }
      else if (keyAttr.flags & NdbRecord::IsVar2ByteLen)
      {
        len += 2;
        memcpy(pos, src-2, len);
      }
      else
        memcpy(pos, src, len);
    }
    while (len & 3)
    {
      * (pos + len++) = 0;
    }
    pos += len;
  }
  len = Uint32(UintPtr(pos) - UintPtr(buf));
  assert((len & 3) == 0);

  Uint32 values[4];
  md5_hash(values, (const Uint64*)buf, len >> 2);
  
  if (retval)
  {
    * retval = values[1];
  }

  if (malloced_buf)
    free(malloced_buf);

  return 0;

euserdeftable:
  return 4544;

  
emalformedstring:
  if (malloced_buf)
    free(malloced_buf);

  return 4279;
}

NdbTransaction*
Ndb::startTransaction(const NdbRecord *keyRec, const char *keyData,
                      void* xfrmbuf, Uint32 xfrmbuflen)
{
  int ret;
  Uint32 hash;
  if ((ret = computeHash(&hash, keyRec, keyData, xfrmbuf, xfrmbuflen)) == 0)
  {
    return startTransaction(keyRec->table, keyRec->table->getPartitionId(hash));
  }
  theError.code = ret;
  return 0;
}

NdbTransaction* 
Ndb::startTransaction(const NdbDictionary::Table *table,
		      const struct Key_part_ptr * keyData, 
		      void* xfrmbuf, Uint32 xfrmbuflen)
{
  int ret;
  Uint32 hash;
  if ((ret = computeHash(&hash, table, keyData, xfrmbuf, xfrmbuflen)) == 0)
  {
    return startTransaction(table, table->getPartitionId(hash));
  }

  theError.code = ret;
  return 0;
}

Uint32
NdbImpl::select_node(NdbTableImpl *table_impl,
                     const Uint16 *nodes,
                     Uint32 cnt)
{
  if (table_impl == NULL)
  {
    return m_ndb_cluster_connection.select_any(this);
  }

  Uint32 nodeId;
  bool readBackup = table_impl->m_read_backup;
  bool fullyReplicated = table_impl->m_fully_replicated;

  if (cnt && !readBackup && !fullyReplicated)
  {
    /**
     * We select the primary replica node normally. If the user
     * have specified location domains we will always ensure that
     * we pick a node within the same location domain before we
     * pick the primary replica.
     *
     * The reason is that the transaction could be large and involve
     * many more operations not necessarily using the same partition
     * key. The jump to the primary is to a different location domain,
     * so we keeping the TC local to this domain always seems preferrable
     * to picking the perfect path for this operation.
     */
    if (m_optimized_node_selection)
    {
      nodeId = m_ndb_cluster_connection.select_location_based(this,
                                                              nodes,
                                                              cnt);
    }
    else
    {
      /* Backwards compatible setting */
      nodeId = nodes[0];
    }
  }
  else if (fullyReplicated)
  {
    /**
     * Consider any fragment and any replica.
     * Both for hinted and not hinted (cnt==0) select.
     */
    cnt = table_impl->m_fragments.size();
    nodes = table_impl->m_fragments.getBase();
    nodeId = m_ndb_cluster_connection.select_node(this, nodes, cnt);
  }
  else if (cnt == 0)
  {
    /**
     * For unhinted select, let caller select node.
     * Except for fully replicated tables, see above.
     */
    nodeId = m_ndb_cluster_connection.select_any(this);
  }
  else
  {
    /**
     * Read backup tables.
     * Consider one fragment and any replica for readBackup
     */
    require(readBackup);
    nodeId = m_ndb_cluster_connection.select_node(this, nodes, cnt);
  }
  return nodeId;
}

NdbTransaction*
Ndb::startTransaction(const NdbDictionary::Table* table,
                      Uint32 partitionId)
{
  DBUG_ENTER("Ndb::startTransaction");
  DBUG_PRINT("enter", 
             ("table: %s partitionId: %u", table->getName(), partitionId));
  if (theInitState == Initialised) 
  {
    theError.code = 0;
    checkFailedNode();

    Uint32 nodeId;
    const Uint16 *nodes;
    NdbTableImpl *impl =  & NdbTableImpl::getImpl(*table);
    Uint32 cnt = impl->get_nodes(partitionId,
                                 &nodes);
    nodeId = theImpl->select_node(impl, nodes, cnt);
    theImpl->incClientStat(TransStartCount, 1);

    NdbTransaction *trans= startTransactionLocal(0, nodeId, 0);
    DBUG_PRINT("exit",("start trans: 0x%lx  transid: 0x%lx",
                       (long) trans,
                       (long) (trans ? trans->getTransactionId() : 0)));
    DBUG_RETURN(trans);
  }
  DBUG_RETURN(NULL);
}

NdbTransaction*
Ndb::startTransaction(Uint32 nodeId,
                      Uint32 instanceId)
{
  DBUG_ENTER("Ndb::startTransaction");
  DBUG_PRINT("enter", 
             ("nodeId: %u instanceId: %u", nodeId, instanceId));
  if (theInitState == Initialised) 
  {
    theError.code = 0;
    checkFailedNode();

    theImpl->incClientStat(TransStartCount, 1);

    NdbTransaction *trans= startTransactionLocal(0, nodeId, instanceId);
    DBUG_PRINT("exit",("start trans: 0x%lx  transid: 0x%lx",
                       (long) trans,
                       (long) (trans ? trans->getTransactionId() : 0)));
    DBUG_RETURN(trans);
  }
  DBUG_RETURN(NULL);
}

NdbTransaction* 
Ndb::startTransaction(const NdbDictionary::Table *table,
		      const char * keyData, Uint32 keyLen)
{
  DBUG_ENTER("Ndb::startTransaction");

  if (theInitState == Initialised) {
    theError.code = 0;
    checkFailedNode();
    /**
     * If the user supplied key data
     * We will make a qualified quess to which node is the primary for the
     * the fragment and contact that node
     */
    Uint32 nodeId = 0;
    
    /**
     * Make this unlikely...assume new interface(s) are prefered
     */
    if(unlikely(table != 0 && keyData != 0))
    {
      NdbTableImpl* impl = &NdbTableImpl::getImpl(*table);
      Uint32 hashValue;
      {
	Uint32 buf[4];
        const Uint32 MaxKeySizeInLongWords= (NDB_MAX_KEY_SIZE + 7) / 8;
        Uint64 tmp[ MaxKeySizeInLongWords ];

        if (keyLen >= sizeof(tmp))
        {
          theError.code = 4207;
          DBUG_RETURN(NULL);
        }
	if((UintPtr(keyData) & 7) == 0 && (keyLen & 3) == 0)
	{
	  md5_hash(buf, (const Uint64*)keyData, keyLen >> 2);
	}
	else
	{
          tmp[keyLen/8] = 0;    // Zero out any 64-bit padding
	  memcpy(tmp, keyData, keyLen);
	  md5_hash(buf, tmp, (keyLen+3) >> 2);	  
	}
	hashValue= buf[1];
      }
      
      const Uint16 *nodes;
      Uint32 cnt= impl->get_nodes(table->getPartitionId(hashValue),  &nodes);
      nodeId = theImpl->select_node(impl, nodes, cnt);
    }
    else
    {
      /* No hint available, calling select_node with zero count */
      NdbTableImpl* impl = NULL;
      if (table != NULL)
      {
        impl = &NdbTableImpl::getImpl(*table);
      }
      nodeId = theImpl->select_node(impl, NULL, 0);
    }

    /* TODO : Should call method above rather than duplicate call to
     * startTransactionLocal
     */
    theImpl->incClientStat( TransStartCount, 1 );

    {
      NdbTransaction *trans= startTransactionLocal(0, nodeId, 0);
      DBUG_PRINT("exit",("start trans: 0x%lx  transid: 0x%lx",
			 (long) trans,
                         (long) (trans ? trans->getTransactionId() : 0)));
      DBUG_RETURN(trans);
    }
  } else {
    DBUG_RETURN(NULL);
  }//if
}//Ndb::startTransaction()

/*****************************************************************************
NdbTransaction* hupp(NdbTransaction* pBuddyTrans);

Return Value:   Returns a pointer to a connection object.
                Connected to the same node as pBuddyTrans
                and also using the same transction id
Remark:         Start transaction. Synchronous.
*****************************************************************************/ 
NdbTransaction* 
Ndb::hupp(NdbTransaction* pBuddyTrans)
{
  DBUG_ENTER("Ndb::hupp");

  DBUG_PRINT("enter", ("trans: 0x%lx", (long) pBuddyTrans));

  Uint32 aPriority = 0;
  if (pBuddyTrans == NULL){
    DBUG_RETURN(startTransaction());
  }

  if (theInitState == Initialised) {
    theError.code = 0;
    checkFailedNode();

    Uint32 nodeId = pBuddyTrans->getConnectedNodeId();
    NdbTransaction* pCon =
      startTransactionLocal(aPriority, nodeId,
                            refToInstance(pBuddyTrans->m_tcRef));
    if(pCon == NULL)
      DBUG_RETURN(NULL);

    if (pCon->getConnectedNodeId() != nodeId){
      // We could not get a connection to the desired node
      // release the connection and return NULL
      closeTransaction(pCon);
      theImpl->decClientStat( TransStartCount, 1 ); /* Correct stats */
      theError.code = 4006;
      DBUG_RETURN(NULL);
    }
    pCon->setTransactionId(pBuddyTrans->getTransactionId());
    pCon->setBuddyConPtr((Uint32)pBuddyTrans->getTC_ConnectPtr());
    DBUG_PRINT("exit", ("hupp trans: 0x%lx transid: 0x%lx",
                        (long) pCon,
                        (long) (pCon ? pCon->getTransactionId() : 0)));
    DBUG_RETURN(pCon);
  } else {
    DBUG_RETURN(NULL);
  }//if
}//Ndb::hupp()

NdbTransaction*
Ndb::startTransactionLocal(Uint32 aPriority, Uint32 nodeId, Uint32 instance)
{
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  char buf[255];
  const char* val = NdbEnv_GetEnv("NDB_TRANSACTION_NODE_ID", buf, 255);
  if(val != 0){
    nodeId = atoi(val);
  }
#endif
#endif

  DBUG_ENTER("Ndb::startTransactionLocal");
  DBUG_PRINT("enter", ("nodeid: %d", nodeId));

#ifdef VM_TRACE
  DBUG_EXECUTE_IF("ndb_start_transaction_fail",
                  {
                    /* Cluster failure */
                    theError.code = 4009;
                    DBUG_RETURN(0);
                  };);
#endif

  if(unlikely(theRemainingStartTransactions == 0))
  {
    theError.code = 4006;
    DBUG_RETURN(0);
  }
  
  NdbTransaction* tConnection;
  Uint64 tFirstTransId = theFirstTransId;
  tConnection = doConnect(nodeId, instance);
  if (tConnection == NULL) {
    DBUG_RETURN(NULL);
  }//if

  theRemainingStartTransactions--;
  NdbTransaction* tConNext = theTransactionList;
  if (tConnection->init())
  {
    theError.code = tConnection->theError.code;
    DBUG_RETURN(NULL);
  }
  theTransactionList = tConnection;        // into a transaction list.
  tConnection->next(tConNext);   // Add the active connection object
  tConnection->setTransactionId(tFirstTransId);
  tConnection->thePriority = aPriority;
  if ((tFirstTransId & 0xFFFFFFFF) == 0xFFFFFFFF) {
    //---------------------------------------------------
// Transaction id rolling round. We will start from
// consecutive identity 0 again.
//---------------------------------------------------
    theFirstTransId = ((tFirstTransId >> 32) << 32);      
  } else {
    theFirstTransId = tFirstTransId + 1;
  }//if
#ifdef VM_TRACE
  if (tConnection->theListState != NdbTransaction::NotInList) {
    printState("startTransactionLocal %lx", (long)tConnection);
    abort();
  }
#endif
  DBUG_RETURN(tConnection);
}//Ndb::startTransactionLocal()

void
Ndb::appendConnectionArray(NdbTransaction *aCon, Uint32 nodeId)
{
  NdbTransaction *last = theConnectionArrayLast[nodeId];
  if (last)
  {
    last->theNext = aCon;
  }
  else
  {
    theConnectionArray[nodeId] = aCon;
  }
  aCon->theNext = NULL;
  theConnectionArrayLast[nodeId] = aCon;
}

void
Ndb::prependConnectionArray(NdbTransaction *aCon, Uint32 nodeId)
{
  NdbTransaction *first = theConnectionArray[nodeId];
  aCon->theNext = first;
  if (!first)
  {
    theConnectionArrayLast[nodeId] = aCon;
  }
  theConnectionArray[nodeId] = aCon;
}

void
Ndb::removeConnectionArray(NdbTransaction *first, Uint32 nodeId)
{
  NdbTransaction *next = first->theNext;
  if (!next)
  {
    theConnectionArray[nodeId] = theConnectionArrayLast[nodeId] = NULL;
  }
  else
  {
    theConnectionArray[nodeId] = next;
  }
}

/*****************************************************************************
void closeTransaction(NdbTransaction* aConnection);

Parameters:     aConnection: the connection used in the transaction.
Remark:         Close transaction by releasing the connection and all operations.
*****************************************************************************/
void
Ndb::closeTransaction(NdbTransaction* aConnection)
{
  DBUG_ENTER("Ndb::closeTransaction");
  NdbTransaction* tCon;
  NdbTransaction* tPreviousCon;

  if (aConnection == NULL) {
//-----------------------------------------------------
// closeTransaction called on NULL pointer, destructive
// application behaviour.
//-----------------------------------------------------
#ifdef VM_TRACE
    printf("NULL into closeTransaction\n");
#endif
    DBUG_VOID_RETURN;
  }//if
  CHECK_STATUS_MACRO_VOID;
  
  tCon = theTransactionList;
  theRemainingStartTransactions++;
  
  DBUG_EXECUTE_IF("ndb_delay_close_txn", {
    fprintf(stderr, "Ndb::closeTransaction() (%p) taking a break\n", this);
    NdbSleep_MilliSleep(1000);
    fprintf(stderr, "Ndb::closeTransaction() resuming\n");
  });
  DBUG_PRINT("info",("close trans: 0x%lx  transid: 0x%lx",
                     (long) aConnection,
                     (long) aConnection->getTransactionId()));
  DBUG_PRINT("info",("magic number: 0x%x TCConPtr: 0x%x theMyRef: 0x%x 0x%x",
		     aConnection->theMagicNumber, aConnection->theTCConPtr,
		     aConnection->theMyRef, getReference()));

  if (aConnection == tCon) {		// Remove the active connection object
    theTransactionList = tCon->next();	// from the transaction list.
  } else { 
    while (aConnection != tCon) {
      if (tCon == NULL) {
//-----------------------------------------------------
// closeTransaction called on non-existing transaction
//-----------------------------------------------------

	if(aConnection->theError.code == 4008){
	  /**
	   * When a SCAN timed-out, returning the NdbTransaction leads
	   * to reuse. And TC crashes when the API tries to reuse it to
	   * something else...
	   */
#ifdef VM_TRACE
	  printf("Scan timeout:ed NdbTransaction-> "
		 "not returning it-> memory leak\n");
#endif
	  DBUG_VOID_RETURN;
	}

#ifdef VM_TRACE
	printf("Non-existing transaction into closeTransaction\n");
	abort();
#endif
	DBUG_VOID_RETURN;
      }//if
      tPreviousCon = tCon;
      tCon = tCon->next();
    }//while
    tPreviousCon->next(tCon->next());
  }//if
  
  aConnection->release();
  
  theImpl->incClientStat(TransCloseCount, 1);

  if(aConnection->theError.code == 4008){
    /**
     * Something timed-out, returning the NdbTransaction leads
     * to reuse. And TC crashes when the API tries to reuse it to
     * something else...
     */
#ifdef VM_TRACE
    printf("Con timeout:ed NdbTransaction-> not returning it-> memory leak\n");
#endif
    DBUG_VOID_RETURN;
  }
  
  /**
   * NOTE: It's ok to call getNodeSequence() here wo/ having mutex,
   */
  Uint32 nodeId = aConnection->getConnectedNodeId();
  Uint32 seq = theImpl->getNodeSequence(nodeId);
  if (aConnection->theNodeSequence != seq)
  {
    aConnection->theReleaseOnClose = true;
  }
  
  if (aConnection->theReleaseOnClose == false) 
  {
    /**
     * Put it back in idle list for that node
     */
    appendConnectionArray(aConnection, nodeId);

    DBUG_VOID_RETURN;
  } else {
    aConnection->theReleaseOnClose = false;
    releaseNdbCon(aConnection);
  }//if
  DBUG_VOID_RETURN;
}//Ndb::closeTransaction()

/****************************************************************************
int getBlockNumber(void);

Remark:		
****************************************************************************/
int
Ndb::getBlockNumber()
{
  return theNdbBlockNumber;
}

NdbDictionary::Dictionary *
Ndb::getDictionary() const {
  return theDictionary;
}

/****************************************************************************
int getNodeId();

Remark:		
****************************************************************************/
int
Ndb::getNodeId()
{
  return theNode;
}

/****************************************************************************
Uint64 getAutoIncrementValue( const char* aTableName,
                              Uint64 & autoValue, 
                              Uint32 cacheSize, 
                              Uint64 step,
                              Uint64 start);

Parameters:     aTableName (IN) : The table name.
                autoValue (OUT) : Returns new autoincrement value
                cacheSize  (IN) : Prefetch this many values
                step       (IN) : Specifies the step between the 
                                  autoincrement values.
                start      (IN) : Start value for first value
Returns:        0 if succesful, -1 if error encountered
Remark:		Returns a new autoincrement value to the application.
                The autoincrement values can be increased by steps
                (default 1) and a number of values can be prefetched
                by specifying cacheSize (default 10).
****************************************************************************/
int
Ndb::getAutoIncrementValue(const char* aTableName,
                           Uint64 & autoValue, Uint32 cacheSize,
                           Uint64 step, Uint64 start)
{
  DBUG_ENTER("Ndb::getAutoIncrementValue");
  ASSERT_NOT_MYSQLD;
  BaseString internal_tabname(internalize_table_name(aTableName));

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname);
  if (info == 0) {
    theError.code = theDictionary->getNdbError().code;
    DBUG_RETURN(-1);
  }
  const NdbTableImpl* table = info->m_table_impl;
  TupleIdRange & range = info->m_tuple_id_range;
  if (getTupleIdFromNdb(table, range, autoValue, cacheSize, step, start) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("value %lu", (ulong) autoValue));
  DBUG_RETURN(0);
}

int
Ndb::getAutoIncrementValue(const NdbDictionary::Table * aTable,
                           Uint64 & autoValue, Uint32 cacheSize,
                           Uint64 step, Uint64 start)
{
  DBUG_ENTER("Ndb::getAutoIncrementValue");
  ASSERT_NOT_MYSQLD;
  assert(aTable != 0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  const BaseString& internal_tabname = table->m_internalName;

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname);
  if (info == 0) {
    theError.code = theDictionary->getNdbError().code;
    DBUG_RETURN(-1);
  }
  TupleIdRange & range = info->m_tuple_id_range;
  if (getTupleIdFromNdb(table, range, autoValue, cacheSize, step, start) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("value %lu", (ulong)autoValue));
  DBUG_RETURN(0);
}

int
Ndb::getAutoIncrementValue(const NdbDictionary::Table * aTable,
                           TupleIdRange & range, Uint64 & autoValue,
                           Uint32 cacheSize, Uint64 step, Uint64 start)
{
  DBUG_ENTER("Ndb::getAutoIncrementValue");
  assert(aTable != 0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);

  if (getTupleIdFromNdb(table, range, autoValue, cacheSize, step, start) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("value %lu", (ulong)autoValue));
  DBUG_RETURN(0);
}

int
Ndb::getTupleIdFromNdb(const NdbTableImpl* table,
                       TupleIdRange & range, Uint64 & tupleId, 
                       Uint32 cacheSize, Uint64 step, Uint64 start)
{
/*
  Returns a new TupleId to the application.
  The TupleId comes from SYSTAB_0 where SYSKEY_0 = TableId.
  It is initialized to (TableId << 48) + 1 in NdbcntrMain.cpp.
  In most cases step= start= 1, in which case we get:
  1,2,3,4,5,...
  If step=10 and start=5 and first number is 1, we get:
  5,15,25,35,...  
*/
  DBUG_ENTER("Ndb::getTupleIdFromNdb");
  DBUG_PRINT("info", ("range.first_id=%llu, last_id=%llu, highest_seen=%llu "
                      "tupleId = %llu, cacheSize=%u step=%llu start=%llu",
                      range.m_first_tuple_id,
                      range.m_last_tuple_id,
                      range.m_highest_seen,
                      tupleId,
                      cacheSize,
                      step,
                      start));

  /*
    If start value is greater than step it is ignored
  */
  Uint64 offset = (start > step) ? 1 : start;

  if (range.m_first_tuple_id != range.m_last_tuple_id)
  {
    /**
     * Range is valid and has span
     * Determine next value *after* m_first_tuple_id
     * meeting start and step constraints, then see
     * if it is inside the cached range.
     * m_first_tuple_id start may not meet the constraints 
     * (if there was a manual insert)
     * c.f. handler.cc compute_next_insert_id()
     */
    assert(step > 0);
    assert(range.m_first_tuple_id >= offset);
    Uint64 desiredNextVal = 0;
    Uint64 numStepsTaken = ((range.m_first_tuple_id - offset) /
                            step);
    desiredNextVal = ((numStepsTaken + 1) * step) + offset;
    DBUG_PRINT("info", ("desiredNextVal = %llu", desiredNextVal));

    if (desiredNextVal <= range.m_last_tuple_id)
    {
      DBUG_PRINT("info", ("Next value from cache %lu", (ulong) tupleId));
      assert(range.m_first_tuple_id < range.m_last_tuple_id);
      range.m_first_tuple_id = tupleId = desiredNextVal; 
      DBUG_RETURN(0);
    }
  }
  
  /*
    Pre-fetch a number of values depending on cacheSize
  */
  if (cacheSize == 0)
    cacheSize = 1;
  
  DBUG_PRINT("info", ("reading %u values from database", (uint)cacheSize));
  /*
   * reserve next cacheSize entries in db.  adds cacheSize to NEXTID
   * and returns first tupleId in the new range. If tupleId's are
   * incremented in steps then multiply the cacheSize with step size.
   */
  Uint64 opValue = cacheSize * step;
  
  if (opTupleIdOnNdb(table, range, opValue, 0) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("Next value fetched from database %lu", (ulong) opValue));
  DBUG_PRINT("info", ("Increasing %lu by offset %lu, increment  is %lu", 
                      (ulong) (ulong) opValue, (ulong) offset, (ulong) step));
  Uint64 current, next;
  Uint64 div = ((Uint64) (opValue + step - offset)) / step;
  next = div * step + offset;
  current = (next < step) ? next : next - step;
  tupleId = (opValue <= current) ? current : next;
  DBUG_PRINT("info", ("Returning %lu", (ulong) tupleId));
  range.m_first_tuple_id = tupleId;

  DBUG_RETURN(0);
}

/****************************************************************************
int readAutoIncrementValue( const char* aTableName,
                            Uint64 & autoValue);

Parameters:     aTableName (IN) : The table name.
                autoValue  (OUT) : The current autoincrement value
Returns:        0 if succesful, -1 if error encountered
Remark:         Returns the current autoincrement value to the application.
****************************************************************************/
int
Ndb::readAutoIncrementValue(const char* aTableName,
                            Uint64 & autoValue)
{
  DBUG_ENTER("Ndb::readAutoIncrementValue");
  ASSERT_NOT_MYSQLD;
  BaseString internal_tabname(internalize_table_name(aTableName));

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname);
  if (info == 0) {
    theError.code = theDictionary->getNdbError().code;
    DBUG_RETURN(-1);
  }
  const NdbTableImpl* table = info->m_table_impl;
  TupleIdRange & range = info->m_tuple_id_range;
  if (readTupleIdFromNdb(table, range, autoValue) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("value %lu", (ulong)autoValue));
  DBUG_RETURN(0);
}

int
Ndb::readAutoIncrementValue(const NdbDictionary::Table * aTable,
                            Uint64 & autoValue)
{
  DBUG_ENTER("Ndb::readAutoIncrementValue");
  ASSERT_NOT_MYSQLD;
  assert(aTable != 0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  const BaseString& internal_tabname = table->m_internalName;

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname);
  if (info == 0) {
    theError.code = theDictionary->getNdbError().code;
    DBUG_RETURN(-1);
  }
  TupleIdRange & range = info->m_tuple_id_range;
  if (readTupleIdFromNdb(table, range, autoValue) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("value %lu", (ulong)autoValue));
  DBUG_RETURN(0);
}

int
Ndb::readAutoIncrementValue(const NdbDictionary::Table * aTable,
                            TupleIdRange & range, Uint64 & autoValue)
{
  DBUG_ENTER("Ndb::readAutoIncrementValue");
  assert(aTable != 0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);

  if (readTupleIdFromNdb(table, range, autoValue) == -1)
    DBUG_RETURN(-1);
  DBUG_PRINT("info", ("value %lu", (ulong)autoValue));
  DBUG_RETURN(0);
}

int
Ndb::readTupleIdFromNdb(const NdbTableImpl* table,
                        TupleIdRange & range, Uint64 & tupleId)
{
  DBUG_ENTER("Ndb::readTupleIdFromNdb");
  if (range.m_first_tuple_id != range.m_last_tuple_id)
  {
    assert(range.m_first_tuple_id < range.m_last_tuple_id);
    tupleId = range.m_first_tuple_id + 1;
  }
  else
  {
    /*
     * peek at NEXTID.  does not reserve it so the value is valid
     * only if no other transactions are allowed.
     */
    Uint64 opValue = 0;
    if (opTupleIdOnNdb(table, range, opValue, 3) == -1)
      DBUG_RETURN(-1);
    tupleId = opValue;
  }
  DBUG_RETURN(0);
}

/****************************************************************************
int setAutoIncrementValue( const char* aTableName,
                           Uint64 autoValue,
                           bool modify);

Parameters:     aTableName (IN) : The table name.
                autoValue  (IN) : The new autoincrement value
                modify     (IN) : Modify existing value (not initialization)
Returns:        0 if succesful, -1 if error encountered
Remark:         Sets a new autoincrement value for the application.
****************************************************************************/
int
Ndb::setAutoIncrementValue(const char* aTableName,
                           Uint64 autoValue, bool modify)
{
  DBUG_ENTER("Ndb::setAutoIncrementValue");
  ASSERT_NOT_MYSQLD;
  BaseString internal_tabname(internalize_table_name(aTableName));

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname);
  if (info == 0) {
    theError.code = theDictionary->getNdbError().code;
    DBUG_RETURN(-1);
  }
  const NdbTableImpl* table = info->m_table_impl;
  TupleIdRange & range = info->m_tuple_id_range;
  if (setTupleIdInNdb(table, range, autoValue, modify) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
Ndb::setAutoIncrementValue(const NdbDictionary::Table * aTable,
                           Uint64 autoValue, bool modify)
{
  DBUG_ENTER("Ndb::setAutoIncrementValue");
  ASSERT_NOT_MYSQLD;
  assert(aTable != 0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  const BaseString& internal_tabname = table->m_internalName;

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname);
  if (info == 0) {
    theError.code = theDictionary->getNdbError().code;
    DBUG_RETURN(-1);
  }
  TupleIdRange & range = info->m_tuple_id_range;
  if (setTupleIdInNdb(table, range, autoValue, modify) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
Ndb::setAutoIncrementValue(const NdbDictionary::Table * aTable,
                           TupleIdRange & range, Uint64 autoValue,
                           bool modify)
{
  DBUG_ENTER("Ndb::setAutoIncrementValue");
  assert(aTable != 0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);

  if (setTupleIdInNdb(table, range, autoValue, modify) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
Ndb::setTupleIdInNdb(const NdbTableImpl* table,
                     TupleIdRange & range, Uint64 tupleId, bool modify)
{
  DBUG_ENTER("Ndb::setTupleIdInNdb");
  DBUG_PRINT("info", ("range first : %llu, last : %llu, tupleId : %llu "
                      "modify %u",
                      range.m_first_tuple_id,
                      range.m_last_tuple_id,
                      tupleId,
                      modify));
  if (modify)
  {
    if (checkTupleIdInNdb(range, tupleId))
    {
      if (range.m_first_tuple_id != range.m_last_tuple_id)
      {
        assert(range.m_first_tuple_id < range.m_last_tuple_id);
        if (tupleId <= range.m_first_tuple_id + 1)
          DBUG_RETURN(0);
        if (tupleId <= range.m_last_tuple_id)
        {
          range.m_first_tuple_id = tupleId - 1;
          DBUG_PRINT("info", 
                     ("Setting next auto increment cached value to %lu",
                      (ulong)tupleId));  
          DBUG_PRINT("info", 
                     ("Range.m_first = %llu, m_last=%llu, m_highest_seen=%llu",
                      range.m_first_tuple_id,
                      range.m_last_tuple_id,
                      range.m_highest_seen));
          DBUG_RETURN(0);
        }
      }
      /*
       * if tupleId <= NEXTID, do nothing.  otherwise update NEXTID to
       * tupleId and set cached range to first = last = tupleId - 1.
       */
      if (opTupleIdOnNdb(table, range, tupleId, 2) == -1)
        DBUG_RETURN(-1);
    }
  }
  else
  {
    /*
     * update NEXTID to given value.  reset cached range.
     */
    if (opTupleIdOnNdb(table, range, tupleId, 1) == -1)
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

int Ndb::initAutoIncrement()
{
  if (m_sys_tab_0)
    return 0;

  BaseString currentDb(getDatabaseName());
  BaseString currentSchema(getDatabaseSchemaName());

  setDatabaseName("sys");
  setDatabaseSchemaName("def");

  m_sys_tab_0 = theDictionary->getTableGlobal("SYSTAB_0");

  // Restore current name space
  setDatabaseName(currentDb.c_str());
  setDatabaseSchemaName(currentSchema.c_str());

  if (m_sys_tab_0 == NULL) {
    assert(theDictionary->m_error.code != 0);
    theError.code = theDictionary->m_error.code;
    return -1;
  }

  return 0;
}

bool
Ndb::checkUpdateAutoIncrementValue(TupleIdRange & range, Uint64 autoValue)
{
  return(checkTupleIdInNdb(range, autoValue) != 0);
}

int
Ndb::checkTupleIdInNdb(TupleIdRange & range, Uint64 tupleId)
{
  DBUG_ENTER("Ndb::checkTupleIdIndNdb");
  if ((range.m_first_tuple_id != ~(Uint64)0) &&
      (range.m_first_tuple_id > tupleId))
  {
   /*
    * If we have ever cached a value in this object and this cached
    * value is larger than the value we're trying to set then we
    * need not check with the real value in the SYSTAB_0 table.
    */
    DBUG_RETURN(0);
  }
  if (range.m_highest_seen > tupleId)
  {
    /*
     * Although we've never cached any higher value we have read
     * a higher value and again it isn't necessary to change the
     * auto increment value.
     */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


int
Ndb::opTupleIdOnNdb(const NdbTableImpl* table,
                    TupleIdRange & range, Uint64 & opValue, Uint32 op)
{
  DBUG_ENTER("Ndb::opTupleIdOnNdb");
  Uint32 aTableId = table->m_id;
  DBUG_PRINT("enter", ("table: %u  value: %lu  op: %u",
                       aTableId, (ulong) opValue, op));

  NdbTransaction*    tConnection = NULL;
  NdbOperation*      tOperation = NULL;
  Uint64             tValue;
  NdbRecAttr*        tRecAttrResult;

  CHECK_STATUS_MACRO;

  if (initAutoIncrement() == -1)
    goto error_handler;

  // Start transaction with table id as hint
  tConnection = this->startTransaction(m_sys_tab_0,
                                       (const char *) &aTableId,
                                       sizeof(Uint32));

  if (tConnection == NULL)
    goto error_handler;

  tOperation = tConnection->getNdbOperation(m_sys_tab_0);
  if (tOperation == NULL)
    goto error_handler;

  switch (op)
    {
    case 0:
      tOperation->interpretedUpdateTuple();
      tOperation->equal("SYSKEY_0", aTableId);
      tOperation->incValue("NEXTID", opValue);
      tRecAttrResult = tOperation->getValue("NEXTID");

      if (tConnection->execute( NdbTransaction::Commit ) == -1 )
        goto error_handler;

      tValue = tRecAttrResult->u_64_value();

      range.m_first_tuple_id = tValue - opValue;
      range.m_last_tuple_id  = tValue - 1;
      opValue = range.m_first_tuple_id; // out
      break;
    case 1:
      // create on first use
      tOperation->writeTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tOperation->setValue("NEXTID", opValue);

      if (tConnection->execute( NdbTransaction::Commit ) == -1 )
        goto error_handler;

      range.reset();
      break;
    case 2:
      tOperation->interpretedUpdateTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tOperation->load_const_u64(1, opValue);
      tOperation->read_attr("NEXTID", 2);
      // compare NEXTID >= opValue
      tOperation->branch_le(2, 1, 0);
      tOperation->write_attr("NEXTID", 1);
      tOperation->interpret_exit_ok();
      tOperation->def_label(0);
      tOperation->interpret_exit_ok();
      tRecAttrResult = tOperation->getValue("NEXTID");
      if (tConnection->execute( NdbTransaction::Commit ) == -1)
      {
        goto error_handler;
      }
      else
      {
        range.m_highest_seen = tRecAttrResult->u_64_value();
        DBUG_PRINT("info", 
                   ("Setting next auto increment value (db) to %lu",
                    (ulong) opValue));  
        range.m_first_tuple_id = range.m_last_tuple_id = opValue - 1;
      }
      break;
    case 3:
      tOperation->readTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tRecAttrResult = tOperation->getValue("NEXTID");
      if (tConnection->execute( NdbTransaction::Commit ) == -1 )
        goto error_handler;
      range.m_highest_seen = opValue = tRecAttrResult->u_64_value(); // out
      break;
    default:
      goto error_handler;
    }

  this->closeTransaction(tConnection);

  DBUG_RETURN(0);

error_handler:
  DBUG_PRINT("error", ("ndb=%d con=%d op=%d",
             theError.code,
             tConnection != NULL ? tConnection->theError.code : -1,
             tOperation != NULL ? tOperation->theError.code : -1));

  if (theError.code == 0 && tConnection != NULL)
    theError.code = tConnection->theError.code;
  if (theError.code == 0 && tOperation != NULL)
    theError.code = tOperation->theError.code;
  DBUG_ASSERT(theError.code != 0);

  NdbError savedError;
  savedError = theError;

  if (tConnection != NULL)
    this->closeTransaction(tConnection);

  theError = savedError;

  DBUG_RETURN(-1);
}

Uint32
convertEndian(Uint32 Data)
{
#ifdef WORDS_BIGENDIAN
  Uint32 t1, t2, t3, t4;
  t4 = (Data >> 24) & 255;
  t3 = (Data >> 16) & 255;
  t4 = t4 + (t3 << 8);
  t2 = (Data >> 8) & 255;
  t4 = t4 + (t2 << 16);
  t1 = Data & 255;
  t4 = t4 + (t1 << 24);
  return t4;
#else
  return Data;
#endif
}

// <internal>
Ndb_cluster_connection &
Ndb::get_ndb_cluster_connection()
{
  return theImpl->m_ndb_cluster_connection;
}

const char * Ndb::getCatalogName() const
{
  return theImpl->m_dbname.c_str();
}

int Ndb::setCatalogName(const char * a_catalog_name)
{
  // TODO can table_name_separator be escaped?
  if (a_catalog_name && ! strchr(a_catalog_name, table_name_separator)) {
    if (!theImpl->m_dbname.assign(a_catalog_name) ||
        theImpl->update_prefix())
    {
      theError.code = 4000;
      return -1;
    }
  }
  return 0;
}

const char * Ndb::getSchemaName() const
{
  return theImpl->m_schemaname.c_str();
}

int Ndb::setSchemaName(const char * a_schema_name)
{
  // TODO can table_name_separator be escaped?
  if (a_schema_name && ! strchr(a_schema_name, table_name_separator)) {
    if (!theImpl->m_schemaname.assign(a_schema_name) ||
        theImpl->update_prefix())
    {
      theError.code = 4000;
      return -1;
    }
  }
  return 0;
}
// </internal>
 
const char* Ndb::getNdbObjectName() const
{
  return theImpl->m_ndbObjectName.c_str();
}

int Ndb::setNdbObjectName(const char *name)
{
  if (!theImpl->m_ndbObjectName.empty())
  {
    theError.code = 4121;
    return -1; // Cannot set twice
  }

  if (theInitState != NotInitialised)
  {
    theError.code = 4122;
    return -1; // Should be set before init() is called
  }

  theImpl->m_ndbObjectName.assign(name);
  return 0;
}

const char * Ndb::getDatabaseName() const
{
  return getCatalogName();
}
 
int Ndb::setDatabaseName(const char * a_catalog_name)
{
  return setCatalogName(a_catalog_name);
}
 
const char * Ndb::getDatabaseSchemaName() const
{
  return getSchemaName();
}
 
int Ndb::setDatabaseSchemaName(const char * a_schema_name)
{
  return setSchemaName(a_schema_name);
}

int Ndb::setDatabaseAndSchemaName(const NdbDictionary::Table* t)
{
  const char* s0 = t->m_impl.m_internalName.c_str();
  const char* s1 = strchr(s0, table_name_separator);
  if (s1 && s1 != s0) {
    const char* s2 = strchr(s1 + 1, table_name_separator);
    if (s2 && s2 != s1 + 1) {
      char buf[NAME_LEN + 1];
      if (s1 - s0 <= NAME_LEN && s2 - (s1 + 1) <= NAME_LEN) {
        sprintf(buf, "%.*s", (int) (s1 - s0), s0);
        setDatabaseName(buf);
        sprintf(buf, "%.*s", (int) (s2 - (s1 + 1)), s1 + 1);
        setDatabaseSchemaName(buf);
#ifdef VM_TRACE
        // verify that m_prefix looks like abc/def/
        const char* s0 = theImpl->m_prefix.c_str();
        const char* s1 = s0 ? strchr(s0, table_name_separator) : 0;
        const char* s2 = s1 ? strchr(s1 + 1, table_name_separator) : 0;
        if (!(s1 && s1 != s0 && s2 && s2 != s1 + 1 && *(s2 + 1) == 0))
        {
          ndbout_c("t->m_impl.m_internalName.c_str(): %s", t->m_impl.m_internalName.c_str());
          ndbout_c("s0: %s", s0);
          ndbout_c("s1: %s", s1);
          ndbout_c("s2: %s", s2);
          assert(s1 && s1 != s0 && s2 && s2 != s1 + 1 && *(s2 + 1) == 0);
        }
#endif
        return 0;
      }
    }
  }
  return -1;
}
 
bool Ndb::usingFullyQualifiedNames()
{
  return fullyQualifiedNames;
}
 
const char *
Ndb::externalizeTableName(const char * internalTableName, bool fullyQualifiedNames)
{
  if (fullyQualifiedNames) {
    const char *ptr = internalTableName;
   
    // Skip database name
    while (*ptr && *ptr++ != table_name_separator)
    {
      ;
    }
    // Skip schema name
    while (*ptr && *ptr++ != table_name_separator)
    {
      ;
    }
    return ptr;
  }
  else
    return internalTableName;
}

const char *
Ndb::externalizeTableName(const char * internalTableName)
{
  return externalizeTableName(internalTableName, usingFullyQualifiedNames());
}

const char *
Ndb::externalizeIndexName(const char * internalIndexName, bool fullyQualifiedNames)
{
  if (fullyQualifiedNames) {
    const char *ptr = internalIndexName;
   
    // Scan name from the end
    while (*ptr++)
    {
      ;
    }
    ptr--; // strend

    while (ptr >= internalIndexName && *ptr != table_name_separator)
    {
      ptr--;
    }
     
    return ptr + 1;
  }
  else
  {
    return internalIndexName;
  }
}

const char *
Ndb::externalizeIndexName(const char * internalIndexName)
{
  return externalizeIndexName(internalIndexName, usingFullyQualifiedNames());
}


const BaseString
Ndb::internalize_table_name(const char *external_name) const
{
  BaseString ret;
  DBUG_ENTER("internalize_table_name");
  DBUG_PRINT("enter", ("external_name: %s", external_name));

  if (fullyQualifiedNames)
  {
    /* Internal table name format <db>/<schema>/<table>
       <db>/<schema>/ is already available in m_prefix
       so just concat the two strings
     */
#ifdef VM_TRACE
    // verify that m_prefix looks like abc/def/
    const char* s0 = theImpl->m_prefix.c_str();
    const char* s1 = s0 ? strchr(s0, table_name_separator) : 0;
    const char* s2 = s1 ? strchr(s1 + 1, table_name_separator) : 0;
    if (!(s1 && s1 != s0 && s2 && s2 != s1 + 1 && *(s2 + 1) == 0))
    {
      ndbout_c("s0: %s", s0);
      ndbout_c("s1: %s", s1);
      ndbout_c("s2: %s", s2);
      assert(s1 && s1 != s0 && s2 && s2 != s1 + 1 && *(s2 + 1) == 0);
    }
#endif
    ret.assfmt("%s%s",
               theImpl->m_prefix.c_str(),
               external_name);
  }
  else
    ret.assign(external_name);

  DBUG_PRINT("exit", ("internal_name: %s", ret.c_str()));
  DBUG_RETURN(ret);
}

const BaseString
Ndb::old_internalize_index_name(const NdbTableImpl * table,
				const char * external_name) const
{
  BaseString ret;
  DBUG_ENTER("old_internalize_index_name");
  DBUG_PRINT("enter", ("external_name: %s, table_id: %d",
                       external_name, table ? table->m_id : ~0));
  if (!table)
  {
    DBUG_PRINT("error", ("!table"));
    DBUG_RETURN(ret);
  }

  if (fullyQualifiedNames)
  {
    /* Internal index name format <db>/<schema>/<tabid>/<table> */
    ret.assfmt("%s%d%c%s",
               theImpl->m_prefix.c_str(),
               table->m_id,
               table_name_separator,
               external_name);
  }
  else
    ret.assign(external_name);

  DBUG_PRINT("exit", ("internal_name: %s", ret.c_str()));
  DBUG_RETURN(ret);
}

const BaseString
Ndb::internalize_index_name(const NdbTableImpl * table,
                           const char * external_name) const
{
  BaseString ret;
  DBUG_ENTER("internalize_index_name");
  DBUG_PRINT("enter", ("external_name: %s, table_id: %d",
                       external_name, table ? table->m_id : ~0));
  if (!table)
  {
    DBUG_PRINT("error", ("!table"));
    DBUG_RETURN(ret);
  }

  if (fullyQualifiedNames)
  {
    /* Internal index name format sys/def/<tabid>/<table> */
    ret.assfmt("%s%d%c%s",
               theImpl->m_systemPrefix.c_str(),
               table->m_id,
               table_name_separator,
               external_name);
  }
  else
    ret.assign(external_name);

  DBUG_PRINT("exit", ("internal_name: %s", ret.c_str()));
  DBUG_RETURN(ret);
}


const BaseString
Ndb::getDatabaseFromInternalName(const char * internalName)
{
  char * databaseName = new char[strlen(internalName) + 1];
  if (databaseName == NULL)
  {
    errno = ENOMEM;
    return BaseString(NULL);
  }
  strcpy(databaseName, internalName);
  char *ptr = databaseName;
   
  /* Scan name for the first table_name_separator */
  while (*ptr && *ptr != table_name_separator)
    ptr++;
  *ptr = '\0';
  BaseString ret = BaseString(databaseName);
  delete [] databaseName;
  return ret;
}
 
const BaseString
Ndb::getSchemaFromInternalName(const char * internalName)
{
  char * schemaName = new char[strlen(internalName)];
  if (schemaName == NULL)
  {
    errno = ENOMEM;
    return BaseString(NULL);
  }
  const char *ptr1 = internalName;
   
  /* Scan name for the second table_name_separator */
  while (*ptr1 && *ptr1 != table_name_separator)
    ptr1++;
  strcpy(schemaName, ptr1 + 1);
  char *ptr = schemaName;
  while (*ptr && *ptr != table_name_separator)
    ptr++;
  *ptr = '\0';
  BaseString ret = BaseString(schemaName);
  delete [] schemaName;
  return ret;
}

unsigned Ndb::get_eventbuf_max_alloc()
{
    return theEventBuffer->m_max_alloc;
}

void
Ndb::set_eventbuf_max_alloc(unsigned sz)
{
  if (theEventBuffer != NULL)
  {
    theEventBuffer->m_max_alloc = sz;
  }
}

unsigned Ndb::get_eventbuffer_free_percent()
{
  return theEventBuffer->get_eventbuffer_free_percent();
}

int
Ndb::set_eventbuffer_free_percent(unsigned free)
{
  if (free < 1 || free > 99)
  {
    theError.code = 4123;
    return -1;
  }
  theEventBuffer->set_eventbuffer_free_percent(free);
  return 0;
}

void Ndb::get_event_buffer_memory_usage(EventBufferMemoryUsage& usage)
{
  theEventBuffer->get_event_buffer_memory_usage(usage);
}

NdbEventOperation* Ndb::createEventOperation(const char* eventName)
{
  DBUG_ENTER("Ndb::createEventOperation");
  NdbEventOperation* tOp= theEventBuffer->createEventOperation(eventName,
							       theError);
  if (tOp)
  {
    // keep track of all event operations
    // Serialize changes to m_ev_op with dropEventOperation
    theImpl->lock();
    NdbEventOperationImpl *op=
      NdbEventBuffer::getEventOperationImpl(tOp);
    op->m_next= theImpl->m_ev_op;
    op->m_prev= 0;
    theImpl->m_ev_op= op;
    if (op->m_next)
      op->m_next->m_prev= op;
    theImpl->unlock();
  }

  DBUG_RETURN(tOp);
}

int Ndb::dropEventOperation(NdbEventOperation* tOp)
{
  DBUG_ENTER("Ndb::dropEventOperation");
  DBUG_PRINT("info", ("name: %s", tOp->getEvent()->getTable()->getName()));
  // remove it from list
  
  theEventBuffer->dropEventOperation(tOp);
  DBUG_RETURN(0);
}

NdbEventOperation *Ndb::getEventOperation(NdbEventOperation* tOp)
{
  NdbEventOperationImpl *op;
  if (tOp)
    op= NdbEventBuffer::getEventOperationImpl(tOp)->m_next;
  else
    op= theImpl->m_ev_op;
  if (op)
    return op->m_facade;
  return 0;
}

int
Ndb::pollEvents2(int aMillisecondNumber, Uint64 *highestQueuedEpoch)
{
  if (unlikely(aMillisecondNumber < 0))
  {
    g_eventLogger->error("Ndb::pollEvents2: negative aMillisecondNumber %d 0x%x %s",
                         aMillisecondNumber,
                         getReference(),
                         getNdbObjectName());
    return -1;
  }

  /* Look for already available events without polling transporter. */
  const int found = theEventBuffer->pollEvents(highestQueuedEpoch);
  if (found)
    return found;

  /**
   * We need to poll the transporter, and possibly wait, to make sure
   * that arrived events are delivered to their clients as soon as possible.
   * ::trp_deliver_signal() will wakeup the client when event arrives.
   */
  PollGuard poll_guard(* theImpl);
  poll_guard.wait_n_unlock(aMillisecondNumber, 0, WAIT_EVENT);
  // PollGuard ends here

  return theEventBuffer->pollEvents(highestQueuedEpoch);
}

bool
Ndb::isExpectingHigherQueuedEpochs()
{
  return !theEventBuffer->m_failure_detected;
}

void
Ndb::printOverflowErrorAndExit()
{
  g_eventLogger->error("Ndb Event Buffer : 0x%x %s",
                       getReference(), getNdbObjectName());
  g_eventLogger->error("Ndb Event Buffer : Event buffer out of memory.");
  g_eventLogger->error("Ndb Event Buffer : Fatal error.");
  Uint32 maxalloc = get_eventbuf_max_alloc();
  if (maxalloc != 0)
  {
    // limited memory is allocated for event buffer, give recommendation
    g_eventLogger->error("Ndb Event Buffer : Change eventbuf_max_alloc (Current max_alloc is %u).", maxalloc);
  }
  g_eventLogger->error("Ndb Event Buffer : Consider using the new API.");
  exit(-1);
}

int
Ndb::pollEvents(int aMillisecondNumber, Uint64 *highestQueuedEpoch)
{
  /* Look for already available events without polling transporter */
  /** Note: pollEvents() does not call pollEvents2() as the other backward
   * compatibility methods do, but directly call theEventBuffer->pollEvents.
   * This is to simplify the code by avoiding the
   * handling of negative aMillisecondNumber rejected by pollEvents2(),
   * but accepted by pollEvents() as an *infinite* maxwait.
   */
  int found = theEventBuffer->pollEvents(highestQueuedEpoch);
  if (!found)
  {
    /**
     * We need to poll the transporter, and possibly wait, to make sure
     * that arrived events are delivered to their clients as soon as possible.
     * ::trp_deliver_signal() will wakeup the client when event arrives,
     * or a new (empty) epoch is completed
     */
    PollGuard poll_guard(* theImpl);
    poll_guard.wait_n_unlock(aMillisecondNumber, 0, WAIT_EVENT);
    // PollGuard ends here

    found = theEventBuffer->pollEvents(highestQueuedEpoch);
  }

  if ((highestQueuedEpoch) && (isExpectingHigherQueuedEpochs() == false))
    *highestQueuedEpoch= NDB_FAILURE_GCI;

  return found;
}

int
Ndb::flushIncompleteEvents(Uint64 gci)
{
  theEventBuffer->lock();
  int ret = theEventBuffer->flushIncompleteEvents(gci);
  theEventBuffer->unlock();
  return ret;
}

NdbEventOperation *Ndb::nextEvent2()
{
  return theEventBuffer->nextEvent2();
}

NdbEventOperation *Ndb::nextEvent()
{
  NdbDictionary::Event::TableEvent errType;

  // Remove the event data from the head
  NdbEventOperation *op = theEventBuffer->nextEvent2();
  if (op == NULL)
    return NULL;

  if (unlikely(op->isErrorEpoch(&errType)))
  {
    if (errType ==  NdbDictionary::Event::TE_INCONSISTENT)
      return NULL;

    if (errType ==  NdbDictionary::Event::TE_OUT_OF_MEMORY)
      printOverflowErrorAndExit();
  }

  if (unlikely(op->isEmptyEpoch()))
  {
    g_eventLogger->error("Ndb::nextEvent: Found exceptional event type "
                         "TE_EMPTY when using old event API. "
                         "Turn off empty epoch queuing by "
                         "setEventBufferQueueEmptyEpoch(false).");
    exit(-1);
  }
  return op;
}

bool
Ndb::isConsistent(Uint64& gci)
{
  return theEventBuffer->isConsistent(gci);
}

bool
Ndb::isConsistentGCI(Uint64 gci)
{
  return theEventBuffer->isConsistentGCI(gci);
}

const NdbEventOperation*
Ndb::getNextEventOpInEpoch2(Uint32* iter, Uint32* event_types)
{
  return getNextEventOpInEpoch3(iter, event_types, NULL);
}

const NdbEventOperation*
Ndb::getNextEventOpInEpoch3(Uint32* iter, Uint32* event_types,
                           Uint32* cumulative_any_value)
{
  NdbEventOperationImpl* op =
    theEventBuffer->getEpochEventOperations(iter, event_types, cumulative_any_value);
  if (op != NULL)
    return op->m_facade;
  return NULL;
}

const NdbEventOperation*
Ndb::getGCIEventOperations(Uint32* iter, Uint32* event_types)
{
  return getNextEventOpInEpoch3(iter, event_types, NULL);
  /*
   * No event operation is added to gci_ops list for exceptional event data.
   * So it is not possible to get them in event_types. No check needed.
   */
}

Uint64 Ndb::getHighestQueuedEpoch()
{
  return theEventBuffer->getHighestQueuedEpoch();
}

Uint64 Ndb::getLatestGCI()
{
  return theEventBuffer->getLatestGCI();
}

void Ndb::setReportThreshEventGCISlip(unsigned thresh)
{
 if (theEventBuffer->m_gci_slip_thresh != thresh)
 {
   theEventBuffer->m_gci_slip_thresh= thresh;
 }
}

void Ndb::setReportThreshEventFreeMem(unsigned thresh)
{
  if (theEventBuffer->m_free_thresh != thresh)
  {
    theEventBuffer->m_free_thresh= thresh;
    theEventBuffer->m_min_free_thresh= thresh;
    theEventBuffer->m_max_free_thresh= 100;
  }
}

void Ndb::setEventBufferQueueEmptyEpoch(bool queue_empty_epoch)
{
  theEventBuffer->setEventBufferQueueEmptyEpoch(queue_empty_epoch);
}

Uint64 Ndb::allocate_transaction_id()
{
  Uint64 ret= theFirstTransId;

  if ((theFirstTransId & 0xFFFFFFFF) == 0xFFFFFFFF) {
    theFirstTransId = (theFirstTransId >> 32) << 32;
  } else {
    theFirstTransId++;
  }

  return ret;
}

#ifdef VM_TRACE
#include <NdbMutex.h>
extern NdbMutex *ndb_print_state_mutex;

static bool
checkdups(NdbTransaction** list, unsigned no)
{
  for (unsigned i = 0; i < no; i++)
    for (unsigned j = i + 1; j < no; j++)
      if (list[i] == list[j])
        return true;
  return false;
}
void
Ndb::printState(const char* fmt, ...)
{
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  NdbMutex_Lock(ndb_print_state_mutex);
  bool dups = false;
  unsigned i;
  ndbout << buf << " ndb=" << hex << (void*)this << endl;
  for (unsigned n = 0; n < MAX_NDB_NODES; n++) {
    NdbTransaction* con = theConnectionArray[n];
    if (con != 0) {
      ndbout << "conn " << n << ":" << endl;
      while (con != 0) {
        con->printState();
        con = con->theNext;
      }
    }
  }
  ndbout << "prepared: " << theNoOfPreparedTransactions<< endl;
  if (checkdups(thePreparedTransactionsArray, theNoOfPreparedTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (i = 0; i < theNoOfPreparedTransactions; i++)
    thePreparedTransactionsArray[i]->printState();
  ndbout << "sent: " << theNoOfSentTransactions<< endl;
  if (checkdups(theSentTransactionsArray, theNoOfSentTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (i = 0; i < theNoOfSentTransactions; i++)
    theSentTransactionsArray[i]->printState();
  ndbout << "completed: " << theNoOfCompletedTransactions<< endl;
  if (checkdups(theCompletedTransactionsArray, theNoOfCompletedTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (i = 0; i < theNoOfCompletedTransactions; i++)
    theCompletedTransactionsArray[i]->printState();
  NdbMutex_Unlock(ndb_print_state_mutex);
}

#endif

const char*
Ndb::getNdbErrorDetail(const NdbError& err, char* buff, Uint32 buffLen) const
{
  DBUG_ENTER("Ndb::getNdbErrorDetail");
  /* If err has non-null details member, prepare a string containing
   * those details
   */
  if (!buff)
    DBUG_RETURN(NULL);

  if (err.details != NULL)
  {
    DBUG_PRINT("info", ("err.code is %u", err.code));
    switch (err.code) {
    case 893: /* Unique constraint violation */
    {
      /* err.details contains the violated Index's object id
       * We'll map it to a name, then map the name to a 
       * base table, schema and database, and put that in
       * string form into the caller's buffer
       */
      UintPtr uip = (UintPtr) err.details;
      Uint32 indexObjectId = (Uint32) (uip - (UintPtr(0)));
      Uint32 primTableObjectId = ~ (Uint32) 0;
      BaseString indexName;
      char splitChars[2] = {table_name_separator, 0};
      BaseString splitString(&splitChars[0]);
      
      {
        DBUG_PRINT("info", ("Index object id is %u", indexObjectId));
        NdbDictionary::Dictionary::List allIndices;
        int rc = theDictionary->listObjects(allIndices, 
                                            NdbDictionary::Object::UniqueHashIndex,
                                            false); // FullyQualified names
        if (rc)
        {
          DBUG_PRINT("info", ("listObjects call 1 failed with rc %u", rc));
          DBUG_RETURN(NULL);
        }
        
        DBUG_PRINT("info", ("Retrieved details for %u indices", allIndices.count));
        
        for (unsigned i = 0; i < allIndices.count; i++)
        {
          if (allIndices.elements[i].id == indexObjectId)
          {
            /* Found the index in question
             * Expect fully qualified name to be in the form :
             * <db>/<schema>/<primTabId>/<IndexName>
             */
            Vector<BaseString> idxNameComponents;
            BaseString idxName(allIndices.elements[i].name);
            
            Uint32 components = idxName.split(idxNameComponents,
                                              splitString);
            require(components == 4);
            
            primTableObjectId = atoi(idxNameComponents[2].c_str());
            indexName = idxNameComponents[3];
            
            DBUG_PRINT("info", ("Found index name : %s, primary table id : %u",
                                indexName.c_str(), primTableObjectId));
            
            break;
          }
        }
      }

      if (primTableObjectId != (~(Uint32) 0))
      {
        NdbDictionary::Dictionary::List allTables;
        int rc = theDictionary->listObjects(allTables,
                                            NdbDictionary::Object::UserTable,
                                            false); // FullyQualified names
        
        if (rc)
        {
          DBUG_PRINT("info", ("listObjects call 2 failed with rc %u", rc));
          DBUG_RETURN(NULL);
        }

        DBUG_PRINT("info", ("Retrieved details for %u tables", allTables.count));

        for (Uint32 t = 0; t < allTables.count; t++)
        {
          
          if (allTables.elements[t].id == primTableObjectId)
          {
            /* Found table, name should be in format :
             * <db>/<schema>/<tablename>
             */
            Vector<BaseString> tabNameComponents;
            BaseString tabName(allTables.elements[t].name);
            
            Uint32 components = tabName.split(tabNameComponents,
                                              splitString);
            require(components == 3);
            
            /* Now we generate a string of the format
             * <dbname>/<schemaname>/<tabname>/<idxname>
             * which should be usable by end users
             */
            BaseString result;
            result.assfmt("%s/%s/%s/%s",
                          tabNameComponents[0].c_str(),
                          tabNameComponents[1].c_str(),
                          tabNameComponents[2].c_str(),
                          indexName.c_str());
            
            DBUG_PRINT("info", ("Found full index details : %s",
                                result.c_str()));
            
            memcpy(buff, result.c_str(), 
                   MIN(buffLen, 
                       (result.length() + 1)));
            buff[buffLen] = 0;

            DBUG_RETURN(buff);
          }
        }

        /* Primary table not found!  
         * Strange - perhaps it's been dropped?
         */          
        DBUG_PRINT("info", ("Table id %u not found", primTableObjectId));
        DBUG_RETURN(NULL);
      }
      else
      {
        /* Index not found from id - strange.
         * Perhaps it has been dropped?
         */
        DBUG_PRINT("info", ("Index id %u not found", indexObjectId));
        DBUG_RETURN(NULL);
      }
    }
    case 255: /* ZFK_NO_PARENT_ROW_EXISTS - Insert/Update failure */
    case 256: /* ZFK_CHILD_ROW_EXISTS - Update/Delete failure */
    case 21080: /* Drop parent failed - child row exists */
    {
      /* Foreign key violation errors.
       * `details` has the violated fk id.
       * We'll fetch the fully qualified fk name
       * and put that in caller's buffer */
      const UintPtr uip = (UintPtr) err.details;
      const Uint32 foreignKeyId = (Uint32) (uip - (UintPtr(0)));

      NdbDictionary::Dictionary::List allForeignKeys;
      int rc = theDictionary->listObjects(allForeignKeys,
                                          NdbDictionary::Object::ForeignKey,
                                          true); // FullyQualified names
      if (rc)
      {
        DBUG_PRINT("info", ("listObjects call 1 failed with rc %u", rc));
        DBUG_RETURN(NULL);
      }

      DBUG_PRINT("info", ("Retrieved details for %u foreign keys",
                          allForeignKeys.count));

      for (unsigned i = 0; i < allForeignKeys.count; i++)
      {
        if (allForeignKeys.elements[i].id == foreignKeyId)
        {
          const char *foreignKeyName = allForeignKeys.elements[i].name;
          DBUG_PRINT("info", ("Found the Foreign Key : %s", foreignKeyName));

          /* Copy foreignKeyName to caller's buffer.
           * If the buffer size is not enough, fk name will be truncated */
          strncpy(buff, foreignKeyName, buffLen);
          buff[buffLen-1] = 0;

          DBUG_RETURN(buff);
        }
      }

      DBUG_PRINT("info", ("Foreign key id %u not found", foreignKeyId));
      DBUG_RETURN(NULL);
    }
    default:
    {
      /* Unhandled details type */
    }
    }
  }
  
  DBUG_PRINT("info", ("No details string for this error"));
  DBUG_RETURN(NULL);
}

void
Ndb::setCustomData(void* _customDataPtr)
{
  theImpl->customData = Uint64(_customDataPtr);
}

void*
Ndb::getCustomData() const
{
  return (void*)theImpl->customData;
}

void
Ndb::setCustomData64(Uint64 _customData)
{
  theImpl->customData = _customData;
}

Uint64
Ndb::getCustomData64() const
{
  return theImpl->customData;
}

Uint64
Ndb::getNextTransactionId() const
{
  return theFirstTransId;
}

Uint32
Ndb::getMinDbNodeVersion() const
{
  return theCachedMinDbNodeVersion;
}

const char* ClientStatNames [] =
{ "WaitExecCompleteCount",
  "WaitScanResultCount",
  "WaitMetaRequestCount",
  "WaitNanosCount",
  "BytesSentCount",
  "BytesRecvdCount",
  "TransStartCount",
  "TransCommitCount",
  "TransAbortCount",
  "TransCloseCount",
  "PkOpCount",
  "UkOpCount",
  "TableScanCount",
  "RangeScanCount",
  "PrunedScanCount",
  "ScanBatchCount",
  "ReadRowCount",
  "TransLocalReadRowCount",
  "DataEventsRecvdCount",
  "NonDataEventsRecvdCount",
  "EventBytesRecvdCount",
  "ForcedSendsCount",
  "UnforcedSendsCount",
  "DeferredSendsCount"
};

Uint64
Ndb::getClientStat(Uint32 id) const
{
  if (likely(id < NumClientStatistics))
    return theImpl->clientStats[id];

  return 0;
}

const char*
Ndb::getClientStatName(Uint32 id) const
{
  if (likely(id < NumClientStatistics))
    return ClientStatNames[id];

  return NULL;
}
