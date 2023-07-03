/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NDBT_TEST_HPP
#define NDBT_TEST_HPP

#include <ndb_global.h>
#include <kernel/ndb_limits.h>

#include "NDBT_ReturnCodes.h"
#include <Properties.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbCondition.h>
#include <NdbTimer.hpp>
#include <Vector.hpp>
#include <NdbApi.hpp>
#include <NdbDictionary.hpp>
#include <ndb_rand.h>
#include "../../src/ndbapi/ndb_cluster_connection_impl.hpp"

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
  const NdbDictionary::Table** getTables();
  int getNumTables() const;
  const char * getTableName(int) const;
  NDBT_TestSuite* getSuite();
  NDBT_TestCase* getCase();

  // Get arguments
  int getNumRecords() const;
  int getNumLoops() const;

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
  Uint32 casProperty(const char *, Uint32 oldValue, Uint32 newValue);

  // Communicate with other tests
  void stopTest();
  bool isTestStopped();

  // Communicate with tests in other API nodes
  // This is done using a "system" table in the database
  Uint32 getDbProperty(const char*);
  bool setDbProperty(const char*, Uint32);

  void setTab(const NdbDictionary::Table*);
  void addTab(const NdbDictionary::Table*);

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

  /**
   * safety for slow machines...
   * 0 means no safety
   */
  bool closeToTimeout(int safety_percent = 0);

  /**
   * Get config by being friend to ndb_cluster_connection_impl - ugly
   */
  NdbApiConfig const& getConfig() const;

  /**
   * get a subrange of records - useful for splitting work amongst
   * threads and avoiding contention.
   */
  static
  void getRecordSubRange(int records,
                         int rangeCount,
                         int rangeId,
                         int& startRecord,
                         int& stopRecord);

private:
  friend class NDBT_Step;
  friend class NDBT_TestSuite;
  friend class NDBT_TestCase;
  friend class NDBT_TestCaseImpl1;

  void setSuite(NDBT_TestSuite*);
  void setCase(NDBT_TestCase*);
  void setNumRecords(int);
  void setNumLoops(int);
  Vector<const NdbDictionary::Table*> tables;
  NDBT_TestSuite* suite;
  NDBT_TestCase* testcase;
  Ndb* ndb;
  int records;
  int loops;
  bool stopped;
  Properties props;
  NdbMutex* propertyMutexPtr;
  NdbCondition* propertyCondPtr;

  int m_env_timeout;
  Uint64 m_test_start_time;
};

typedef int (NDBT_TESTFUNC)(NDBT_Context*, NDBT_Step*);

class NDBT_Step {
public:
  NDBT_Step(NDBT_TestCase* ptest,
            const char* pname,
            NDBT_TESTFUNC* pfunc);
  virtual ~NDBT_Step() {}
  int execute(NDBT_Context*);
  void setContext(NDBT_Context*);
  NDBT_Context* getContext();
  void print();
  const char* getName() { return name; }
  int getStepNo() { return step_no; }
  void setStepNo(int n) { step_no = n; }
  /* Parallel steps : Step x/y (x counting from 0) */
  int getStepTypeNo() { return step_type_no; }
  int getStepTypeCount() { return step_type_count; }
protected:
  NDBT_Context* m_ctx;
  const char* name;
  NDBT_TESTFUNC* func;
  NDBT_TestCase* testcase;
  int step_no;
  int step_type_no;
  int step_type_count;

private:
  int setUp(Ndb_cluster_connection&);
  void tearDown();
  Ndb* m_ndb;

public:
  Ndb* getNdb() const;

};

class NDBT_ParallelStep : public NDBT_Step {
public:
  NDBT_ParallelStep(NDBT_TestCase* ptest,
		    const char* pname,
		    NDBT_TESTFUNC* pfunc,
                    int num = 0,
                    int count = 1);
  ~NDBT_ParallelStep() override {}
};

class NDBT_Verifier : public NDBT_Step {
public:
  NDBT_Verifier(NDBT_TestCase* ptest,
		const char* name,
		NDBT_TESTFUNC* func);
  ~NDBT_Verifier() override {}
};

