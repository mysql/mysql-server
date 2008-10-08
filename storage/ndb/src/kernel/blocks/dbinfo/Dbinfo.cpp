/* Copyright (C) 2007 MySQL AB

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

#include "Dbinfo.hpp"
#include <ndbinfo.h>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include "ndbinfo_tables.h"
#include "ndbinfo_tableids.h"
#include <AttributeHeader.hpp>

Uint32 dbinfo_blocks[] = { DBACC, DBTUP, BACKUP, DBTC, SUMA, DBUTIL, TRIX, DBTUX, DBDICT, 0};

Dbinfo::Dbinfo(Block_context& ctx) :
  SimulatedBlock(DBINFO, ctx),
  c_nodes(c_nodePool)
{
  BLOCK_CONSTRUCTOR(Dbinfo);

  c_nodePool.setSize(MAX_NDB_NODES);

  /* Add Received Signals */
  addRecSignal(GSN_STTOR, &Dbinfo::execSTTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbinfo::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbinfo::execREAD_CONFIG_REQ, true);

  addRecSignal(GSN_DBINFO_SCANREQ, &Dbinfo::execDBINFO_SCANREQ);
  addRecSignal(GSN_DBINFO_SCANCONF, &Dbinfo::execDBINFO_SCANCONF);

  addRecSignal(GSN_DBINFO_TRANSID_AI, &Dbinfo::execDBINFO_TRANSID_AI);

  addRecSignal(GSN_READ_NODESCONF, &Dbinfo::execREAD_NODESCONF);
  addRecSignal(GSN_NODE_FAILREP, &Dbinfo::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Dbinfo::execINCL_NODEREQ);

}

Dbinfo::~Dbinfo()
{
  /* Nothing */
}

BLOCK_FUNCTIONS(Dbinfo)

void Dbinfo::execSTTOR(Signal *signal)
{
  jamEntry();

  const Uint32 startphase  = signal->theData[1];

  if (startphase == 3) {
    jam();
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }//if

  sendSTTORRY(signal);
  return;
}

void Dbinfo::execREAD_CONFIG_REQ(Signal *signal)
{
  jamEntry();
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  /* In the future, do something sensible here. */

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal,
	     ReadConfigConf::SignalLength, JBB);
}

void Dbinfo::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
}

/*
 * Executing DBINFO_TRANSID_AI is only for debugging
 * We use this as part of the DUMP interface
 * and during debugging.
 */
void Dbinfo::execDBINFO_TRANSID_AI(Signal* signal)
{
  jamEntry();
  TransIdAI *tidai= (TransIdAI*)signal->theData;

  if(!assembleFragments(signal)){
    return;
  }

  const Uint32 tableId= tidai->transId[0];
  const int ncols = ndbinfo_tables[tableId]->ncols;

  SectionHandle handle(this, signal);
  SegmentedSectionPtr ptr;
  handle.getSection(ptr, 0);

  char rowbuf[1024];
  char *row= rowbuf;
  copy((Uint32*)rowbuf, ptr);

  Uint32 rowsz= ptr.sz;
  int len;

  for(int i=0; i<ncols; i++)
  {
    jam();
    AttributeHeader ah(*(Uint32*)row);
    row+=ah.getHeaderSize()*sizeof(Uint32);

    ndbout << "AI " << ah.getAttributeId() << " ";
    len= ah.getByteSize();
    ndbout << "||" << len << "||" << " ";
    ndbout << "Table " << tableId << " ::: ";
    switch(ndbinfo_tables[tableId]->col[i].coltype)
    {
    case NDBINFO_TYPE_NUMBER:
      jam();
      ndbout << *(Uint32*)row;
      row+= len;
      break;
    case NDBINFO_TYPE_STRING:
      jam();
      char b[512];
      memcpy(b,row,len);
      b[len]=0;
      ndbout << b;
      row+= len;
      break;
    default:
      ndbassert(false);
      break;
    };
    ndbout << endl;
  }
  releaseSections(handle);
}

