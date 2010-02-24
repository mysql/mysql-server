/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBSPJ_C
#include "Dbspj.hpp"

#include <SectionReader.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/DbspjErr.hpp>
#include <signaldata/QueryTree.hpp>
#include <signaldata/TcKeyRef.hpp>
#include <signaldata/RouteOrd.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/DiGetNodes.hpp>
#include <signaldata/AttrInfo.hpp>
#include <Interpreter.hpp>
#include <AttributeHeader.hpp>
#include <KeyDescriptor.hpp>
#include <md5_hash.hpp>

// Use DEBUG to print messages that should be
// seen only when we debug the product
#ifdef VM_TRACE

#define DEBUG(x) ndbout << "DBSPJ: "<< x << endl;
#define DEBUG_LQHKEYREQ
#define DEBUG_SCAN_FRAGREQ

#else

#define DEBUG(x)

#endif

#if 1
#define DEBUG_CRASH() ndbrequire(false)
#else
#define DEBUG_CRASH()
#endif


#undef DEBUG
#undef DEBUG_LQHKEYREQ
#undef DEBUG_SCAN_FRAGREQ

#define DEBUG(x)


/** A noop for now.*/
void Dbspj::execREAD_CONFIG_REQ(Signal* signal) 
{
  jamEntry();
  const ReadConfigReq req = 
    *reinterpret_cast<const ReadConfigReq*>(signal->getDataPtr());
  
  Pool_context pc;
  pc.m_block = this;

  DEBUG("execREAD_CONFIG_REQ");
  DEBUG("sizeof(Request): " << sizeof(Request) <<
        " sizeof(TreeNode): " << sizeof(TreeNode));

  m_arenaAllocator.init(1024, RT_SPJ_ARENA_BLOCK, pc);
  m_request_pool.arena_pool_init(&m_arenaAllocator, RT_SPJ_REQUEST, pc);
  m_treenode_pool.arena_pool_init(&m_arenaAllocator, RT_SPJ_TREENODE, pc);
  m_lookup_request_hash.setSize(16);
  m_scan_request_hash.setSize(16);

  Record_info ri;
  Dependency_map::createRecordInfo(ri, RT_SPJ_DATABUFFER);
  m_dependency_map_pool.init(&m_arenaAllocator, ri, pc);

  ReadConfigConf* const conf = 
    reinterpret_cast<ReadConfigConf*>(signal->getDataPtrSend());
  conf->senderRef = reference();
  conf->senderData = req.senderData;
  
  sendSignal(req.senderRef, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}//Dbspj::execREAD_CONF_REQ()


void Dbspj::execSTTOR(Signal* signal) 
{
//#define UNIT_TEST_DATABUFFER2

  jamEntry();
  /* START CASE */
  const Uint16 tphase = signal->theData[1];

  ndbout << "Dbspj::execSTTOR() inst:" << instance() 
	 << " phase=" << tphase << endl;
  const Uint16 csignalKey = signal->theData[6];
  signal->theData[0] = csignalKey;
  signal->theData[1] = 3;    /* BLOCK CATEGORY */
  signal->theData[2] = 2;    /* SIGNAL VERSION NUMBER */
#ifdef UNIT_TEST_DATABUFFER2
  signal->theData[3] = 120;  /* Start phase end*/
#else
  signal->theData[3] = 255;
#endif
  signal->theData[4] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);

#ifdef UNIT_TEST_DATABUFFER2
  if (tphase == 120)
  {
    ndbout_c("basic test of ArenaPool / DataBuffer2");

    for (Uint32 i = 0; i<100; i++)
    {
      ArenaHead ah;
      if (!m_arenaAllocator.seize(ah))
      {
        ndbout_c("Failed to allocate arena");
        break;
      }

      ndbout_c("*** LOOP %u", i);
      Uint32 sum = 0;
      Dependency_map::Head head;
      LocalArenaPoolImpl pool(ah, m_dependency_map_pool);
      for (Uint32 j = 0; j<100; j++)
      {
        Uint32 sz = rand() % 1000;
        if (0)
          ndbout_c("adding %u", sz);
        Local_dependency_map list(pool, head);
        for (Uint32 i = 0; i<sz; i++)
          signal->theData[i] = sum + i;
        list.append(signal->theData, sz);
        sum += sz;
      }

      {
        ndbrequire(head.getSize() == sum);
        Local_dependency_map list(pool, head);
        Dependency_map::ConstDataBufferIterator it;
        Uint32 cnt = 0;
        for (list.first(it); !it.isNull(); list.next(it))
        {
          ndbrequire(* it.data == cnt);
          cnt++;
        }

        ndbrequire(cnt == sum);
      }

      Resource_limit rl;
      if (m_ctx.m_mm.get_resource_limit(7, rl))
      {
        ndbout_c("Resource %d min: %d max: %d curr: %d",
                 7, rl.m_min, rl.m_max, rl.m_curr);
      }

      {
        ndbout_c("release map");
        Local_dependency_map list(pool, head);
        list.release();
      }

      ndbout_c("release all");
      m_arenaAllocator.release(ah);
      ndbout_c("*** LOOP %u sum: %u", i, sum);
    }
  }
#endif
}//Dbspj::execSTTOR()

/**
 * MODULE LQHKEYREQ
 */
void Dbspj::execLQHKEYREQ(Signal* signal)
{
  jamEntry();

  const LqhKeyReq* req = reinterpret_cast<const LqhKeyReq*>(signal->getDataPtr());

  /**
   * #0 - KEYINFO contains key for first operation (used for hash in TC)
   * #1 - ATTRINFO contains tree + parameters
   *      (unless StoredProcId is set, when only paramters are sent,
   *       but this is not yet implemented)
   */
  SectionHandle handle = SectionHandle(this, signal);
  SegmentedSectionPtr ssPtr;
  handle.getSection(ssPtr, LqhKeyReq::AttrInfoSectionNum);

  Uint32 err;
  Ptr<Request> requestPtr = { 0, RNIL };
  do
  {
    ArenaHead ah;
    err = DbspjErr::OutOfQueryMemory;
    if (unlikely(!m_arenaAllocator.seize(ah)))
      break;


    m_request_pool.seize(ah, requestPtr);

    new (requestPtr.p) Request(ah);
    do_init(requestPtr.p, req, signal->getSendersBlockRef());

    Uint32 len_cnt;

    {
      SectionReader r0(ssPtr, getSectionSegmentPool());

      err = DbspjErr::ZeroLengthQueryTree;
      if (unlikely(!r0.getWord(&len_cnt)))
	break;
    }

    Uint32 len = QueryTree::getLength(len_cnt);
    Uint32 cnt = QueryTree::getNodeCnt(len_cnt);

    {
      SectionReader treeReader(ssPtr, getSectionSegmentPool());
      SectionReader paramReader(ssPtr, getSectionSegmentPool());
      paramReader.step(len); // skip over tree to parameters

      Build_context ctx;
      ctx.m_resultRef = req->variableData[0];
      ctx.m_savepointId = req->savePointId;
      err = build(ctx, requestPtr, treeReader, paramReader);
      if (unlikely(err != 0))
	break;
    }

    /**
     * a query being shipped as a LQHKEYREQ may only return finite rows
     *   i.e be a (multi-)lookup
     */
    ndbassert(requestPtr.p->isLookup());
    ndbassert(requestPtr.p->m_node_cnt == cnt);
    err = DbspjErr::InvalidRequest;
    if (unlikely(!requestPtr.p->isLookup() || requestPtr.p->m_node_cnt != cnt))
      break;

    /**
     * Store request in list(s)/hash(es)
     */
    store_lookup(requestPtr);

    release(ssPtr);
    handle.getSection(ssPtr, LqhKeyReq::KeyInfoSectionNum);
    handle.clear();

    start(signal, requestPtr, ssPtr);
    return;
  } while (0);

  /**
   * Error handling below,
   *  'err' may contain error code.
   */
  if (!requestPtr.isNull())
  {
    jam();
    m_request_pool.release(requestPtr);
  }
  releaseSections(handle);
  handle_early_lqhkey_ref(signal, req, err);
}

void
Dbspj::do_init(Request* requestP, const LqhKeyReq* req, Uint32 senderRef)
{
  requestP->m_bits = 0;
  requestP->m_node_cnt = 0;
  requestP->m_cnt_active = 0;
  requestP->m_transId[0] = req->transId1;
  requestP->m_transId[1] = req->transId2;
  requestP->m_node_mask.clear();

  const Uint32 reqInfo = req->requestInfo;
  Uint32 tmp = req->clientConnectPtr;
  if (LqhKeyReq::getDirtyFlag(reqInfo) &&
      LqhKeyReq::getOperation(reqInfo) == ZREAD)
  {
    jam();

    ndbrequire(LqhKeyReq::getApplicationAddressFlag(reqInfo));
    //const Uint32 apiRef   = lqhKeyReq->variableData[0];
    //const Uint32 apiOpRec = lqhKeyReq->variableData[1];
    tmp = req->variableData[1];
    requestP->m_senderData = tmp;
    requestP->m_senderRef = 0;
  }
  else
  {
    if (LqhKeyReq::getSameClientAndTcFlag(reqInfo) == 1)
    {
      if (LqhKeyReq::getApplicationAddressFlag(reqInfo))
	tmp = req->variableData[2];
      else
	tmp = req->variableData[0];
    }
    requestP->m_senderData = tmp;
    requestP->m_senderRef = senderRef;
  }
  requestP->m_rootResultData = tmp;
}

void
Dbspj::store_lookup(Ptr<Request> requestPtr)
{
  ndbassert(requestPtr.p->isLookup());
  Ptr<Request> tmp;
  bool found = m_lookup_request_hash.find(tmp, *requestPtr.p);
  ndbrequire(found == false);
  m_lookup_request_hash.add(requestPtr);
}

void
Dbspj::handle_early_lqhkey_ref(Signal* signal,
			       const LqhKeyReq * lqhKeyReq,
			       Uint32 err)
{
  /**
   * Error path...
   */
  ndbrequire(err);
  const Uint32 reqInfo = lqhKeyReq->requestInfo;
  const Uint32 transid[2] = { lqhKeyReq->transId1, lqhKeyReq->transId2 };

  if (LqhKeyReq::getDirtyFlag(reqInfo) &&
      LqhKeyReq::getOperation(reqInfo) == ZREAD)
  {
    jam();
    /* Dirty read sends TCKEYREF direct to client, and nothing to TC */
    ndbrequire(LqhKeyReq::getApplicationAddressFlag(reqInfo));
    const Uint32 apiRef   = lqhKeyReq->variableData[0];
    const Uint32 apiOpRec = lqhKeyReq->variableData[1];

    TcKeyRef* const tcKeyRef = reinterpret_cast<TcKeyRef*>(signal->getDataPtrSend());

    tcKeyRef->connectPtr = apiOpRec;
    tcKeyRef->transId[0] = transid[0];
    tcKeyRef->transId[1] = transid[1];
    tcKeyRef->errorCode = err;
    sendTCKEYREF(signal, apiRef, signal->getSendersBlockRef());
  }
  else
  {
    jam();
    const Uint32 returnref = signal->getSendersBlockRef();
    const Uint32 clientPtr = lqhKeyReq->clientConnectPtr;

    Uint32 TcOprec = clientPtr;
    if (LqhKeyReq::getSameClientAndTcFlag(reqInfo) == 1)
    {
      if (LqhKeyReq::getApplicationAddressFlag(reqInfo))
	TcOprec = lqhKeyReq->variableData[2];
      else
	TcOprec = lqhKeyReq->variableData[0];
    }

    LqhKeyRef* const ref = reinterpret_cast<LqhKeyRef*>(signal->getDataPtrSend());
    ref->userRef = clientPtr;
    ref->connectPtr = TcOprec;
    ref->errorCode = err;
    ref->transId1 = transid[0];
    ref->transId2 = transid[1];
    sendSignal(returnref, GSN_LQHKEYREF, signal,
	       LqhKeyRef::SignalLength, JBB);
  }
}

void
Dbspj::sendTCKEYREF(Signal* signal, Uint32 ref, Uint32 routeRef)
{
  const Uint32 nodeId = refToNode(ref);
  const bool connectedToNode = getNodeInfo(nodeId).m_connected;

  if (likely(connectedToNode))
  {
    jam();
    sendSignal(ref, GSN_TCKEYREF, signal, TcKeyRef::SignalLength, JBB);
  }
  else
  {
    if (routeRef)
    {
      jam();
      memmove(signal->theData+25, signal->theData, 4*TcKeyRef::SignalLength);
      RouteOrd* ord = (RouteOrd*)signal->getDataPtrSend();
      ord->dstRef = ref;
      ord->srcRef = reference();
      ord->gsn = GSN_TCKEYREF;
      ord->cnt = 0;
      LinearSectionPtr ptr[3];
      ptr[0].p = signal->theData+25;
      ptr[0].sz = TcKeyRef::SignalLength;
      sendSignal(routeRef, GSN_ROUTE_ORD, signal, RouteOrd::SignalLength, JBB,
		 ptr, 1);
    }
    else
    {
      ndbrequire(false);
    }
  }
}

/**
 * END - MODULE LQHKEYREQ
 */


/**
 * MODULE SCAN_FRAGREQ
 */