class NDBT_Initializer  : public NDBT_Step {
public:
  NDBT_Initializer(NDBT_TestCase* ptest,
		   const char* name,
		   NDBT_TESTFUNC* func);
  ~NDBT_Initializer() override {}
};

class NDBT_Finalizer  : public NDBT_Step {
public:
  NDBT_Finalizer(NDBT_TestCase* ptest,
		 const char* name,
		 NDBT_TESTFUNC* func);
  ~NDBT_Finalizer() override {}
};


enum NDBT_DriverType {
  DummyDriver,
  NdbApiDriver
};


class NDBT_TestCase {
public:
  NDBT_TestCase(NDBT_TestSuite* psuite, 
		const char* name, 
		const char* comment);
  virtual ~NDBT_TestCase() {}

  static const char* getStepThreadStackSizePropName()
    { return "StepThreadStackSize"; }

  // This is the default executor of a test case
  // When a test case is executed it will need to be supplied with a number of 
  // different parameters and settings, these are passed to the test in the 
  // NDBT_Context object
  virtual int execute(NDBT_Context*);
  void setProperty(const char*, Uint32);
  void setProperty(const char*, const char*);
  virtual void print() = 0;
  virtual void printHTML() = 0;

  const char* getName() const { return _name.c_str(); }
  virtual bool tableExists(NdbDictionary::Table* aTable) = 0;
  virtual bool isVerify(const NdbDictionary::Table* aTable) = 0;

  virtual void saveTestResult(const char*, int result) = 0;
  virtual void printTestResult() = 0;
  void initBeforeTest(){ timer.doReset();}

  void setDriverType(NDBT_DriverType type) { m_driverType= type; }
  NDBT_DriverType getDriverType() const { return m_driverType; }

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
  NDBT_TestSuite* suite;
  Properties props;
  NdbTimer timer;
  bool isVerifyTables;
  NDBT_DriverType m_driverType;
};

static const int FAILED_TO_CREATE = 1000;
static const int FAILED_TO_DISCOVER = 1001;


class NDBT_TestCaseResult{
public: 
  NDBT_TestCaseResult(const char* name, int _result, Uint64 _elapsed):
    m_result(_result){
    m_name.assign(name); 
    m_elapsed = _elapsed;
    
  }
  const char* getName(){return m_name.c_str(); }
  int getResult(){return m_result; }
  const char* getTimeStr(){
    // Convert to Uint32 in order to be able to print it to screen
    Uint32 lapTime = (Uint32)m_elapsed;
    Uint32 secTime = lapTime/1000;
    BaseString::snprintf(buf, 255, "%d secs (%d ms)", secTime, lapTime);
    return buf;
  }
private:
  char buf[255];
  int m_result;
  BaseString m_name;
  Uint64 m_elapsed;  // Milliseconds
};

class NDBT_TestCaseImpl1 : public NDBT_TestCase {
public:
  NDBT_TestCaseImpl1(NDBT_TestSuite* psuite, 
		const char* name, 
		const char* comment);
  ~NDBT_TestCaseImpl1() override;
  int addStep(NDBT_Step*);
  int addVerifier(NDBT_Verifier*);
  int addInitializer(NDBT_Initializer*, bool first= false);
  int addFinalizer(NDBT_Finalizer*);
  void addTable(const char*, bool) override;
  bool tableExists(NdbDictionary::Table*) override;
  bool isVerify(const NdbDictionary::Table*) override;
  void reportStepResult(const NDBT_Step*, int result);
  //  int execute(NDBT_Context* ctx);
  int runInit(NDBT_Context* ctx) override;
  int runSteps(NDBT_Context* ctx) override;
  int runVerifier(NDBT_Context* ctx) override;
  int runFinal(NDBT_Context* ctx) override;
  void print() override;
  void printHTML() override;

  int getNoOfRunningSteps() const override;
  int getNoOfCompletedSteps() const override;
private:
  static const int  NORESULT = 999;
  
  void saveTestResult(const char*, int result) override;
  void printTestResult() override;

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

