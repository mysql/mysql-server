#include <assert.h>
#include <mysql.h>
#include <mysqld_error.h>

#include <iostream>
#include <stdio.h>

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbQueryOperation.hpp>
#include <NdbQueryBuilder.hpp>
#include <pthread.h>
#include <NdbTick.h>

#ifdef NDEBUG
// Some asserts have side effects, and there is no other error handling anyway.
#define ASSERT_ALWAYS(cond) if(unlikely(!(cond))){abort();}
#else
#define ASSERT_ALWAYS assert
#endif

/**
 * Helper debugging macros
 */
#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl

#define APIERROR(error) { \
  PRINT_ERROR((error).code,(error).message); \
  exit(-1); }



//const char* databaseName = "TEST_DB";
//const char* tableName = "T";
const char* databaseName = "PTDB";
const char* tableName = "TT";

class TestParameters{
public:
  int m_iterations;
  int m_depth;
  int m_scanLength; // m_scanLength==0 means root should be lookup.
  int m_queryDefReuse;
  bool m_useLinkedOperations;

  explicit TestParameters(){bzero(this, sizeof *this);}
};

/*class TestSeriesParameters{
public:
  TestParameters m_testParameters;
  int m_depthDelta;
  int m_queryDefReuseDelta;
  int m_testCount;

  explicit TestParameters();
  };*/

static void *callback(void* thread);

class TestThread{
  friend void *callback(void* thread);
public:
  explicit TestThread(Ndb_cluster_connection& con);
  ~TestThread();
  void start(const TestParameters& params);
  void wait();
private:
  const TestParameters* m_params;
  Ndb m_ndb;
  enum {State_Active, State_Stopping, State_Stopped} m_state;
  pthread_t m_posixThread;
  pthread_mutex_t m_mutex;  
  pthread_cond_t m_condition;   
  void run();
};

static void *callback(void* thread){
  reinterpret_cast<TestThread*>(thread)->run();
  return NULL;
}

class Test{
public:
  explicit Test(const TestParameters& params);
  void execute();
};

// TestThread methods.
TestThread::TestThread(Ndb_cluster_connection& con): 
  m_params(NULL),
  m_ndb(&con, databaseName),
  m_state(State_Active){
  ASSERT_ALWAYS(m_ndb.init()==0);
  ASSERT_ALWAYS(pthread_mutex_init(&m_mutex, NULL)==0);
  ASSERT_ALWAYS(pthread_cond_init(&m_condition, NULL)==0);
  ASSERT_ALWAYS(pthread_create(&m_posixThread, NULL, callback, this)
                ==0);
}

TestThread::~TestThread(){
  ASSERT_ALWAYS(pthread_mutex_lock(&m_mutex)==0);
  // Tell thread to stop.
  m_state = State_Stopping;
  ASSERT_ALWAYS(pthread_cond_signal(&m_condition)==0);
  // Wait for thread to stop.
  while(m_state != State_Stopped){
    ASSERT_ALWAYS(pthread_cond_wait(&m_condition, &m_mutex)==0);
  }
  ASSERT_ALWAYS(m_params == NULL);
  ASSERT_ALWAYS(pthread_mutex_unlock(&m_mutex)==0);

  ASSERT_ALWAYS(pthread_cond_destroy(&m_condition)==0);
  ASSERT_ALWAYS(pthread_mutex_destroy(&m_mutex)==0);
}

void TestThread::start(const TestParameters& params){
  ASSERT_ALWAYS(pthread_mutex_lock(&m_mutex)==0);
  ASSERT_ALWAYS(m_params == NULL);
  m_params = &params;
  ASSERT_ALWAYS(pthread_cond_signal(&m_condition)==0);
  ASSERT_ALWAYS(pthread_mutex_unlock(&m_mutex)==0);
}

