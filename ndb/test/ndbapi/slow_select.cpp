
#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbTick.h>

struct
S_Scan {
  const char * m_table;
  const char * m_index;
  NdbIndexScanOperation * m_scan;
  NdbResultSet * m_result;
};

static S_Scan g_scans[] = {
  { "affiliatestometa", "ind_affiliatestometa", 0, 0 },
  { "media", "ind_media", 0, 0 },
  { "meta", "PRIMARY", 0, 0 },
  { "artiststometamap", "PRIMARY", 0, 0 },
  { "subgenrestometamap", "metaid", 0, 0 }
};

#define require(x) if(!x) abort()

Uint32 g_affiliateid = 2;
Uint32 g_formatids[] = { 8, 31, 76 };
Uint32 g_formattypeid = 2;

Uint64 start;

int
main(void){
  Ndb g_ndb("test");
  g_ndb.init(1024);

  require(g_ndb.waitUntilReady() == 0);

  NdbConnection * g_trans = g_ndb.startTransaction();
  require(g_trans);
  
  size_t i;
  const size_t cnt = sizeof(g_scans)/sizeof(g_scans[0]);

  start = NdbTick_CurrentMillisecond();

  for(i = 0; i<cnt; i++){
    ndbout_c("starting scan on: %s %s", 
	     g_scans[i].m_table, g_scans[i].m_index);
    g_scans[i].m_scan = g_trans->getNdbIndexScanOperation(g_scans[i].m_index, 
							  g_scans[i].m_table);
    NdbIndexScanOperation* scan = g_scans[i].m_scan;
    require(scan);
    g_scans[i].m_result = scan->readTuples(NdbScanOperation::LM_CommittedRead, 
					   0, 0, true);
    require(g_scans[i].m_result);
  }
  
  require(!g_scans[0].m_scan->setBound((Uint32)0, 
				       NdbIndexScanOperation::BoundEQ, 
				       &g_affiliateid, 
				       sizeof(g_affiliateid)));

  require(!g_scans[1].m_scan->setBound((Uint32)0, 
				       NdbIndexScanOperation::BoundLE,
				       &g_formatids[0],
				       sizeof(g_formatids[0])));  

  NdbScanFilter sf(g_scans[1].m_scan);
  sf.begin(NdbScanFilter::OR);
  sf.eq(2, g_formatids[0]);
  sf.eq(2, g_formatids[1]);
  sf.eq(2, g_formatids[2]);
  sf.end();
  
  Uint32 metaid[5];
  for(i = 0; i<cnt; i++){
    g_scans[i].m_scan->getValue("metaid", (char*)&metaid[0]);
  }

  g_trans->execute(NoCommit, AbortOnError, 1);
  
  Uint32 rows[] = {0,0,0,0,0};
  Uint32 done[] = { 2, 2, 2, 2, 2 };
  Uint32 run = 0;
  do {
    run = 0;
    for(i = 0; i<cnt; i++){
      int res;
      switch(done[i]){
      case 0:
	rows[i]++;
	res = g_scans[i].m_result->nextResult(false);
	break;
      case 1:
	run++;
	res = 1;
	break;
      case 2:
	res = g_scans[i].m_result->nextResult(true);
	break;
      default:
	ndbout_c("done[%d] = %d", i, done[i]);
	ndbout << g_scans[i].m_scan->getNdbError() << endl;
	abort();
      }
      done[i] = res;
    }
  } while(run < cnt);

  start = NdbTick_CurrentMillisecond() - start;
  ndbout_c("Elapsed: %lldms", start);

  for(i = 0; i<cnt; i++){
    ndbout_c("rows[%d]: %d", i, rows[i]);
  }
}

