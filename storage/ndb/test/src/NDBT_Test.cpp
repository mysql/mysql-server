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

#include <ndb_global.h>
#include <ndb_opts.h>

#include "NDBT.hpp"
#include "NDBT_Test.hpp"

NDBT_Context::NDBT_Context(Ndb_cluster_connection& con)
  : m_cluster_connection(con)
{
  suite = NULL;
  testcase = NULL;
  ndb = NULL;
  records = 1;
  loops = 1;
  stopped = false;
  propertyMutexPtr = NdbMutex_Create();
  propertyCondPtr = NdbCondition_Create();
}


NDBT_Context::~NDBT_Context(){
  NdbCondition_Destroy(propertyCondPtr);
  NdbMutex_Destroy(propertyMutexPtr);
}

const NdbDictionary::Table* NDBT_Context::getTab(){
  assert(tables.size());
  return tables[0];
}

NDBT_TestSuite* NDBT_Context::getSuite(){ 
  assert(suite != NULL);
  return suite;
}

NDBT_TestCase* NDBT_Context::getCase(){ 
  assert(testcase != NULL);
  return testcase;
}

const char* NDBT_Context::getTableName(int n) const
{ 
  assert(suite != NULL);
  return suite->m_tables_in_test[n].c_str();
}

int NDBT_Context::getNumTables() const
{ 
  assert(suite != NULL);
  return suite->m_tables_in_test.size();
}

int NDBT_Context::getNumRecords() const{ 
  return records;   
}

int NDBT_Context::getNumLoops() const{ 
  return loops;
}

int NDBT_Context::getNoOfRunningSteps() const {
  return testcase->getNoOfRunningSteps();
  
}
int NDBT_Context::getNoOfCompletedSteps() const {
  return testcase->getNoOfCompletedSteps();
}


Uint32 NDBT_Context::getProperty(const char* _name, Uint32 _default){ 
  Uint32 val;
  NdbMutex_Lock(propertyMutexPtr);
  if(!props.get(_name, &val))
    val = _default;
  NdbMutex_Unlock(propertyMutexPtr);
  return val;
}

bool NDBT_Context::getPropertyWait(const char* _name, Uint32 _waitVal){ 
  bool result;
  NdbMutex_Lock(propertyMutexPtr);
  Uint32 val =! _waitVal;
  
  while((!props.get(_name, &val) || (props.get(_name, &val) && val != _waitVal)) &&  
	!stopped)
    NdbCondition_Wait(propertyCondPtr,
		      propertyMutexPtr);
  result = (val == _waitVal);
  NdbMutex_Unlock(propertyMutexPtr);
  return stopped;
}

const char* NDBT_Context::getProperty(const char* _name, const char* _default){ 
  const char* val;
  NdbMutex_Lock(propertyMutexPtr);
  if(!props.get(_name, &val))
    val = _default;
  NdbMutex_Unlock(propertyMutexPtr);
  return val;
}

const char* NDBT_Context::getPropertyWait(const char* _name, const char* _waitVal){ 
  const char* val;
  NdbMutex_Lock(propertyMutexPtr);
  while(!props.get(_name, &val) && (strcmp(val, _waitVal)==0))
    NdbCondition_Wait(propertyCondPtr,
		      propertyMutexPtr);

  NdbMutex_Unlock(propertyMutexPtr);
  return val;
}

