/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql.h>
#include <mysqld_error.h>

//#include <iostream>
//#include <stdio.h>

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include "../../src/ndbapi/NdbQueryBuilder.hpp"
#include "../../src/ndbapi/NdbQueryOperation.hpp"
#include <pthread.h>
#include <NdbTick.h>

#if 0
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
#endif


//const char* databaseName = "TEST_DB";
//const char* tableName = "T";
const char* databaseName = "PTDB";
const char* tableName = "TT";

class TestParameters{
public:
  int m_iterations;
  /** Number of child lookup operations.*/
  int m_depth;
  int m_scanLength; // m_scanLength==0 means root should be lookup.
  /** Specifies how many times a query definition should be reused before.
   * It is recreated. Setting this to 0 means that the definition is never
   * recreated.*/ 
  int m_queryDefReuse;
  bool m_useLinkedOperations;
  /** If true, run an equivalent SQL query.*/
  bool m_useSQL;

  explicit TestParameters(){bzero(this, sizeof *this);}
};

/** Entry point for new posix threads.*/
static void *callback(void* thread);

class TestThread{
  friend void *callback(void* thread);
public:
  explicit TestThread(Ndb_cluster_connection& con, const char* host, int port);
  ~TestThread();
  /** Initiate a new test.*/
  void start(const TestParameters& params);
  /** Wait fo current test to complete.*/
  void wait();
private:
  struct Row{
    Uint32 a;
    Uint32 b;
  };
  
  struct KeyRow{
    Uint32 a;
  };

  const TestParameters* m_params;
  Ndb m_ndb;
  enum {State_Active, State_Stopping, State_Stopped} m_state;
  pthread_t m_posixThread;
  pthread_mutex_t m_mutex;  
  pthread_cond_t m_condition;
  const NdbDictionary::Table* m_tab;
  const NdbDictionary::Index* m_index;
  const NdbRecord* m_resultRec;
  const NdbRecord* m_keyRec; 
  const NdbRecord* m_indexRec;
  MYSQL m_mysql;

  /** Entry point for POSIX thread.*/
  void run();
  void doLinkedAPITest();
  void doNonLinkedAPITest();
  void doSQLTest();
};

static void *callback(void* thread){
  reinterpret_cast<TestThread*>(thread)->run();
  return NULL;
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
  mysql_free_result(mysql_use_result(&mysql));
}

// TestThread methods.
TestThread::TestThread(Ndb_cluster_connection& con, 
                       const char* host, 
                       int port): 
  m_params(NULL),
  m_ndb(&con, databaseName),
  m_state(State_Active){
  require(m_ndb.init()==0);
  require(pthread_mutex_init(&m_mutex, NULL)==0);
  require(pthread_cond_init(&m_condition, NULL)==0);
  require(pthread_create(&m_posixThread, NULL, callback, this)
                ==0);
  NdbDictionary::Dictionary*  const dict = m_ndb.getDictionary();
  m_tab = dict->getTable(tableName);

  m_index = dict->getIndex("PRIMARY", tableName);
  require(m_index != NULL);

  /* Create NdbRecord for row. */
  m_resultRec = m_tab->getDefaultRecord();
  require(m_resultRec!=NULL);

  /* Create NdbRecord for primary key. */
  const NdbDictionary::Column *col1= m_tab->getColumn("a");
  require(col1 != NULL);
  NdbDictionary::RecordSpecification spec = {
    col1, 0, 0, 0
  };
  
  m_keyRec = dict->createRecord(m_tab, &spec, 1, sizeof spec);
  require(m_keyRec != NULL);

  m_indexRec = m_index->getDefaultRecord();
  require(m_indexRec != NULL);

  // Make SQL connection.
  require(mysql_init(&m_mysql));
  if(!mysql_real_connect(&m_mysql, host, "root", "", "",
                         port, NULL, 0)){
    printMySQLError(m_mysql, "mysql_real_connect() failed:");
    require(false);
  }
  char text[50];
  sprintf(text, "use %s", databaseName);
  mySQLExec(m_mysql, text);
}

