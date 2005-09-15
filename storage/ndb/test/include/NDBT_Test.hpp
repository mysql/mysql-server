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

#ifndef NDBT_TEST_HPP
#define NDBT_TEST_HPP

#include <ndb_global.h>

#include "NDBT_ReturnCodes.h"
#include <Properties.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbCondition.h>
#include <NdbTimer.hpp>
#include <Vector.hpp>
#include <NdbApi.hpp>
#include <NdbDictionary.hpp>

class NDBT_Step;
class NDBT_TestCase;
class NDBT_TestSuite;
class NDBT_TestCaseImpl1;

class NDBT_Context {
public:
  Ndb_cluster_connection& m_cluster_connection;
  
  NDBT_Context(Ndb_cluster_connection&);
  ~NDBT_Context();
  const NdbDictionary::Table* getTab();
  int getNumTables() const;
  const char * getTableName(int) const;
  NDBT_TestSuite* getSuite();
  NDBT_TestCase* getCase();

  // Get arguments
  int getNumRecords() const;
  int getNumLoops() const;
  char * getRemoteMgm() const;
  // Common place to store state between 
  // steps, for example information from one step to the 
  // verifier about how many records have been inserted
  Uint32 getProperty(const char*, Uint32 = 0 );
  const char* getProperty(const char*, const char* );
  void setProperty(const char*, Uint32);
  void setProperty(const char*, const char*);

  // Signal that a property value that another 
  // thread might be waiting for has changed
  void broadcast();
  // Wait for the signal that a property has changed
  void wait();
  void wait_timeout(int msec);

  // Wait until the property has been set to a certain value
  bool getPropertyWait(const char*, Uint32);
  const char* getPropertyWait(const char*, const char* );

  void decProperty(const char *);
  void incProperty(const char *);

  // Communicate with other tests
  void stopTest();
  bool isTestStopped();

  // Communicate with tests in other API nodes
  // This is done using a "system" table in the database
  Uint32 getDbProperty(const char*);
  bool setDbProperty(const char*, Uint32);

  void setTab(const NdbDictionary::Table*);
  void setRemoteMgm(char * mgm);

  /**
   * Get no of steps running/completed
   */
  int getNoOfRunningSteps() const ;
  int getNoOfCompletedSteps() const ;

  /**
   * Thread sync
   */
  void sync_down(const char * key);
  void sync_up_and_wait(const char * key, Uint32 count = 0);
private:
  friend class NDBT_Step;
  friend class NDBT_TestSuite;
  friend class NDBT_TestCase;
  friend class NDBT_TestCaseImpl1;

  void setSuite(NDBT_TestSuite*);
  void setCase(NDBT_TestCase*);
  void setNumRecords(int);
  void setNumLoops(int);
  const NdbDictionary::Table* tab;
  NDBT_TestSuite* suite;
  NDBT_TestCase* testcase;
  Ndb* ndb;
  int records;
  int loops;
  bool stopped;
  char * remote_mgm;
  Properties props;
  NdbMutex* propertyMutexPtr;
  NdbCondition* propertyCondPtr;
};

typedef int (NDBT_TESTFUNC)(NDBT_Context*, NDBT_Step*);

class NDBT_Step {
public:
  NDBT_Step(NDBT_TestCase* ptest, 
		const char* pname,
		NDBT_TESTFUNC* pfunc);
  virtual ~NDBT_Step() {}
  int execute(NDBT_Context*);
  virtual int setUp(Ndb_cluster_connection&) = 0;
  virtual void tearDown() = 0;
  void setContext(NDBT_Context*);
  NDBT_Context* getContext();
  void print();
  const char* getName() { return name; }
  int getStepNo() { return step_no; }
  void setStepNo(int n) { step_no = n; }
protected:
  NDBT_Context* m_ctx;
  const char* name;
  NDBT_TESTFUNC* func;
  NDBT_TestCase* testcase;
  int step_no;
};

class NDBT_NdbApiStep : public NDBT_Step {
public:
  NDBT_NdbApiStep(NDBT_TestCase* ptest,
		  const char* pname,
		  NDBT_TESTFUNC* pfunc);
  virtual ~NDBT_NdbApiStep() {}
  virtual int setUp(Ndb_cluster_connection&);
  virtual void tearDown();

  Ndb* getNdb();
protected:
  Ndb* ndb;
};

class NDBT_ParallelStep : public NDBT_NdbApiStep {
public:
  NDBT_ParallelStep(NDBT_TestCase* ptest,
		    const char* pname,
		    NDBT_TESTFUNC* pfunc);
  virtual ~NDBT_ParallelStep() {}
};

class NDBT_Verifier : public NDBT_NdbApiStep {
public:
  NDBT_Verifier(NDBT_TestCase* ptest,
		const char* name,
		NDBT_TESTFUNC* func);
  virtual ~NDBT_Verifier() {}
};