void
Dbspj::execSCAN_FRAGREQ(Signal* signal)
{
  jamEntry();

  /* Reassemble if the request was fragmented */
  if (!assembleFragments(signal))
  {
    jam();
    return;
  }

  const ScanFragReq * req = (ScanFragReq *)&signal->theData[0];

#ifdef DEBUG_SCAN_FRAGREQ
  ndbout_c("Incomming SCAN_FRAGREQ ");
  printSCAN_FRAGREQ(stdout, signal->getDataPtrSend(),
                    ScanFragReq::SignalLength + 2,
                    DBLQH);
#endif

  /**
   * #0 - ATTRINFO contains tree + parameters
   *      (unless StoredProcId is set, when only paramters are sent,
   *       but this is not yet implemented)
   * #1 - KEYINFO if first op is index scan - contains bounds for first scan
   *              if first op is lookup - contains keyinfo for lookup
   */
  SectionHandle handle = SectionHandle(this, signal);
  SegmentedSectionPtr ssPtr;
  handle.getSection(ssPtr, ScanFragReq::AttrInfoSectionNum);

  Uint32 err;
  Ptr<Request> requestPtr = { 0, RNIL };
  do
  {
    ArenaHead ah;
    err = DbspjErr::OutOfQueryMemory;
    if (unlikely(!m_arenaAllocator.seize(ah)))
      break;

    m_request_pool.seize(ah, requestPtr);

    new (requestPtr.p) Request(ah);
    do_init(requestPtr.p, req, signal->getSendersBlockRef());

    Uint32 len_cnt;
    {
      SectionReader r0(ssPtr, getSectionSegmentPool());
      err = DbspjErr::ZeroLengthQueryTree;
      if (unlikely(!r0.getWord(&len_cnt)))
	break;
    }

    Uint32 len = QueryTree::getLength(len_cnt);
    Uint32 cnt = QueryTree::getNodeCnt(len_cnt);

    {
      SectionReader treeReader(ssPtr, getSectionSegmentPool());
      SectionReader paramReader(ssPtr, getSectionSegmentPool());
      paramReader.step(len); // skip over tree to parameters

      Build_context ctx;
      ctx.m_resultRef = req->resultRef;
      ctx.m_scanPrio = ScanFragReq::getScanPrio(req->requestInfo);
      ctx.m_savepointId = req->savePointId;
      err = build(ctx, requestPtr, treeReader, paramReader);
      if (unlikely(err != 0))
	break;
    }

    ndbassert(requestPtr.p->isScan());
    ndbassert(requestPtr.p->m_node_cnt == cnt);
    err = DbspjErr::InvalidRequest;
    if (unlikely(!requestPtr.p->isScan() || requestPtr.p->m_node_cnt != cnt))
      break;

    /**
     * Store request in list(s)/hash(es)
     */
    store_scan(requestPtr);

    release(ssPtr);
    ssPtr.i = RNIL;
    ssPtr.p = 0;
    if (handle.m_cnt > 1)
    {
      jam();
      handle.getSection(ssPtr, ScanFragReq::KeyInfoSectionNum);
    }
    handle.clear();

    start(signal, requestPtr, ssPtr);
    return;
  } while (0);

  if (!requestPtr.isNull())
  {
    jam();
    m_request_pool.release(requestPtr);
  }
  releaseSections(handle);
  handle_early_scanfrag_ref(signal, req, err);
}

void
Dbspj::do_init(Request* requestP, const ScanFragReq* req, Uint32 senderRef)
{
  requestP->m_bits = 0;
  requestP->m_node_cnt = 0;
  requestP->m_cnt_active = 0;
  requestP->m_senderRef = senderRef;
  requestP->m_senderData = req->senderData;
  requestP->m_transId[0] = req->transId1;
  requestP->m_transId[1] = req->transId2;
  requestP->m_node_mask.clear();
  requestP->m_rootResultData = req->resultData;
  requestP->m_currentNodePtrI = RNIL;
}

void
Dbspj::store_scan(Ptr<Request> requestPtr)
{
  ndbassert(requestPtr.p->isScan());
  Ptr<Request> tmp;
  bool found = m_scan_request_hash.find(tmp, *requestPtr.p);
  ndbrequire(found == false);
  m_scan_request_hash.add(requestPtr);
}

void
Dbspj::handle_early_scanfrag_ref(Signal* signal,
				 const ScanFragReq * _req,
				 Uint32 err)
{
  ScanFragReq req = *_req;
  Uint32 senderRef = signal->getSendersBlockRef();

  ScanFragRef * ref = (ScanFragRef*)&signal->theData[0];
  ref->senderData = req.senderData;
  ref->transId1 = req.transId1;
  ref->transId2 = req.transId2;
  ref->errorCode = err;
  sendSignal(senderRef, GSN_SCAN_FRAGREF, signal,
	     ScanFragRef::SignalLength, JBB);
}

/**
 * END - MODULE SCAN_FRAGREQ
 */

/**
 * MODULE GENERIC
 */
Uint32
Dbspj::build(Build_context& ctx,
	     Ptr<Request> requestPtr,
             SectionReader & tree,
             SectionReader & param)
{
  Uint32 tmp0, tmp1;
  Uint32 err = DbspjErr::ZeroLengthQueryTree;
  ctx.m_cnt = 0;

  tree.getWord(&tmp0);
  Uint32 loop = QueryTree::getNodeCnt(tmp0);

  DEBUG("::build()");
  if (loop == 0)
  {
    DEBUG_CRASH();
    goto error;
  }

  while (ctx.m_cnt < loop)
  {
    DEBUG(" - loop " << ctx.m_cnt << " pos: " << tree.getPos().currPos);
    tree.peekWord(&tmp0);
    param.peekWord(&tmp1);
    Uint32 node_op = QueryNode::getOpType(tmp0);
    Uint32 node_len = QueryNode::getLength(tmp0);
    Uint32 param_op = QueryNodeParameters::getOpType(tmp1);
    Uint32 param_len = QueryNodeParameters::getLength(tmp1);

    err = DbspjErr::QueryNodeTooBig;
    if (unlikely(node_len >= NDB_ARRAY_SIZE(m_buffer0)))
    {
      DEBUG_CRASH();
      goto error;
    }

    err = DbspjErr::QueryNodeParametersTooBig;
    if (unlikely(param_len >= NDB_ARRAY_SIZE(m_buffer1)))
    {
      DEBUG_CRASH();
      goto error;
    }

    err = DbspjErr::InvalidTreeNodeSpecification;
    if (unlikely(tree.getWords(m_buffer0, node_len) == false))
    {
      DEBUG_CRASH();
      goto error;
    }

    err = DbspjErr::InvalidTreeParametersSpecification;
    if (unlikely(param.getWords(m_buffer1, param_len) == false))
    {
      DEBUG_CRASH();
      goto error;
    }

#if defined(DEBUG_LQHKEYREQ) || defined(DEBUG_SCAN_FRAGREQ)
    printf("node: ");
    for (Uint32 i = 0; i<node_len; i++)
      printf("0x%.8x ", m_buffer0[i]);
    printf("\n");

    printf("param: ");
    for (Uint32 i = 0; i<param_len; i++)
      printf("0x%.8x ", m_buffer1[i]);
    printf("\n");
#endif

    err = DbspjErr::UnknowQueryOperation;
    if (unlikely(node_op != param_op))
    {
      DEBUG_CRASH();
      goto error;
    }
    const OpInfo* info = getOpInfo(node_op);
    if (unlikely(info == 0))
    {
      DEBUG_CRASH();
      goto error;
    }

    QueryNode* qn = (QueryNode*)m_buffer0;
    QueryNodeParameters * qp = (QueryNodeParameters*)m_buffer1;
    qn->len = node_len;
    qp->len = param_len;
    err = (this->*(info->m_build))(ctx, requestPtr, qn, qp);
    if (unlikely(err != 0))
    {
      DEBUG_CRASH();
      goto error;
    }

    /**
     * TODO handle error, by aborting request
     */
    ndbrequire(ctx.m_cnt < NDB_ARRAY_SIZE(ctx.m_node_list));
    ctx.m_cnt++;
  }
  requestPtr.p->m_node_cnt = ctx.m_cnt;
  return 0;

error:
  jam();
  return err;
}

Uint32
Dbspj::createNode(Build_context& ctx, Ptr<Request> requestPtr,
		  Ptr<TreeNode> & treeNodePtr)
{
  /**
   * In the future, we can have different TreeNode-allocation strategies
   *   that can be setup using the Build_context
   *
   */
  if (m_treenode_pool.seize(requestPtr.p->m_arena, treeNodePtr))
  {
    DEBUG("createNode - seize -> ptrI: " << treeNodePtr.i);
    new (treeNodePtr.p) TreeNode(ctx.m_cnt, requestPtr.i);
    ctx.m_node_list[ctx.m_cnt] = treeNodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    list.addLast(treeNodePtr);
    return 0;
  }
  return DbspjErr::OutOfOperations;
}

void
Dbspj::start(Signal* signal,
	     Ptr<Request> requestPtr,
	     SegmentedSectionPtr keyPtr)
{
  Ptr<TreeNode> nodePtr;
  {
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    ndbrequire(list.first(nodePtr));
  }
  ndbrequire(nodePtr.p->m_info != 0 && nodePtr.p->m_info->m_start != 0);
  (this->*(nodePtr.p->m_info->m_start))(signal, requestPtr, nodePtr, keyPtr);
}

void
Dbspj::nodeFinished(Signal* signal,
                    Ptr<Request> requestPtr,
                    Ptr<TreeNode> treeNodePtr)
{
  ndbrequire(treeNodePtr.p->m_state == TreeNode::TN_ACTIVE);
  treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;

  Uint32 cnt = requestPtr.p->m_cnt_active;
  DEBUG("nodeFinished(" << cnt << ")");
  ndbrequire(cnt);
  requestPtr.p->m_cnt_active = cnt - 1;

  if (cnt == 1)
  {
    jam();
    DEBUG("->requestFinished");
    /**
     * TODO add complete/abort phase
     */
    cleanup(requestPtr);
  }
}

void
Dbspj::cleanup(Ptr<Request> requestPtr)
{
  {
    Ptr<TreeNode> nodePtr;
    Local_TreeNode_list list(m_treenode_pool, requestPtr.p->m_nodes);
    for (list.first(nodePtr); !nodePtr.isNull(); )
    {
      jam();
      ndbrequire(nodePtr.p->m_info != 0 && nodePtr.p->m_info->m_cleanup != 0);
      (this->*(nodePtr.p->m_info->m_cleanup))(requestPtr, nodePtr);

      Ptr<TreeNode> tmp = nodePtr;
      list.next(nodePtr);
      m_treenode_pool.release(tmp);
    }
  }
  if (requestPtr.p->isScan())
  {
    jam();
#ifdef VM_TRACE
    {
      Request key;
      key.m_transId[0] = requestPtr.p->m_transId[0];
      key.m_transId[1] = requestPtr.p->m_transId[1];
      key.m_senderData = requestPtr.p->m_senderData;
      Ptr<Request> tmp;
      ndbrequire(m_scan_request_hash.find(tmp, key));
    }
#endif
    m_scan_request_hash.remove(requestPtr);
  }
  else
  {
    jam();
#ifdef VM_TRACE
    {
      Request key;
      key.m_transId[0] = requestPtr.p->m_transId[0];
      key.m_transId[1] = requestPtr.p->m_transId[1];
      key.m_senderData = requestPtr.p->m_senderData;
      Ptr<Request> tmp;
      ndbrequire(m_lookup_request_hash.find(tmp, key));
    }
#endif
    m_lookup_request_hash.remove(requestPtr);
  }
  ArenaHead ah = requestPtr.p->m_arena;
  m_request_pool.release(requestPtr);
  m_arenaAllocator.release(ah);
}

void
Dbspj::cleanup_common(Ptr<Request> requestPtr, Ptr<TreeNode> treeNodePtr)
{
  jam();

  LocalArenaPoolImpl pool(requestPtr.p->m_arena, m_dependency_map_pool);
  {
    Local_dependency_map list(pool, treeNodePtr.p->m_dependent_nodes);
    list.release();
  }

  {
    Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);
    pattern.release();
  }

  {
    Local_pattern_store pattern(pool, treeNodePtr.p->m_attrParamPattern);
    pattern.release();
  }

  if (treeNodePtr.p->m_send.m_keyInfoPtrI != RNIL)
  {
    jam();
    releaseSection(treeNodePtr.p->m_send.m_keyInfoPtrI);
  }

  if (treeNodePtr.p->m_send.m_attrInfoPtrI != RNIL)
  {
    jam();
    releaseSection(treeNodePtr.p->m_send.m_attrInfoPtrI);
  }

  if (treeNodePtr.p->m_send.m_attrInfoParamPtrI != RNIL)
  {
    jam();
    releaseSection(treeNodePtr.p->m_send.m_attrInfoParamPtrI);
  }
}

/**
 * Processing of signals from LQH
 */
void
Dbspj::execLQHKEYREF(Signal* signal)
{
  jamEntry();

  const LqhKeyRef* ref = reinterpret_cast<const LqhKeyRef*>(signal->getDataPtr());

  DEBUG("execLQHKEYREF, errorCode:" << ref->errorCode);
  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, ref->connectPtr);

  Ptr<Request> requestPtr;
  m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI);

  ndbrequire(treeNodePtr.p->m_info && treeNodePtr.p->m_info->m_execLQHKEYREF);
  (this->*(treeNodePtr.p->m_info->m_execLQHKEYREF))(signal,
                                                    requestPtr,
                                                    treeNodePtr);
}

void
Dbspj::execLQHKEYCONF(Signal* signal)
{
  jamEntry();

  DEBUG("execLQHKEYCONF");

  const LqhKeyConf* conf = reinterpret_cast<const LqhKeyConf*>(signal->getDataPtr());
  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, conf->opPtr);

  Ptr<Request> requestPtr;
  m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI);

  ndbrequire(treeNodePtr.p->m_info && treeNodePtr.p->m_info->m_execLQHKEYCONF);
  (this->*(treeNodePtr.p->m_info->m_execLQHKEYCONF))(signal,
                                                     requestPtr,
                                                     treeNodePtr);
}

void
Dbspj::execSCAN_FRAGREF(Signal* signal)
{
  jamEntry();
  const ScanFragRef* ref = reinterpret_cast<const ScanFragRef*>(signal->getDataPtr());

  DEBUG("execSCAN_FRAGREF, errorCode:" << ref->errorCode);

  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, ref->senderData);
  Ptr<Request> requestPtr;
  m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI);

  ndbrequire(treeNodePtr.p->m_info&&treeNodePtr.p->m_info->m_execSCAN_FRAGREF);
  (this->*(treeNodePtr.p->m_info->m_execSCAN_FRAGREF))(signal,
                                                       requestPtr,
                                                       treeNodePtr);
}

void
Dbspj::execSCAN_HBREP(Signal* signal)
{
  jamEntry();

  Uint32 senderData = signal->theData[0];
  //Uint32 transId[2] = { signal->theData[1], signal->theData[2] };

  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, senderData);
  Ptr<Request> requestPtr;
  m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI);

  Uint32 ref = requestPtr.p->m_senderRef;
  signal->theData[0] = requestPtr.p->m_senderData;
  sendSignal(ref, GSN_SCAN_HBREP, signal, 3, JBB);
}

void
Dbspj::execSCAN_FRAGCONF(Signal* signal)
{
  jamEntry();
  DEBUG("execSCAN_FRAGCONF");

  const ScanFragConf* conf = reinterpret_cast<const ScanFragConf*>(signal->getDataPtr());

  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, conf->senderData);
  Ptr<Request> requestPtr;
  m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI);

  ndbrequire(treeNodePtr.p->m_info&&treeNodePtr.p->m_info->m_execSCAN_FRAGCONF);
  (this->*(treeNodePtr.p->m_info->m_execSCAN_FRAGCONF))(signal,
                                                        requestPtr,
                                                        treeNodePtr);
}