TestThread::~TestThread(){
  require(pthread_mutex_lock(&m_mutex)==0);
  // Tell thread to stop.
  m_state = State_Stopping;
  require(pthread_cond_signal(&m_condition)==0);
  // Wait for thread to stop.
  while(m_state != State_Stopped){
    require(pthread_cond_wait(&m_condition, &m_mutex)==0);
  }
  require(m_params == NULL);
  require(pthread_mutex_unlock(&m_mutex)==0);
  require(pthread_cond_destroy(&m_condition)==0);
  require(pthread_mutex_destroy(&m_mutex)==0);
}

void TestThread::start(const TestParameters& params){
  require(pthread_mutex_lock(&m_mutex)==0);
  require(m_params == NULL);
  m_params = &params;
  require(pthread_cond_signal(&m_condition)==0);
  require(pthread_mutex_unlock(&m_mutex)==0);
}

void TestThread::run(){

  require(pthread_mutex_lock(&m_mutex)==0);
  while(true){
    while(m_params==NULL && m_state==State_Active){
      // Wait for a new command from master thread.
      require(pthread_cond_wait(&m_condition, &m_mutex)==0);
    }
    if(m_state != State_Active){
      // We have been told to stop.
      require(m_state == State_Stopping);
      m_state = State_Stopped;
      // Wake up master thread and release lock.
      require(pthread_cond_signal(&m_condition)==0);
      require(pthread_mutex_unlock(&m_mutex)==0);
      // Exit thread.
      return;
    }
    
    if(m_params->m_useSQL){
      doSQLTest();
    }else{
      if(m_params->m_useLinkedOperations){
        doLinkedAPITest();
      }else{
        doNonLinkedAPITest();
      }
    }

    require(m_params != NULL);
    m_params = NULL;
    require(pthread_cond_signal(&m_condition)==0);
  }
}