void TestThread::run(){
  struct Row{
    Uint32 a;
    Uint32 b;
  };
  
  struct KeyRow{
    Uint32 a;
  };

  NdbDictionary::Dictionary*  const dict = m_ndb.getDictionary();
  const NdbDictionary::Table* const tab = dict->getTable(tableName);
  //char indexName[100];
  //sprintf(indexName, "%", tableName);
  const NdbDictionary::Index* const index
    = dict->getIndex("PRIMARY", tableName);
  ASSERT_ALWAYS(index != NULL);

  /* Create NdbRecord for row. */
  const NdbRecord* const resultRec = tab->getDefaultRecord();
  ASSERT_ALWAYS(resultRec!=NULL);

  /* Create NdbRecord for primary key. */
  const NdbDictionary::Column *col1= tab->getColumn("a");
  ASSERT_ALWAYS(col1 != NULL);
  NdbDictionary::RecordSpecification spec = {
    col1, 0, 0, 0
  };
  const NdbRecord* const keyRec =
    dict->createRecord(tab, &spec, 1, sizeof spec);
  ASSERT_ALWAYS(keyRec != NULL);

  /* Create NdbRecord for index.*/
  /*const NdbDictionary::Column *indexCol1= index->getColumn("a");
  ASSERT_ALWAYS(indexCol1 != NULL);
  NdbDictionary::RecordSpecification spec = {
    indexCol1, 0, 0, 0
  };
  const NdbRecord* const indexRec =
    dict->createRecord(tab, &spec, 1, sizeof spec);
    ASSERT_ALWAYS(keyRec != NULL);*/
  const NdbRecord* const indexRec = index->getDefaultRecord();
  ASSERT_ALWAYS(indexRec != NULL);

  ASSERT_ALWAYS(pthread_mutex_lock(&m_mutex)==0);
  while(true){
    while(m_params==NULL && m_state==State_Active){
      // Wait for a new command from master thread.
      ASSERT_ALWAYS(pthread_cond_wait(&m_condition, &m_mutex)==0);
    }
    if(m_state != State_Active){
      // We have been told to stop.
      ASSERT_ALWAYS(m_state == State_Stopping);
      m_state = State_Stopped;
      // Wake up master thread and release lock.
      ASSERT_ALWAYS(pthread_cond_signal(&m_condition)==0);
      ASSERT_ALWAYS(pthread_mutex_unlock(&m_mutex)==0);
      return;
    }
    
    if(m_params->m_useLinkedOperations){
      NdbQueryBuilder builder(m_ndb);
      
      const NdbQueryDef* queryDef = NULL;
      const Row** resultPtrs = new const Row*[m_params->m_depth+1];
      
      NdbTransaction* trans = NULL;

      for(int iterNo = 0; iterNo<m_params->m_iterations; iterNo++){
        //ndbout << "Starting next iteration " << endl;
        // Build query definition if needed.
        if(iterNo==0 || (m_params->m_queryDefReuse>0 && 
                         iterNo%m_params->m_queryDefReuse==0)){
          if(queryDef != NULL){
            queryDef->release();
          }
          NdbQueryOperationDef* parentOpDef = NULL;
          if(m_params->m_scanLength==0){
            const NdbQueryOperand* rootKey[] = {  
              builder.constValue(0), //a
              NULL
            };
            parentOpDef = builder.readTuple(tab, rootKey);
          }else if(m_params->m_scanLength==1){ //Pruned scan
            const NdbQueryOperand* const key[] = {
              builder.constValue(m_params->m_scanLength),
              NULL
            };

            const NdbQueryIndexBound eqBound(key);
            parentOpDef = builder.scanIndex(index, tab, &eqBound);
          }else{
            const NdbQueryOperand* const highKey[] = {
              builder.constValue(m_params->m_scanLength),
              NULL
            };

            const NdbQueryIndexBound bound(NULL, false, highKey, false);
            parentOpDef = builder.scanIndex(index, tab, &bound);
          }
          
          for(int i = 0; i<m_params->m_depth; i++){
            const NdbQueryOperand* key[] = {  
              builder.linkedValue(parentOpDef, "b"),
              NULL
            };
            parentOpDef = builder.readTuple(tab, key);
          }
          queryDef = builder.prepare();
        }
        
        if (!trans) {
          trans = m_ndb.startTransaction();
        }
        // Execute query.
        NdbQuery* const query = trans->createQuery(queryDef, NULL);
        for(int i = 0; i<m_params->m_depth+1; i++){
          query->getQueryOperation(i)
            ->setResultRowRef(resultRec,
                              reinterpret_cast<const char*&>(resultPtrs[i]),
                              NULL);
        }
        int res = trans->execute(NoCommit);
        if (res != 0)
          APIERROR(trans->getNdbError());
        ASSERT_ALWAYS(res == 0);
        int cnt=0;
        while(true){
          const NdbQuery::NextResultOutcome outcome 
            = query->nextResult(true, false);
          if(outcome ==  NdbQuery::NextResult_scanComplete){
            break;
          }
          ASSERT_ALWAYS(outcome== NdbQuery::NextResult_gotRow);
          cnt++;
//        if (m_params->m_scanLength==0)
//          break;
        }
        ASSERT_ALWAYS(cnt== MAX(1,m_params->m_scanLength));
//      query->close();
        if ((iterNo % 5) == 0) {
          m_ndb.closeTransaction(trans);
          trans = NULL;
        }
      }
      if (trans) {
        m_ndb.closeTransaction(trans);
        trans = NULL;
      }
    }else{ // non-linked
      Row row = {0, 0};
      NdbTransaction* const trans = m_ndb.startTransaction();
      for(int iterNo = 0; iterNo<m_params->m_iterations; iterNo++){
//      NdbTransaction* const trans = m_ndb.startTransaction();
        if(m_params->m_scanLength>0){
          const KeyRow highKey = { m_params->m_scanLength };
          NdbIndexScanOperation* scanOp = NULL;
          if(m_params->m_scanLength==1){ // Pruned scan
            const NdbIndexScanOperation::IndexBound bound = {
              reinterpret_cast<const char*>(&highKey),
              1, // Low key count.
              true, // Low key inclusive
              reinterpret_cast<const char*>(&highKey),
              1, // High key count.
              true, // High key inclusive.
              0
            };
          
            scanOp = 
              trans->scanIndex(indexRec, 
                               resultRec, 
                               NdbOperation::LM_Dirty,
                               NULL, // Result mask
                               &bound);
          }else{
            const NdbIndexScanOperation::IndexBound bound = {
              NULL, // Low key
              0, // Low key count.
              false, // Low key inclusive
              reinterpret_cast<const char*>(&highKey),
              1, // High key count.
              false, // High key inclusive.
              0
            };
          
            scanOp = 
              trans->scanIndex(indexRec, 
                               resultRec, 
                               NdbOperation::LM_Dirty,
                               NULL, // Result mask
                               &bound);
          }
          ASSERT_ALWAYS(scanOp != NULL);

          ASSERT_ALWAYS(trans->execute(NoCommit) == 0);
          
          // Iterate over scan result
          int cnt = 0;
          while(true){
            const Row* scanRow = NULL;
            const int retVal = 
              scanOp->nextResult(reinterpret_cast<const char**>(&scanRow), 
                                 true, 
                                 false);
            if(retVal==1){
              break;
            }
            ASSERT_ALWAYS(retVal== 0);
            //ndbout << "ScanRow: " << scanRow->a << " " << scanRow->b << endl;
            row = *scanRow;
            
            // Do a chain of lookups for each scan row.
            for(int i = 0; i < m_params->m_depth; i++){
              const KeyRow key = {row.b};
              const NdbOperation* const lookupOp = 
                trans->readTuple(keyRec, 
                                 reinterpret_cast<const char*>(&key),
                                 resultRec,
                                 reinterpret_cast<char*>(&row),
                                 NdbOperation::LM_Dirty);
              ASSERT_ALWAYS(lookupOp != NULL);
              ASSERT_ALWAYS(trans->execute(NoCommit) == 0);
              //ndbout << "LookupRow: " << row.a << " " << row.b << endl;
            }
            cnt++;
//          if (m_params->m_scanLength==0)
//            break;
          }
          ASSERT_ALWAYS(cnt== m_params->m_scanLength);
          scanOp->close(false,true);
        }else{ 
          // Root is lookup.
          for(int i = 0; i < m_params->m_depth+1; i++){
            const KeyRow key = {row.b};
            const NdbOperation* const lookupOp = 
              trans->readTuple(keyRec, 
                               reinterpret_cast<const char*>(&key),
                               resultRec,
                               reinterpret_cast<char*>(&row),
                               NdbOperation::LM_Dirty);
            ASSERT_ALWAYS(lookupOp != NULL);
            ASSERT_ALWAYS(trans->execute(NoCommit) == 0);
          }
        }//if(m_params->m_isScan)
//      m_ndb.closeTransaction(trans);
      }//for(int iterNo = 0; iterNo<m_params->m_iterations; iterNo++)
      m_ndb.closeTransaction(trans);
    }
    ASSERT_ALWAYS(m_params != NULL);
    m_params = NULL;
    ASSERT_ALWAYS(pthread_cond_signal(&m_condition)==0);
  }
}