void
Dbspj::execSCAN_NEXTREQ(Signal* signal)
{
  jamEntry();
  const ScanFragNextReq * req = (ScanFragNextReq*)&signal->theData[0];

  Request key;
  key.m_transId[0] = req->transId1;
  key.m_transId[1] = req->transId2;
  key.m_senderData = req->senderData;

  Ptr<Request> requestPtr;
  if (unlikely(!m_scan_request_hash.find(requestPtr, key)))
  {
    jam();
    ndbrequire(req->closeFlag == ZTRUE);
    DEBUG(key.m_senderData << " Received SCAN_NEXTREQ with close when closed");
    return;
  }

  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, requestPtr.p->m_currentNodePtrI);

  if (treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_CLOSING)
  {
    jam();
    /**
     * Duplicate of a close request already sent to datanodes.
     * Ignore this and wait for reply on pending request.
     */
    DEBUG("execSCAN_NEXTREQ, is SF_CLOSING -> ignore request");
    return;
  }

  if (req->closeFlag == ZTRUE)                             // Requested close scan
  {
    if (treeNodePtr.p->m_scanfrag_data.m_scan_status == 2) // Is closed on LQH
    {
      jam();
      ndbassert (treeNodePtr.p->m_scanfrag_data.m_scan_state != ScanFragData::SF_RUNNING);

      ScanFragConf* conf = reinterpret_cast<ScanFragConf*>(signal->getDataPtrSend());
      conf->senderData = requestPtr.p->m_senderData;
      conf->transId1 = requestPtr.p->m_transId[0];
      conf->transId2 = requestPtr.p->m_transId[1];
      conf->completedOps = 0;
      conf->fragmentCompleted = 2; // =ZSCAN_FRAG_CLOSED -> Finished...
      conf->total_len = 0; // Not supported...

      DEBUG("execSCAN_NEXTREQ(close), LQH has conf'ed 'w/ ZSCAN_FRAG_CLOSED");
      sendSignal(requestPtr.p->m_senderRef, GSN_SCAN_FRAGCONF, signal,
                 ScanFragConf::SignalLength, JBB);

      cleanup(requestPtr);
      return;
    }
    else if (treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_RUNNING)
    {
      jam();
      DEBUG("execSCAN_NEXTREQ, make PENDING CLOSE");
      treeNodePtr.p->m_scanfrag_data.m_pending_close = true;
      return;
    }
    // else; fallthrough & send to datanodes:
  }

  ndbassert (!treeNodePtr.p->m_scanfrag_data.m_pending_close);
  ndbassert (treeNodePtr.p->m_scanfrag_data.m_scan_status != 2);
  ndbrequire(treeNodePtr.p->m_info != 0 &&
             treeNodePtr.p->m_info->m_execSCAN_NEXTREQ != 0);
  (this->*(treeNodePtr.p->m_info->m_execSCAN_NEXTREQ))(signal,
                                                       requestPtr, treeNodePtr);
}

void
Dbspj::execTRANSID_AI(Signal* signal)
{
  jamEntry();
  DEBUG("execTRANSID_AI");
  TransIdAI * req = (TransIdAI *)signal->getDataPtr();
  Uint32 ptrI = req->connectPtr;
  //Uint32 transId[2] = { req->transId[0], req->transId[1] };

  Ptr<TreeNode> treeNodePtr;
  m_treenode_pool.getPtr(treeNodePtr, ptrI);
  Ptr<Request> requestPtr;
  m_request_pool.getPtr(requestPtr, treeNodePtr.p->m_requestPtrI);

  ndbrequire(signal->getNoOfSections() != 0); // TODO check if this can happen

  SegmentedSectionPtr dataPtr;
  {
    SectionHandle handle(this, signal);
    handle.getSection(dataPtr, 0);
    handle.clear();
  }

#if defined(DEBUG_LQHKEYREQ) || defined(DEBUG_SCAN_FRAGREQ)
  printf("execTRANSID_AI: ");
  print(dataPtr, stdout);
#endif

  /**
   * build easy-access-array for row
   */
  Uint32 tmp[2+MAX_ATTRIBUTES_IN_TABLE];
  RowRef::Header* const header = reinterpret_cast<RowRef::Header*>(tmp);

  Uint32 cnt = buildRowHeader(header, dataPtr);
  ndbassert(header->m_len <= 1+MAX_ATTRIBUTES_IN_TABLE);

  /**
   * TODO: If row needs to be buffered (m_bits & ROW_BUFFER)
   *   we should here allocate a row, and store it...
   */
  struct RowRef row;
  row.m_type = RowRef::RT_SECTION;
  row.m_src_node_ptrI = treeNodePtr.i;
  row.m_src_node_no = treeNodePtr.p->m_node_no;
  row.m_row_data.m_section.m_header = (RowRef::Header*)tmp;
  row.m_row_data.m_section.m_dataPtr.assign(dataPtr);
  Uint32 rootStreamId = 0;
  getCorrelationData(row.m_row_data.m_section, 
                     cnt - 1, 
                     rootStreamId, 
                     row.m_src_correlation);
  ndbrequire(requestPtr.p->m_rootResultData == rootStreamId);
  ndbrequire(treeNodePtr.p->m_info&&treeNodePtr.p->m_info->m_execTRANSID_AI);
  (this->*(treeNodePtr.p->m_info->m_execTRANSID_AI))(signal,
                                                     requestPtr,
                                                     treeNodePtr,
                                                     row);
  release(row.m_row_data.m_section.m_dataPtr);
}
/**
 * END - MODULE GENERIC
 */

/**
 * MODULE LOOKUP
 */
const Dbspj::OpInfo
Dbspj::g_LookupOpInfo =
{
  &Dbspj::lookup_build,
  &Dbspj::lookup_start,
  &Dbspj::lookup_execTRANSID_AI,
  &Dbspj::lookup_execLQHKEYREF,
  &Dbspj::lookup_execLQHKEYCONF,
  0, // execSCAN_FRAGREF
  0, // execSCAN_FRAGCONF
  &Dbspj::lookup_start_child,
  0, // Dbspj::lookup_execSCAN_NEXTREQ
  0, // Dbspj::lookup_complete
  0, // Dbspj::lookup_abort
  &Dbspj::lookup_cleanup,
  &Dbspj::lookup_count_descendant_signal
};

Uint32
Dbspj::lookup_build(Build_context& ctx,
		    Ptr<Request> requestPtr,
		    const QueryNode* qn,
                    const QueryNodeParameters* qp)
{
  Uint32 err = 0;
  Ptr<TreeNode> treeNodePtr;
  const QN_LookupNode * node = (const QN_LookupNode*)qn;
  const QN_LookupParameters * param = (const QN_LookupParameters*)qp;
  do
  {
    err = createNode(ctx, requestPtr, treeNodePtr);
    if (unlikely(err != 0))
    {
      DEBUG_CRASH();
      break;
    }

    treeNodePtr.p->m_info = &g_LookupOpInfo;
    Uint32 transId1 = requestPtr.p->m_transId[0];
    Uint32 transId2 = requestPtr.p->m_transId[1];
    Uint32 savePointId = ctx.m_savepointId;

    Uint32 treeBits = node->requestInfo;
    Uint32 paramBits = param->requestInfo;
    //ndbout_c("Dbspj::lookup_build() treeBits=%.8x paramBits=%.8x", 
    //         treeBits, paramBits);
    LqhKeyReq* dst = (LqhKeyReq*)treeNodePtr.p->m_lookup_data.m_lqhKeyReq;
    {
      /**
       * static variables
       */
      dst->tcBlockref = reference();
      dst->clientConnectPtr = treeNodePtr.i;

      /**
       * TODO reference()+treeNodePtr.i is passed twice
       *   this can likely be optimized using the requestInfo-bits
       * UPDATE: This can be accomplished by *not* setApplicationAddressFlag
       *         and patch LQH to then instead use tcBlockref/clientConnectPtr
       */
      dst->transId1 = transId1;
      dst->transId2 = transId2;
      dst->savePointId = savePointId;
      dst->scanInfo = 0;
      dst->attrLen = 0;
      /** Initialy set reply ref to client, do_send will set SPJ refs if non-LEAF */
      dst->variableData[0] = ctx.m_resultRef;
      dst->variableData[1] = param->resultData;  
      Uint32 requestInfo = 0;
      LqhKeyReq::setOperation(requestInfo, ZREAD);
      LqhKeyReq::setApplicationAddressFlag(requestInfo, 1);
      LqhKeyReq::setDirtyFlag(requestInfo, 1);
      LqhKeyReq::setSimpleFlag(requestInfo, 1);
      LqhKeyReq::setNormalProtocolFlag(requestInfo, 0);  // Assume T_LEAF 
      LqhKeyReq::setAnyValueFlag(requestInfo, 1);
      LqhKeyReq::setNoDiskFlag(requestInfo, 
                               (treeBits & DABits::NI_LINKED_DISK) == 0 &&
                               (paramBits & DABits::PI_DISK_ATTR) == 0);
      dst->requestInfo = requestInfo;
    }

    err = DbspjErr::InvalidTreeNodeSpecification;
    if (unlikely(node->len < QN_LookupNode::NodeSize))
    {
      DEBUG_CRASH();
      break;
    }


    Uint32 tableId = node->tableId;
    Uint32 schemaVersion = node->tableVersion;

    Uint32 tableSchemaVersion = tableId + ((schemaVersion << 16) & 0xFFFF0000);
    dst->tableSchemaVersion = tableSchemaVersion;

    err = DbspjErr::InvalidTreeParametersSpecification;
    DEBUG("param len: " << param->len);
    if (unlikely(param->len < QN_LookupParameters::NodeSize))
    {
      DEBUG_CRASH();
      break;
    }

    ctx.m_resultData = param->resultData;
    treeNodePtr.p->m_lookup_data.m_api_resultRef = ctx.m_resultRef;
    treeNodePtr.p->m_lookup_data.m_api_resultData = param->resultData;
    treeNodePtr.p->m_lookup_data.m_outstanding = 0;

    /**
     * Parse stuff common lookup/scan-frag
     */
    struct DABuffer nodeDA, paramDA;
    nodeDA.ptr = node->optional;
    nodeDA.end = nodeDA.ptr + (node->len - QN_LookupNode::NodeSize);
    paramDA.ptr = param->optional;
    paramDA.end = paramDA.ptr + (param->len - QN_LookupParameters::NodeSize);
    err = parseDA(ctx, requestPtr, treeNodePtr,
                  nodeDA, treeBits, paramDA, paramBits);
    if (unlikely(err != 0))
    {
      DEBUG_CRASH();
      break;
    }

    treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;

    return 0;
  } while (0);

  return err;
}

void
Dbspj::lookup_start(Signal* signal,
		    Ptr<Request> requestPtr,
		    Ptr<TreeNode> treeNodePtr,
		    SegmentedSectionPtr keyInfo)
{
  const LqhKeyReq* src = reinterpret_cast<const LqhKeyReq*>(signal->getDataPtr());

#if NOT_YET
  Uint32 instanceNo = blockToInstance(signal->header.theReceiversBlockNumber);
  treeNodePtr.p->m_send.m_ref = numberToRef(DBLQH, instanceNo, getOwnNodeId());
#else
  treeNodePtr.p->m_send.m_ref = 
    numberToRef(DBLQH, getInstanceKey(src->tableSchemaVersion & 0xFFFF,
                                      src->fragmentData & 0xFFFF),
                getOwnNodeId());
#endif

  Uint32 hashValue = src->hashValue;
  Uint32 fragId = src->fragmentData;
  Uint32 requestInfo = src->requestInfo;
  Uint32 attrLen = src->attrLen; // fragdist-key is in here
  Uint32 interpretedFlag = LqhKeyReq::getInterpretedFlag(requestInfo);
  Uint32 noDiskFlag = LqhKeyReq::getNoDiskFlag(requestInfo);

  /**
   * assertions
   */
  ndbassert(LqhKeyReq::getAttrLen(attrLen) == 0);         // Only long
  ndbassert(LqhKeyReq::getScanTakeOverFlag(attrLen) == 0);// Not supported
  ndbassert(LqhKeyReq::getReorgFlag(attrLen) == 0);       // Not supported
  ndbassert(LqhKeyReq::getOperation(requestInfo) == ZREAD);
  ndbassert(LqhKeyReq::getKeyLen(requestInfo) == 0);      // Only long
  ndbassert(LqhKeyReq::getMarkerFlag(requestInfo) == 0);  // Only read
  ndbassert(LqhKeyReq::getAIInLqhKeyReq(requestInfo) == 0);
  ndbassert(LqhKeyReq::getSeqNoReplica(requestInfo) == 0);
  ndbassert(LqhKeyReq::getLastReplicaNo(requestInfo) == 0);
  ndbassert(LqhKeyReq::getApplicationAddressFlag(requestInfo) != 0);
  ndbassert(LqhKeyReq::getSameClientAndTcFlag(requestInfo) == 0);
  /**
   * Handled using tree
   */
  ndbassert(LqhKeyReq::getInterpretedFlag(requestInfo) == 0);

#if TODO
  /**
   * Handle various lock-modes
   */
  static Uint8 getDirtyFlag(const UintR & requestInfo);
  static Uint8 getSimpleFlag(const UintR & requestInfo);
#endif

  LqhKeyReq * dst = (LqhKeyReq*)treeNodePtr.p->m_lookup_data.m_lqhKeyReq;
  Uint32 dst_requestInfo = dst->requestInfo;
  dst_requestInfo &= ~Uint32(1 << RI_INTERPRETED_SHIFT);
  LqhKeyReq::setInterpretedFlag(dst_requestInfo, interpretedFlag);

  ndbassert(noDiskFlag == 1 || LqhKeyReq::getNoDiskFlag(dst_requestInfo) == 0);

  dst->hashValue = hashValue;
  dst->requestInfo = dst_requestInfo;
  dst->fragmentData = fragId;
  dst->attrLen = attrLen; // fragdist is in here

  treeNodePtr.p->m_send.m_keyInfoPtrI = keyInfo.i;

  lookup_send(signal, requestPtr, treeNodePtr);
}