void  NDBT_Context::setProperty(const char* _name, Uint32 _val){ 
  NdbMutex_Lock(propertyMutexPtr);
  const bool b = props.put(_name, _val, true);
  assert(b == true);
  NdbCondition_Broadcast(propertyCondPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

void
NDBT_Context::decProperty(const char * name){
  NdbMutex_Lock(propertyMutexPtr);
  Uint32 val = 0;
  if(props.get(name, &val)){
    assert(val > 0);
    props.put(name, (val - 1), true);
  }
  NdbCondition_Broadcast(propertyCondPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

void
NDBT_Context::incProperty(const char * name){
  NdbMutex_Lock(propertyMutexPtr);
  Uint32 val = 0;
  props.get(name, &val);
  props.put(name, (val + 1), true);
  NdbCondition_Broadcast(propertyCondPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

Uint32
NDBT_Context::casProperty(const char * name, Uint32 oldValue, Uint32 newValue)
{
  NdbMutex_Lock(propertyMutexPtr);
  Uint32 val = 0;
  props.get(name, &val);
  if (val == oldValue)
  {
    props.put(name, newValue, true);
    NdbCondition_Broadcast(propertyCondPtr);
  }
  NdbMutex_Unlock(propertyMutexPtr);
  return val;
}

void  NDBT_Context::setProperty(const char* _name, const char* _val){ 
  NdbMutex_Lock(propertyMutexPtr);
  const bool b = props.put(_name, _val, true);
  assert(b == true);
  NdbCondition_Broadcast(propertyCondPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

void NDBT_Context::stopTest(){ 
  NdbMutex_Lock(propertyMutexPtr);
  g_info << "|- stopTest called" << endl;
  stopped = true;
  NdbCondition_Broadcast(propertyCondPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

bool NDBT_Context::isTestStopped(){ 
  NdbMutex_Lock(propertyMutexPtr);
  bool val = stopped;
  NdbMutex_Unlock(propertyMutexPtr);
  return val;
}

void NDBT_Context::wait(){
  NdbMutex_Lock(propertyMutexPtr);
  NdbCondition_Wait(propertyCondPtr,
		    propertyMutexPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

void NDBT_Context::wait_timeout(int msec){
  NdbMutex_Lock(propertyMutexPtr);
  NdbCondition_WaitTimeout(propertyCondPtr,
			   propertyMutexPtr,
			   msec);
  NdbMutex_Unlock(propertyMutexPtr);
}

void NDBT_Context::broadcast(){
  NdbMutex_Lock(propertyMutexPtr);
  NdbCondition_Broadcast(propertyCondPtr);
  NdbMutex_Unlock(propertyMutexPtr);
}

Uint32 NDBT_Context::getDbProperty(const char*){ 
  abort();
  return 0;
}

bool NDBT_Context::setDbProperty(const char*, Uint32){ 
  abort();
  return true;
}

void NDBT_Context::setTab(const NdbDictionary::Table* ptab){ 
  tables.clear();
  tables.push_back(ptab);
  tables.push_back(0);
}

void NDBT_Context::addTab(const NdbDictionary::Table* ptab){ 
  if(tables.size())
    tables.back() = ptab;
  else
    tables.push_back(ptab);

  tables.push_back(0);
}


const NdbDictionary::Table**
NDBT_Context::getTables()
{
  return tables.getBase();
}

void NDBT_Context::setSuite(NDBT_TestSuite* psuite){ 
  assert(psuite != NULL);
  suite = psuite;
}

void NDBT_Context::setCase(NDBT_TestCase* pcase){ 
  assert(pcase != NULL);
  testcase = pcase;
}

void NDBT_Context::setNumRecords(int _records){ 
  records = _records;
  
}

void NDBT_Context::setNumLoops(int _loops){ 
  loops = _loops;
}

NDBT_Step::NDBT_Step(NDBT_TestCase* ptest, const char* pname,
                     NDBT_TESTFUNC* pfunc) :
  m_ctx(NULL), name(pname), func(pfunc),
  testcase(ptest), step_no(-1), m_ndb(NULL)
{
}

#include "../../src/ndbapi/ndb_internal.hpp"

int
NDBT_Step::setUp(Ndb_cluster_connection& con){

  switch(testcase->getDriverType())
  {
  case DummyDriver:
    break;

  case NdbApiDriver:
  {
    m_ndb = new Ndb(&con, "TEST_DB" );
    m_ndb->init(1024);

    Ndb_internal::setForceShortRequests(m_ndb, 
                                        m_ctx->suite->getForceShort());

    int result = m_ndb->waitUntilReady(300); // 5 minutes
    if (result != 0){
      g_err << "Ndb was not ready" << endl;
      return NDBT_FAILED;
    }
    break;
  }

  default:
    abort();
    break;

  }

  return NDBT_OK;
}


void
NDBT_Step::tearDown(){
  delete m_ndb;
  m_ndb = NULL;
}


Ndb* NDBT_Step::getNdb() const {
  assert(m_ndb != NULL);
  return m_ndb;
}


int NDBT_Step::execute(NDBT_Context* ctx) {
  assert(ctx != NULL);

  int result;  

  g_info << "  |- " << name << " started [" << ctx->suite->getDate() << "]" 
	 << endl;
  
  result = setUp(ctx->m_cluster_connection);
  if (result != NDBT_OK){
    return result;
  }

  result = func(ctx, this);

  if (result != NDBT_OK) {
    g_err << "  |- " << name << " FAILED [" << ctx->suite->getDate() 
	   << "]" << endl;
  }	 
   else {
    g_info << "  |- " << name << " PASSED [" << ctx->suite->getDate() << "]"
	   << endl;
  }
  
  tearDown();
  
  return result;
}

void NDBT_Step::setContext(NDBT_Context* pctx){
  assert(pctx != NULL);
  m_ctx = pctx;
}

NDBT_Context* NDBT_Step::getContext(){
  assert(m_ctx != NULL);
  return m_ctx;
}


NDBT_ParallelStep::NDBT_ParallelStep(NDBT_TestCase* ptest, 
				     const char* pname, 
				     NDBT_TESTFUNC* pfunc)
  : NDBT_Step(ptest, pname, pfunc) {
}
NDBT_Verifier::NDBT_Verifier(NDBT_TestCase* ptest, 
			     const char* pname, 
			     NDBT_TESTFUNC* pfunc)
  : NDBT_Step(ptest, pname, pfunc) {
}
NDBT_Initializer::NDBT_Initializer(NDBT_TestCase* ptest, 
				   const char* pname, 
				   NDBT_TESTFUNC* pfunc)
  : NDBT_Step(ptest, pname, pfunc) {
}
NDBT_Finalizer::NDBT_Finalizer(NDBT_TestCase* ptest, 
			       const char* pname, 
			       NDBT_TESTFUNC* pfunc)
  : NDBT_Step(ptest, pname, pfunc) {
}

NDBT_TestCase::NDBT_TestCase(NDBT_TestSuite* psuite, 
			     const char* pname, 
			     const char* pcomment) : 
  _name(pname) ,
  _comment(pcomment),
  suite(psuite)
{
  assert(suite != NULL);

  m_all_tables = false;
  m_has_run = false;
}

NDBT_TestCaseImpl1::NDBT_TestCaseImpl1(NDBT_TestSuite* psuite, 
				       const char* pname, 
				       const char* pcomment) : 
  NDBT_TestCase(psuite, pname, pcomment){

  numStepsOk = 0;
  numStepsFail = 0;
  numStepsCompleted = 0;
  waitThreadsMutexPtr = NdbMutex_Create();
  waitThreadsCondPtr = NdbCondition_Create();

  m_driverType= psuite->getDriverType();
}

NDBT_TestCaseImpl1::~NDBT_TestCaseImpl1(){
  NdbCondition_Destroy(waitThreadsCondPtr);
  NdbMutex_Destroy(waitThreadsMutexPtr);
  size_t i;
  for(i = 0; i < initializers.size();  i++)
    delete initializers[i];
  initializers.clear();
  for(i = 0; i < verifiers.size();  i++)
    delete verifiers[i];
  verifiers.clear();
  for(i = 0; i < finalizers.size();  i++)
    delete finalizers[i];
  finalizers.clear();
  for(i = 0; i < steps.size();  i++)
    delete steps[i];
  steps.clear();
  results.clear();
  for(i = 0; i < testTables.size();  i++)
    delete testTables[i];
  testTables.clear();
  for(i = 0; i < testResults.size();  i++)
    delete testResults[i];
  testResults.clear();

}

int NDBT_TestCaseImpl1::addStep(NDBT_Step* pStep){
  assert(pStep != NULL);
  steps.push_back(pStep);
  pStep->setStepNo(steps.size());
  int res = NORESULT;
  results.push_back(res);
  return 0;
}

int NDBT_TestCaseImpl1::addVerifier(NDBT_Verifier* pVerifier){
  assert(pVerifier != NULL);
  verifiers.push_back(pVerifier);
  return 0;
}

int NDBT_TestCaseImpl1::addInitializer(NDBT_Initializer* pInitializer,
                                       bool first){
  assert(pInitializer != NULL);
  if (first)
    initializers.push(pInitializer, 0);
  else
    initializers.push_back(pInitializer);
  return 0;
}

int NDBT_TestCaseImpl1::addFinalizer(NDBT_Finalizer* pFinalizer){
  assert(pFinalizer != NULL);
  finalizers.push_back(pFinalizer);
  return 0;
}

void NDBT_TestCaseImpl1::addTable(const char* tableName, bool isVerify) {
  assert(tableName != NULL);
  const NdbDictionary::Table* pTable = NDBT_Tables::getTable(tableName);
  assert(pTable != NULL);
  testTables.push_back(pTable);
  isVerifyTables = isVerify;
}

bool NDBT_TestCaseImpl1::tableExists(NdbDictionary::Table* aTable) {
  for (unsigned i = 0; i < testTables.size(); i++) {
    if (strcasecmp(testTables[i]->getName(), aTable->getName()) == 0) {
      return true;
    }
  }
  return false;
}

bool NDBT_TestCaseImpl1::isVerify(const NdbDictionary::Table* aTable) {
  if (testTables.size() > 0) {
    int found = false;
    // OK, we either exclude or include this table in the actual test
    for (unsigned i = 0; i < testTables.size(); i++) {
      if (strcasecmp(testTables[i]->getName(), aTable->getName()) == 0) {
	// Found one!
	if (isVerifyTables) {
	  // Found one to test
	  found = true;
	} else {
	  // Skip this one!
	  found = false;
	}
      }
    } // for
    return found;
  } else {
    // No included or excluded test tables, i.e., all tables should be      
    // tested
    return true;
  }
  return true;
}

void  NDBT_TestCase::setProperty(const char* _name, Uint32 _val){ 
  const bool b = props.put(_name, _val);
  assert(b == true);
}

void  NDBT_TestCase::setProperty(const char* _name, const char* _val){ 
  const bool b = props.put(_name, _val);
  assert(b == true);
}


void *
runStep(void * s){
  assert(s != NULL);
  NDBT_Step* pStep = (NDBT_Step*)s;
  NDBT_Context* ctx = pStep->getContext();
  assert(ctx != NULL);
   // Execute function
  int res = pStep->execute(ctx);
  if(res != NDBT_OK){
    ctx->stopTest();
  }
  // Report 
  NDBT_TestCaseImpl1* pCase = (NDBT_TestCaseImpl1*)ctx->getCase();
  assert(pCase != NULL);
  pCase->reportStepResult(pStep, res);
  return NULL;
}

extern "C" 
void *
runStep_C(void * s)
{
  runStep(s);
  return NULL;
}


void NDBT_TestCaseImpl1::startStepInThread(int stepNo, NDBT_Context* ctx){  
  NDBT_Step* pStep = steps[stepNo];
  pStep->setContext(ctx);
  char buf[16];
  BaseString::snprintf(buf, sizeof(buf), "step_%d", stepNo);
  Uint32 stackSize = ctx->getProperty(NDBT_TestCase::getStepThreadStackSizePropName(), 
                                      Uint32(0));

  NdbThread* pThread = NdbThread_Create(runStep_C,
					(void**)pStep,
                                        stackSize,
					buf, 
					NDB_THREAD_PRIO_LOW);
  threads.push_back(pThread);
}

void NDBT_TestCaseImpl1::waitSteps(){
  NdbMutex_Lock(waitThreadsMutexPtr);
  while(numStepsCompleted != steps.size())
    NdbCondition_Wait(waitThreadsCondPtr,
		     waitThreadsMutexPtr);

  unsigned completedSteps = 0;  
  unsigned i;
  for(i=0; i<steps.size(); i++){
    if (results[i] != NORESULT){
      completedSteps++;
      if (results[i] == NDBT_OK)
	numStepsOk++;
      else
	numStepsFail++;
    }       
  }
  assert(completedSteps == steps.size());
  assert(completedSteps == numStepsCompleted);
  
  NdbMutex_Unlock(waitThreadsMutexPtr);
  void *status;
  for(i=0; i<steps.size();i++){
    NdbThread_WaitFor(threads[i], &status);
    NdbThread_Destroy(&threads[i]);   
  }
  threads.clear();
}


int
NDBT_TestCaseImpl1::getNoOfRunningSteps() const {
  return steps.size() - getNoOfCompletedSteps();
}

int 
NDBT_TestCaseImpl1::getNoOfCompletedSteps() const {
  return numStepsCompleted;
}

void NDBT_TestCaseImpl1::reportStepResult(const NDBT_Step* pStep, int result){
  NdbMutex_Lock(waitThreadsMutexPtr);
  assert(pStep != NULL);
  for (unsigned i = 0; i < steps.size(); i++){
    if(steps[i] != NULL && steps[i] == pStep){
      results[i] = result;
      numStepsCompleted++;
    }
  }
  if(numStepsCompleted == steps.size()){
    NdbCondition_Signal(waitThreadsCondPtr);
  }
  NdbMutex_Unlock(waitThreadsMutexPtr);
}


int NDBT_TestCase::execute(NDBT_Context* ctx){
  int res;

  ndbout << "- " << _name << " started [" << ctx->suite->getDate()
	 << "]" << endl;

  ctx->setCase(this);

  // Copy test case properties to ctx
  Properties::Iterator it(&props);
  for(const char * key = it.first(); key != 0; key = it.next()){

    PropertiesType pt;
    const bool b = props.getTypeOf(key, &pt);
    assert(b == true);
    switch(pt){
    case PropertiesType_Uint32:{
      Uint32 val;
      props.get(key, &val);
      ctx->setProperty(key, val);
      break;
    }
    case PropertiesType_char:{
      const char * val;
      props.get(key, &val);
      ctx->setProperty(key, val);
      break;
    }
    default:
      abort();
    }
  }

  // start timer so that we get a time even if
  // test case consist only of initializer
  startTimer(ctx);
  
  if ((res = runInit(ctx)) == NDBT_OK){
    // If initialiser is ok, run steps
    
    res = runSteps(ctx);
    if (res == NDBT_OK){
      // If steps is ok, run verifier
      res = runVerifier(ctx);
    } 
    
  }

  stopTimer(ctx);
  printTimer(ctx);

  // Always run finalizer to clean up db
  runFinal(ctx); 

  if (res == NDBT_OK) {
    ndbout << "- " << _name << " PASSED [" << ctx->suite->getDate() << "]"
	   << endl;
  }
  else {
    ndbout << "- " << _name << " FAILED [" << ctx->suite->getDate() << "]"
	   << endl;
  }
  return res;
}

void NDBT_TestCase::startTimer(NDBT_Context* ctx){
  timer.doStart();
}

void NDBT_TestCase::stopTimer(NDBT_Context* ctx){
  timer.doStop();
}

void NDBT_TestCase::printTimer(NDBT_Context* ctx){
  if (suite->timerIsOn()){
    g_info << endl; 
    timer.printTestTimer(ctx->getNumLoops(), ctx->getNumRecords());
  }
}

int NDBT_TestCaseImpl1::runInit(NDBT_Context* ctx){
  int res = NDBT_OK;
  for (unsigned i = 0; i < initializers.size(); i++){
    initializers[i]->setContext(ctx);
    res = initializers[i]->execute(ctx);
    if (res != NDBT_OK)
      break;
  }
  return res;
}

int NDBT_TestCaseImpl1::runSteps(NDBT_Context* ctx){
  int res = NDBT_OK;

  // Reset variables
  numStepsOk = 0;
  numStepsFail = 0;
  numStepsCompleted = 0;
  unsigned i;
  for (i = 0; i < steps.size(); i++)
    startStepInThread(i, ctx);
  waitSteps();

  for(i = 0; i < steps.size(); i++)
    if (results[i] != NDBT_OK)
      res = NDBT_FAILED;
  return res;
}

int NDBT_TestCaseImpl1::runVerifier(NDBT_Context* ctx){
  int res = NDBT_OK;
  for (unsigned i = 0; i < verifiers.size(); i++){
    verifiers[i]->setContext(ctx);
    res = verifiers[i]->execute(ctx);
    if (res != NDBT_OK)
      break;
  }
  return res;
}

int NDBT_TestCaseImpl1::runFinal(NDBT_Context* ctx){
  int res = NDBT_OK;
  for (unsigned i = 0; i < finalizers.size(); i++){
    finalizers[i]->setContext(ctx);
    res = finalizers[i]->execute(ctx);
    if (res != NDBT_OK)
      break;
  }
  return res;
}


void NDBT_TestCaseImpl1::saveTestResult(const char* test_name,
					int result){
  testResults.push_back(new NDBT_TestCaseResult(test_name,
						result,
						timer.elapsedTime()));
}

void NDBT_TestCaseImpl1::printTestResult(){

  char buf[255];
  ndbout << _name<<endl;

  for (unsigned i = 0; i < testResults.size(); i++){
    NDBT_TestCaseResult* tcr = testResults[i];
    const char* res = "<unknown>";
    if (tcr->getResult() == NDBT_OK)
      res = "OK";
    else if (tcr->getResult() == NDBT_FAILED)
      res = "FAIL";
    else if (tcr->getResult() == FAILED_TO_CREATE)
      res = "FAILED TO CREATE TABLE";
    else if (tcr->getResult() == FAILED_TO_DISCOVER)
      res = "FAILED TO DISCOVER TABLE";
    BaseString::snprintf(buf, 255," %-10s %-5s %-20s",
                         tcr->getName(), 
                         res, 
                         tcr->getTimeStr());
    ndbout << buf<<endl;
  }
}





NDBT_TestSuite::NDBT_TestSuite(const char* pname) :
  name(pname),
  m_createTable(true),
  m_createAll(false),
  m_connect_cluster(true),
  m_logging(true),
  m_driverType(NdbApiDriver)
{
   numTestsOk = 0;
   numTestsFail = 0;
   numTestsExecuted = 0;
   records = 0;
   loops = 0;
   diskbased = false;
   tsname = NULL;
   temporaryTables = false;
   runonce = false;
   m_noddl = false;
   m_forceShort = false;
}


NDBT_TestSuite::~NDBT_TestSuite(){
  for(unsigned i=0; i<tests.size(); i++){
    delete tests[i];
  }
  tests.clear();
}

void NDBT_TestSuite::setCreateTable(bool _flag){
  m_createTable = _flag;
}

void NDBT_TestSuite::setRunAllTables(bool _flag){
  runonce = _flag;
}
void NDBT_TestSuite::setCreateAllTables(bool _flag){
  m_createAll = _flag;
}
void NDBT_TestSuite::setConnectCluster(bool _flag){
  assert(m_createTable == false);
  m_connect_cluster = _flag;
}

void NDBT_TestSuite::setTemporaryTables(bool val){
  temporaryTables = val;
}

bool NDBT_TestSuite::getTemporaryTables() const {
  return temporaryTables;
}

void NDBT_TestSuite::setLogging(bool val){
  m_logging = val;
}

bool NDBT_TestSuite::getLogging() const {
  return m_logging;
}

bool NDBT_TestSuite::getForceShort() const {
  return m_forceShort;
}

bool NDBT_TestSuite::timerIsOn(){
  return (timer != 0);
}

int NDBT_TestSuite::addTest(NDBT_TestCase* pTest){
  assert(pTest != NULL);
  tests.push_back(pTest);
  return 0;
}

int NDBT_TestSuite::executeAll(Ndb_cluster_connection& con,
			       const char* _testname){

  if(tests.size() == 0)
    return NDBT_FAILED;

  ndbout << name << " started [" << getDate() << "]" << endl;

  if(!runonce)
  {
    testSuiteTimer.doStart();
    for (int t=0; t < NDBT_Tables::getNumTables(); t++){
      const NdbDictionary::Table* ptab = NDBT_Tables::getTable(t);
      ndbout << "|- " << ptab->getName() << endl;
      execute(con, ptab, _testname);
    }
    testSuiteTimer.doStop();
  }
  else
  {
    for (unsigned i = 0; i < tests.size(); i++){
      if (_testname != NULL && strcasecmp(tests[i]->getName(), _testname) != 0)
	continue;

      tests[i]->initBeforeTest();
      ctx = new NDBT_Context(con);

      ctx->setNumRecords(records);
      ctx->setNumLoops(loops);
      ctx->setSuite(this);
      ctx->setProperty("NoDDL", (Uint32) m_noddl);

      int result = tests[i]->execute(ctx);

      tests[i]->saveTestResult("", result);
      if (result != NDBT_OK)
	numTestsFail++;
      else
	numTestsOk++;
      numTestsExecuted++;

      delete ctx;
    }
  }
  return reportAllTables(_testname);
}

int 
NDBT_TestSuite::executeOne(Ndb_cluster_connection& con,
			   const char* _tabname, const char* _testname){

  if(tests.size() == 0)
    return NDBT_FAILED;

  ndbout << name << " started [" << getDate() << "]" << endl;

  const NdbDictionary::Table* ptab = NDBT_Tables::getTable(_tabname);
  if (ptab == NULL)
    return NDBT_FAILED;

  ndbout << "|- " << ptab->getName() << endl;

  execute(con, ptab, _testname);

  if (numTestsFail > 0){
    return NDBT_FAILED;
  }else{
    return NDBT_OK;
  }
}

int 
NDBT_TestSuite::executeOneCtx(Ndb_cluster_connection& con,
			   const NdbDictionary::Table *ptab, const char* _testname){
  
  testSuiteTimer.doStart(); 
  
  do{
    if(tests.size() == 0)
      break;

    Ndb ndb(&con, "TEST_DB");
    ndb.init(1024);

    Ndb_internal::setForceShortRequests(&ndb, m_forceShort);

    int result = ndb.waitUntilReady(300); // 5 minutes
    if (result != 0){
      g_err << name <<": Ndb was not ready" << endl;
      break;
    }

    ndbout << name << " started [" << getDate() << "]" << endl;
    ndbout << "|- " << ptab->getName() << endl;

    for (unsigned t = 0; t < tests.size(); t++){

      if (_testname != NULL && 
	      strcasecmp(tests[t]->getName(), _testname) != 0)
        continue;

      tests[t]->initBeforeTest();
    
      ctx = new NDBT_Context(con);
      ctx->setTab(ptab);
      ctx->setNumRecords(records);
      ctx->setNumLoops(loops);
      ctx->setSuite(this);
      ctx->setProperty("NoDDL", (Uint32) m_noddl);
    
      result = tests[t]->execute(ctx);
      if (result != NDBT_OK)
        numTestsFail++;
      else
        numTestsOk++;
      numTestsExecuted++;

    delete ctx;
  }

    if (numTestsFail > 0)
      break;
  }while(0);

  testSuiteTimer.doStop();
  int res = report(_testname);
  return NDBT_ProgramExit(res);
}

int
NDBT_TestSuite::createHook(Ndb* ndb, NdbDictionary::Table& tab, int when)
{
  if (when == 0) {
    if (diskbased) 
    {
      for (int i = 0; i < tab.getNoOfColumns(); i++) 
      {
        NdbDictionary::Column* col = tab.getColumn(i);
        if (! col->getPrimaryKey()) 
	{
          col->setStorageType(NdbDictionary::Column::StorageTypeDisk);
        }
      }
    }
    else if (temporaryTables)
    {
      tab.setTemporary(true);
      tab.setLogging(false);
    }
    
    if (tsname != NULL) {
      tab.setTablespaceName(tsname);
    }
  }
  return 0;
}

void NDBT_TestSuite::execute(Ndb_cluster_connection& con,
			     const NdbDictionary::Table* pTab, 
			     const char* _testname){
  int result; 

  for (unsigned t = 0; t < tests.size(); t++){

    if (_testname != NULL && 
	strcasecmp(tests[t]->getName(), _testname) != 0)
      continue;

    if (tests[t]->m_all_tables && tests[t]->m_has_run)
    {
      continue;
    }

    if (tests[t]->isVerify(pTab) == false) {
      continue;
    }

    tests[t]->initBeforeTest();

    ctx = new NDBT_Context(con);
    ctx->setNumRecords(records);
    ctx->setNumLoops(loops);
    ctx->setSuite(this);
    ctx->setTab(pTab);
    ctx->setProperty("NoDDL", (Uint32) m_noddl);

    result = tests[t]->execute(ctx);
    tests[t]->saveTestResult(pTab->getName(), result);
    if (result != NDBT_OK)
      numTestsFail++;
    else
      numTestsOk++;
    numTestsExecuted++;

    tests[t]->m_has_run = true;

    delete ctx;
  }
}



int
NDBT_TestSuite::createTables(Ndb_cluster_connection& con) const
{
  Ndb ndb(&con, "TEST_DB");
  ndb.init(1);

  NdbDictionary::Dictionary* pDict = ndb.getDictionary();
  for(unsigned i = 0; i<m_tables_in_test.size(); i++)
  {
    const char *tab_name=  m_tables_in_test[i].c_str();
    if (pDict->dropTable(tab_name) != 0 &&
        pDict->getNdbError().code != 723) // No such table
    {
      g_err << "runCreateTables: Failed to drop table " << tab_name << endl
            << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    if(NDBT_Tables::createTable(&ndb, tab_name, !getLogging()) != 0)
    {
      g_err << "runCreateTables: Failed to create table " << tab_name << endl
            << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }

    if (i == 0){
      // Update ctx with a pointer to the first created table
      const NdbDictionary::Table* pTab2 = pDict->getTable(tab_name);
      ctx->setTab(pTab2);
    }
    g_info << "created " << tab_name << endl;
  }

  return NDBT_OK;
}


static int
runCreateTables(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_TestSuite* suite= ctx->getSuite();
  return suite->createTables(ctx->m_cluster_connection);
}


static int
runCreateTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb ndb(&ctx->m_cluster_connection, "TEST_DB");
  ndb.init(1);

  NdbDictionary::Dictionary* pDict = ndb.getDictionary();
  const NdbDictionary::Table* pTab = ctx->getTab();
  const char *tab_name=  pTab->getName();
  if (pDict->dropTable(tab_name) != 0 &&
      pDict->getNdbError().code != 723) // No such table
  {
    g_err << "runCreateTable: Failed to drop table " << tab_name << endl
          << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  if(NDBT_Tables::createTable(&ndb, tab_name,
                              !ctx->getSuite()->getLogging()) != 0)
  {
    g_err << "runCreateTable: Failed to create table " << tab_name
          << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  // Update ctx with a pointer to the created table
  const NdbDictionary::Table* pTab2 = pDict->getTable(tab_name);
  ctx->setTab(pTab2);
  ctx->setProperty("$table", tab_name);

  return NDBT_OK;
}


int
NDBT_TestSuite::dropTables(Ndb_cluster_connection& con) const
{
  Ndb ndb(&con, "TEST_DB");
  ndb.init(1);

  NdbDictionary::Dictionary* pDict = ndb.getDictionary();
  for(unsigned i = 0; i<m_tables_in_test.size(); i++)
  {
    const char *tab_name=  m_tables_in_test[i].c_str();
    pDict->dropTable(tab_name);
  }
  return NDBT_OK;
}


static int
runDropTables(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_TestSuite* suite= ctx->getSuite();
  return suite->dropTables(ctx->m_cluster_connection);
}


static int
runDropTable(NDBT_Context* ctx, NDBT_Step* step)
{
  const char * tab_name = ctx->getProperty("$table", (const char*)0);
  if (tab_name)
  {
    Ndb ndb(&ctx->m_cluster_connection, "TEST_DB");
    ndb.init(1);
    
    NdbDictionary::Dictionary* pDict = ndb.getDictionary();
    pDict->dropTable(tab_name);
  }
  return NDBT_OK;
}


static int
runCheckTableExists(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb ndb(&ctx->m_cluster_connection, "TEST_DB");
  ndb.init(1);

  NdbDictionary::Dictionary* pDict = ndb.getDictionary();
  const NdbDictionary::Table* pTab = ctx->getTab();
  const char *tab_name=  pTab->getName();

  const NdbDictionary::Table* pDictTab = pDict->getTable(tab_name);

  if (pDictTab == NULL)
  {
    g_err << "runCheckTableExists : Failed to find table " 
          << tab_name << endl;
    g_err << "Required schema : " << *((NDBT_Table*)pTab) << endl;
    return NDBT_FAILED;
  }

  /* Todo : better check that table in DB is same as
   * table we expect
   */

  // Update ctx with a pointer to dict table
  ctx->setTab(pDictTab);
  ctx->setProperty("$table", tab_name);

  return NDBT_OK;
}

static int
runEmptyDropTable(NDBT_Context* ctx, NDBT_Step* step)
{
  return NDBT_OK;
}

int 
NDBT_TestSuite::report(const char* _tcname){
  int result;
  ndbout << "Completed " << name << " [" << getDate() << "]" << endl;
  printTestCaseSummary(_tcname);
  ndbout << numTestsExecuted << " test(s) executed" << endl;
  ndbout << numTestsOk << " test(s) OK" 
	 << endl;
  if(numTestsFail > 0)
    ndbout << numTestsFail << " test(s) failed"
	   << endl;
  testSuiteTimer.printTotalTime();
  if (numTestsFail > 0 || numTestsExecuted == 0){
    result = NDBT_FAILED;
  }else{
    result = NDBT_OK;
  }
  return result;
}

void NDBT_TestSuite::printTestCaseSummary(const char* _tcname){
  ndbout << "= SUMMARY OF TEST EXECUTION ==============" << endl;
  for (unsigned t = 0; t < tests.size(); t++){
    if (_tcname != NULL && 
	strcasecmp(tests[t]->getName(), _tcname) != 0)
      continue;

    tests[t]->printTestResult();
  }
  ndbout << "==========================================" << endl;
}

int NDBT_TestSuite::reportAllTables(const char* _testname){
  int result;
  ndbout << "Completed running test [" << getDate() << "]" << endl;
  const int totalNumTests = numTestsExecuted;
  printTestCaseSummary(_testname);
  ndbout << numTestsExecuted<< " test(s) executed" << endl;
  ndbout << numTestsOk << " test(s) OK("
	 <<(int)(((float)numTestsOk/totalNumTests)*100.0) <<"%)" 
	 << endl;
  if(numTestsFail > 0)
    ndbout << numTestsFail << " test(s) failed("
	   <<(int)(((float)numTestsFail/totalNumTests)*100.0) <<"%)" 
	   << endl;
  testSuiteTimer.printTotalTime();
  if (numTestsExecuted > 0){
    if (numTestsFail > 0){
      result = NDBT_FAILED;
    }else{
      result = NDBT_OK;
    }
  } else {
    result = NDBT_FAILED;
  }
  return result;
}

static int opt_print = false;
static int opt_print_html = false;
static int opt_print_cases = false;
static int opt_records;
static int opt_loops;
static int opt_timer;
static char * opt_testname = NULL;
static int opt_verbose;
unsigned opt_seed = 0;
static int opt_nologging = 0;
static int opt_temporary = 0;
static int opt_noddl = 0;
static int opt_forceShort = 0;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS(""),
  { "print", NDB_OPT_NOSHORT, "Print execution tree",
    (uchar **) &opt_print, (uchar **) &opt_print, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_html", NDB_OPT_NOSHORT, "Print execution tree in html table format",
    (uchar **) &opt_print_html, (uchar **) &opt_print_html, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "print_cases", NDB_OPT_NOSHORT, "Print list of test cases",
    (uchar **) &opt_print_cases, (uchar **) &opt_print_cases, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "records", 'r', "Number of records", 
    (uchar **) &opt_records, (uchar **) &opt_records, 0,
    GET_INT, REQUIRED_ARG, 1000, 0, 0, 0, 0, 0 },
  { "loops", 'l', "Number of loops",
    (uchar **) &opt_loops, (uchar **) &opt_loops, 0,
    GET_INT, REQUIRED_ARG, 5, 0, 0, 0, 0, 0 },
  { "seed", NDB_OPT_NOSHORT, "Random seed",
    (uchar **) &opt_seed, (uchar **) &opt_seed, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "testname", 'n', "Name of test to run",
    (uchar **) &opt_testname, (uchar **) &opt_testname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "timer", 't', "Print execution time",
    (uchar **) &opt_timer, (uchar **) &opt_timer, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", 'v', "Print verbose status",
    (uchar **) &opt_verbose, (uchar **) &opt_verbose, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "temporary-tables", 'T', "Create temporary table(s)",
    (uchar **) &opt_temporary, (uchar **) &opt_temporary, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nologging", NDB_OPT_NOSHORT, "Create table(s) wo/ logging",
    (uchar **) &opt_nologging, (uchar **) &opt_nologging, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "noddl", NDB_OPT_NOSHORT,
    "Don't create/drop tables as part of running tests",
    (uchar**) &opt_noddl, (uchar**) &opt_noddl, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "forceshortreqs", NDB_OPT_NOSHORT, "Use short signals for NdbApi requests",
    (uchar**) &opt_forceShort, (uchar**) &opt_forceShort, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

extern int global_flag_skip_invalidate_cache;

const char *load_default_groups[]= { "mysql_cluster",0 };

static void short_usage_sub(void)
{
  ndb_short_usage_sub("[tabname1 tabname2 ... tabnameN]");
}
static void usage()
{
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

int NDBT_TestSuite::execute(int argc, const char** argv){
  int res = NDBT_FAILED;
  /* Arguments:
       Run only a subset of tests
       -n testname Which test to run
       Recommendations to test functions:
       --records Number of records to use(default: 10000)
       --loops Number of loops to execute in the test(default: 100)

       Other parameters should:
       * be calculated from the above two parameters 
       * be divided into different test cases, ex. one testcase runs
         with FragmentType = Single and another perfoms the same 
         test with FragmentType = Large
       * let the test case iterate over all/subset of appropriate parameters
         ex. iterate over FragmentType = Single to FragmentType = AllLarge

       Remeber that the intention is that it should be _easy_ to run 
       a complete test suite without any greater knowledge of what 
       should be tested ie. keep arguments at a minimum
  */

  char **_argv= (char **)argv;

  if (!my_progname)
    my_progname= _argv[0];

  ndb_opt_set_usage_funcs(short_usage_sub, usage);

  load_defaults("my",load_default_groups,&argc,&_argv);

  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:i:F:L";
#endif
  if ((ho_error=handle_options(&argc, &_argv, my_long_options,
			       ndb_std_get_one_option)))
  {
    usage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  if (opt_verbose)
    setOutputLevel(2); // Show g_info
  else 
    setOutputLevel(0); // Show only g_err ?

  records = opt_records;
  loops = opt_loops;
  timer = opt_timer;
  if (opt_nologging)
    setLogging(false);
  temporaryTables = opt_temporary;
  m_noddl = opt_noddl;
  m_forceShort = opt_forceShort;

  if (opt_seed == 0)
  {
    opt_seed = (unsigned)NdbTick_CurrentMillisecond();
  }
  ndbout_c("random seed: %u", opt_seed);
  srand(opt_seed);
  srandom(opt_seed);

  global_flag_skip_invalidate_cache = 1;

  int num_tables= argc;
  if (argc == 0)
    num_tables = NDBT_Tables::getNumTables();

  for(int i = 0; i<num_tables; i++)
  {
    if (argc == 0)
      m_tables_in_test.push_back(NDBT_Tables::getTable(i)->getName());
    else
      m_tables_in_test.push_back(_argv[i]);
  }

  if (m_createTable)
  {
    for (unsigned t = 0; t < tests.size(); t++)
    {
      const char* createFuncName= NULL;
      NDBT_TESTFUNC* createFunc= NULL;
      const char* dropFuncName= NULL;
      NDBT_TESTFUNC* dropFunc= NULL;

      if (!m_noddl)
      {
        createFuncName= m_createAll ? "runCreateTable" : "runCreateTable";
        createFunc=   m_createAll ? &runCreateTables : &runCreateTable;
        dropFuncName= m_createAll ? "runDropTables" : "runDropTable";
        dropFunc= m_createAll ? &runDropTables : &runDropTable;
      }
      else
      {
        /* No DDL allowed, so we substitute 'do nothing' variants
         * of the create + drop table test procs
         */
        createFuncName= "runCheckTableExists";
        createFunc= &runCheckTableExists;
        dropFuncName= "runEmptyDropTable";
        dropFunc= &runEmptyDropTable;
      }

      NDBT_TestCaseImpl1* pt= (NDBT_TestCaseImpl1*)tests[t];
      NDBT_Initializer* pti =
        new NDBT_Initializer(pt,
                             createFuncName,
                             *createFunc);
      pt->addInitializer(pti, true);
      NDBT_Finalizer* ptf =
        new NDBT_Finalizer(pt,
                           dropFuncName,
                           *dropFunc);
      pt->addFinalizer(ptf);
    }
  }

  if (opt_print == true){
    printExecutionTree();
    return 0;
  }

  if (opt_print_html == true){
    printExecutionTreeHTML();
    return 0;
  }

  if (opt_print_cases == true){
    printCases();
    return 0;
  }

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  if(m_connect_cluster && con.connect(12, 5, 1))
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if(argc == 0){
    // No table specified
    res = executeAll(con, opt_testname);
  } else {
    testSuiteTimer.doStart(); 
    for(int i = 0; i<argc; i++){
      executeOne(con, _argv[i], opt_testname);
    }
    testSuiteTimer.doStop();
    res = report(opt_testname);
  }

  return NDBT_ProgramExit(res);
}


void NDBT_TestSuite::printExecutionTree(){
  ndbout << "Testsuite: " << name << endl;
  for (unsigned t = 0; t < tests.size(); t++){
    tests[t]->print();
    ndbout << endl;
  } 
}

void NDBT_TestSuite::printExecutionTreeHTML(){
  ndbout << "<tr>" << endl;
  ndbout << "<td><h3>" << name << "</h3></td>" << endl;
  ndbout << "</tr>" << endl;
  for (unsigned t = 0; t < tests.size(); t++){
    tests[t]->printHTML();
    ndbout << endl;
  } 

}

void NDBT_TestSuite::printCases(){
  ndbout << "# Testsuite: " << name << endl;
  ndbout << "# Number of tests: " << tests.size() << endl; 
  for (unsigned t = 0; t < tests.size(); t++){
    ndbout << name << " -n " << tests[t]->getName() << endl;
  } 
}

const char* NDBT_TestSuite::getDate(){
  static char theTime[128];
  struct tm* tm_now;
  time_t now;
  now = time((time_t*)NULL);
#ifdef NDB_WIN32
  tm_now = localtime(&now);
#else
  tm_now = gmtime(&now);
#endif
  
  BaseString::snprintf(theTime, 128,
	   "%d-%.2d-%.2d %.2d:%.2d:%.2d",
	   tm_now->tm_year + 1900, 
	   tm_now->tm_mon + 1, 
	   tm_now->tm_mday,
	   tm_now->tm_hour,
	   tm_now->tm_min,
	   tm_now->tm_sec);

  return theTime;
}

void NDBT_TestCaseImpl1::printHTML(){

  ndbout << "<tr><td>&nbsp;</td>" << endl;
  ndbout << "<td name=tc>" << endl << _name << "</td><td width=70%>" 
	 << _comment << "</td></tr>" << endl;  
}

void NDBT_TestCaseImpl1::print(){
  ndbout << "Test case: " << _name << endl;
  ndbout << "Description: "<< _comment << endl;

  ndbout << "Parameters: " << endl;

  Properties::Iterator it(&props);
  for(const char * key = it.first(); key != 0; key = it.next()){
    PropertiesType pt;
    const bool b = props.getTypeOf(key, &pt);
    assert(b == true);
    switch(pt){
    case PropertiesType_Uint32:{
      Uint32 val;
      props.get(key, &val);
      ndbout << "      " << key << ": " << val << endl;
      break;
    }
    case PropertiesType_char:{
      const char * val;
      props.get(key, &val);
      ndbout << "    " << key << ": " << val << endl;
      break;
    }
    default:
      abort();
    }  
  }
  unsigned i; 
  for(i=0; i<initializers.size(); i++){
    ndbout << "Initializers[" << i << "]: " << endl;
    initializers[i]->print();
  }
  for(i=0; i<steps.size(); i++){
    ndbout << "Step[" << i << "]: " << endl;
    steps[i]->print();
  }
  for(i=0; i<verifiers.size(); i++){
    ndbout << "Verifier[" << i << "]: " << endl;
    verifiers[i]->print();
  }
  for(i=0; i<finalizers.size(); i++){
    ndbout << "Finalizer[" << i << "]: " << endl;
    finalizers[i]->print();
  }
      
}

void NDBT_Step::print(){
  ndbout << "      "<< name << endl;

}

void
NDBT_Context::sync_down(const char * key){
  Uint32 threads = getProperty(key, (unsigned)0);
  if(threads){
    decProperty(key);
  }
}

void
NDBT_Context::sync_up_and_wait(const char * key, Uint32 value){
  setProperty(key, value);
  getPropertyWait(key, (unsigned)0);
}

template class Vector<NDBT_TestCase*>;
template class Vector<NDBT_TestCaseResult*>;
template class Vector<NDBT_Step*>;
template class Vector<NdbThread*>;
template class Vector<NDBT_Verifier*>;
template class Vector<NDBT_Initializer*>;
template class Vector<NDBT_Finalizer*>;
template class Vector<const NdbDictionary::Table*>;