void TestThread::wait(){
  ASSERT_ALWAYS(pthread_mutex_lock(&m_mutex)==0);
  while(m_params!=NULL){
    ASSERT_ALWAYS(pthread_cond_wait(&m_condition, &m_mutex)==0);
  }
  ASSERT_ALWAYS(pthread_mutex_unlock(&m_mutex)==0);
}


static void printMySQLError(MYSQL& mysql, const char* before=NULL){
  if(before!=NULL){
    ndbout << before;
  }
  ndbout << mysql_error(&mysql) << endl;
  exit(-1);
}

static void mySQLExec(MYSQL& mysql, const char* stmt){
  //ndbout << stmt << endl;
  if(mysql_query(&mysql, stmt) != 0){
    ndbout << "Error executing '" << stmt << "' : ";
    printMySQLError(mysql);
  }
}



static void makeDatabase(const char* host, int port, int rowCount){
  MYSQL mysql;
  ASSERT_ALWAYS(mysql_init(&mysql));
  if(!mysql_real_connect(&mysql, host, "root", "", "",
                         port, NULL, 0)){
    printMySQLError(mysql, "mysql_real_connect() failed:");
    ASSERT_ALWAYS(false);
  }
  char text[200];
  sprintf(text, "create database if not exists %s", databaseName);
  mySQLExec(mysql, text);
  sprintf(text, "use %s", databaseName);
  mySQLExec(mysql, text);
  sprintf(text, "drop table if exists %s", tableName);
  mySQLExec(mysql, text);
  sprintf(text, "create table %s(a int not null," 
          "b int not null,"
          "primary key(a)) ENGINE=NDB", tableName);
  mySQLExec(mysql, text);
  for(int i = 0; i<rowCount; i++){
    sprintf(text, "insert into %s values(%d, %d)", tableName, 
            i, (i+1)%rowCount);
    mySQLExec(mysql, text);
  }
}