void
Dbspj::lookup_send(Signal* signal,
		   Ptr<Request> requestPtr,
		   Ptr<TreeNode> treeNodePtr)
{
  jam();

  if (treeNodePtr.p->m_state == TreeNode::TN_INACTIVE)
  {
    jam();
    treeNodePtr.p->m_state = TreeNode::TN_ACTIVE;
    requestPtr.p->m_cnt_active++;
  }

  LqhKeyReq* req = reinterpret_cast<LqhKeyReq*>(signal->getDataPtrSend());

  memcpy(req, treeNodePtr.p->m_lookup_data.m_lqhKeyReq,
	 sizeof(treeNodePtr.p->m_lookup_data.m_lqhKeyReq));
  req->variableData[2] = requestPtr.p->m_rootResultData;
  req->variableData[3] = treeNodePtr.p->m_send.m_correlation;

  if (!(requestPtr.p->isLookup() && treeNodePtr.p->isLeaf()))
  {
    // Non-LEAF want reply to SPJ instead of ApiClient.
    LqhKeyReq::setNormalProtocolFlag(req->requestInfo, 1);
    req->variableData[0] = reference();
    req->variableData[1] = treeNodePtr.i;
  }

  SectionHandle handle(this);

  Uint32 ref = treeNodePtr.p->m_send.m_ref;
  Uint32 keyInfoPtrI = treeNodePtr.p->m_send.m_keyInfoPtrI;
  Uint32 attrInfoPtrI = treeNodePtr.p->m_send.m_attrInfoPtrI;

  if (treeNodePtr.p->m_bits & TreeNode::T_ONE_SHOT)
  {
    jam();
    /**
     * Pass sections to send
     */
    treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
    treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
  }
  else
  {
    if ((treeNodePtr.p->m_bits & TreeNode::T_KEYINFO_CONSTRUCTED) == 0)
    {
      jam();
      Uint32 tmp = RNIL;
      ndbrequire(dupSection(tmp, keyInfoPtrI)); // TODO handle error
      keyInfoPtrI = tmp;
    }
    else
    {
      jam();
      treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
    }

    if ((treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) == 0)
    {
      jam();
      Uint32 tmp = RNIL;
      ndbrequire(dupSection(tmp, attrInfoPtrI)); // TODO handle error
      attrInfoPtrI = tmp;
    }
    else
    {
      jam();
      treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
    }
  }

  getSection(handle.m_ptr[0], keyInfoPtrI);
  getSection(handle.m_ptr[1], attrInfoPtrI);
  handle.m_cnt = 2;

#ifdef DEBUG_LQHKEYREQ
  ndbout_c("LQHKEYREQ to %x", ref);
  printLQHKEYREQ(stdout, signal->getDataPtrSend(),
		 NDB_ARRAY_SIZE(treeNodePtr.p->m_lookup_data.m_lqhKeyReq),
                 DBLQH);
  printf("KEYINFO: ");
  print(handle.m_ptr[0], stdout);
  printf("ATTRINFO: ");
  print(handle.m_ptr[1], stdout);
#endif

  sendSignal(ref, GSN_LQHKEYREQ, signal,
	     NDB_ARRAY_SIZE(treeNodePtr.p->m_lookup_data.m_lqhKeyReq),
             JBB, &handle);

  Uint32 add = 2;
  if (treeNodePtr.p->isLeaf())
  {
    jam();
    /** Lookup queries leaf nodes should not reply to SPJ */ 
    add = requestPtr.p->isLookup() ? 0 : 1;
  }
  treeNodePtr.p->m_lookup_data.m_outstanding += add;

  const Ptr<TreeNode> root = getRoot(requestPtr.p->m_nodes);
  (this->*(root.p->m_info->m_count_descendant_signal))(NULL,
                                                       requestPtr,
                                                       treeNodePtr,
                                                       root,
                                                       GSN_LQHKEYREQ);

  /** Lookup leaf-request may finish immediately - LQH reply directly to API */ 
  if (treeNodePtr.p->m_lookup_data.m_outstanding == 0)
  {
    jam();
    ndbrequire(requestPtr.p->isLookup());
    ndbrequire(treeNodePtr.p->isLeaf());
    nodeFinished(signal, requestPtr, treeNodePtr);
  }
}

void
Dbspj::lookup_execTRANSID_AI(Signal* signal,
			     Ptr<Request> requestPtr,
			     Ptr<TreeNode> treeNodePtr,
			     const RowRef & rowRef)
{
  jam();

  {
    LocalArenaPoolImpl pool(requestPtr.p->m_arena, m_dependency_map_pool);
    Local_dependency_map list(pool, treeNodePtr.p->m_dependent_nodes);
    Dependency_map::ConstDataBufferIterator it;
    for (list.first(it); !it.isNull(); list.next(it))
    {
      jam();
      Ptr<TreeNode> childPtr;
      m_treenode_pool.getPtr(childPtr, * it.data);
      ndbrequire(childPtr.p->m_info != 0&&childPtr.p->m_info->m_start_child!=0);
      (this->*(childPtr.p->m_info->m_start_child))(signal,
                                                   requestPtr, childPtr,rowRef);
    }
  }
  ndbrequire(!(requestPtr.p->isLookup() && treeNodePtr.p->isLeaf()));
  ndbrequire(treeNodePtr.p->m_lookup_data.m_outstanding);
  treeNodePtr.p->m_lookup_data.m_outstanding --;

  const Ptr<TreeNode> root = getRoot(requestPtr.p->m_nodes);
  (this->*(root.p->m_info->m_count_descendant_signal))(signal,
                                                       requestPtr,
                                                       treeNodePtr,
                                                       root,
                                                       GSN_TRANSID_AI);
  if (treeNodePtr.p->m_lookup_data.m_outstanding == 0)
  {
    jam();
    nodeFinished(signal, requestPtr, treeNodePtr);
  }
}

void
Dbspj::lookup_execLQHKEYREF(Signal* signal,
                            Ptr<Request> requestPtr,
                            Ptr<TreeNode> treeNodePtr)
{
  if (requestPtr.p->isLookup())
  {
    /* CONF/REF not requested for lookup-Leaf: */
    ndbrequire(!treeNodePtr.p->isLeaf());

    /**
     * Scan-request does not need to
     *   send TCKEYREF...
     */
    const LqhKeyRef * rep = (LqhKeyRef*)signal->getDataPtr();
    Uint32 errCode = rep->errorCode;

    /**
     * Return back to api...
     *   NOTE: assume that signal is tampered with
     */
    Uint32 resultRef = treeNodePtr.p->m_lookup_data.m_api_resultRef;
    Uint32 resultData = treeNodePtr.p->m_lookup_data.m_api_resultData;
    Uint32 transId[2] = { requestPtr.p->m_transId[0],
                          requestPtr.p->m_transId[1] };
    TcKeyRef* ref = (TcKeyRef*)signal->getDataPtr();
    ref->connectPtr = resultData;
    ref->transId[0] = transId[0];
    ref->transId[1] = transId[1];
    ref->errorCode = errCode;
    ref->errorData = 0;

    DEBUG("lookup_execLQHKEYREF, errorCode:" << errCode);

    sendSignal(resultRef, GSN_TCKEYREF, signal,
               TcKeyRef::SignalLength, JBB);
  }

  Uint32 cnt = 2;
  if (treeNodePtr.p->isLeaf())  // Can't be a lookup-Leaf, asserted above
    cnt = 1;

  ndbrequire(treeNodePtr.p->m_lookup_data.m_outstanding >= cnt);
  treeNodePtr.p->m_lookup_data.m_outstanding -= cnt;

  const Ptr<TreeNode> root = getRoot(requestPtr.p->m_nodes);
  (this->*(root.p->m_info->m_count_descendant_signal))(signal,
                                                       requestPtr,
                                                       treeNodePtr,
                                                       root,
                                                       GSN_LQHKEYREF);
  if (treeNodePtr.p->m_lookup_data.m_outstanding == 0)
  {
    jam();
    nodeFinished(signal, requestPtr, treeNodePtr);
  }
}

void
Dbspj::lookup_execLQHKEYCONF(Signal* signal,
                             Ptr<Request> requestPtr,
                             Ptr<TreeNode> treeNodePtr)
{
  ndbrequire(!(requestPtr.p->isLookup() && treeNodePtr.p->isLeaf()));
  ndbrequire(treeNodePtr.p->m_lookup_data.m_outstanding);
  treeNodePtr.p->m_lookup_data.m_outstanding --;

  const Ptr<TreeNode> root = getRoot(requestPtr.p->m_nodes);
  (this->*(root.p->m_info->m_count_descendant_signal))(signal,
                                                       requestPtr,
                                                       treeNodePtr,
                                                       root,
                                                       GSN_LQHKEYCONF);

  if (treeNodePtr.p->m_lookup_data.m_outstanding == 0)
  {
    jam();
    nodeFinished(signal, requestPtr, treeNodePtr);
  }
}

void
Dbspj::lookup_start_child(Signal* signal,
                          Ptr<Request> requestPtr,
                          Ptr<TreeNode> treeNodePtr,
                          const RowRef & rowRef)
{
  /**
   * Here we need to...
   *   1) construct a key
   *   2) compute hash     (normally TC)
   *   3) get node for row (normally TC)
   */
  Uint32 err;
  const LqhKeyReq* src = (LqhKeyReq*)treeNodePtr.p->m_lookup_data.m_lqhKeyReq;
  const Uint32 tableId = LqhKeyReq::getTableId(src->tableSchemaVersion);
  const Uint32 corrVal = rowRef.m_src_correlation;

  DEBUG("::lookup_start_child");

  do
  {
    Uint32 ptrI = RNIL;
    if (treeNodePtr.p->m_bits & TreeNode::T_KEYINFO_CONSTRUCTED)
    {
      jam();
      DEBUG("start_child w/ T_KEYINFO_CONSTRUCTED");
      /**
       * Get key-pattern
       */
      LocalArenaPoolImpl pool(requestPtr.p->m_arena, m_dependency_map_pool);
      Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);

      err = expand(ptrI, pattern, rowRef.m_row_data.m_section);
      if (unlikely(err != 0))
        break;

      if (ptrI == RNIL)
      {
        jam();
        /**
         * We constructed a null-key...construct a zero-length key (even if we don't support it *now*)
         *
         *   (we actually did prior to joining mysql where null was treated as any other
         *   value in a key). But mysql treats null in unique key as *wildcard*
         *   which we don't support so well...and do nasty tricks in handler
         *
         * NOTE: should be *after* check for error
         */
        err = createEmptySection(ptrI);
        if (unlikely(err != 0))
          break;
      }

      treeNodePtr.p->m_send.m_keyInfoPtrI = ptrI;
    }

    BuildKeyReq tmp;
    err = computeHash(signal, tmp, tableId, ptrI);
    if (unlikely(err != 0))
      break;

    err = getNodes(signal, tmp, tableId);
    if (unlikely(err != 0))
      break;

    /**
     * Now send...
     */

    /**
     * TODO merge better with lookup_start (refactor)
     */
    {
      /* We set the upper half word of m_correlation to the tuple ID
       * of the parent, such that the API can match this tuple with its 
       * parent.
       * Then we re-use the tuple ID of the parent as the 
       * tuple ID for this tuple also. Since the tuple ID
       * is unique within this batch and SPJ block for the parent operation,
       * it must also be unique for this operation. 
       * This ensures that lookup operations with no user projection will 
       * work, since such operations will have the same tuple ID as their 
       * parents. The API will then be able to match a tuple with its 
       * grandparent, even if it gets no tuple for the parent operation.*/
      treeNodePtr.p->m_send.m_correlation = 
        (corrVal << 16) + (corrVal & 0xffff);

      treeNodePtr.p->m_send.m_ref = tmp.receiverRef;
      LqhKeyReq * dst = (LqhKeyReq*)treeNodePtr.p->m_lookup_data.m_lqhKeyReq;
      dst->hashValue = tmp.hashInfo[0];
      dst->fragmentData = tmp.fragId;
      Uint32 attrLen = 0;
      LqhKeyReq::setDistributionKey(attrLen, tmp.fragDistKey);
      dst->attrLen = attrLen;
      lookup_send(signal, requestPtr, treeNodePtr);
    }
    return;
  } while (0);

  ndbrequire(false);
}

void
Dbspj::lookup_cleanup(Ptr<Request> requestPtr,
                      Ptr<TreeNode> treeNodePtr)
{
  cleanup_common(requestPtr, treeNodePtr);
}


Uint32
Dbspj::handle_special_hash(Uint32 tableId, Uint32 dstHash[4],
                           const Uint64* src,
                           Uint32 srcLen,       // Len in #32bit words
                           const KeyDescriptor* desc)
{
  const Uint32 MAX_KEY_SIZE_IN_LONG_WORDS= 
    (MAX_KEY_SIZE_IN_WORDS + 1) / 2;
  Uint64 alignedWorkspace[MAX_KEY_SIZE_IN_LONG_WORDS * MAX_XFRM_MULTIPLY];
  const bool hasVarKeys = desc->noOfVarKeys > 0;
  const bool hasCharAttr = desc->hasCharAttr;
  const bool compute_distkey = desc->noOfDistrKeys > 0;
  
  const Uint64 *hashInput = 0;
  Uint32 inputLen = 0;
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 * keyPartLenPtr;

  /* Normalise KeyInfo into workspace if necessary */
  if (hasCharAttr || (compute_distkey && hasVarKeys))
  {
    hashInput = alignedWorkspace;
    keyPartLenPtr = keyPartLen;
    inputLen = xfrm_key(tableId, 
                        (Uint32*)src, 
                        (Uint32*)alignedWorkspace, 
                        sizeof(alignedWorkspace) >> 2, 
                        keyPartLenPtr);
    if (unlikely(inputLen == 0))
    {
      return 290;  // 'Corrupt key in TC, unable to xfrm'
    }
  } 
  else 
  {
    /* Keyinfo already suitable for hash */
    hashInput = src;
    inputLen = srcLen;
    keyPartLenPtr = 0;
  }
  
  /* Calculate primary key hash */
  md5_hash(dstHash, hashInput, inputLen);
  
  /* If the distribution key != primary key then we have to
   * form a distribution key from the primary key and calculate 
   * a separate distribution hash based on this
   */
  if (compute_distkey)
  {
    jam();
    
    Uint32 distrKeyHash[4];
    /* Reshuffle primary key columns to get just distribution key */
    Uint32 len = create_distr_key(tableId, (Uint32*)hashInput, (Uint32*)alignedWorkspace, keyPartLenPtr);
    /* Calculate distribution key hash */
    md5_hash(distrKeyHash, alignedWorkspace, len);

    /* Just one word used for distribution */
    dstHash[1] = distrKeyHash[1];
  }
  return 0;
}