class NDBT_Initializer  : public NDBT_NdbApiStep {
public:
  NDBT_Initializer(NDBT_TestCase* ptest,
		   const char* name,
		   NDBT_TESTFUNC* func);
  virtual ~NDBT_Initializer() {}
};

class NDBT_Finalizer  : public NDBT_NdbApiStep {
public:
  NDBT_Finalizer(NDBT_TestCase* ptest,
		 const char* name,
		 NDBT_TESTFUNC* func);
  virtual ~NDBT_Finalizer() {}
};


class NDBT_TestCase {
public:
  NDBT_TestCase(NDBT_TestSuite* psuite, 
		const char* name, 
		const char* comment);
  virtual ~NDBT_TestCase() {}

  // This is the default executor of a test case
  // When a test case is executed it will need to be suplied with a number of 
  // different parameters and settings, these are passed to the test in the 
  // NDBT_Context object
  virtual int execute(NDBT_Context*);
  void setProperty(const char*, Uint32);
  void setProperty(const char*, const char*);
  virtual void print() = 0;
  virtual void printHTML() = 0;

  const char* getName(){return name;};
  virtual bool tableExists(NdbDictionary::Table* aTable) = 0;
  virtual bool isVerify(const NdbDictionary::Table* aTable) = 0;

  virtual void saveTestResult(const NdbDictionary::Table* ptab, int result) = 0;
  virtual void printTestResult() = 0;
  void initBeforeTest(){ timer.doReset();};

  /**
   * Get no of steps running/completed
   */
  virtual int getNoOfRunningSteps() const = 0;
  virtual int getNoOfCompletedSteps() const = 0;

  bool m_all_tables;
  bool m_has_run;

protected:
  virtual int runInit(NDBT_Context* ctx) = 0;
  virtual int runSteps(NDBT_Context* ctx) = 0;
  virtual int runVerifier(NDBT_Context* ctx) = 0;
  virtual int runFinal(NDBT_Context* ctx) = 0;
  virtual void addTable(const char* aTableName, bool isVerify=true) = 0;

  void startTimer(NDBT_Context*);
  void stopTimer(NDBT_Context*);
  void printTimer(NDBT_Context*);

  BaseString _name;
  BaseString _comment;
  const char* name;
  const char* comment;
  NDBT_TestSuite* suite;
  Properties props;
  NdbTimer timer;
  bool isVerifyTables;
};

static const int FAILED_TO_CREATE = 1000;
static const int FAILED_TO_DISCOVER = 1001;


class NDBT_TestCaseResult{
public: 
  NDBT_TestCaseResult(const char* name, int _result, NDB_TICKS _ticks):
    m_result(_result){
    m_name.assign(name); 
    m_ticks = _ticks;
    
  };
  const char* getName(){return m_name.c_str(); };
  int getResult(){return m_result; };
  const char* getTimeStr(){
      // Convert to Uint32 in order to be able to print it to screen
    Uint32 lapTime = (Uint32)m_ticks;
    Uint32 secTime = lapTime/1000;
    BaseString::snprintf(buf, 255, "%d secs (%d ms)", secTime, lapTime);
    return buf;
  }
private:
  char buf[255];
  int m_result;
  BaseString m_name;
  NDB_TICKS m_ticks;
};

class NDBT_TestCaseImpl1 : public NDBT_TestCase {
public:
  NDBT_TestCaseImpl1(NDBT_TestSuite* psuite, 
		const char* name, 
		const char* comment);
  virtual ~NDBT_TestCaseImpl1();
  int addStep(NDBT_Step*);
  int addVerifier(NDBT_Verifier*);
  int addInitializer(NDBT_Initializer*);
  int addFinalizer(NDBT_Finalizer*);
  void addTable(const char*, bool);
  bool tableExists(NdbDictionary::Table*);
  bool isVerify(const NdbDictionary::Table*);
  void reportStepResult(const NDBT_Step*, int result);
  //  int execute(NDBT_Context* ctx);
  int runInit(NDBT_Context* ctx);
  int runSteps(NDBT_Context* ctx);
  int runVerifier(NDBT_Context* ctx);
  int runFinal(NDBT_Context* ctx);
  void print();
  void printHTML();

  virtual int getNoOfRunningSteps() const;
  virtual int getNoOfCompletedSteps() const;
private:
  static const int  NORESULT = 999;
  
  void saveTestResult(const NdbDictionary::Table* ptab, int result);
  void printTestResult();

