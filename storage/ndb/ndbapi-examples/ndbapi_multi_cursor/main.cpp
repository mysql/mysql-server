/* Copyright (C) 2008 Sun Microsystems Inc.

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

/**************************************************************
 *
 * NOTE THAT THIS TOOL CAN ONLY BE RUN AGAINST THE EMPLOYEES DATABASE 
 * TABLES WITH WHICH IS A SEPERATE DOWNLOAD AVAILABLE AT WWW.MYSQL.COM.
 **************************************************************/


#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>
// Used for cout
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"

/**
 * Helper debugging macros
 */
#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(-1); }
#define APIERROR(error) { \
  PRINT_ERROR((error).code,(error).message); \
  exit(-1); }



/**
 * Define NDB_CONNECT_STRING if you don't connect through the default localhost:1186
 */
  #define NDB_CONNECT_STRING "loki43:2360"


/*****************************************************
** Defines record structure for the rows in our tables
******************************************************/
struct ManagerRow
{
  char   dept_no[1+4+1];
  Uint32 emp_no;
  Int32  from_date;
  Int32  to_date;
};

struct EmployeeRow
{
  Uint32 emp_no;
  Int32  birth_date;  // sizeof(date)....?
  char   first_name[14+1];
  char   last_name[16+1];
  char   gender;
  Int32  hire_date;
};

struct SalaryRow
{
  Uint32 emp_no;
  Int32  from_date;
  Uint32 salary;
  Int32  to_date;
};


 NdbQuery*
 NdbTransaction::createQuery(const NdbQueryDef* def,
              const void* const param[],
              NdbOperation::LockMode lock_mode)
{
  NdbQuery* query = NdbQuery::buildQuery(*this, *def);

  return query;
}

const char* employeeDef = 
"CREATE TABLE employees ("
"    emp_no      INT             NOT NULL,"
"    birth_date  DATE            NOT NULL,"
"    first_name  VARCHAR(14)     NOT NULL,"
"    last_name   VARCHAR(16)     NOT NULL,"
"    gender      ENUM ('M','F')  NOT NULL,  "  
"    hire_date   DATE            NOT NULL,"
"    PRIMARY KEY (emp_no))"
" ENGINE=NDB";

const char* departmentsDef = 
"CREATE TABLE departments ("
"    dept_no     CHAR(4)         NOT NULL,"
"    dept_name   VARCHAR(40)     NOT NULL,"
"    PRIMARY KEY (dept_no),"
"    UNIQUE  KEY (dept_name))"
" ENGINE=NDB";

const char* dept_managerDef = 
"CREATE TABLE dept_manager ("
"   dept_no      CHAR(4)         NOT NULL,"
"   emp_no       INT             NOT NULL,"
"   from_date    DATE            NOT NULL,"
"   to_date      DATE            NOT NULL,"
"   KEY         (emp_no),"
"   KEY         (dept_no),"
"   FOREIGN KEY (emp_no)  REFERENCES employees (emp_no)    ON DELETE CASCADE,"
"   FOREIGN KEY (dept_no) REFERENCES departments (dept_no) ON DELETE CASCADE,"
"   PRIMARY KEY (emp_no,dept_no))"
" ENGINE=NDB";

const char* dept_empDef = 
"CREATE TABLE dept_emp ("
"    emp_no      INT             NOT NULL,"
"    dept_no     CHAR(4)         NOT NULL,"
"    from_date   DATE            NOT NULL,"
"    to_date     DATE            NOT NULL,"
"    KEY         (emp_no),"
"    KEY         (dept_no),"
"    FOREIGN KEY (emp_no)  REFERENCES employees   (emp_no)  ON DELETE CASCADE,"
"    FOREIGN KEY (dept_no) REFERENCES departments (dept_no) ON DELETE CASCADE,"
"    PRIMARY KEY (emp_no,dept_no))"
" ENGINE=NDB";

const char* titlesDef =
"CREATE TABLE titles ("
"    emp_no      INT             NOT NULL,"
"    title       VARCHAR(50)     NOT NULL,"
"    from_date   DATE            NOT NULL,"
"    to_date     DATE,"
"    KEY         (emp_no),"
"    FOREIGN KEY (emp_no) REFERENCES employees (emp_no) ON DELETE CASCADE,"
"    PRIMARY KEY (emp_no,title, from_date))"
" ENGINE=NDB";

const char* salariesDef =
"CREATE TABLE salaries ("
"    emp_no      INT             NOT NULL,"
"    salary      INT             NOT NULL,"
"    from_date   DATE            NOT NULL,"
"    to_date     DATE            NOT NULL,"
"    KEY         (emp_no),"
"    FOREIGN KEY (emp_no) REFERENCES employees (emp_no) ON DELETE CASCADE,"
"    PRIMARY KEY (emp_no, from_date))"
" ENGINE=NDB";