Uint32
Dbspj::computeHash(Signal* signal,
		   BuildKeyReq& dst, Uint32 tableId, Uint32 ptrI)
{
  /**
   * Essentially the same code as in Dbtc::hash().
   * The code for user defined partitioning has been removed though.
   */
  SegmentedSectionPtr ptr;
  getSection(ptr, ptrI);

  /* NOTE:  md5_hash below require 64-bit alignment
   */
  const Uint32 MAX_KEY_SIZE_IN_LONG_WORDS=
    (MAX_KEY_SIZE_IN_WORDS + 1) / 2;
  Uint64 tmp64[MAX_KEY_SIZE_IN_LONG_WORDS];
  Uint32 *tmp32 = (Uint32*)tmp64;
  copy(tmp32, ptr);

  const KeyDescriptor* desc = g_key_descriptor_pool.getPtr(tableId);
  ndbrequire(desc != NULL);

  bool need_special_hash = desc->hasCharAttr | (desc->noOfDistrKeys > 0);
  if (need_special_hash)
  {
    jam();
    return handle_special_hash(tableId, dst.hashInfo, tmp64, ptr.sz, desc);
  }
  else
  {
    jam();
    md5_hash(dst.hashInfo, tmp64, ptr.sz);
    return 0;
  }
}

Uint32
Dbspj::getNodes(Signal* signal, BuildKeyReq& dst, Uint32 tableId)
{
  Uint32 err;
  DiGetNodesReq * req = (DiGetNodesReq *)&signal->theData[0];
  req->tableId = tableId;
  req->hashValue = dst.hashInfo[1];
  req->distr_key_indicator = 0; // userDefinedPartitioning not supported!

#if 1
  EXECUTE_DIRECT(DBDIH, GSN_DIGETNODESREQ, signal,
                 DiGetNodesReq::SignalLength);
#else
  sendSignal(DBDIH_REF, GSN_DIGETNODESREQ, signal,
             DiGetNodesReq::SignalLength, JBB);
  jamEntry();

#endif

  DiGetNodesConf * conf = (DiGetNodesConf *)&signal->theData[0];
  err = signal->theData[0];
  Uint32 Tdata2 = conf->reqinfo;
  Uint32 nodeId = conf->nodes[0];
  Uint32 instanceKey = (Tdata2 >> 24) & 127;

  DEBUG("HASH to nodeId:" << nodeId << ", instanceKey:" << instanceKey);

  jamEntry();
  if (unlikely(err != 0))
    goto error;

  dst.fragId = conf->fragId;
  dst.fragDistKey = (Tdata2 >> 16) & 255;
  dst.receiverRef = numberToRef(DBLQH, instanceKey, nodeId);

  return 0;

error:
  /**
   * TODO handle error
   */
  ndbrequire(false);
  return err;
}

/**
 * END - MODULE LOOKUP
 */

/**
 * MODULE SCAN FRAG
 */
const Dbspj::OpInfo
Dbspj::g_ScanFragOpInfo =
{
  &Dbspj::scanFrag_build,
  &Dbspj::scanFrag_start,
  &Dbspj::scanFrag_execTRANSID_AI,
  0, // execLQHKEYREF
  0, // execLQHKEYCONF
  &Dbspj::scanFrag_execSCAN_FRAGREF,
  &Dbspj::scanFrag_execSCAN_FRAGCONF,
  &Dbspj::scanFrag_start_child,
  &Dbspj::scanFrag_execSCAN_NEXTREQ,
  0, // Dbspj::scanFrag_complete
  0, // Dbspj::scanFrag_abort
  &Dbspj::scanFrag_cleanup,
  &Dbspj::scanFrag_count_descendant_signal
};

Uint32
Dbspj::scanFrag_build(Build_context& ctx,
		      Ptr<Request> requestPtr,
		      const QueryNode* qn,
		      const QueryNodeParameters* qp)
{
  Uint32 err = 0;
  Ptr<TreeNode> treeNodePtr;
  const QN_ScanFragNode * node = (const QN_ScanFragNode*)qn;
  const QN_ScanFragParameters * param = (const QN_ScanFragParameters*)qp;

  do
  {
    err = createNode(ctx, requestPtr, treeNodePtr);
    if (unlikely(err != 0))
      break;

    requestPtr.p->m_bits |= Request::RT_SCAN;
    treeNodePtr.p->m_info = &g_ScanFragOpInfo;
    treeNodePtr.p->m_bits |= TreeNode::T_ATTR_INTERPRETED;

    treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_IDLE;
    treeNodePtr.p->m_scanfrag_data.m_scan_status = 0;
    treeNodePtr.p->m_scanfrag_data.m_pending_close = false;

    ScanFragReq*dst=(ScanFragReq*)treeNodePtr.p->m_scanfrag_data.m_scanFragReq;
    dst->senderData = treeNodePtr.i;
    dst->resultRef = reference();
    dst->resultData = treeNodePtr.i;
    dst->savePointId = ctx.m_savepointId;

    Uint32 transId1 = requestPtr.p->m_transId[0];
    Uint32 transId2 = requestPtr.p->m_transId[1];
    dst->transId1 = transId1;
    dst->transId2 = transId2;

    Uint32 treeBits = node->requestInfo;
    Uint32 paramBits = param->requestInfo;
    //ndbout_c("Dbspj::scanFrag_build() treeBits=%.8x paramBits=%.8x", 
    //         treeBits, paramBits);
    Uint32 requestInfo = 0;
    ScanFragReq::setReadCommittedFlag(requestInfo, 1);
    ScanFragReq::setScanPrio(requestInfo, ctx.m_scanPrio);
    ScanFragReq::setAnyValueFlag(requestInfo, 1);
    ScanFragReq::setNoDiskFlag(requestInfo, 
                               (treeBits & DABits::NI_LINKED_DISK) == 0 &&
                               (paramBits & DABits::PI_DISK_ATTR) == 0);

#if 0
    static void setDescendingFlag(Uint32 & requestInfo, Uint32 descending);
    static void setTupScanFlag(Uint32 & requestInfo, Uint32 tupScan);
    static void setAttrLen(Uint32 & requestInfo, Uint32 attrLen);
    static void setScanPrio(Uint32& requestInfo, Uint32 prio);
    static void setLcpScanFlag(Uint32 & requestInfo, Uint32 val);

    static void setReorgFlag(Uint32 & requestInfo, Uint32 val);
    static Uint32 getReorgFlag(const Uint32 & requestInfo);
#endif
    dst->requestInfo = requestInfo;

    err = DbspjErr::InvalidTreeNodeSpecification;
    DEBUG("scanFrag_build: len=" << node->len);
    if (unlikely(node->len < QN_ScanFragNode::NodeSize))
      break;

    dst->tableId = node->tableId;
    dst->schemaVersion = node->tableVersion;

    err = DbspjErr::InvalidTreeParametersSpecification;
    DEBUG("param len: " << param->len);
    if (unlikely(param->len < QN_ScanFragParameters::NodeSize))
    {
      jam();
      DEBUG_CRASH();
      break;
    }

    ctx.m_resultData = param->resultData;

    /**
     * Parse stuff common lookup/scan-frag
     */
    struct DABuffer nodeDA, paramDA;
    nodeDA.ptr = node->optional;
    nodeDA.end = nodeDA.ptr + (node->len - QN_ScanFragNode::NodeSize);
    paramDA.ptr = param->optional;
    paramDA.end = paramDA.ptr + (param->len - QN_ScanFragParameters::NodeSize);
    err = parseDA(ctx, requestPtr, treeNodePtr,
                  nodeDA, treeBits, paramDA, paramBits);
    if (unlikely(err != 0))
    {
      jam();
      DEBUG_CRASH();
      break;
    }

    treeNodePtr.p->m_state = TreeNode::TN_INACTIVE;

    return 0;
  } while (0);

  return err;
}

void
Dbspj::scanFrag_start(Signal* signal,
		      Ptr<Request> requestPtr,
		      Ptr<TreeNode> treeNodePtr,
		      SegmentedSectionPtr keyInfo)
{
  const ScanFragReq* src = reinterpret_cast<const ScanFragReq*>(signal->getDataPtr());

#if NOT_YET
  Uint32 instanceNo = blockToInstance(signal->header.theReceiversBlockNumber);
  treeNodePtr.p->m_send.m_ref = numberToRef(DBLQH, instanceNo, getOwnNodeId());
#else
  treeNodePtr.p->m_send.m_ref = 
    numberToRef(DBLQH, getInstanceKey(src->tableId,
                                      src->fragmentNoKeyLen),
                getOwnNodeId());
#endif

  Uint32 fragId = src->fragmentNoKeyLen;
  Uint32 requestInfo = src->requestInfo;
  Uint32 batch_size_bytes = src->batch_size_bytes;
  Uint32 batch_size_rows = src->batch_size_rows;

#ifdef VM_TRACE
  Uint32 savePointId = src->savePointId;
  Uint32 tableId = src->tableId;
  Uint32 schemaVersion = src->schemaVersion;
  Uint32 transId1 = src->transId1;
  Uint32 transId2 = src->transId2;
#endif

  ndbassert(ScanFragReq::getLockMode(requestInfo) == 0);
  ndbassert(ScanFragReq::getHoldLockFlag(requestInfo) == 0);
  ndbassert(ScanFragReq::getKeyinfoFlag(requestInfo) == 0);
  ndbassert(ScanFragReq::getReadCommittedFlag(requestInfo) == 1);
  ndbassert(ScanFragReq::getLcpScanFlag(requestInfo) == 0);
  //ScanFragReq::getAttrLen(requestInfo); // ignore

#if TODO
  ndbassert(ScanFragReq::getReorgFlag(requestInfo) == 0);
  ndbassert(ScanFragReq::getDescendingFlag(requestInfo) == 0);
  ndbassert(ScanFragReq::getTupScanFlag(requestInfo) == 0);
  ndbassert(ScanFragReq::getScanPrio(requestInfo) == 1);
#endif

  ndbassert(ScanFragReq::getReorgFlag(requestInfo) == 0);
  Uint32 tupScanFlag = ScanFragReq::getTupScanFlag(requestInfo);
  Uint32 rangeScanFlag = ScanFragReq::getRangeScanFlag(requestInfo);
  Uint32 descendingFlag = ScanFragReq::getDescendingFlag(requestInfo);
  Uint32 noDiskFlag = ScanFragReq::getNoDiskFlag(requestInfo);
  Uint32 scanPrio = ScanFragReq::getScanPrio(requestInfo);

  ScanFragReq * dst =(ScanFragReq*)treeNodePtr.p->m_scanfrag_data.m_scanFragReq;
  Uint32 dst_requestInfo = dst->requestInfo;

  ScanFragReq::setTupScanFlag(dst_requestInfo,tupScanFlag);
  ScanFragReq::setRangeScanFlag(dst_requestInfo,rangeScanFlag);
  ScanFragReq::setDescendingFlag(dst_requestInfo,descendingFlag);
  ndbassert(noDiskFlag == 1 || 
            ScanFragReq::getNoDiskFlag(dst_requestInfo) == 0);
  ScanFragReq::setScanPrio(dst_requestInfo,scanPrio);

  dst->fragmentNoKeyLen = fragId;
  dst->requestInfo = dst_requestInfo;
  dst->batch_size_bytes = batch_size_bytes;
  dst->batch_size_rows = batch_size_rows;

#ifdef VM_TRACE
  ndbassert(dst->savePointId == savePointId);
  ndbassert(dst->tableId == tableId);
  ndbassert(dst->schemaVersion == schemaVersion);
  ndbassert(dst->transId1 == transId1);
  ndbassert(dst->transId2 == transId2);
#endif

  treeNodePtr.p->m_send.m_keyInfoPtrI = keyInfo.i;

  scanFrag_send(signal, requestPtr, treeNodePtr);
}

void
Dbspj::scanFrag_send(Signal* signal,
		     Ptr<Request> requestPtr,
		     Ptr<TreeNode> treeNodePtr)
{
  jam();

  ndbrequire(treeNodePtr.p->m_state == TreeNode::TN_INACTIVE);
  treeNodePtr.p->m_state = TreeNode::TN_ACTIVE;
  requestPtr.p->m_cnt_active++;

  ScanFragReq* req = reinterpret_cast<ScanFragReq*>(signal->getDataPtrSend());

  memcpy(req, treeNodePtr.p->m_scanfrag_data.m_scanFragReq,
	 sizeof(treeNodePtr.p->m_scanfrag_data.m_scanFragReq));
  req->variableData[0] = requestPtr.p->m_rootResultData;
  req->variableData[1] = treeNodePtr.p->m_send.m_correlation;

  SectionHandle handle(this);

  Uint32 ref = treeNodePtr.p->m_send.m_ref;
  Uint32 keyInfoPtrI = treeNodePtr.p->m_send.m_keyInfoPtrI;
  Uint32 attrInfoPtrI = treeNodePtr.p->m_send.m_attrInfoPtrI;

  if (treeNodePtr.p->m_bits & TreeNode::T_ONE_SHOT)
  {
    jam();
    /**
     * Pass sections to send
     */
    treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
    treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
  }
  else
  {
    if (keyInfoPtrI != RNIL)
    {
      jam();
      if ((treeNodePtr.p->m_bits & TreeNode::T_KEYINFO_CONSTRUCTED) == 0)
      {
        jam();
        Uint32 tmp = RNIL;
        ndbrequire(dupSection(tmp, keyInfoPtrI)); // TODO handle error
        keyInfoPtrI = tmp;
      }
      else
      {
        jam();
        treeNodePtr.p->m_send.m_keyInfoPtrI = RNIL;
      }
    }

    if ((treeNodePtr.p->m_bits & TreeNode::T_ATTRINFO_CONSTRUCTED) == 0)
    {
      jam();
      Uint32 tmp = RNIL;
      ndbrequire(dupSection(tmp, attrInfoPtrI)); // TODO handle error
      attrInfoPtrI = tmp;
    }
    else
    {
      jam();
      treeNodePtr.p->m_send.m_attrInfoPtrI = RNIL;
    }
  }

  getSection(handle.m_ptr[0], attrInfoPtrI);
  handle.m_cnt = 1;

  if (keyInfoPtrI != RNIL)
  {
    jam();
    getSection(handle.m_ptr[1], keyInfoPtrI);
    handle.m_cnt = 2;
  }

#ifdef DEBUG_SCAN_FRAGREQ
  ndbout_c("SCAN_FRAGREQ to %x", ref);
  printSCAN_FRAGREQ(stdout, signal->getDataPtrSend(),
                    NDB_ARRAY_SIZE(treeNodePtr.p->m_scanfrag_data.m_scanFragReq),
                    DBLQH);
  printf("ATTRINFO: ");
  print(handle.m_ptr[0], stdout);
  if (handle.m_cnt > 1)
  {
    printf("KEYINFO: ");
    print(handle.m_ptr[1], stdout);
  }
#endif

  sendSignal(ref, GSN_SCAN_FRAGREQ, signal,
	     NDB_ARRAY_SIZE(treeNodePtr.p->m_scanfrag_data.m_scanFragReq),
             JBB, &handle);

  ndbassert (!treeNodePtr.p->m_scanfrag_data.m_pending_close);
  treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_RUNNING;
  treeNodePtr.p->m_scanfrag_data.m_scan_status = 0;
  treeNodePtr.p->m_scanfrag_data.m_scan_fragconf_received = false;
  treeNodePtr.p->m_scanfrag_data.m_rows_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_rows_expecting = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_keyconfs_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_silent_keyconfs_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_keyrefs_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_keyreqs_sent = 0;
  treeNodePtr.p->m_scanfrag_data.m_missing_descendant_rows = 0;

  /**
   * Save position where next-scan-req should continue or close
   */
  treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_RUNNING;
  requestPtr.p->m_currentNodePtrI = treeNodePtr.i;
}