void Dbinfo::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();

  switch(signal->theData[0])
  {
  case DumpStateOrd::DbinfoListTables:
    jam();
    ndbout_c("--- BEGIN NDB$INFO.TABLES ---");
    char create_sql[512];
    for(Uint32 i=0;i<number_ndbinfo_tables;i++)
    {
      ndbinfo_create_sql(ndbinfo_tables[i],
                         create_sql, sizeof(create_sql));
      ndbout_c("%d,%s,%s",i,ndbinfo_tables[i]->name,create_sql);
    }
    ndbout_c("--- END NDB$INFO.TABLES ---");
    break;

  case DumpStateOrd::DbinfoListColumns:
    jam();
    ndbout_c("--- BEGIN NDB$INFO.COLUMNS ---");
    for(Uint32 i=0;i<number_ndbinfo_tables;i++)
    {
      struct ndbinfo_table *t= ndbinfo_tables[i];

      for(int j=0;j<t->ncols;j++)
        ndbout_c("%d,%d,%s,%d",i,j,t->col[j].name,t->col[j].coltype);
    }
    ndbout_c("--- END NDB$INFO.COLUMNS ---");
    break;

  case DumpStateOrd::DbinfoScanTable:
    jam();
    const Uint32 tableId= signal->theData[1];

    DbinfoScanReq *req = (DbinfoScanReq*)signal->theData;
    req->tableId= tableId;
    req->senderRef= reference();
    req->apiTxnId= tableId;
    req->requestInfo= DbinfoScanReq::AllColumns | DbinfoScanReq::StartScan;
    req->colBitmapLo= ~0;
    req->colBitmapHi= ~0;
    req->maxRows= 2;
    req->maxBytes= 0;
    req->rows_total= 0;
    req->word_total= 0;

    ndbout_c("BEGIN DBINFO DUMP SCAN on %u",tableId);

    sendSignal(reference(), GSN_DBINFO_SCANREQ,
               signal, DbinfoScanReq::SignalLength, JBB);
    break;
  };
}