  void startStepInThread(int stepNo, NDBT_Context* ctx);
  void waitSteps();
  Vector<NDBT_Step*> steps;
  Vector<NdbThread*> threads;
  Vector<int> results;
  Vector<NDBT_Verifier*> verifiers; 
  Vector<NDBT_Initializer*> initializers; 
  Vector<NDBT_Finalizer*> finalizers; 
  Vector<const NdbDictionary::Table*> testTables; 
  Vector<NDBT_TestCaseResult*> testResults;
  unsigned numStepsFail;
  unsigned numStepsOk;
  unsigned numStepsCompleted;
  NdbMutex* waitThreadsMutexPtr;
  NdbCondition* waitThreadsCondPtr;
};


// A NDBT_TestSuite is a collection of TestCases
// the test suite will know how to execute the test cases
class NDBT_TestSuite {
public:
  NDBT_TestSuite(const char* name);
  ~NDBT_TestSuite();

  // Default executor of a test suite
  // supply argc and argv as parameters
  int execute(int, const char**);


  // These function can be used from main in the test program 
  // to control the behaviour of the testsuite
  void setCreateTable(bool);     // Create table before test func is called
  void setCreateAllTables(bool); // Create all tables before testsuite is executed 

  // Prints the testsuite, testcases and teststeps
  void printExecutionTree();
  void printExecutionTreeHTML();

  // Prints list of testcases
  void printCases();

  // Print summary of executed tests
  void printTestCaseSummary(const char* tcname = NULL);

  /**
   * Returns current date and time in the format of 2002-12-04 10:00:01
   */
  const char* getDate();

  // Returns true if timing info should be printed
  bool timerIsOn();


  int addTest(NDBT_TestCase* pTest);

  Vector<BaseString> m_tables_in_test;
private:
  int executeOne(Ndb_cluster_connection&,
		 const char* _tabname, const char* testname = NULL);
  int executeAll(Ndb_cluster_connection&,
		 const char* testname = NULL);
  void execute(Ndb_cluster_connection&,
	       Ndb*, const NdbDictionary::Table*, const char* testname = NULL);
  
  int report(const char* _tcname = NULL);
  int reportAllTables(const char* );
  const char* name;
  char* remote_mgm;
  int numTestsOk;
  int numTestsFail;
  int numTestsExecuted;
  Vector<NDBT_TestCase*> tests;
  NDBT_Context* ctx;
  int records;
  int loops;
  int timer;
  NdbTimer testSuiteTimer;
  bool createTable;
  bool createAllTables;
};



#define NDBT_TESTSUITE(suitname) \
class C##suitname : public NDBT_TestSuite { \
public: \
C##suitname():NDBT_TestSuite(#suitname){ \
 NDBT_TestCaseImpl1* pt; pt = NULL; \
 NDBT_Step* pts; pts = NULL; \
 NDBT_Verifier* ptv; ptv = NULL; \
 NDBT_Initializer* pti; pti = NULL; \
 NDBT_Finalizer* ptf; ptf = NULL; 

#define TESTCASE(testname, comment) \
  pt = new NDBT_TestCaseImpl1(this, testname, comment); \
  addTest(pt);

#define TC_PROPERTY(propname, propval) \
  pt->setProperty(propname, propval);

#define STEP(stepfunc) \
  pts = new NDBT_ParallelStep(pt, #stepfunc, stepfunc); \
  pt->addStep(pts);

// Add a number of equal steps to the testcase
#define STEPS(stepfunc, num) \
  { int i; for (i = 0; i < num; i++){ \
    pts = new NDBT_ParallelStep(pt, #stepfunc, stepfunc); \
    pt->addStep(pts);\
  } }

#define VERIFIER(stepfunc) \
  ptv = new NDBT_Verifier(pt, #stepfunc, stepfunc); \
  pt->addVerifier(ptv);

#define INITIALIZER(stepfunc) \
  pti = new NDBT_Initializer(pt, #stepfunc, stepfunc); \
  pt->addInitializer(pti);

#define FINALIZER(stepfunc) \
  ptf = new NDBT_Finalizer(pt, #stepfunc, stepfunc); \
  pt->addFinalizer(ptf);

// Test case can be run only on this table(s), can be multiple tables
// Ex TABLE("T1")
//    TABLE("T3")
// Means test will only be run on T1 and T3
#define TABLE(tableName) \
  pt->addTable(tableName, true);

// Test case can be run on all tables except
// Ex NOT_TABLE("T10")
// Means test will be run on all tables execept T10
#define NOT_TABLE(tableName) \
  pt->addTable(tableName, false);

// Text case will only be run once, not once per table as normally
#define ALL_TABLES() \
  pt->m_all_tables= true;

#define NDBT_TESTSUITE_END(suitname) \
 } } ; C##suitname suitname

// Helper functions for retrieving variables from NDBT_Step
#define GETNDB(ps) ((NDBT_NdbApiStep*)ps)->getNdb()

#endif