int createEmployeeDb()
{
  /**************************************************************
   * Connect to mysql server and create testDB                  *
   **************************************************************/
  if (true)
  {
    MYSQL mysql;
    if ( !mysql_init(&mysql) ) {
      std::cout << "mysql_init failed\n";
      exit(-1);
    }
//    mysql_options(&mysql, MYSQL_READ_DEFAULT_FILE, "/home/oa136780/mysql/mysql-5.1-telco-7.0-spj/install/config/my.cnf");

    const char *mysqld_sock = "/tmp/mysql.sock";
    if ( !mysql_real_connect(&mysql, "loki43", "root", "", "",
			     4401, NULL, 0) )
      return 0;

    printf("Mysql connected\n");
    mysql_query(&mysql, "DROP DATABASE employees");
    printf("Dropped existing employees DB\n");
    mysql_query(&mysql, "CREATE DATABASE employees");
    mysql_commit(&mysql);
    printf("Created new employees DB\n");

    if (mysql_query(&mysql, "USE employees") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("USE employees DB\n");

    if (mysql_query(&mysql, employeeDef) != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("Created 'employee' table\n");

    if (mysql_query(&mysql, departmentsDef) != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("Created 'departments' table\n");

    if (mysql_query(&mysql, dept_managerDef) != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("Created 'dept_manager' table\n");

    if (mysql_query(&mysql, dept_empDef) != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("Created 'dept_emp' table\n");

    if (mysql_query(&mysql, titlesDef) != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("Created 'titles' table\n");

    if (mysql_query(&mysql, salariesDef) != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    printf("Created 'salaries' table\n");

    mysql_close(&mysql);
  }

  return 1;
}

#if 0
/**************************************************************
 * Initialise NdbRecord structures for table and index access *
 **************************************************************/
static void init_ndbrecord_info(Ndb &myNdb)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  manager = myDict->getTable("dept_manager");
  employee= myDict->getTable("employees");
  salary  = myDict->getTable("salaries");

  if (!employee || !manager || !salary) 
    APIERROR(myDict->getNdbError());

  rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  rowEmployeeRecord = employee->getDefaultRecord();
  if (rowEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  rowSalaryRecord = salary->getDefaultRecord();
  if (rowSalaryRecord == NULL) APIERROR(myDict->getNdbError());

  // Lookup Primary key for salaries table
  const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", "salaries");
  if (myPIndex == NULL)
    APIERROR(myDict->getNdbError());

  indexSalaryRecord = myPIndex->getDefaultRecord();
  if (indexSalaryRecord == NULL) APIERROR(myDict->getNdbError());
}
#endif


/**
 * Simple example of intended usage of the new (SPJ) QueryBuilder API.
 *
 * STATUS:
 *   Compilable code, NdbQueryBuilder do some semantics checks.
 *
 */

int testQueryBuilder(Ndb &myNdb)
{
  const NdbDictionary::Table *manager, *employee, *salary;

  printf("\n -- Building query --\n");

  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  manager = myDict->getTable("dept_manager");
  employee= myDict->getTable("employees");
  salary  = myDict->getTable("salaries");

  if (!employee || !manager || !salary) 
    APIERROR(myDict->getNdbError());

  /**
   * Some very basic examples which are actually not Query*Trees*, but rather
   * single QueryOperation defined with the NdbQueryBuilder.
   * Mainly to illustrate how the NdbQueryOperand may be specified
   * either as a constant or a parameter value - A combination
   * thereoff would also be sensible.
   *
   * Main purpose is to examplify how NdbQueryBuilder is used to prepare
   * reusable query object - no ::execute() is performed yet.
   */
  NdbQueryBuilder myBuilder(myNdb);

  /* qt1 is 'const defined' */
  printf("q1\n");
  NdbQueryDef* q1 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    const NdbQueryOperand* managerKey[] =  // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->constValue("d005"),             // dept_no = "d005"
       qb->constValue(110567),             // emp_no  = 110567
       0
    };
    const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, managerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    q1 = qb->prepare();
    if (q1 == NULL) APIERROR(qb->getNdbError());

    // Some operations are intentionally disallowed through private declaration 
//  delete readManager;
//  NdbQueryLookupOperationDef illegalAssign = *readManager;
//  NdbQueryLookupOperationDef *illegalCopy1 = new NdbQueryLookupOperationDef(*readManager);
//  NdbQueryLookupOperationDef illegalCopy2(*readManager);
  }

  printf("q2\n");
  NdbQueryDef* q2 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    // Manager key defined as parameter 
    const NdbQueryOperand* managerKey[] =       // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->paramValue(),         // dept_no parameter,
       qb->paramValue("emp"),    // emp_no parameter - param naming is optional
       0
    };
    // Lookup on a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryLookupOperationDef* readManager = qb->readTuple(manager, managerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    q2 = qb->prepare();
    if (q2 == NULL) APIERROR(qb->getNdbError());
  }

/**** UNFINISHED...
  printf("q3\n");
  NdbQueryDef* q3 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    const NdbQueryIndexBound* managerBound =       // Manager is indexed om {"dept_no", "emp_no"}
    {  ....
    };
    // Lookup on a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryScanNode *scanManager = qb->scanIndex(manager, managerKey);
    if (scanManager == NULL) APIERROR(qb->getNdbError());

    q3 = qb->prepare();
    if (q3 == NULL) APIERROR(qb->getNdbError());
  }
*****/


  /* Composite operations building real *trees* aka. linked operations.
   * (First part is identical to building 'qt2' above)
   *
   * The related SQL query which this simulates would be something like:
   *
   * select * from dept_manager join employees using(emp_no)
   *  where dept_no = 'd005' and emp_no = 110567;
   */
  printf("q4\n");
  NdbQueryDef* q4 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    const NdbQueryOperand* managerKey[] =       // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->paramValue(),         // dept_no parameter,
       qb->paramValue("emp"),    // emp_no parameter - param naming is optional
       0
    };
    // Lookup a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, managerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    // THEN: employee table is joined:
    //    A linked value is used to let employee lookup refer values
    //    from the parent operation on manger.

    const NdbQueryOperand* empJoinKey[] =         // Employee is indexed om {"emp_no"}
    {  qb->linkedValue(readManager, "emp_no"),  // where '= readManger.emp_no'
       0
    };
    const NdbQueryLookupOperationDef* readEmployee = qb->readTuple(employee, empJoinKey);
    if (readEmployee == NULL) APIERROR(qb->getNdbError());

    q4 = qb->prepare();
    if (q4 == NULL) APIERROR(qb->getNdbError());

    assert (q4->getNoOfOperations() == 2);
    assert (q4->getQueryOperation((Uint32)0) == readManager);
    assert (q4->getQueryOperation((Uint32)1) == readEmployee);
//  assert (q4->getQueryOperation((Uint32)2) == NULL);

    assert (q4->getQueryOperation((Uint32)0)->getNoOfParentOperations() == 0);
//  assert (q4->getQueryOperation((Uint32)0)->getParentOperation((Uint32)0) == NULL);
    assert (q4->getQueryOperation((Uint32)0)->getNoOfChildOperations() == 1);
    assert (q4->getQueryOperation((Uint32)0)->getChildOperation((Uint32)0) == readEmployee);
    assert (q4->getQueryOperation((Uint32)1)->getNoOfParentOperations() == 1);
    assert (q4->getQueryOperation((Uint32)1)->getParentOperation((Uint32)0) == readManager);
    assert (q4->getQueryOperation((Uint32)1)->getNoOfChildOperations() == 0);
//  assert (q4->getQueryOperation((Uint32)1)->getChildOperation((Uint32)0) == NULL);
  }

  ///////////////////////////////////////////////////
  // q4 may later be executed as:
  // (Possibly multiple ::execute() or multiple NdbQueryDef instances 
  // within the same NdbTransaction::execute(). )
  ////////////////////////////////////////////////////
  char* dept_no = "d005";
  Uint32 emp_no = 132323;
  void* paramList[] = {&dept_no, &emp_no};

  NdbTransaction* myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbQuery* myQuery = myTransaction->createQuery(q4, paramList);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

  assert (myQuery->getNoOfOperations() == 2);

  assert (myQuery->getQueryOperation((Uint32)0)->getNoOfParentOperations() == 0);
//assert (myQuery->getQueryOperation((Uint32)0)->getParentOperation((Uint32)0) == NULL);
  assert (myQuery->getQueryOperation((Uint32)0)->getNoOfChildOperations() == 1);
  assert (myQuery->getQueryOperation((Uint32)0)->getChildOperation((Uint32)0) == myQuery->getQueryOperation((Uint32)1));
  assert (myQuery->getQueryOperation((Uint32)1)->getNoOfParentOperations() == 1);
  assert (myQuery->getQueryOperation((Uint32)1)->getParentOperation((Uint32)0) == myQuery->getQueryOperation((Uint32)0));
  assert (myQuery->getQueryOperation((Uint32)1)->getNoOfChildOperations() == 0);
//assert (myQuery->getQueryOperation((Uint32)1)->getChildOperation((Uint32)0) == NULL);

  ManagerRow managerRow;
  memset (&managerRow, 0, sizeof(managerRow));
  const NdbRecord* rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  // Specify result handling NdbRecord style - need the (single) NdbQueryOperation:
  assert(myQuery->getNoOfOperations()==2);
  NdbQueryOperation* op = myQuery->getQueryOperation((Uint32)0);

  op->setResultRowBuf(rowManagerRecord, (char*)&managerRow);

  if (myTransaction->execute( NdbTransaction::NoCommit ) == -1)
    APIERROR(myTransaction->getNdbError());

  // All NdbQuery operations are handled as scans with cursor placed 'before'
  // first record: Fetch next to retrieve result:
  int res = myQuery->nextResult();
  if (res == -1)
    APIERROR(myQuery->getNdbError());

  // NOW: Result is available in 'managerRow' buffer

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;

  // Example: ::readTuple() using Index for unique key lookup
  printf("q5\n");

  NdbQueryDef* q5 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    // Lookup Primary key for manager table
    const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", manager->getName());
    if (myPIndex == NULL)
      APIERROR(myDict->getNdbError());

    // Manager index-key defined as parameter, NB: Reversed order compared to hash key
    const NdbQueryOperand* managerKey[] =  // Manager PK index is {"emp_no","dept_no", }
    {  qb->constValue(110567),             // emp_no  = 110567
       qb->constValue("d005"),             // dept_no = "d005"
       0
    };
    // Lookup on a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryLookupOperationDef* readManager = qb->readTuple(myPIndex, manager, managerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    q5 = qb->prepare();
    if (q5 == NULL) APIERROR(qb->getNdbError());
  }

  return 0;
}