  // NDBT's test tables are fixed and it always create 
  // and drop fixed table when execute, add this method 
  // in order to run CTX only and adapt to some new 
  // customized testsuite
  int executeOneCtx(Ndb_cluster_connection&,
		 const NdbDictionary::Table* ptab, const char* testname = NULL);

  // These function can be used from main in the test program 
  // to control the behaviour of the testsuite
  void setCreateTable(bool);     // Create table before test func is called
  void setCreateAllTables(bool); // Create all tables before testsuite is executed 
  void setRunAllTables(bool); // Run once with all tables
  void setConnectCluster(bool); // Connect to cluster before testsuite is executed

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
  const char* getDate(char* str, size_t len);

  // Returns true if timing info should be printed
  bool timerIsOn();

  int addTest(NDBT_TestCase* pTest);
  int addExplicitTest(NDBT_TestCase* pTest);

  // Table create tweaks
  int createHook(Ndb*, NdbDictionary::Table&, int when);
  Vector<BaseString> m_tables_in_test;

  void setTemporaryTables(bool val);
  bool getTemporaryTables() const;

  void setLogging(bool val);
  bool getLogging() const;

  bool getForceShort() const;

  void setEnsureIndexStatTables(bool val);

  int createTables(Ndb_cluster_connection&) const;
  int dropTables(Ndb_cluster_connection&) const;

  void setDriverType(NDBT_DriverType type) { m_driverType= type; }
  NDBT_DriverType getDriverType() const { return m_driverType; }

private:
  int executeOne(Ndb_cluster_connection&,
		 const char* _tabname, const char* testname = NULL);
  int executeAll(Ndb_cluster_connection&,
		 const char* testname = NULL);
  void execute(Ndb_cluster_connection&,
	       const NdbDictionary::Table*, const char* testname = NULL);

  void execute(Ndb_cluster_connection&, NDBT_TestCase*,
               const NdbDictionary::Table * pTab);

  int report(const char* _tcname = NULL);
  int reportAllTables(const char* );
  const char* name;
  char* remote_mgm;
  int numTestsOk;
  int numTestsFail;
  int numTestsSkipped;
  int numTestsExecuted;
  Vector<NDBT_TestCase*> tests;
  Vector<NDBT_TestCase*> explicitTests;
  NDBT_TestCase * findTest(const char * name, bool explicitOK = true);

  NDBT_Context* ctx;
  int records;
  int loops;
  int timer;
  NdbTimer testSuiteTimer;
  bool m_createTable;
  bool m_createAll;
  bool m_connect_cluster;
  bool diskbased;
  bool runonce;
  const char* tsname;
  bool temporaryTables;
  bool m_logging;
  NDBT_DriverType m_driverType;
  bool m_noddl;
  bool m_forceShort;
  bool m_ensureIndexStatTables;
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

// The default driver type to use for all tests in suite
#define DRIVER(type) \
  setDriverType(type)

#define TESTCASE(testname, comment) \
  pt = new NDBT_TestCaseImpl1(this, testname, comment); \
  addTest(pt);

#define X_TESTCASE(testname, comment) \
  pt = new NDBT_TestCaseImpl1(this, testname, comment); \
  addExplicitTest(pt);

// The driver type to use for a particular testcase
#define TESTCASE_DRIVER(type) \
  pt->setDriverType(type);

#define TC_PROPERTY(propname, propval) \
  pt->setProperty(propname, propval);

#define STEP(stepfunc) \
  pts = new NDBT_ParallelStep(pt, #stepfunc, stepfunc); \
  pt->addStep(pts);

// Add a number of equal steps to the testcase
#define STEPS(stepfunc, num) \
  { int i; for (i = 0; i < num; i++){ \
    pts = new NDBT_ParallelStep(pt, #stepfunc, stepfunc, i, num); \
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
 } } ; 

#define NDBT_TESTSUITE_INSTANCE(suitname) \
  C##suitname suitname

// Helper functions for retrieving variables from NDBT_Step
#define GETNDB(ps) ((NDBT_Step*)ps)->getNdb()

#define POSTUPGRADE(testname) \
  TESTCASE(testname "--post-upgrade", \
           "checks being run after upgrade has completed")

#endif