void Dbinfo::execDBINFO_SCANREQ(Signal *signal)
{
  jamEntry();
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;

  const Uint32 tableId= req.tableId;
  const Uint32 senderRef= req.senderRef;
  const Uint32 apiTxnId= req.apiTxnId;
  const Uint32 colBitmapLo= req.colBitmapLo;
  const Uint32 colBitmapHi= req.colBitmapHi;
  const Uint32 requestInfo= req.requestInfo;

  Uint32 i;
  int j;
  int startid= 0;

  int startTableId, startColumnId;

  int continue_sending;

  char buf[512];
  struct dbinfo_row r;
  struct dbinfo_ratelimit rl;

  switch(req.tableId)
  {
  case NDBINFO_TABLES_TABLEID:
    jam();

    char create_sql[512];

    dbinfo_ratelimit_init(&rl, &req);

    if(!(req.requestInfo & DbinfoScanReq::StartScan))
      startid= req.cur_item;

    for(i=startid;dbinfo_ratelimit_continue(&rl) && i<number_ndbinfo_tables;i++)
    {
      jam();
      dbinfo_write_row_init(&r, buf, sizeof(buf));
      ndbinfo_create_sql(ndbinfo_tables[i],
                         create_sql, sizeof(create_sql));

      dbinfo_write_row_column(&r, (char*)&i, sizeof(i));
      dbinfo_write_row_column(&r, ndbinfo_tables[i]->name,
                              strlen(ndbinfo_tables[i]->name));
      dbinfo_write_row_column(&r, create_sql, strlen(create_sql));

      dbinfo_send_row(signal,r,rl,apiTxnId,senderRef);
    }

    if(!dbinfo_ratelimit_continue(&rl) && i < number_ndbinfo_tables)
    {
      jam();
      dbinfo_ratelimit_sendconf(signal,req,rl,i);
    }
    else
    {
      DbinfoScanConf *conf= (DbinfoScanConf*)signal->getDataPtrSend();
      conf->tableId= req.tableId;
      conf->senderRef= req.senderRef;
      conf->apiTxnId= req.apiTxnId;
      conf->requestInfo= 0;
      sendSignal(req.senderRef, GSN_DBINFO_SCANCONF, signal,
                 DbinfoScanConf::SignalLength, JBB);
    }

    break;

  case NDBINFO_COLUMNS_TABLEID:
    jam();

    startTableId= 0;
    startColumnId= 0;

    if(!(req.requestInfo & DbinfoScanReq::StartScan))
    {
      startTableId= req.cur_item >> 8;
      startColumnId= req.cur_item & 0xFF;
    }

    struct ndbinfo_table *t;

    dbinfo_ratelimit_init(&rl, &req);

    continue_sending= 1;

    for(i=startTableId; continue_sending && i<number_ndbinfo_tables; i++)
    {
      jam();
      t= ndbinfo_tables[i];

      for(j=startColumnId; continue_sending && j<t->ncols;j++)
      {
        dbinfo_write_row_init(&r, buf, sizeof(buf));
        dbinfo_write_row_column(&r, (char*)&i, sizeof(i));
        dbinfo_write_row_column(&r, (char*)&j, sizeof(j));
        dbinfo_write_row_column(&r, t->col[j].name, strlen(t->col[j].name));
        const char* coltype_name= ndbinfo_coltype_to_string(t->col[j].coltype);
        dbinfo_write_row_column(&r, coltype_name, strlen(coltype_name));
        dbinfo_send_row(signal,r,rl, apiTxnId,senderRef);

        continue_sending= dbinfo_ratelimit_continue(&rl);
      }
      startColumnId= 0;
    }

    if((i < number_ndbinfo_tables || j < t->ncols))
    {
      jam();
      i--;
      dbinfo_ratelimit_sendconf(signal, req, rl, (i << 8) | j);
    }
    else
    {
      DbinfoScanConf *conf= (DbinfoScanConf*)signal->getDataPtrSend();
      conf->tableId= req.tableId;
      conf->senderRef= req.senderRef;
      conf->apiTxnId= req.apiTxnId;
      conf->requestInfo= 0;
      sendSignal(req.senderRef, GSN_DBINFO_SCANCONF, signal,
                 DbinfoScanConf::SignalLength, JBB);
    }

    break;

  default:
    jam();

    if(tableId > number_ndbinfo_tables)
    {
      jam();
      DbinfoScanRef *ref= (DbinfoScanRef*)signal->getDataPtrSend();
      ref->tableId= tableId;
      ref->apiTxnId= apiTxnId;
      ref->errorCode= 1;
      sendSignal(senderRef, GSN_DBINFO_SCANREF, signal,
                 DbinfoScanRef::SignalLength, JBB);
      break;
    }

    ndbassert(tableId > 1);

    if(signal->getLength() == DbinfoScanReq::SignalLength)
    {
      /*
       * We've gotten a request from application, first
       * ScanReq signal. start from beginning
       */

      jam();

      DbinfoScanReq ireq= *(DbinfoScanReq*)signal->theData;
      DbinfoScanReq *oreq= (DbinfoScanReq*)signal->getDataPtrSend();

      memcpy(signal->getDataPtrSend(),&ireq,DbinfoScanReq::SignalLength*sizeof(Uint32));
      oreq->cur_requestInfo= 0;
      oreq->cur_node= 0;
      oreq->cur_block= DBINFO;
      oreq->cur_item= 0;

      for(oreq->cur_node= 0;
          !c_aliveNodes.get(oreq->cur_node);
          oreq->cur_node++)
        ;

      sendSignal(numberToRef(DBINFO,oreq->cur_node), GSN_DBINFO_SCANREQ,
                 signal, DbinfoScanReq::SignalLengthWithCursor, JBB);
    }
    else
    {
      /**
       * We have a cursor, so we need to continue scanning.
       */
      int next_dbinfo_block= 0;
      if(req.cur_block != DBINFO)
      {
        while(dbinfo_blocks[next_dbinfo_block] != req.cur_block
            && dbinfo_blocks[next_dbinfo_block] != 0)
        {
          jam();
          next_dbinfo_block++;
        }
      }

      DbinfoScanReq ireq= *(DbinfoScanReq*)signal->theData;
      DbinfoScanReq *oreq= (DbinfoScanReq*)signal->getDataPtrSend();

      memcpy(signal->getDataPtrSend(),&ireq,signal->getLength()*sizeof(Uint32));

      oreq->cur_block= dbinfo_blocks[next_dbinfo_block];

      sendSignal(numberToRef(oreq->cur_block,oreq->cur_node),
                 GSN_DBINFO_SCANREQ,
                 signal, signal->getLength(), JBB);
    }
    break;
  };
}