/** Return true if scan batch is complete. This happens when all scan 
 * rows and all results for descendant lookups have been received.*/
static bool isScanComplete(const Dbspj::ScanFragData& scanFragData)
{
  return scanFragData.m_scan_fragconf_received &&
    // All rows for root scan received.
    scanFragData.m_rows_received == scanFragData.m_rows_expecting &&
    // All rows for descendant lookups received.
    scanFragData.m_missing_descendant_rows == 0 &&
    // All descendant lookup operations are complete.
    scanFragData.m_descendant_keyreqs_sent == 
    scanFragData.m_descendant_keyconfs_received + 
    scanFragData.m_descendant_silent_keyconfs_received + 
    scanFragData.m_descendant_keyrefs_received;
}

void
Dbspj::scanFrag_execTRANSID_AI(Signal* signal,
			       Ptr<Request> requestPtr,
			       Ptr<TreeNode> treeNodePtr,
			       const RowRef & rowRef)
{
  jam();
  treeNodePtr.p->m_scanfrag_data.m_rows_received++;

  {
    LocalArenaPoolImpl pool(requestPtr.p->m_arena, m_dependency_map_pool);
    Local_dependency_map list(pool, treeNodePtr.p->m_dependent_nodes);
    Dependency_map::ConstDataBufferIterator it;
    for (list.first(it); !it.isNull(); list.next(it))
    {
      jam();
      Ptr<TreeNode> childPtr;
      m_treenode_pool.getPtr(childPtr, * it.data);
      ndbrequire(childPtr.p->m_info != 0&&childPtr.p->m_info->m_start_child!=0);
      (this->*(childPtr.p->m_info->m_start_child))(signal,
                                                   requestPtr, childPtr,rowRef);
    }
  }

  if (isScanComplete(treeNodePtr.p->m_scanfrag_data))
  {
    jam();
    scanFrag_batch_complete(signal, requestPtr, treeNodePtr);
  }
}

void
Dbspj::scanFrag_execSCAN_FRAGREF(Signal* signal,
                                 Ptr<Request> requestPtr,
                                 Ptr<TreeNode> treeNodePtr)
{
  const ScanFragRef* const rep = reinterpret_cast<const ScanFragRef*>(signal->getDataPtr());
  Uint32 errCode = rep->errorCode;

  /**
   * Return back to api...
   *   NOTE: assume that signal is tampered with
   */
  ndbassert (rep->transId1 == requestPtr.p->m_transId[0]);
  ndbassert (rep->transId2 == requestPtr.p->m_transId[1]);

  DEBUG("scanFrag_execSCAN_FRAGREF, rep->senderData:" << rep->senderData
         << ", requestPtr.p->m_senderData:" << requestPtr.p->m_senderData);

  ScanFragRef* const ref = reinterpret_cast<ScanFragRef*>(signal->getDataPtrSend());

  ref->senderData = requestPtr.p->m_senderData;
  ref->errorCode = errCode;
  ref->transId1 = requestPtr.p->m_transId[0];
  ref->transId2 = requestPtr.p->m_transId[1];

  sendSignal(requestPtr.p->m_senderRef, GSN_SCAN_FRAGREF, signal,
	     ScanFragRef::SignalLength, JBB);

  treeNodePtr.p->m_scanfrag_data.m_scan_fragconf_received = true;
//treeNodePtr.p->m_scanfrag_data.m_scan_status = 2;  // (2=ZSCAN_FRAG_CLOSED)
  ndbassert (isScanComplete(treeNodePtr.p->m_scanfrag_data));

  /**
   * SCAN_FRAGREF implies that datanodes closed the cursor.
   *  -> Pending close is effectively a NOOP, reset it
   */
  if (treeNodePtr.p->m_scanfrag_data.m_pending_close)
  {
    jam();
    treeNodePtr.p->m_scanfrag_data.m_pending_close = false;
    DEBUG(" SCAN_FRAGREF, had pending close which can be ignored (is closed)");
  }

  /**
   * Cleanup operation on SPJ block, remove all allocated resources.
   */
  {
    jam();
    treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_IDLE;
    nodeFinished(signal, requestPtr, treeNodePtr);
  }
}


void
Dbspj::scanFrag_execSCAN_FRAGCONF(Signal* signal,
                                  Ptr<Request> requestPtr,
                                  Ptr<TreeNode> treeNodePtr)
{
  const ScanFragConf* conf = reinterpret_cast<const ScanFragConf*>(signal->getDataPtr());
  Uint32 rows = conf->completedOps;
  Uint32 done = conf->fragmentCompleted;

  ndbrequire(done <= 2); // 0, 1, 2 (=ZSCAN_FRAG_CLOSED)

  treeNodePtr.p->m_scanfrag_data.m_scan_status = done;
  treeNodePtr.p->m_scanfrag_data.m_rows_expecting = rows;
  if (treeNodePtr.p->isLeaf())
  {
    /**
     * If this is a leaf node, then no rows will be sent to the SPJ block,
     * as there are no child operations to instantiate.
     */
    treeNodePtr.p->m_scanfrag_data.m_rows_received = rows;
  }
  treeNodePtr.p->m_scanfrag_data.m_scan_fragconf_received = true;
  if (isScanComplete(treeNodePtr.p->m_scanfrag_data))
  {
    jam();
    scanFrag_batch_complete(signal, requestPtr, treeNodePtr);
  }
}

void
Dbspj::scanFrag_batch_complete(Signal* signal,
                               Ptr<Request> requestPtr,
                               Ptr<TreeNode> treeNodePtr)
{
  DEBUG("scanFrag_batch_complete()");

  if (treeNodePtr.p->m_scanfrag_data.m_pending_close)
  {
    jam();
    ndbrequire(treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_RUNNING);
    treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_STARTED;

    DEBUG("scanFrag_batch_complete() - has pending close, ignore this reply, request close");

    ScanFragNextReq* req = reinterpret_cast<ScanFragNextReq*>(signal->getDataPtrSend());

    /**
     * SCAN_NEXTREQ(close) was requested while we where waiting for 
     * datanodes to complete this request. 
     *   - Send close request to LQH now.
     *   - Suppress reply to TC/API, will reply later when close is conf'ed
     */
    req->closeFlag = ZTRUE;
    req->senderData = treeNodePtr.i;
    req->transId1 = requestPtr.p->m_transId[0];
    req->transId2 = requestPtr.p->m_transId[1];
    req->batch_size_rows = 0;
    req->batch_size_bytes = 0;

    treeNodePtr.p->m_scanfrag_data.m_pending_close = false;
    scanFrag_execSCAN_NEXTREQ(signal, requestPtr, treeNodePtr);
    return;
  }

  /**
   * one batch complete...
   *   if tree contains several scans...this is harder...
   *   but for now just reply to TC (and possibly cleanup)
   */
  ScanFragConf* conf = reinterpret_cast<ScanFragConf*>(signal->getDataPtrSend());
  conf->senderData = requestPtr.p->m_senderData;
  conf->transId1 = requestPtr.p->m_transId[0];
  conf->transId2 = requestPtr.p->m_transId[1];
  conf->completedOps = treeNodePtr.p->m_scanfrag_data.m_rows_expecting
    + treeNodePtr.p->m_scanfrag_data.m_descendant_keyconfs_received;
  conf->fragmentCompleted = treeNodePtr.p->m_scanfrag_data.m_scan_status;
  conf->total_len = 0; // Not supported...

  sendSignal(requestPtr.p->m_senderRef, GSN_SCAN_FRAGCONF, signal,
	     ScanFragConf::SignalLength, JBB);

  if (treeNodePtr.p->m_scanfrag_data.m_scan_status == 2)
  {
    jam();
    ndbrequire(treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_RUNNING ||
               treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_CLOSING);
    /**
     * EOF for scan
     */
    treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_IDLE;
    nodeFinished(signal, requestPtr, treeNodePtr);
  }
  else
  {
    jam();
    ndbrequire(treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_RUNNING);
    /**
     * Check position where next-scan-req should continue
     */
    treeNodePtr.p->m_scanfrag_data.m_scan_state = ScanFragData::SF_STARTED;
    assert(requestPtr.p->m_currentNodePtrI == treeNodePtr.i);
  }
}

void
Dbspj::scanFrag_start_child(Signal* signal,
                            Ptr<Request> requestPtr,
                            Ptr<TreeNode> treeNodePtr,
                            const RowRef & rowRef)
{
  jam();
  ndbrequire(false);
}

void
Dbspj::scanFrag_execSCAN_NEXTREQ(Signal* signal, 
                                 Ptr<Request> requestPtr,
                                 Ptr<TreeNode> treeNodePtr)
{
  jamEntry();
  ndbassert (treeNodePtr.p->m_scanfrag_data.m_scan_state == ScanFragData::SF_STARTED);

  ScanFragNextReq* nextReq = reinterpret_cast<ScanFragNextReq*>(signal->getDataPtrSend());
  nextReq->senderData = treeNodePtr.i;
  ndbassert (nextReq->transId1 == requestPtr.p->m_transId[0]);
  ndbassert (nextReq->transId2 == requestPtr.p->m_transId[1]);

  DEBUG("scanFrag_execSCAN_NEXTREQ to: " << treeNodePtr.p->m_send.m_ref
      << ", senderData: " << nextReq->senderData);

  sendSignal(treeNodePtr.p->m_send.m_ref, 
             GSN_SCAN_NEXTREQ, 
             signal, 
             ScanFragNextReq::SignalLength, 
             JBB);

  treeNodePtr.p->m_scanfrag_data.m_scan_state = (nextReq->closeFlag == ZTRUE)
    ? ScanFragData::SF_CLOSING 
    : ScanFragData::SF_RUNNING;

  treeNodePtr.p->m_scanfrag_data.m_scan_status = 0;
  treeNodePtr.p->m_scanfrag_data.m_scan_fragconf_received = false;
  treeNodePtr.p->m_scanfrag_data.m_rows_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_rows_expecting = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_keyconfs_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_silent_keyconfs_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_keyrefs_received = 0;
  treeNodePtr.p->m_scanfrag_data.m_descendant_keyreqs_sent = 0;
  treeNodePtr.p->m_scanfrag_data.m_missing_descendant_rows = 0;
}//Dbspj::scanFrag_execSCAN_NEXTREQ()


void
Dbspj::scanFrag_cleanup(Ptr<Request> requestPtr,
                        Ptr<TreeNode> treeNodePtr)
{
  cleanup_common(requestPtr, treeNodePtr);
}

void
Dbspj::scanFrag_count_descendant_signal(Signal* signal,
                                        Ptr<Request> requestPtr,
                                        Ptr<TreeNode> treeNodePtr,
                                        Ptr<TreeNode> rootPtr,
                                        Uint32 globalSignalNo)
{
  const bool trace = false;

  switch(globalSignalNo){
  case GSN_TRANSID_AI:
    rootPtr.p->m_scanfrag_data.m_missing_descendant_rows--;
    if (trace)
    {
      ndbout << "Dbspj::scanFrag_count_descendant_signal() decremented "
        "m_scanfrag_data.m_missing_descendant_rows to "<< 
        rootPtr.p->m_scanfrag_data.m_missing_descendant_rows << endl;
    }
    break;
  case GSN_LQHKEYCONF:
    jam();
    if (treeNodePtr.p->m_bits & TreeNode::T_USER_PROJECTION)
    {
      rootPtr.p->m_scanfrag_data.m_descendant_keyconfs_received++;
      if (trace)
      {
        ndbout << "Dbspj::scanFrag_count_descendant_signal() incremented "
          "m_scanfrag_data.m_descendant_keyconfs_received to "<< 
          rootPtr.p->m_scanfrag_data.m_descendant_keyconfs_received << endl;
      }
    }
    else
    {
      /* There is no user projection. Typically, this will be the operation
       * that retrieves an index tuple as part of an index lookup operation.
       * (Only the base table tuple will then be sent to the API.)*/
      rootPtr.p->m_scanfrag_data.m_descendant_silent_keyconfs_received++;
      if (trace)
      {
        ndbout << "Dbspj::scanFrag_count_descendant_signal() incremented "
          "m_scanfrag_data.m_descendant_silent_keyconfs_received to "
               << rootPtr.p->m_scanfrag_data.
          m_descendant_silent_keyconfs_received 
               << endl;
      }
    }
    // Check if this is a non-leaf.
    if (treeNodePtr.p->m_dependent_nodes.firstItem!=RNIL)
    {
      /* Since this is a non-leaf, the SPJ block should also receive
       * a TRANSID_AI message for this operation.*/
      rootPtr.p->m_scanfrag_data.m_missing_descendant_rows++;
      if (trace)
      {
        ndbout << "Dbspj::scanFrag_count_descendant_signal() incremented "
          "m_scanfrag_data.m_missing_descendant_rows to "<< 
          rootPtr.p->m_scanfrag_data.m_missing_descendant_rows << endl;
      }
    }
    break;
  case GSN_LQHKEYREF:
    jam();
    rootPtr.p->m_scanfrag_data.m_descendant_keyrefs_received++;
    if (trace)
    {
      ndbout << "Dbspj::scanFrag_count_descendant_signal() incremented "
        "m_scanfrag_data.m_descendant_keyrefs_received to "<< 
        rootPtr.p->m_scanfrag_data.m_descendant_keyrefs_received << endl;
    }
    break;
  case GSN_LQHKEYREQ:
    jam();
    rootPtr.p->m_scanfrag_data.m_descendant_keyreqs_sent++;
    if (trace)
    {
      ndbout << "Dbspj::scanFrag_count_descendant_signal() incremented "
        "m_scanfrag_data.m_descendant_keyreqs_sent to "<< 
        rootPtr.p->m_scanfrag_data.m_descendant_keyreqs_sent << endl;
    }
    break;
  default:
    jam();
    ndbrequire(false);
  }
  if (isScanComplete(rootPtr.p->m_scanfrag_data))
  {
    jam();
    ndbrequire(globalSignalNo!=GSN_LQHKEYREQ);
    scanFrag_batch_complete(signal, requestPtr, rootPtr);
  }
}