void TestThread::doLinkedAPITest(){
  NdbQueryBuilder* const builder = NdbQueryBuilder::create();
      
  const NdbQueryDef* queryDef = NULL;
  const Row** resultPtrs = new const Row*[m_params->m_depth+1];
      
  NdbTransaction* trans = NULL;

  for(int iterNo = 0; iterNo<m_params->m_iterations; iterNo++){
    //ndbout << "Starting next iteration " << endl;
    // Build query definition if needed.
    if(iterNo==0 || (m_params->m_queryDefReuse>0 && 
                     iterNo%m_params->m_queryDefReuse==0)){
      if(queryDef != NULL){
        queryDef->destroy();
      }
      const NdbQueryOperationDef* parentOpDef = NULL;
      if(m_params->m_scanLength==0){
        // Root is lookup
        const NdbQueryOperand* rootKey[] = {  
          builder->constValue(0), //a
          NULL
        };
        parentOpDef = builder->readTuple(m_tab, rootKey);
      }else if(m_params->m_scanLength==1){ //Pruned scan
        const NdbQueryOperand* const key[] = {
          builder->constValue(m_params->m_scanLength),
          NULL
        };

        const NdbQueryIndexBound eqBound(key);
        parentOpDef = builder->scanIndex(m_index, m_tab, &eqBound);
      }else{
        // Root is index scan with single bound.
        const NdbQueryOperand* const highKey[] = {
          builder->constValue(m_params->m_scanLength),
          NULL
        };

        const NdbQueryIndexBound bound(NULL, false, highKey, false);
        parentOpDef = builder->scanIndex(m_index, m_tab, &bound);
      }
          
      // Add child lookup operations.
      for(int i = 0; i<m_params->m_depth; i++){
        const NdbQueryOperand* key[] = {  
          builder->linkedValue(parentOpDef, "b"),
          NULL
        };
        parentOpDef = builder->readTuple(m_tab, key);
      }
      queryDef = builder->prepare();
    }
        
    if (!trans) {
      trans = m_ndb.startTransaction();
    }
    // Execute query.
    NdbQuery* const query = trans->createQuery(queryDef);
    for(int i = 0; i<m_params->m_depth+1; i++){
      query->getQueryOperation(i)
        ->setResultRowRef(m_resultRec,
                          reinterpret_cast<const char*&>(resultPtrs[i]),
                          NULL);
    }
    int res = trans->execute(NoCommit);
    //        if (res != 0)
    //          APIERROR(trans->getNdbError());
    require(res == 0);
    int cnt=0;
    while(true){
      const NdbQuery::NextResultOutcome outcome 
        = query->nextResult(true, false);
      if(outcome ==  NdbQuery::NextResult_scanComplete){
        break;
      }
      require(outcome== NdbQuery::NextResult_gotRow);
      cnt++;
      //        if (m_params->m_scanLength==0)
      //          break;
    }
    require(cnt== MAX(1,m_params->m_scanLength));
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
  builder->destroy();
}

void TestThread::doNonLinkedAPITest(){
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
          trans->scanIndex(m_indexRec, 
                           m_resultRec, 
                           NdbOperation::LM_Dirty,
                           NULL, // Result mask
                           &bound);
      }else{
        // Scan with upper bound only.
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
          trans->scanIndex(m_indexRec, 
                           m_resultRec, 
                           NdbOperation::LM_Dirty,
                           NULL, // Result mask
                           &bound);
      }
      require(scanOp != NULL);

      require(trans->execute(NoCommit) == 0);
          
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
        require(retVal== 0);
        //ndbout << "ScanRow: " << scanRow->a << " " << scanRow->b << endl;
        row = *scanRow;
            
        // Do a chain of lookups for each scan row.
        for(int i = 0; i < m_params->m_depth; i++){
          const KeyRow key = {row.b};
          const NdbOperation* const lookupOp = 
            trans->readTuple(m_keyRec, 
                             reinterpret_cast<const char*>(&key),
                             m_resultRec,
                             reinterpret_cast<char*>(&row),
                             NdbOperation::LM_Dirty);
          require(lookupOp != NULL);
          require(trans->execute(NoCommit) == 0);
          //ndbout << "LookupRow: " << row.a << " " << row.b << endl;
        }
        cnt++;
        //          if (m_params->m_scanLength==0)
        //            break;
      }
      require(cnt== m_params->m_scanLength);
      scanOp->close(false,true);
    }else{ 
      // Root is lookup.
      for(int i = 0; i < m_params->m_depth+1; i++){
        const KeyRow key = {row.b};
        const NdbOperation* const lookupOp = 
          trans->readTuple(m_keyRec, 
                           reinterpret_cast<const char*>(&key),
                           m_resultRec,
                           reinterpret_cast<char*>(&row),
                           NdbOperation::LM_Dirty);
        require(lookupOp != NULL);
        require(trans->execute(NoCommit) == 0);
      }
    }//if(m_params->m_isScan)
    //      m_ndb.closeTransaction(trans);
  }//for(int iterNo = 0; iterNo<m_params->m_iterations; iterNo++)
  m_ndb.closeTransaction(trans);
}

static bool printQuery = false;

void TestThread::doSQLTest(){
  if(m_params->m_useLinkedOperations){
    mySQLExec(m_mysql, "set ndb_join_pushdown = on;");
  }else{
    mySQLExec(m_mysql, "set ndb_join_pushdown = off;");
  }
  mySQLExec(m_mysql, "SET SESSION query_cache_type = OFF");

  class TextBuf{
  public:
    char m_buffer[1000];

    explicit TextBuf(){m_buffer[0] = '\0';}

    // For appending to the string.
    char* tail(){ return m_buffer + strlen(m_buffer);}
  };

  TextBuf text;

  sprintf(text.tail(), "select * from ");
  for(int i = 0; i<m_params->m_depth+1; i++){
    sprintf(text.tail(), "%s t%d", tableName, i);
    if(i < m_params->m_depth){
      sprintf(text.tail(), ", ");
    }else{
      sprintf(text.tail(), " where ");
    }
  }

  if(m_params->m_scanLength==0){
    // Root is lookup
    sprintf(text.tail(), "t0.a=0 ");
  }else{
    // Root is scan.
    sprintf(text.tail(), "t0.a<%d ", m_params->m_scanLength);
  }

  for(int i = 1; i<m_params->m_depth+1; i++){
    // Compare primary key of Tn to attribute of Tn-1.
    sprintf(text.tail(), "and t%d.b=t%d.a ", i-1, i);
  }
  if(printQuery){
    ndbout << text.m_buffer << endl;
  }
          
  for(int i = 0; i < m_params->m_iterations; i++){
    mySQLExec(m_mysql, text.m_buffer);
  }
}