void Dbinfo::execDBINFO_SCANCONF(Signal *signal)
{
  DbinfoScanConf conf= *(DbinfoScanConf*)signal->theData;

  jamEntry();

  const Uint32 tableId= conf.tableId;
  const Uint32 senderRef= conf.senderRef;
  const Uint32 apiTxnId= conf.apiTxnId;
  const Uint32 colBitmapLo= conf.colBitmapLo;
  const Uint32 colBitmapHi= conf.colBitmapHi;

  DbinfoScanReq *oreq= (DbinfoScanReq*)signal->getDataPtrSend();

  memcpy(signal->getDataPtrSend(),&conf,signal->getLength()*sizeof(Uint32));

  if(conf.requestInfo & DbinfoScanConf::MoreData)
  {
    /*
     * Continue a DUMP scan of DBINFO table (hit maxrows/maxbytes)
     */
    jam();
    oreq->requestInfo &= ~(DbinfoScanReq::StartScan);
    sendSignal(numberToRef(oreq->cur_block,oreq->cur_node),
               GSN_DBINFO_SCANREQ,
               signal, signal->getLength(), JBB);
    return;
  }

  int next_dbinfo_block= 0;

  if(signal->getLength() == 3) // we have the ACK from a DUMP initiated scan
  {
    jam();
    ndbout_c("FINISHED DBINFO DUMP Scan on %u",signal->theData[0]);
    return;
  }

  if(conf.cur_block != DBINFO)
  {
    while(dbinfo_blocks[next_dbinfo_block] != conf.cur_block
          && dbinfo_blocks[next_dbinfo_block] != 0)
    {
      jam();
      next_dbinfo_block++;
    }
  }

  next_dbinfo_block++;

  if(dbinfo_blocks[next_dbinfo_block]!=0)
  {
    oreq->cur_block= dbinfo_blocks[next_dbinfo_block];
  }
  else
  {
    for(oreq->cur_node++;
        !c_aliveNodes.get(oreq->cur_node)
          && oreq->cur_node < MAX_NDB_NODES;
        oreq->cur_node++)
      ;

    if(oreq->cur_node < MAX_NDB_NODES)
    {
      oreq->cur_requestInfo= 0;
      oreq->cur_block= DBINFO;
      oreq->cur_item= 0;
    }
    else
    {
      jam();
      DbinfoScanConf *apiconf= (DbinfoScanConf*)signal->getDataPtrSend();
      apiconf->tableId= tableId;
      apiconf->senderRef= senderRef;
      apiconf->apiTxnId= apiTxnId;
      sendSignal(senderRef, GSN_DBINFO_SCANCONF, signal, 3, JBB);
      return;
    }
  }

  sendSignal(numberToRef(oreq->cur_block,oreq->cur_node),
             GSN_DBINFO_SCANREQ,
             signal, signal->getLength(), JBB);
}


/**
 * Maintain bitmap of active nodes
 */

void Dbinfo::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();
  ReadNodesConf * conf = (ReadNodesConf *)signal->getDataPtr();

  c_aliveNodes.clear();

  Uint32 count = 0;
  for (Uint32 i = 0; i<MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(conf->allNodes, i)){
      jam();
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seize(node));

      node.p->nodeId = i;
      if(NdbNodeBitmask::get(conf->inactiveNodes, i)) {
        jam();
	node.p->alive = 0;
      } else {
        jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }//if
    }//if
  }//for
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);
  sendSTTORRY(signal);
}

void Dbinfo::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();

  const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(inclNode == nodeId){
      jam();

      ndbrequire(node.p->alive == 0);
      ndbrequire(!c_aliveNodes.get(nodeId));

      node.p->alive = 1;
      c_aliveNodes.set(nodeId);

      break;
    }//if
  }//for
  signal->theData[0] = inclNode;
  signal->theData[1] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 2, JBB);
}

void Dbinfo::execNODE_FAILREP(Signal* signal)
{
  jamEntry();

  NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();

  bool doStuff = false;

  NodeId new_master_node_id = rep->masterNodeId;
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  for (Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
    theFailedNodes[i] = rep->theNodes[i];

  c_masterNodeId = new_master_node_id;

  NodePtr nodePtr;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)){
      if(nodePtr.p->alive){
	jam();
	ndbrequire(c_aliveNodes.get(nodePtr.p->nodeId));
	doStuff = true;
      } else {
        jam();
	ndbrequire(!c_aliveNodes.get(nodePtr.p->nodeId));
      }//if
      nodePtr.p->alive = 0;
      c_aliveNodes.clear(nodePtr.p->nodeId);
    }//if
  }//for

  if(!doStuff){
    jam();
    return;
  }//if

#ifdef DEBUG_ABORT
  ndbout_c("****************** Node fail rep ******************");
#endif

  // DO STUFF TO HANDLE NF
}
