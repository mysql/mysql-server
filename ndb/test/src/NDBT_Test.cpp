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

#include <ndb_global.h>
#include <my_sys.h>

#include "NDBT.hpp"
#include "NDBT_Test.hpp"

#include <PortDefs.h>

#include <getarg.h>
#include <time.h>

// No verbose outxput

NDBT_Context::NDBT_Context(){
  tab = NULL;
  suite = NULL;
  testcase = NULL;
  ndb = NULL;
  records = 1;
  loops = 1;
  stopped = false;
  remote_mgm ="";
  propertyMutexPtr = NdbMutex_Create();
  propertyCondPtr = NdbCondition_Create();
}

 
char * NDBT_Context::getRemoteMgm() const {
  return remote_mgm;
} 
void NDBT_Context::setRemoteMgm(char * mgm) {
  remote_mgm = strdup(mgm);
} 


NDBT_Context::~NDBT_Context(){
  NdbCondition_Destroy(propertyCondPtr);
  NdbMutex_Destroy(propertyMutexPtr);
}

const NdbDictionary::Table* NDBT_Context::getTab(){
  assert(tab != NULL);
  return tab;
}

NDBT_TestSuite* NDBT_Context::getSuite(){ 
  assert(suite != NULL);
  return suite;
}

NDBT_TestCase* NDBT_Context::getCase(){ 
  assert(testcase != NULL);
  return testcase;
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

void  NDBT_Context::setProperty(const char* _name, const char* _val){ 
  NdbMutex_Lock(propertyMutexPtr);
  const bool b = props.put(_name, _val);
  assert(b == true);
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
  assert(ptab != NULL);
  tab = ptab;
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
		     NDBT_TESTFUNC* pfunc): name(pname){
  assert(pfunc != NULL);
  func = pfunc;
  testcase = ptest;
  step_no = -1;
}

int NDBT_Step::execute(NDBT_Context* ctx) {
  assert(ctx != NULL);

  int result;  

  g_info << "  |- " << name << " started [" << ctx->suite->getDate() << "]" 
	 << endl;
  
  result = setUp();
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

NDBT_NdbApiStep::NDBT_NdbApiStep(NDBT_TestCase* ptest, 
			   const char* pname, 
			   NDBT_TESTFUNC* pfunc)
  : NDBT_Step(ptest, pname, pfunc),
    ndb(NULL) {
}


int
NDBT_NdbApiStep::setUp(){
  ndb = new Ndb( "TEST_DB" );
  ndb->init(1024);

  int result = ndb->waitUntilReady(300); // 5 minutes
  if (result != 0){
    g_err << name << ": Ndb was not ready" << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

void 
NDBT_NdbApiStep::tearDown(){
  delete ndb;
  ndb = NULL;
}

Ndb* NDBT_NdbApiStep::getNdb(){ 
  assert(ndb != NULL);
  return ndb;
}


NDBT_ParallelStep::NDBT_ParallelStep(NDBT_TestCase* ptest, 
				     const char* pname, 
				     NDBT_TESTFUNC* pfunc)
  : NDBT_NdbApiStep(ptest, pname, pfunc) {
}
NDBT_Verifier::NDBT_Verifier(NDBT_TestCase* ptest, 
			     const char* pname, 
			     NDBT_TESTFUNC* pfunc)
  : NDBT_NdbApiStep(ptest, pname, pfunc) {
}
NDBT_Initializer::NDBT_Initializer(NDBT_TestCase* ptest, 
				   const char* pname, 
				   NDBT_TESTFUNC* pfunc)
  : NDBT_NdbApiStep(ptest, pname, pfunc) {
}
NDBT_Finalizer::NDBT_Finalizer(NDBT_TestCase* ptest, 
			       const char* pname, 
			       NDBT_TESTFUNC* pfunc)
  : NDBT_NdbApiStep(ptest, pname, pfunc) {
}

NDBT_TestCase::NDBT_TestCase(NDBT_TestSuite* psuite, 
			     const char* pname, 
			     const char* pcomment) : 
  name(pname) ,
  comment(pcomment),
  suite(psuite){
  assert(suite != NULL);
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

int NDBT_TestCaseImpl1::addInitializer(NDBT_Initializer* pInitializer){
  assert(pInitializer != NULL);
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
  NdbThread_Exit(0);
  return NULL;
}


void NDBT_TestCaseImpl1::startStepInThread(int stepNo, NDBT_Context* ctx){  
  NDBT_Step* pStep = steps[stepNo];
  pStep->setContext(ctx);
  char buf[16];
  snprintf(buf, sizeof(buf), "step_%d", stepNo);
  NdbThread* pThread = NdbThread_Create(runStep_C,
					(void**)pStep,
					65535,
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

  ndbout << "- " << name << " started [" << ctx->suite->getDate()
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
    ndbout << "- " << name << " PASSED [" << ctx->suite->getDate() << "]" 
	   << endl;
  }
  else {
    ndbout << "- " << name << " FAILED [" << ctx->suite->getDate() << "]" 
	   << endl;
  }
  return res;
};


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


void NDBT_TestCaseImpl1::saveTestResult(const NdbDictionary::Table* ptab, 
					int result){
  testResults.push_back(new NDBT_TestCaseResult(ptab->getName(), 
						result,
						timer.elapsedTime()));
}

void NDBT_TestCaseImpl1::printTestResult(){

  char buf[255];
  ndbout << name<<endl;

  for (unsigned i = 0; i < testResults.size(); i++){
    NDBT_TestCaseResult* tcr = testResults[i];
    const char* res;
    if (tcr->getResult() == NDBT_OK)
      res = "OK";
    else if (tcr->getResult() == NDBT_FAILED)
      res = "FAIL";
    else if (tcr->getResult() == FAILED_TO_CREATE)
      res = "FAILED TO CREATE TABLE";
    else if (tcr->getResult() == FAILED_TO_DISCOVER)
      res = "FAILED TO DISCOVER TABLE";
    snprintf(buf, 255," %-10s %-5s %-20s", tcr->getName(), res, tcr->getTimeStr());
    ndbout << buf<<endl;    
  }
}





NDBT_TestSuite::NDBT_TestSuite(const char* pname):name(pname){
   numTestsOk = 0;
   numTestsFail = 0;
   numTestsExecuted = 0;
   records = 0;
   loops = 0;
   createTable = true;
}


NDBT_TestSuite::~NDBT_TestSuite(){
  for(unsigned i=0; i<tests.size(); i++){
    delete tests[i];
  }
  tests.clear();
}

void NDBT_TestSuite::setCreateTable(bool _flag){
  createTable = _flag;
}

bool NDBT_TestSuite::timerIsOn(){
  return (timer != 0);
}

int NDBT_TestSuite::addTest(NDBT_TestCase* pTest){
  assert(pTest != NULL);
  tests.push_back(pTest);
  return 0;
}

int NDBT_TestSuite::executeAll(const char* _testname){

  if(tests.size() == 0)
    return NDBT_FAILED;
  Ndb ndb("TEST_DB");
  ndb.init(1024);

  int result = ndb.waitUntilReady(300); // 5 minutes
  if (result != 0){
    g_err << name <<": Ndb was not ready" << endl;
    return NDBT_FAILED;
  }

  ndbout << name << " started [" << getDate() << "]" << endl;

  testSuiteTimer.doStart();

  for (int t=0; t < NDBT_Tables::getNumTables(); t++){
    const NdbDictionary::Table* ptab = NDBT_Tables::getTable(t);
    ndbout << "|- " << ptab->getName() << endl;
    execute(&ndb, ptab, _testname);
  }
  testSuiteTimer.doStop();
  return reportAllTables(_testname);
}

int 
NDBT_TestSuite::executeOne(const char* _tabname, const char* _testname){
  
  if(tests.size() == 0)
    return NDBT_FAILED;
  Ndb ndb("TEST_DB");
  ndb.init(1024);

  int result = ndb.waitUntilReady(300); // 5 minutes
  if (result != 0){
    g_err << name <<": Ndb was not ready" << endl;
    return NDBT_FAILED;
  }

  ndbout << name << " started [" << getDate() << "]" << endl;

  const NdbDictionary::Table* ptab = NDBT_Tables::getTable(_tabname);
  if (ptab == NULL)
    return NDBT_FAILED;

  ndbout << "|- " << ptab->getName() << endl;

  execute(&ndb, ptab, _testname);

  if (numTestsFail > 0){
    return NDBT_FAILED;
  }else{
    return NDBT_OK;
  }
}

void NDBT_TestSuite::execute(Ndb* ndb, const NdbDictionary::Table* pTab, 
			     const char* _testname){
  int result; 

 
  for (unsigned t = 0; t < tests.size(); t++){

    if (_testname != NULL && 
	strcasecmp(tests[t]->getName(), _testname) != 0)
      continue;

    if (tests[t]->isVerify(pTab) == false) {
      continue;
    }

    tests[t]->initBeforeTest();

    NdbDictionary::Dictionary* pDict = ndb->getDictionary();
    const NdbDictionary::Table* pTab2 = pDict->getTable(pTab->getName());
    if (createTable == true){

      if(pTab2 != 0 && pDict->dropTable(pTab->getName()) != 0){
	numTestsFail++;
	numTestsExecuted++;
	g_err << "ERROR0: Failed to drop table " << pTab->getName() << endl;
	tests[t]->saveTestResult(pTab, FAILED_TO_CREATE);
	continue;
      }
      
      if(NDBT_Tables::createTable(ndb, pTab->getName()) != 0){
	numTestsFail++;
	numTestsExecuted++;
	g_err << "ERROR1: Failed to create table " << pTab->getName()
              << pDict->getNdbError() << endl;
	tests[t]->saveTestResult(pTab, FAILED_TO_CREATE);
	continue;
      }
      pTab2 = pDict->getTable(pTab->getName());
    } else {
      pTab2 = pTab;
    }
    
    ctx = new NDBT_Context();
    ctx->setTab(pTab2);
    ctx->setNumRecords(records);
    ctx->setNumLoops(loops);
    if(remote_mgm != NULL)
      ctx->setRemoteMgm(remote_mgm);
    ctx->setSuite(this);

    result = tests[t]->execute(ctx);
    tests[t]->saveTestResult(pTab, result);
    if (result != NDBT_OK)
      numTestsFail++;
    else
      numTestsOk++;
    numTestsExecuted++;

    if (result == NDBT_OK && createTable == true){
      pDict->dropTable(pTab->getName());
    }
    
    delete ctx;
  }
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
  int _records = 1000;
  int _loops = 5;
  int _timer = 0;
  char * _remote_mgm =NULL;
  char* _testname = NULL;
  const char* _tabname = NULL;
  int _print = false;
  int _print_html = false;

  int _print_cases = false;
  int _verbose = false;
#ifndef DBUG_OFF
  const char *debug_option= 0;
#endif

  struct getargs args[] = {
    { "print", '\0', arg_flag, &_print, "Print execution tree", "" },
    { "print_html", '\0', arg_flag, &_print_html, "Print execution tree in html table format", "" },
    { "print_cases", '\0', arg_flag, &_print_cases, "Print list of test cases", "" },
    { "records", 'r', arg_integer, &_records, "Number of records", "records" },
    { "loops", 'l', arg_integer, &_loops, "Number of loops", "loops" },
    { "testname", 'n', arg_string, &_testname, "Name of test to run", "testname" },
    { "remote_mgm", 'm', arg_string, &_remote_mgm, 
      "host:port to mgmsrv of remote cluster", "host:port" },
    { "timer", 't', arg_flag, &_timer, "Print execution time", "time" },
#ifndef DBUG_OFF
    { "debug", 0, arg_string, &debug_option,
      "Specify debug options e.g. d:t:i:o,out.trace", "options" },
#endif
    { "verbose", 'v', arg_flag, &_verbose, "Print verbose status", "verbose" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;

  if(getarg(args, num_args, argc, argv, &optind)) {
    arg_printusage(args, num_args, argv[0], "tabname1 tabname2 ... tabnameN\n");
    return NDBT_WRONGARGS;
  }

#ifndef DBUG_OFF
  my_init();
  if (debug_option)
    DBUG_PUSH(debug_option);
#endif

  // Check if table name is supplied
  if (argv[optind] != NULL)
    _tabname = argv[optind];

  if (_print == true){
    printExecutionTree();
    return 0;
  }

  if (_print_html == true){
    printExecutionTreeHTML();
    return 0;
  }

  if (_print_cases == true){
    printCases();
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if (_verbose)
    setOutputLevel(2); // Show g_info
  else 
   setOutputLevel(0); // Show only g_err ?

  remote_mgm = _remote_mgm;
  records = _records;
  loops = _loops;
  timer = _timer;

  if(optind == argc){
    // No table specified
    res = executeAll(_testname);
  } else {
    testSuiteTimer.doStart(); 
    Ndb ndb("TEST_DB"); ndb.init();
    for(int i = optind; i<argc; i++){
      executeOne(argv[i], _testname);
    }
    testSuiteTimer.doStop();
    res = report(_testname);
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
  
  snprintf(theTime, 128,
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
  ndbout << "<td name=tc>" << endl << name << "</td><td width=70%>" 
	 << comment << "</td></tr>" << endl;  
}

void NDBT_TestCaseImpl1::print(){
  ndbout << "Test case: " << name << endl;
  ndbout << "Description: "<< comment << endl;

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

template class Vector<NDBT_TestCase*>;
template class Vector<NDBT_TestCaseResult*>;
template class Vector<NDBT_Step*>;
template class Vector<NdbThread*>;
template class Vector<NDBT_Verifier*>;
template class Vector<NDBT_Initializer*>;
template class Vector<NDBT_Finalizer*>;
template class Vector<const NdbDictionary::Table*>;
