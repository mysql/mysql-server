/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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
#include <ndb_global.h>
#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbTick.h>

struct
S_Scan {
  const char * m_table;
  const char * m_index;
  NdbIndexScanOperation * m_scan;
  Uint32 metaid;
  Uint32 match_count;
  Uint32 row_count;
};

static S_Scan g_scans[] = {
  { "affiliatestometa", "ind_affiliatestometa", 0, 0, 0, 0 },
  { "media", "metaid", 0, 0, 0, 0 },
  { "meta", "PRIMARY", 0, 0, 0, 0 },
  { "artiststometamap", "PRIMARY", 0, 0, 0, 0 },
  { "subgenrestometamap", "metaid", 0, 0, 0, 0 }
};

#define require2(o, x) \
    if(!(x))\
    {\
      ndbout << o->getNdbError() << endl;\
      require(false); \
    }

Uint32 g_affiliateid = 2;
Uint32 g_formatids[] = { 8, 31, 76 };

Uint64 start;
Uint32 g_artistid = 0;
Uint32 g_subgenreid = 0;

NdbConnection* g_trans =  0;
static void lookup();

int
main(void){
  ndb_init();

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return 1;
  }

  Ndb g_ndb(&con, "test");
  g_ndb.init(1024);

  require(g_ndb.waitUntilReady() == 0);

  while(true){
    g_trans = g_ndb.startTransaction();
    require(g_trans);
  
    size_t i, j;
    const size_t cnt = sizeof(g_scans)/sizeof(g_scans[0]);

    start = NdbTick_CurrentMillisecond();

    for(i = 0; i<cnt; i++){
      ndbout_c("starting scan on: %s %s", 
	       g_scans[i].m_table, g_scans[i].m_index);
      g_scans[i].m_scan = g_trans->getNdbIndexScanOperation(g_scans[i].m_index, 
							    g_scans[i].m_table);
      NdbIndexScanOperation* scan = g_scans[i].m_scan;
      require(scan);
      require(scan->readTuples(NdbScanOperation::LM_CommittedRead, 
			       0, 0, true) == 0);
    }
  
    require(!g_scans[0].m_scan->setBound((Uint32)0, 
					 NdbIndexScanOperation::BoundEQ, 
					 &g_affiliateid, 
					 sizeof(g_affiliateid)));
#if 0
    require(!g_scans[1].m_scan->setBound((Uint32)0, 
					 NdbIndexScanOperation::BoundLE,
					 &g_formatids[0],
					 sizeof(g_formatids[0])));  
#endif

    NdbScanFilter sf(g_scans[1].m_scan);
    sf.begin(NdbScanFilter::OR);
    sf.eq(2, g_formatids[0]);
    sf.eq(2, g_formatids[1]);
    sf.eq(2, g_formatids[2]);
    sf.end();

    // affiliatestometa
    require(g_scans[0].m_scan->getValue("uniquekey"));
    require(g_scans[0].m_scan->getValue("xml"));

    // media
    require(g_scans[1].m_scan->getValue("path"));
    require(g_scans[1].m_scan->getValue("mediaid"));
    require(g_scans[1].m_scan->getValue("formatid"));
  
    // meta
    require(g_scans[2].m_scan->getValue("name"));
    require(g_scans[2].m_scan->getValue("xml"));

    // artiststometamap
    require(g_scans[3].m_scan->getValue("artistid", (char*)&g_artistid));
	  
    // subgenrestometamap
    require(g_scans[4].m_scan->getValue("subgenreid", (char*)&g_subgenreid));
  
    for(i = 0; i<cnt; i++){
      g_scans[i].m_scan->getValue("metaid", (char*)&g_scans[i].metaid);
    }

    g_trans->execute(NoCommit, AbortOnError, 1);

    Uint32 max_val = 0;
    Uint32 match_val = 0;
  
    S_Scan * F [5], * Q [5], * nextF [5];
    Uint32 F_sz = 0, Q_sz = 0;
    for(i = 0; i<cnt; i++){
      F_sz++;
      F[i] = &g_scans[i];
    }

    Uint32 match_count = 0;
    while(F_sz > 0){
      Uint32 prev_F_sz = F_sz;
      F_sz = 0;
      bool found = false;
      //for(i = 0; i<cnt; i++)
      //ndbout_c("%s - %d", g_scans[i].m_table, g_scans[i].metaid);
    
      for(i = 0; i<prev_F_sz; i++){
	int res = F[i]->m_scan->nextResult();
	if(res == -1)
	  abort();

	if(res == 1){
	  continue;
	}

	Uint32 metaid = F[i]->metaid;
	F[i]->row_count++;
      
	if(metaid == match_val){
	  //ndbout_c("flera");
	  nextF[F_sz++] = F[i];
	  require(F_sz <= cnt);
	  F[i]->match_count++;
	  Uint32 comb = 1;
	  for(j = 0; j<cnt; j++){
	    comb *= (&g_scans[j] == F[i] ? 1 : g_scans[j].match_count);
	  }
	  match_count += comb;
	  found = true;
	  continue;
	}
	if(metaid < max_val){
	  nextF[F_sz++] = F[i];
	  require(F_sz <= cnt);
	  continue;
	}
	if(metaid > max_val){
	  for(j = 0; j<Q_sz; j++)
	    nextF[F_sz++] = Q[j];
	  require(F_sz <= cnt);
	  Q_sz = 0;
	  max_val = metaid;
	}
	Q[Q_sz++] = F[i];
	require(Q_sz <= cnt);
      }
      if(F_sz == 0 && Q_sz > 0){
	match_val = max_val;
	for(j = 0; j<Q_sz; j++){
	  nextF[F_sz++] = Q[j];
	  Q[j]->match_count = 1;
	}
	require(F_sz <= cnt);
	require(Q_sz <= cnt);
	Q_sz = 0;
	match_count++;
	lookup();
      } else if(!found && F_sz + Q_sz < cnt){
	F_sz = 0;
      }
      require(F_sz <= cnt);
      for(i = 0; i<F_sz; i++)
	F[i] = nextF[i];
    }
  
    start = NdbTick_CurrentMillisecond() - start;
    ndbout_c("Elapsed: %lldms", start);
  
    ndbout_c("rows: %d", match_count);
    for(i = 0; i<cnt; i++){
      ndbout_c("%s : %d", g_scans[i].m_table, g_scans[i].row_count);
    }
    g_trans->close();
  }
}

static
void
lookup(){
  {
    NdbOperation* op = g_trans->getNdbOperation("artists");
    require2(g_trans, op);
    require2(op, op->readTuple() == 0);
    require2(op, op->equal("artistid", g_artistid) == 0);
    require2(op, op->getValue("name"));
  }
  
  {
    NdbOperation* op = g_trans->getNdbOperation("subgenres");
    require2(g_trans, op);
    require2(op, op->readTuple() == 0);
    require2(op, op->equal("subgenreid", g_subgenreid) == 0);
    require2(op, op->getValue("name"));
  }

  static int loop = 0;
  if(loop++ >= 16){
    loop = 0;
    require(g_trans->execute(NoCommit) == 0);
  }
  //require(g_trans->restart() == 0);
}