/**
 * END - MODULE SCAN FRAG
 */

/**
 * Static OpInfo handling
 */
const Dbspj::OpInfo*
Dbspj::getOpInfo(Uint32 op)
{
  DEBUG("getOpInfo(" << op << ")");
  switch(op){
  case QueryNode::QN_LOOKUP:
    return &Dbspj::g_LookupOpInfo;
  case QueryNode::QN_SCAN_FRAG:
    return &Dbspj::g_ScanFragOpInfo;
  default:
    return 0;
  }
}

/**
 * MODULE COMMON PARSE/UNPACK
 */

/**
 *  @returns dstLen + 1 on error
 */
static
Uint32
unpackList(Uint32 dstLen, Uint32 * dst, Dbspj::DABuffer & buffer)
{
  const Uint32 * ptr = buffer.ptr;
  if (likely(ptr != buffer.end))
  {
    Uint32 tmp = * ptr++;
    Uint32 cnt = tmp & 0xFFFF;

    * dst ++ = (tmp >> 16); // Store first
    DEBUG("cnt: " << cnt << " first: " << (tmp >> 16));

    if (cnt > 1)
    {
      Uint32 len = cnt / 2;
      if (unlikely(cnt >= dstLen || (ptr + len > buffer.end)))
        goto error;

      cnt --; // subtract item stored in header

      for (Uint32 i = 0; i < cnt/2; i++)
      {
        * dst++ = (* ptr) & 0xFFFF;
        * dst++ = (* ptr) >> 16;
        ptr++;
      }

      if (cnt & 1)
      {
        * dst ++ = * ptr & 0xFFFF;
        ptr++;
      }

      cnt ++; // readd item stored in header
    }
    buffer.ptr = ptr;
    return cnt;
  }
  return 0;

error:
  return dstLen + 1;
}

/**
 * This fuctions takes an array of attrinfo, and builds "header"
 *   which can be used to do random access inside the row
 */
Uint32
Dbspj::buildRowHeader(RowRef::Header * header, SegmentedSectionPtr ptr)
{
  Uint32 tmp, len;
  Uint32 * dst = (Uint32*)header->m_headers;
  const Uint32 * const save = dst;
  SectionReader r0(ptr, getSectionSegmentPool());
  do
  {
    r0.getWord(&tmp);
    len = AttributeHeader::getDataSize(tmp);
    * dst++ = tmp;
  } while (r0.step(len));

  return header->m_len = (dst - save);
}

/**
 * This fuctions takes an array of attrinfo, and builds "header"
 *   which can be used to do random access inside the row
 */
Uint32
Dbspj::buildRowHeader(RowRef::Header * header, const Uint32 *& src, Uint32 len)
{
  Uint32 * dst = (Uint32*)header->m_headers;
  const Uint32 * save = dst;
  for (Uint32 i = 0; i<len; i++)
  {
    Uint32 tmp = * src++;
    Uint32 tmp_len = AttributeHeader::getDataSize(tmp);
    * dst++ = tmp;
    src += tmp_len;
  }

  return header->m_len = (dst - save);
}

Uint32
Dbspj::appendToPattern(Local_pattern_store & pattern,
                       DABuffer & tree, Uint32 len)
{
  if (unlikely(tree.ptr + len > tree.end))
    return DbspjErr::InvalidTreeNodeSpecification;

  if (unlikely(pattern.append(tree.ptr, len)==0))
    return  DbspjErr::OutOfQueryMemory;

  tree.ptr += len;
  return 0;
}

Uint32
Dbspj::appendColToPattern(Local_pattern_store& dst,
                          const RowRef::Linear & row, Uint32 col)
{
  /**
   * TODO handle errors
   */
  const Uint32 * header = (const Uint32*)row.m_header->m_headers;
  Uint32 offset = 0;
  for (Uint32 i = 0; i<col; i++)
  {
    offset += 1 + AttributeHeader::getDataSize(* header++);
  }
  const Uint32 * ptr = row.m_data + offset;
  Uint32 len = AttributeHeader::getDataSize(* ptr ++);
  /* Param COL's converted to DATA when appended to pattern */
  Uint32 info = QueryPattern::data(len);
  return dst.append(&info,1) && dst.append(ptr,len) ? 0 : DbspjErr::OutOfQueryMemory;
}

Uint32
Dbspj::appendTreeToSection(Uint32 & ptrI, SectionReader & tree, Uint32 len)
{
  /**
   * TODO handle errors
   */
  Uint32 SZ = 16;
  Uint32 tmp[16];
  while (len > SZ)
  {
    jam();
    tree.getWords(tmp, SZ);
    ndbrequire(appendToSection(ptrI, tmp, SZ));
    len -= SZ;
  }

  tree.getWords(tmp, len);
  return appendToSection(ptrI, tmp, len) ? 0 : /** todo error code */ 1;
#if TODO
err:
  return 1;
#endif
}

void
Dbspj::getCorrelationData(const RowRef::Section & row, 
                          Uint32 col,
                          Uint32& rootStreamId,
                          Uint32& correlationNumber)
{
  /**
   * TODO handle errors
   */
  const Uint32 * header = (const Uint32*)row.m_header->m_headers;
  SegmentedSectionPtr ptr(row.m_dataPtr);
  SectionReader reader(ptr, getSectionSegmentPool());
  Uint32 offset = 0;
  for (Uint32 i = 0; i<col; i++)
  {
    offset += 1 + AttributeHeader::getDataSize(* header++);
  }
  ndbrequire(reader.step(offset));
  Uint32 tmp;
  ndbrequire(reader.getWord(&tmp));
  Uint32 len = AttributeHeader::getDataSize(tmp);
  ndbrequire(len == 2);
  ndbrequire(reader.getWord(&rootStreamId));
  ndbrequire(reader.getWord(&correlationNumber));
}

Uint32
Dbspj::appendColToSection(Uint32 & dst, const RowRef::Section & row, Uint32 col)
{
  /**
   * TODO handle errors
   */
  const Uint32 * header = (const Uint32*)row.m_header->m_headers;
  SegmentedSectionPtr ptr(row.m_dataPtr);
  SectionReader reader(ptr, getSectionSegmentPool());
  Uint32 offset = 0;
  for (Uint32 i = 0; i<col; i++)
  {
    offset += 1 + AttributeHeader::getDataSize(* header++);
  }
  ndbrequire(reader.step(offset));
  Uint32 tmp;
  ndbrequire(reader.getWord(&tmp));
  Uint32 len = AttributeHeader::getDataSize(tmp);
  return appendTreeToSection(dst, reader, len);
}

Uint32
Dbspj::appendColToSection(Uint32 & dst, const RowRef::Linear & row, Uint32 col)
{
  /**
   * TODO handle errors
   */
  const Uint32 * header = (const Uint32*)row.m_header->m_headers;
  Uint32 offset = 0;
  for (Uint32 i = 0; i<col; i++)
  {
    offset += 1 + AttributeHeader::getDataSize(* header++);
  }
  const Uint32 * ptr = row.m_data + offset;
  Uint32 len = AttributeHeader::getDataSize(* ptr ++);
  return appendToSection(dst, ptr, len) ? 0 : DbspjErr::InvalidPattern;
}

/**
 * 'PkCol' is the composite NDB$PK column in an unique index consisting of
 * a fragment id and the composite PK value (all PK columns concatenated)
 */
Uint32
Dbspj::appendPkColToSection(Uint32 & dst, const RowRef::Section & row, Uint32 col)
{
  /**
   * TODO handle errors
   */
  const Uint32 * header = (const Uint32*)row.m_header->m_headers;
  SegmentedSectionPtr ptr(row.m_dataPtr);
  SectionReader reader(ptr, getSectionSegmentPool());
  Uint32 offset = 0;
  for (Uint32 i = 0; i<col; i++)
  {
    offset += 1 + AttributeHeader::getDataSize(* header++);
  }
  ndbrequire(reader.step(offset));
  Uint32 tmp;
  ndbrequire(reader.getWord(&tmp));
  Uint32 len = AttributeHeader::getDataSize(tmp);
  ndbrequire(reader.step(1)); // Skip fragid
  return appendTreeToSection(dst, reader, len-1);
}

Uint32
Dbspj::appendDataToSection(Uint32 & ptrI,
                           Local_pattern_store& pattern,
			   Local_pattern_store::ConstDataBufferIterator& it,
			   Uint32 len)
{

#if 0
  /**
   * TODO handle errors
   */
  Uint32 tmp[NDB_SECTION_SEGMENT_SZ];
  while (len > NDB_SECTION_SEGMENT_SZ)
  {
    pattern.copyout(tmp, NDB_SECTION_SEGMENT_SZ, it);
    appendToSection(ptrI, tmp, NDB_SECTION_SEGMENT_SZ);
    len -= NDB_SECTION_SEGMENT_SZ;
  }

  pattern.copyout(tmp, len, it);
  appendToSection(ptrI, tmp, len);
  return 0;
#else
  Uint32 remaining = len;
  Uint32 dstIdx = 0;
  Uint32 tmp[NDB_SECTION_SEGMENT_SZ];
  
  while (remaining > 0 && !it.isNull())
  {
    tmp[dstIdx] = *it.data;
    remaining--;
    dstIdx++;
    pattern.next(it);
    if (dstIdx == NDB_SECTION_SEGMENT_SZ || remaining == 0)
    {
      if (!appendToSection(ptrI, tmp, dstIdx))
      {
	DEBUG_CRASH();
	return DbspjErr::InvalidPattern;
      }
      dstIdx = 0;
    }
  }
  if (remaining > 0)
  {
    DEBUG_CRASH();
    return DbspjErr::InvalidPattern;
  }
  else
  {
    return 0;
  }
#endif
}

Uint32
Dbspj::zeroFill(Uint32 & dst, Uint32 cnt)
{
  Uint32 tmp[NDB_SECTION_SEGMENT_SZ];
  bzero(tmp, sizeof(tmp));
  while (cnt > NDB_SECTION_SEGMENT_SZ)
  {
    if (unlikely(appendToSection(dst, tmp, NDB_SECTION_SEGMENT_SZ) == false))
      goto error;
    cnt -= NDB_SECTION_SEGMENT_SZ;
  }

  if (unlikely(appendToSection(dst, tmp, cnt) == false))
    goto error;

  return 0;

error:
  jam();
  return DbspjErr::OutOfSectionMemory;
}

Uint32
Dbspj::createEmptySection(Uint32 & dst)
{
  Uint32 tmp;
  SegmentedSectionPtr ptr;
  if (likely(import(ptr, &tmp, 0)))
  {
    jam();
    dst = ptr.i;
    return 0;
  }

  jam();
  return DbspjErr::OutOfSectionMemory;
}

const Ptr<Dbspj::TreeNode> 
Dbspj::getRoot(TreeNode_list::Head& head)
{
  //assert(rootNode->m_magic==TreeNode::MAGIC);
  Ptr<TreeNode> rootPtr;
  const Local_TreeNode_list list(m_treenode_pool, head);
  const bool found = list.first(rootPtr); 
  ndbassert(found);
  ndbassert(!rootPtr.isNull());
  ndbassert(rootPtr.p->m_node_no==0);
  return rootPtr;
}

/**
 * This function takes a pattern and a row and expands it into a section
 */
Uint32
Dbspj::expand(Uint32 & _dst, Local_pattern_store& pattern,
              const RowRef::Section & row)
{
  Uint32 err;
  Uint32 dst = _dst;
  Local_pattern_store::ConstDataBufferIterator it;
  pattern.first(it);
  while (!it.isNull())
  {
    Uint32 info = *it.data;
    Uint32 type = QueryPattern::getType(info);
    Uint32 val = QueryPattern::getLength(info);
    pattern.next(it);
    switch(type){
    case QueryPattern::P_COL:
      jam();
      err = appendColToSection(dst, row, val);
      break;
    case QueryPattern::P_UNQ_PK:
      jam();
      err = appendPkColToSection(dst, row, val);
      break;
    case QueryPattern::P_DATA:
      jam();
      err = appendDataToSection(dst, pattern, it, val);
      break;
    // PARAM's converted to DATA by ::expand(pattern...)
    case QueryPattern::P_PARAM:
    default:
      jam();
      err = DbspjErr::InvalidPattern;
      DEBUG_CRASH();
    }
    if (unlikely(err != 0))
    {
      jam();
      DEBUG_CRASH();
      goto error;
    }
  }

  _dst = dst;
  return 0;
error:
  jam();
  return err;
}

Uint32
Dbspj::expand(Uint32 & ptrI, DABuffer& pattern, Uint32 len,
              DABuffer& param, Uint32 paramCnt)
{
  /**
   * TODO handle error
   */
  Uint32 err;
  Uint32 tmp[1+MAX_ATTRIBUTES_IN_TABLE];
  struct RowRef::Linear row;
  row.m_data = param.ptr;
  row.m_header = (RowRef::Header*)tmp;
  buildRowHeader((RowRef::Header*)tmp, param.ptr, paramCnt);

  Uint32 dst = ptrI;
  const Uint32 * ptr = pattern.ptr;
  const Uint32 * end = ptr + len;

  for (; ptr < end; )
  {
    Uint32 info = * ptr++;
    Uint32 type = QueryPattern::getType(info);
    Uint32 val = QueryPattern::getLength(info);
    switch(type){
    case QueryPattern::P_PARAM:
      jam();
      ndbassert(val < paramCnt);
      err = appendColToSection(dst, row, val);
      break;
    case QueryPattern::P_DATA:
      if (likely(appendToSection(dst, ptr, val)))
      {
        jam();
	err = 0;
      }
      else
      {
        jam();
        err = DbspjErr::InvalidPattern;
      }
      ptr += val;
      break;
    case QueryPattern::P_COL: // (linked) COL's not expected here
    case QueryPattern::P_UNQ_PK:
    default:
      jam();
      err = DbspjErr::InvalidPattern;
      DEBUG_CRASH();
    }
    if (unlikely(err != 0))
    {
      jam();
      DEBUG_CRASH();
      goto error;
    }
  }

  /**
   * Iterate forward
   */
  pattern.ptr = end;

error:
  jam();
  ptrI = dst;
  return err;
}