int
main(int argc, const char** argv){
  ndb_init();

  /**************************************************************
   * Connect to ndb cluster                                     *
   **************************************************************/

#if defined(NDB_CONNECT_STRING)
  Ndb_cluster_connection cluster_connection(NDB_CONNECT_STRING);
#else
  Ndb_cluster_connection cluster_connection;
#endif

//printf("sizeof(NdbOperation): %d\n", sizeof(NdbOperation));
//printf("sizeof(NdbScanOperation): %d\n", sizeof(NdbScanOperation));
//printf("sizeof(NdbIndexScanOperation): %d\n", sizeof(NdbIndexScanOperation));
  //printf("offset: %d\n", offsetof(NdbOperation, NdbOperation::m_type));


  if (!createEmployeeDb())
  {  std::cout << "Create of employee DB failed" << std::endl;
     exit(-1);
  }

  if (cluster_connection.connect(4, 5, 1))
  {
    std::cout << "Unable to connect to cluster within 30 secs." << std::endl;
    exit(-1);
  }
  // Optionally connect and wait for the storage nodes (ndbd's)
  if (cluster_connection.wait_until_ready(30,0) < 0)
  {
    std::cout << "Cluster was not ready within 30 secs.\n";
    exit(-1);
  }
  Ndb myNdb(&cluster_connection,"employees");
  if (myNdb.init(1024) == -1) {      // Set max 1024  parallel transactions
    APIERROR(myNdb.getNdbError());
    exit(-1);
  }
  std::cout << "Connected to Cluster\n";


  /*******************************************
   * Check table existence                   *
   *******************************************/
  if (true)
  {
    bool has_tables = true;
    const NdbDictionary::Dictionary* myDict= myNdb.getDictionary();

    if (myDict->getTable("departments") == 0)
    {  std::cout << "Table 'departments' not found" << std::endl;
       has_tables = false;
    }
    if (myDict->getTable("employees") == 0)
    {  std::cout << "Table 'employees' not found" << std::endl;
       has_tables = false;
    }
    if (myDict->getTable("dept_emp") == 0)
    {  std::cout << "Table 'dept_emp' not found" << std::endl;
       has_tables = false;
    }
    if (myDict->getTable("dept_manager") == 0)
    {  std::cout << "Table 'dept_manager' not found" << std::endl;
       has_tables = false;
    }
    if (myDict->getTable("salaries") == 0)
    {  std::cout << "Table 'salaries' not found" << std::endl;
       has_tables = false;
    }
    if (myDict->getTable("titles") == 0)
    {  std::cout << "Table 'titles' not found" << std::endl;
       has_tables = false;
    }
    if (!has_tables)
    {  std::cout << "Table(s) was missing from the 'employees' DB" << std::endl;
       exit(-1);
    }
    std::cout << "All tables in 'employees' DB was found" << std::endl;
  }

  testQueryBuilder(myNdb);

  return 0;
}