void printHeading(TestParameters& param){
  ndbout << endl << "Use linked; Thread count; Iterations; Scan length;"
    " Depth; Def re-use; Duration (ms); Tuples per sec;" << endl;
}

void runTest(TestThread** threads, int threadCount, 
             TestParameters& param){
  //ndbout << "Doing test " << name << endl;
  const NDB_TICKS start = NdbTick_CurrentMillisecond();
  for(int i = 0; i<threadCount; i++){
    threads[i]->start(param);
  }
  for(int i = 0; i<threadCount; i++){
    threads[i]->wait();
  }
  const NDB_TICKS duration = NdbTick_CurrentMillisecond() - start;
  ndbout << param.m_useLinkedOperations << "; ";
  ndbout << threadCount << "; ";
  ndbout << param.m_iterations << "; ";
  ndbout << param.m_scanLength << "; ";
  ndbout << param.m_depth <<"; ";
  ndbout << param.m_queryDefReuse << "; ";
  ndbout << duration << "; ";
  int tupPerSec;
  if(duration==0){
    tupPerSec = -1;
  }else{
    if(param.m_scanLength==0){
      tupPerSec = threadCount * 
        param.m_iterations * 
        (param.m_depth+1) * 1000 / duration;
    }else{
      tupPerSec = threadCount * 
        param.m_iterations * 
        param.m_scanLength * 
        (param.m_depth+1) * 1000 / duration;
    }
  }
  ndbout << tupPerSec << "; ";
  ndbout << endl;
  //ndbout << "Test " << name << " done in " << duration << "ms"<< endl;
}

const int threadCount = 1;
TestThread** threads = NULL;

void testLookupDepth(){
  TestParameters param;
  param.m_iterations = 1000;
  param.m_useLinkedOperations = false;
  param.m_scanLength = 0;
  param.m_queryDefReuse = 0;

  printHeading(param);
  for(int i = 0; i<20; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
  printHeading(param);
  param.m_useLinkedOperations = true;
  for(int i = 0; i<20; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
}

void testScanDepth(int n){
  TestParameters param;
  param.m_iterations = 200;
  param.m_useLinkedOperations = false;
  param.m_scanLength = n;
  param.m_queryDefReuse = 0;
  printHeading(param);
  for(int i = 0; i<10; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
  printHeading(param);
  param.m_useLinkedOperations = true;
  for(int i = 0; i<10; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
}

int main(int argc, char* argv[]){
  if(argc!=4){
    ndbout << "Usage: " << argv[0] 
           << " <mysql IP address> <mysql port> <cluster connect string>" 
           << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  const char* const host=argv[1];
  const int port = atoi(argv[2]);
  const char* const connectString = argv[3];

  NDB_INIT(argv[0]);
  makeDatabase(host, port, 200);
  {
    Ndb_cluster_connection con(connectString);
    ASSERT_ALWAYS(con.connect(12, 5, 1) == 0);
    ASSERT_ALWAYS(con.wait_until_ready(30,30) == 0);

    /*Ndb ndb(&con, databaseName);
    ASSERT_ALWAYS(ndb.init()==0);
    //ASSERT_ALWAYS(ndb.waitUntilReady()==0);
    NdbDictionary::Dictionary*  const dict = ndb.getDictionary();
    const NdbDictionary::Table* const tab = dict->getTable(tableName);
    ASSERT_ALWAYS(tab!=NULL);*/

   const int threadCount = 1;
    threads = new TestThread*[threadCount];
    for(int i = 0; i<threadCount; i++){
      threads[i] = new TestThread(con);
    }
    sleep(1);

    testScanDepth(1);
    testScanDepth(2);
    testScanDepth(5);
    testScanDepth(50);
    testLookupDepth();
 
    for(int i = 0; i<threadCount; i++){
      delete threads[i];
    }
    delete[] threads;
  } // Must call ~Ndb_cluster_connection() before ndb_end().
  ndb_end(0);
  return 0;
}
    