void TestThread::wait(){
  require(pthread_mutex_lock(&m_mutex)==0);
  while(m_params!=NULL){
    require(pthread_cond_wait(&m_condition, &m_mutex)==0);
  }
  require(pthread_mutex_unlock(&m_mutex)==0);
}




static void makeDatabase(const char* host, int port, int rowCount){
  MYSQL mysql;
  require(mysql_init(&mysql));
  if(!mysql_real_connect(&mysql, host, "root", "", "",
                         port, NULL, 0)){
    printMySQLError(mysql, "mysql_real_connect() failed:");
    require(false);
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

static void printHeading(){
  ndbout << endl << "Use SQL; Use linked; Thread count; Iterations; "
    "Scan length; Depth; Def re-use; Duration (ms); Tuples per sec;" << endl;
}


void runTest(TestThread** threads, int threadCount, 
             TestParameters& param){
  //ndbout << "Doing test " << name << endl;
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  for(int i = 0; i<threadCount; i++){
    threads[i]->start(param);
  }
  for(int i = 0; i<threadCount; i++){
    threads[i]->wait();
  }
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const Uint64 duration = NdbTick_Elapsed(start,now).milliSec();
  ndbout << param.m_useSQL << "; ";
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

void warmUp(){
  ndbout << endl << "warmUp()" << endl;
  TestParameters param;
  param.m_useSQL = true;
  param.m_iterations = 10;
  param.m_useLinkedOperations = false;
  param.m_scanLength = 0;
  param.m_queryDefReuse = 0;

  printHeading();
  for(int i = 0; i<20; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
  printHeading();
  param.m_useLinkedOperations = true;
  for(int i = 0; i<20; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
}

void testLookupDepth(bool useSQL){
  ndbout << endl << "testLookupDepth()" << endl;
  TestParameters param;
  param.m_useSQL = useSQL;
  param.m_iterations = 100;
  param.m_useLinkedOperations = false;
  param.m_scanLength = 0;
  param.m_queryDefReuse = 0;

  printHeading();
  for(int i = 0; i<20; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
  printHeading();
  param.m_useLinkedOperations = true;
  for(int i = 0; i<20; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
}

void testScanDepth(int scanLength, bool useSQL){
  ndbout  << endl << "testScanDepth()" << endl;
  TestParameters param;
  param.m_useSQL = useSQL;
  param.m_iterations = 20;
  param.m_useLinkedOperations = false;
  param.m_scanLength = scanLength;
  param.m_queryDefReuse = 0;
  printHeading();
  for(int i = 0; i<10; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
  printHeading();
  param.m_useLinkedOperations = true;
  for(int i = 0; i<10; i++){
    param.m_depth = i;
    runTest(threads, threadCount, param);
  }
}

int main(int argc, char* argv[]){
  NDB_INIT(argv[0]);
  if(argc!=4 && argc!=5){
    ndbout << "Usage: " << argv[0] << " [--print-query]" 
           << " <mysql IP address> <mysql port> <cluster connect string>" 
           << endl;
    return -1;
  }
  int argno = 1;
  if(strcmp(argv[argno],"--print-query")==0){
    printQuery = true;
    argno++;
  }
  const char* const host=argv[argno++];
  const int port = atoi(argv[argno++]);
  const char* const connectString = argv[argno];

  makeDatabase(host, port, 200);
  {
    Ndb_cluster_connection con(connectString);
    require(con.connect(12, 5, 1) == 0);
    require(con.wait_until_ready(30,30) == 0);

    const int threadCount = 1;
    threads = new TestThread*[threadCount];
    for(int i = 0; i<threadCount; i++){
      threads[i] = new TestThread(con, host, port);
    }
    sleep(1);

    //testScanDepth(1);
    //testScanDepth(2);
    //testScanDepth(5);
    warmUp();
    testScanDepth(50, true);
    testLookupDepth(true);
 
    for(int i = 0; i<threadCount; i++){
      delete threads[i];
    }
    delete[] threads;
  } // Must call ~Ndb_cluster_connection() before ndb_end().
  ndb_end(0);
  return 0;
}
    