Uint32
Dbspj::expand(Local_pattern_store& dst, DABuffer& pattern, Uint32 len,
              DABuffer& param, Uint32 paramCnt)
{
  /**
   * TODO handle error
   */
  /**
   * Optimization: If no params in key, const + linked col are 
   * transfered unaltered to the pattern to be parsed when the
   * parent source of the linked columns are available.
   */
  if (paramCnt == 0)
  {
    jam();
    return appendToPattern(dst, pattern, len);
  }

  Uint32 err;
  Uint32 tmp[1+MAX_ATTRIBUTES_IN_TABLE];
  struct RowRef::Linear row;
  row.m_header = (RowRef::Header*)tmp;
  row.m_data = param.ptr;
  buildRowHeader((RowRef::Header*)tmp, param.ptr, paramCnt);

  const Uint32 * end = pattern.ptr + len;
  for (; pattern.ptr < end; )
  {
    Uint32 info = *pattern.ptr;
    Uint32 type = QueryPattern::getType(info);
    Uint32 val = QueryPattern::getLength(info);
    switch(type){
    case QueryPattern::P_COL:
    case QueryPattern::P_UNQ_PK:
      jam();
      err = appendToPattern(dst, pattern, 1);
      break;
    case QueryPattern::P_DATA:
      jam();
      err = appendToPattern(dst, pattern, val+1);
      break;
    case QueryPattern::P_PARAM:
      jam();
      // NOTE: Converted to P_DATA by appendColToPattern
      ndbassert(val < paramCnt);
      err = appendColToPattern(dst, row, val);
      pattern.ptr++;
      break;
   default:
      jam();
      err = DbspjErr::InvalidPattern;
      DEBUG_CRASH();
    }

    if (unlikely(err != 0))
    {
      DEBUG_CRASH();
      goto error;
    }
  }
  return 0;

error:
  jam();
  return err;
}

Uint32
Dbspj::parseDA(Build_context& ctx,
               Ptr<Request> requestPtr,
               Ptr<TreeNode> treeNodePtr,
               DABuffer tree, Uint32 treeBits,
               DABuffer param, Uint32 paramBits)
{
  Uint32 err;
  Uint32 attrInfoPtrI = RNIL;
  Uint32 attrParamPtrI = RNIL;

  do
  {
    if (treeBits & DABits::NI_HAS_PARENT)
    {
      jam();
      DEBUG("NI_HAS_PARENT");
      /**
       * OPTIONAL PART 1:
       *
       * Parent nodes are stored first in optional part
       *   this is a list of 16-bit numbers refering to
       *   *earlier* nodes in tree
       *   the list stores length of list as first 16-bit
       */
      err = DbspjErr::InvalidTreeNodeSpecification;
      Uint32 dst[63];
      Uint32 cnt = unpackList(NDB_ARRAY_SIZE(dst), dst, tree);
      if (unlikely(cnt > NDB_ARRAY_SIZE(dst)))
      {
        DEBUG_CRASH();
        break;
      }

      err = 0;
      for (Uint32 i = 0; i<cnt; i++)
      {
        DEBUG("adding " << dst[i] << " as parent");
        Ptr<TreeNode> parentPtr = ctx.m_node_list[dst[i]];
        LocalArenaPoolImpl pool(requestPtr.p->m_arena, m_dependency_map_pool);
        Local_dependency_map map(pool, parentPtr.p->m_dependent_nodes);
        if (unlikely(!map.append(&treeNodePtr.i, 1)))
        {
          err = DbspjErr::OutOfQueryMemory;
          DEBUG_CRASH();
          break;
        }
        parentPtr.p->m_bits &= ~(Uint32)TreeNode::T_LEAF;
      }

      if (unlikely(err != 0))
        break;
    }

    ndbrequire(((treeBits  & DABits::NI_KEY_PARAMS)==0)
            == ((paramBits & DABits::PI_KEY_PARAMS)==0));

    if (treeBits & (DABits::NI_KEY_PARAMS 
		    | DABits::NI_KEY_LINKED 
		    | DABits::NI_KEY_CONSTS))
    {
      jam();
      DEBUG("NI_KEY_PARAMS | NI_KEY_LINKED | NI_KEY_CONSTS");

      /**
       * OPTIONAL PART 2:
       *
       * If keys are parametrized or linked
       *   DATA0[LO/HI] - Length of key pattern/#parameters to key
       */
      Uint32 len_cnt = * tree.ptr ++;
      Uint32 len = len_cnt & 0xFFFF; // length of pattern in words
      Uint32 cnt = len_cnt >> 16;    // no of parameters

      LocalArenaPoolImpl pool(requestPtr.p->m_arena, m_dependency_map_pool);
      Local_pattern_store pattern(pool, treeNodePtr.p->m_keyPattern);

      ndbrequire((cnt==0) == ((treeBits & DABits::NI_KEY_PARAMS) ==0));
      ndbrequire((cnt==0) == ((paramBits & DABits::PI_KEY_PARAMS)==0));

      if (treeBits & DABits::NI_KEY_LINKED)
      {
        jam();
        DEBUG("LINKED-KEY PATTERN w/ " << cnt << " PARAM values");
        /**
         * Expand pattern into a new pattern (with linked values)
         */
        err = expand(pattern, tree, len, param, cnt);

        /**
         * This node constructs a new key for each send
         */
        treeNodePtr.p->m_bits |= TreeNode::T_KEYINFO_CONSTRUCTED;
      }
      else
      {
        jam();
        DEBUG("FIXED-KEY w/ " << cnt << " PARAM values");
        /**
         * Expand pattern directly into keyinfo
         *   This means a "fixed" key from here on
         */
        Uint32 keyInfoPtrI = RNIL;
        err = expand(keyInfoPtrI, tree, len, param, cnt);
        treeNodePtr.p->m_send.m_keyInfoPtrI = keyInfoPtrI;
      }

      if (unlikely(err != 0))
      {
        DEBUG_CRASH();
        break;
      }
    }

    const Uint32 mask =
      DABits::NI_LINKED_ATTR | DABits::NI_ATTR_INTERPRET |
      DABits::NI_ATTR_LINKED | DABits::NI_ATTR_PARAMS;

    if (((treeBits & mask) | (paramBits & DABits::PI_ATTR_LIST)) != 0)
    {
      jam();
      /**
       * OPTIONAL PART 3: attrinfo handling
       * - NI_LINKED_ATTR - these are attributes to be passed to children
       * - PI_ATTR_LIST   - this is "user-columns" (passed as parameters)

       * - NI_ATTR_INTERPRET - tree contains interpreted program
       * - NI_ATTR_LINKED - means that the attr-info contains linked-values
       * - NI_ATTR_PARAMS - means that the attr-info is parameterized
       *   PI_ATTR_PARAMS - means that the parameters contains attr parameters
       *
       * IF NI_ATTR_INTERPRET
       *   DATA0[LO/HI] = Length of program / total #arguments to program
       *   DATA1..N     = Program
       *
       * IF NI_ATTR_PARAMS
       *   DATA0[LO/HI] = Length / #param
       *   DATA1..N     = PARAM-0...PARAM-M
       *
       * IF PI_ATTR_INTERPRET
       *   DATA0[LO] = Length of program
       *   DATA1..N     = Program (scan filter)
       *
       * IF NI_ATTR_LINKED
       *   DATA0[LO/HI] = Length / #
       *
       *
       */
      Uint32 sections[5] = { 0, 0, 0, 0, 0 };
      Uint32 * sectionptrs = 0;

      bool interpreted =
        (treeBits & DABits::NI_ATTR_INTERPRET) ||
        (treeNodePtr.p->m_bits & TreeNode::T_ATTR_INTERPRETED);

      if (interpreted)
      {
        /**
         * Add section headers for interpreted execution
         *   and create pointer so that they can be updated later
         */
        jam();
        err = DbspjErr::OutOfSectionMemory;
        if (unlikely(!appendToSection(attrInfoPtrI, sections, 5)))
        {
          DEBUG_CRASH();
          break;
        }

        SegmentedSectionPtr ptr;
        getSection(ptr, attrInfoPtrI);
        sectionptrs = ptr.p->theData;

        if (treeBits & DABits::NI_ATTR_INTERPRET)
        {
          jam();
          /** 
           * Having two interpreter programs is an error.
           */
          ndbrequire(!(paramBits & DABits::PI_ATTR_INTERPRET));
          Uint32 cnt_len = * tree.ptr++;
          Uint32 len = cnt_len & 0xFFFF; // Length of interpret program
          Uint32 cnt = cnt_len >> 16;    // #Arguments to program
          err = DbspjErr::OutOfSectionMemory;
          if (unlikely(!appendToSection(attrInfoPtrI, tree.ptr, len)))
          {
            DEBUG_CRASH();
            break;
          }

          tree.ptr += len;
          sectionptrs[1] = len; // size of interpret program

          if (cnt)
          {
            jam();
            err = DbspjErr::InvalidTreeNodeSpecification;
            const Uint32 check = DABits::NI_ATTR_PARAMS|DABits::NI_ATTR_LINKED;
            if (unlikely((treeBits & check) == 0))
            {
              /**
               * If program has arguments, it must either has
               *   DABits::NI_ATTR_PARAMS - i.e parameters
               *   DABits::NI_ATTR_LINKED - i.e linked
               */
              DEBUG_CRASH();
              break;
            }

            /**
             * Prepare directory for parameters
             */
            err = zeroFill(attrParamPtrI, cnt);
            if (unlikely(err != 0))
            {
              DEBUG_CRASH();
              break;
            }

            /**
             * TODO...continue here :-(
             */
            ndbrequire(false);
          }

          if (treeBits & DABits::NI_ATTR_LINKED)
          {
            jam();
            /**
             * The parameter section is recreated prior to each send...
             */
            treeNodePtr.p->m_bits |= TreeNode::T_ATTRINFO_CONSTRUCTED;
          }
        }
        else // if (treeBits & DABits::NI_ATTR_INTERPRET)
        {
          jam();
          /**
           * Only relevant for interpreted stuff
           */
          ndbrequire((treeBits & DABits::NI_ATTR_PARAMS) == 0);
          ndbrequire((paramBits & DABits::PI_ATTR_PARAMS) == 0);
          ndbrequire((treeBits & DABits::NI_ATTR_LINKED) == 0);

          ndbassert((treeNodePtr.p->m_bits & TreeNode::T_ATTR_INTERPRETED)!= 0);

          if (!(paramBits & DABits::PI_ATTR_INTERPRET)){
            jam();

            /**
             * Tree node has interpreted execution,
             *   but no interpreted program specified
             *   auto-add Exit_ok (i.e return each row)
             */
            Uint32 tmp = Interpreter::ExitOK();
            err = DbspjErr::OutOfSectionMemory;
            if (unlikely(!appendToSection(attrInfoPtrI, &tmp, 1)))
            {
              DEBUG_CRASH();
              break;
            }
            sectionptrs[1] = 1;
          }
        } // if (treeBits & DABits::NI_ATTR_INTERPRET)
      } // if (interpreted)

      if (paramBits & DABits::PI_ATTR_INTERPRET)
      {
        jam();
        
        /**
         * Add the interpreted code that represents the scan filter.
         */
        const Uint32 len = * param.ptr++;
        ndbassert(len <= 0xFFFF);
        ndbassert(len > 0);
        err = DbspjErr::OutOfSectionMemory;
        if (unlikely(!appendToSection(attrInfoPtrI, param.ptr, len)))
        {
          DEBUG_CRASH();
          break;
        }
        DEBUG("Dbspj::parseDA() adding interpreter program of " 
              << len << " words.");
        
        param.ptr += len;
        /**
         * The interpreted code is added is in the "Interpreted execute region"
         * of the attrinfo (see Dbtup::interpreterStartLab() for details).
         * It will thus execute before reading the attributes that constitutes
         * the projections.
         */
        sectionptrs[1] = len; 
      }

      Uint32 sum_read = 0;
      Uint32 dst[MAX_ATTRIBUTES_IN_TABLE + 2];

      if (paramBits & DABits::PI_ATTR_LIST)
      {
        jam();
        Uint32 len = * param.ptr++;
        DEBUG("PI_ATTR_LIST");

        treeNodePtr.p->m_bits |= TreeNode::T_USER_PROJECTION;
        err = DbspjErr::OutOfSectionMemory;
        if (!appendToSection(attrInfoPtrI, param.ptr, len))
        {
          DEBUG_CRASH();
          break;
        }

        param.ptr += len;

        /**
         * Insert a flush of this partial result set
         */
        Uint32 flush[3];
        flush[0] = AttributeHeader::FLUSH_AI << 16;
        flush[1] = ctx.m_resultRef;
        flush[2] = ctx.m_resultData;
        if (!appendToSection(attrInfoPtrI, flush, 3))
        {
          DEBUG_CRASH();
          break;
        }

        sum_read += len + 3;
      }

      if (treeBits & DABits::NI_LINKED_ATTR)
      {
        jam();
        DEBUG("NI_LINKED_ATTR");
        err = DbspjErr::InvalidTreeNodeSpecification;
        Uint32 cnt = unpackList(MAX_ATTRIBUTES_IN_TABLE, dst, tree);
        if (unlikely(cnt > MAX_ATTRIBUTES_IN_TABLE))
        {
          DEBUG_CRASH();
          break;
        }

        /**
         * AttributeHeader contains attrId in 16-higher bits
         */
        for (Uint32 i = 0; i<cnt; i++)
          dst[i] <<= 16;

        /**
         * Read correlation factor
         */
        dst[cnt++] = AttributeHeader::READ_ANY_VALUE << 16;

        err = DbspjErr::OutOfSectionMemory;
        if (!appendToSection(attrInfoPtrI, dst, cnt))
        {
          DEBUG_CRASH();
          break;
        }

        sum_read += cnt;
      }

      if (interpreted)
      {
        jam();
        /**
         * Let reads be performed *after* interpreted program
         *   i.e in "final read"-section
         */
        sectionptrs[3] = sum_read;
      }

      treeNodePtr.p->m_send.m_attrInfoParamPtrI = attrParamPtrI;
      treeNodePtr.p->m_send.m_attrInfoPtrI = attrInfoPtrI;
    } // if (((treeBits & mask) | (paramBits & DABits::PI_ATTR_LIST)) != 0)

    return 0;
  } while (0);

  return err;
}
/**
 * END - MODULE COMMON PARSE/UNPACK
 */
