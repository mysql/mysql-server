/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/**************************************************************
 *
 * NOTE THAT THIS TOOL CAN ONLY BE RUN AGAINST THE EMPLOYEES DATABASE 
 * TABLES WHICH IS A SEPERATE DOWNLOAD AVAILABLE AT WWW.MYSQL.COM.
 **************************************************************/


// Used for cout
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>

#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"

#define USE_RECATTR

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
#define PRINT_APIERROR(error) { \
  PRINT_ERROR((error).code,(error).message); }
#define APIERROR(error) { \
  PRINT_APIERROR(error); \
  exit(-1); }


/*****************************************************
** Defines record structure for the rows in our tables
******************************************************/
struct ManagerRow
{
  char   dept_no[4];
  Uint32 emp_no;
  Int32  from_date;
  Int32  to_date;
  Uint32 my_key;
};

struct ManagerPKRow
{
  Uint32 emp_no;
  char   dept_no[4];
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


const char* employeeDef = 
"CREATE TABLE employees ("
"    emp_no      INT             NOT NULL,"
"    dept_no     CHAR(4)         NOT NULL,"   // Temporary added OJA
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
"   my_key       INT             NOT NULL,"
"   KEY         (emp_no),"
"   KEY         (dept_no),"
//"   FOREIGN KEY (emp_no)  REFERENCES employees (emp_no)    ON DELETE CASCADE,"
//"   FOREIGN KEY (dept_no) REFERENCES departments (dept_no) ON DELETE CASCADE,"
"   UNIQUE KEY MYINDEXNAME (my_key),"
"   PRIMARY KEY (emp_no,dept_no))"
" ENGINE=NDB"
//" PARTITION BY KEY(dept_no)"
;

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


int createEmployeeDb(MYSQL& mysql)
{
  if (true)
  {
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


    printf("Insert simple test data\n");

    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('d005',110567,110567)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('c005',11057,11067)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('e005',210567,210567)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);

    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('f005',210568,210568)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('g005',210569,210569)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('h005',210560,210560)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);
    if (mysql_query(&mysql, "Insert into dept_manager(dept_no,emp_no,my_key) values ('i005',210561,210561)") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);

    if (mysql_query(&mysql, "Insert into employees(emp_no,dept_no) values (110567,'d005')") != 0) MYSQLERROR(mysql);
    mysql_commit(&mysql);

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
  int res;
  NdbTransaction* myTransaction = NULL;
  NdbQuery* myQuery = NULL;

  const char* dept_no = "d005";
  Uint32 emp_no = 110567;

  ManagerRow  managerRow;
  EmployeeRow employeeRow;

  printf("\n -- Building query --\n");

  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  manager = myDict->getTable("dept_manager");
  employee= myDict->getTable("employees");
  salary  = myDict->getTable("salaries");

  if (!employee || !manager || !salary) 
    APIERROR(myDict->getNdbError());

  ////////////////////////////////////////////////
  // Prepare alternatine non-default NdbRecords for MANAGER table
  ////////////////////////////////////////////////
  NdbRecord *rowManagerRecord;

  {
    const NdbDictionary::Column *manager_dept_no;
    const NdbDictionary::Column *manager_emp_no;
    const NdbDictionary::Column *manager_from_date;
    const NdbDictionary::Column *manager_to_date;

    manager_dept_no = manager->getColumn("dept_no");
    if (manager_dept_no == NULL) APIERROR(myDict->getNdbError());
    manager_emp_no = manager->getColumn("emp_no");
    if (manager_emp_no == NULL) APIERROR(myDict->getNdbError());
    manager_from_date = manager->getColumn("from_date");
    if (manager_from_date == NULL) APIERROR(myDict->getNdbError());
    manager_to_date = manager->getColumn("to_date");
    if (manager_to_date == NULL) APIERROR(myDict->getNdbError());

    const NdbDictionary::RecordSpecification mngSpec[] = {
      {manager_emp_no,    offsetof(ManagerRow, emp_no),    0,0},
//      {manager_dept_no,   offsetof(ManagerRow, dept_no),   0,0},
//      {manager_from_date, offsetof(ManagerRow, from_date), 0,0},
      {manager_to_date,   offsetof(ManagerRow, to_date),   0,0}
    };

    rowManagerRecord = 
      myDict->createRecord(manager, mngSpec, 2, sizeof(mngSpec[0]));
    if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());
  }


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
  NdbQueryBuilder* const myBuilder = NdbQueryBuilder::create(myNdb);

#if 0
  printf("Compare with old API interface\n");

  {
    myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

    // Lookup Primary key for manager table
    const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", manager->getName());
    if (myPIndex == NULL)
      APIERROR(myDict->getNdbError());

    NdbIndexScanOperation* ixScan = 
      myTransaction->scanIndex(myPIndex->getDefaultRecord(),
                               manager->getDefaultRecord());

    if (ixScan == NULL)
      APIERROR(myTransaction->getNdbError());


    /* Add a bound
     */
    ManagerPKRow low={0,"d005"};
    ManagerPKRow high={110567,"d005"};

    NdbIndexScanOperation::IndexBound bound;
    bound.low_key=(char*)&low;
    bound.low_key_count=2;
    bound.low_inclusive=true;
    bound.high_key=(char*)&high;
    bound.high_key_count=2;
    bound.high_inclusive=false;
    bound.range_no=0;

    if (ixScan->setBound(myPIndex->getDefaultRecord(), bound))
      APIERROR(myTransaction->getNdbError());
  }
#endif

#if 1
  /* qt1 is 'const defined' */
  printf("q1\n");
  const NdbQueryDef* q1 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

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
  const NdbQueryDef* q2 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

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
  const NdbQueryDef* q3 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

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
#endif


#if 1
{
  /* Composite operations building real *trees* aka. linked operations.
   * (First part is identical to building 'qt2' above)
   *
   * The related SQL query which this simulates would be something like:
   *
   * select * from dept_manager join employees using(emp_no)
   *  where dept_no = 'd005' and emp_no = 110567;
   */
  printf("q4\n");
  const NdbQueryDef* q4 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

    const NdbQueryOperand* constManagerKey[] =  // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->constValue("d005"),   // dept_no = "d005"
       qb->constValue(110567),   // emp_no  = 110567
       0
    };
    const NdbQueryOperand* paramManagerKey[] =       // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->paramValue(),         // dept_no parameter,
       qb->paramValue("emp"),    // emp_no parameter - param naming is optional
       0
    };
    // Lookup a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, paramManagerKey);
    //const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, constManagerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    // THEN: employee table is joined:
    //    A linked value is used to let employee lookup refer values
    //    from the parent operation on manger.

    const NdbQueryOperand* joinEmployeeKey[] =       // Employee is indexed om {"emp_no"}
    {  qb->linkedValue(readManager, "emp_no"),  // where '= readManger.emp_no'
       0
    };
    const NdbQueryLookupOperationDef* readEmployee = qb->readTuple(employee, joinEmployeeKey);
    if (readEmployee == NULL) APIERROR(qb->getNdbError());

    q4 = qb->prepare();
    if (q4 == NULL) APIERROR(qb->getNdbError());
  }

  ///////////////////////////////////////////////////
  // q4 may later be executed as:
  // (Possibly multiple ::execute() or multiple NdbQueryDef instances 
  // within the same NdbTransaction::execute(). )
  ////////////////////////////////////////////////////
  NdbQueryParamValue paramList[] = {dept_no, emp_no};

  myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  myQuery = myTransaction->createQuery(q4,paramList);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

#ifdef USE_RECATTR
  const NdbRecAttr *key[2][2];

  for (Uint32 i=0; i<myQuery->getNoOfOperations(); ++i)
  {
    NdbQueryOperation* op = myQuery->getQueryOperation(i);
    const NdbDictionary::Table* table = op->getQueryOperationDef().getTable();

    key[i][0] =  op->getValue(table->getColumn(0));
    key[i][1] =  op->getValue(table->getColumn(1));
  }

#else
{
  memset (&managerRow,  0, sizeof(managerRow));
  memset (&employeeRow, 0, sizeof(employeeRow));
  const NdbRecord* rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  const NdbRecord* rowEmployeeRecord = employee->getDefaultRecord();
  if (rowEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  assert(myQuery->getNoOfOperations()==2);
  NdbQueryOperation* op0 = myQuery->getQueryOperation(0U);
  NdbQueryOperation* op1 = myQuery->getQueryOperation(1U);

  op0->setResultRowBuf(rowManagerRecord, (char*)&managerRow);
  op1->setResultRowBuf(rowEmployeeRecord, (char*)&employeeRow);
}
#endif

  printf("Start execute\n");
  if (myTransaction->execute(NdbTransaction::NoCommit) != 0 ||
      myQuery->getNdbError().code)
  {
    APIERROR(myQuery->getNdbError());
  }
  printf("Done executed\n");

  // All NdbQuery operations are handled as scans with cursor placed 'before'
  // first record: Fetch next to retrieve result:
  res = myQuery->nextResult();
  if (res == NdbQuery::NextResult_error)
    APIERROR(myQuery->getNdbError());

#ifdef USE_RECATTR
  printf("manager  emp_no: %d\n", key[0][1]->u_32_value());
  printf("employee emp_no: %d\n", key[1][0]->u_32_value());
  assert(!key[0][1]->isNULL() && key[0][1]->u_32_value()==emp_no);
  assert(!key[1][0]->isNULL() && key[1][0]->u_32_value()==emp_no);
#else
  // NOW: Result is available in 'managerRow' buffer
  printf("manager  emp_no: %d\n", managerRow.emp_no);
  printf("employee emp_no: %d\n", employeeRow.emp_no);
  assert(managerRow.emp_no==emp_no);
  assert(employeeRow.emp_no==emp_no);
#endif

  myQuery->close();

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;
}
#endif


#if 1
{
  //////////////////////////////////////////////////
  printf("q4_1\n");
  const NdbQueryDef* q4_1 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

    const NdbQueryOperand* empKey[] =       // Employee is indexed om {"emp_no"}
    {
       //qb->constValue(110567),   // emp_no  = 110567
       qb->paramValue(),
       0
    };
    const NdbQueryLookupOperationDef* readEmployee = qb->readTuple(employee, empKey);
    if (readEmployee == NULL) APIERROR(qb->getNdbError());

    const NdbQueryOperand* joinManagerKey[] =  // Manager is indexed om {"dept_no", "emp_no"}
    {
      qb->paramValue(),
      //qb->constValue(1005),   // dept_no = "d005"
      //qb->linkedValue(readEmployee,"dept_no"),
      qb->linkedValue(readEmployee,"emp_no"),   // emp_no  = 110567
      //qb->constValue(110567),
      //qb->paramValue(),
      0
    };

    // Join with a single tuple with key defined by linked employee fields
    const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, joinManagerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    q4_1 = qb->prepare();
    if (q4_1 == NULL) APIERROR(qb->getNdbError());
  }

  ///////////////////////////////////////////////////
  // q4 may later be executed as:
  // (Possibly multiple ::execute() or multiple NdbQueryDef instances 
  // within the same NdbTransaction::execute(). )
  ////////////////////////////////////////////////////

//NdbQueryParamValue paramList_q4[] = {emp_no};
//NdbQueryParamValue paramList_q4[] = {dept_no};
  NdbQueryParamValue paramList_q4[] = {emp_no, dept_no};

  myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  myQuery = myTransaction->createQuery(q4_1,paramList_q4);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

#ifdef USE_RECATTR
  const NdbRecAttr *value_q4[2][2];

  for (Uint32 i=0; i<myQuery->getNoOfOperations(); ++i)
  {
    NdbQueryOperation* op = myQuery->getQueryOperation(i);
    const NdbDictionary::Table* table = op->getQueryOperationDef().getTable();

    value_q4[i][0] =  op->getValue(table->getColumn(0));
    value_q4[i][1] =  op->getValue(table->getColumn(1));
  }
#else
{
  memset (&managerRow,  0, sizeof(managerRow));
  memset (&employeeRow, 0, sizeof(employeeRow));
  const NdbRecord* rowEmployeeRecord = employee->getDefaultRecord();
  if (rowEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  const NdbRecord* rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  assert(myQuery->getNoOfOperations()==2);
  NdbQueryOperation* op0 = myQuery->getQueryOperation(0U);
  NdbQueryOperation* op1 = myQuery->getQueryOperation(1U);

  op0->setResultRowBuf(rowEmployeeRecord, (char*)&employeeRow);
  op1->setResultRowBuf(rowManagerRecord, (char*)&managerRow);
}
#endif

  printf("Start execute\n");
  if (myTransaction->execute(NdbTransaction::NoCommit) != 0 ||
      myQuery->getNdbError().code)
  {
    APIERROR(myQuery->getNdbError());
  }
  printf("Done executed\n");

  // All NdbQuery operations are handled as scans with cursor placed 'before'
  // first record: Fetch next to retrieve result:
  res = myQuery->nextResult();
  if (res == NdbQuery::NextResult_error)
    APIERROR(myQuery->getNdbError());

#ifdef USE_RECATTR
  printf("employee emp_no: %d\n", value_q4[0][0]->u_32_value());
  printf("manager  emp_no: %d\n", value_q4[1][1]->u_32_value());
  assert(!value_q4[0][0]->isNULL() && value_q4[0][0]->u_32_value()==emp_no);
  assert(!value_q4[1][1]->isNULL() && value_q4[1][1]->u_32_value()==emp_no);

#else
  printf("employee emp_no: %d\n", employeeRow.emp_no);
  printf("manager  emp_no: %d\n", managerRow.emp_no);
  assert(managerRow.emp_no==emp_no);
  assert(employeeRow.emp_no==emp_no);
#endif

  myQuery->close();

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;
}
#endif

  /////////////////////////////////////////////////

#if 1
{
  // Example: ::readTuple() using Index for unique key lookup
  printf("q5\n");

  const NdbQueryDef* q5 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

    // Lookup Primary key for manager table
    const NdbDictionary::Index *myPIndex= myDict->getIndex("MYINDEXNAME$unique", manager->getName());
    if (myPIndex == NULL)
      APIERROR(myDict->getNdbError());

    // Manager index-key defined as parameter, NB: Reversed order compared to hash key
    const NdbQueryOperand* managerKey[] =  // Manager PK index is {"emp_no","dept_no", }
    {
       //qb->constValue(110567),   // emp_no  = 110567
       qb->paramValue(),
       0
    };
    // Lookup on a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryLookupOperationDef* readManager = qb->readTuple(myPIndex, manager, managerKey);
    if (readManager == NULL) APIERROR(qb->getNdbError());

    q5 = qb->prepare();
    if (q5 == NULL) APIERROR(qb->getNdbError());
  }

  myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbQueryParamValue paramList_q5[] = {emp_no};
  myQuery = myTransaction->createQuery(q5,paramList_q5);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

#ifdef USE_RECATTR
  const NdbRecAttr *value_q5[2];

  NdbQueryOperation* op = myQuery->getQueryOperation(0U);
  const NdbDictionary::Table* table = op->getQueryOperationDef().getTable();

  value_q5[0] = op->getValue(table->getColumn(0));
  value_q5[1] = op->getValue(table->getColumn(1));
#else
{
  memset (&managerRow, 0, sizeof(managerRow));
  const NdbRecord* rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  // Specify result handling NdbRecord style - need the (single) NdbQueryOperation:
  NdbQueryOperation* op = myQuery->getQueryOperation(0U);

  op->setResultRowBuf(rowManagerRecord, (char*)&managerRow);
}
#endif

  printf("Start execute\n");
  if (myTransaction->execute(NdbTransaction::NoCommit) != 0 ||
      myQuery->getNdbError().code)
  {
    APIERROR(myQuery->getNdbError());
  }
  printf("Done executed\n");

  // All NdbQuery operations are handled as scans with cursor placed 'before'
  // first record: Fetch next to retrieve result:
  res = myQuery->nextResult();
  if (res == NdbQuery::NextResult_error)
    APIERROR(myQuery->getNdbError());

#ifdef USE_RECATTR
  printf("employee emp_no: %d\n", value_q5[1]->u_32_value());
  assert(!value_q5[1]->isNULL() && value_q5[1]->u_32_value()==emp_no);
#else
  printf("employee emp_no: %d\n", managerRow.emp_no);
  assert(managerRow.emp_no==emp_no);
#endif

  myQuery->close();

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;
}
#endif

#if 1
{
  printf("q6: Table scan + linked lookup\n");

  const NdbQueryDef* q6 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

/****
    // Lookup Primary key for manager table
    const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", manager->getName());
    if (myPIndex == NULL)
      APIERROR(myDict->getNdbError());

    const NdbQueryOperand* low[] =  // Manager PK index is {"emp_no","dept_no", }
    {  qb->constValue(110567),      // emp_no  = 110567
       0
    };
    const NdbQueryOperand* high[] =  // Manager PK index is {"emp_no","dept_no", }
    {  qb->constValue("illegal key"),
       0
    };
    const NdbQueryIndexBound  bound        (low, NULL);   // emp_no = [110567, oo]
    const NdbQueryIndexBound  bound_illegal(low, high);   // 'high' is char type -> illegal
    const NdbQueryIndexBound  boundEq(low);
****/
    // Lookup on a single tuple with key define by 'managerKey' param. tuple
//  const NdbQueryScanOperationDef* scanManager = qb->scanIndex(myPIndex, manager, &boundEq);
    const NdbQueryScanOperationDef* scanManager = qb->scanTable(manager);
    if (scanManager == NULL) APIERROR(qb->getNdbError());

    // THEN: employee table is joined:
    //    A linked value is used to let employee lookup refer values
    //    from the parent operation on manager.

    const NdbQueryOperand* empJoinKey[] =       // Employee is indexed om {"emp_no"}
    {  qb->linkedValue(scanManager, "emp_no"),  // where '= readManger.emp_no'
       0
    };
    const NdbQueryLookupOperationDef* readEmployee = qb->readTuple(employee, empJoinKey);
    if (readEmployee == NULL) APIERROR(qb->getNdbError());

    q6 = qb->prepare();
    if (q6 == NULL) APIERROR(qb->getNdbError());
  }

  myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  myQuery = myTransaction->createQuery(q6, (NdbQueryParamValue*)0);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

#ifdef USE_RECATTR
  const NdbRecAttr* value_q6[2][2];

  for (Uint32 i=0; i<myQuery->getNoOfOperations(); ++i)
  {
    NdbQueryOperation* op = myQuery->getQueryOperation(i);
    const NdbDictionary::Table* table = op->getQueryOperationDef().getTable();

    value_q6[i][0] =  op->getValue(table->getColumn(0));
    value_q6[i][1] =  op->getValue(table->getColumn(1));
  }
#else
{
  int err;
  const NdbRecord* rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  assert(myQuery->getNoOfOperations()==2);
  NdbQueryOperation* op0 = myQuery->getQueryOperation(0U);
  err = op0->setResultRowBuf(rowManagerRecord, (char*)&managerRow);
  assert (err==0);
//if (err == NULL) APIERROR(op0->getNdbError());

  const NdbRecord* rowEmployeeRecord = employee->getDefaultRecord();
  if (rowEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  NdbQueryOperation* op1 = myQuery->getQueryOperation(1U);
  err = op1->setResultRowBuf(rowEmployeeRecord, (char*)&employeeRow);
  assert (err==0);
//if (err == NULL) APIERROR(op1->getNdbError());
}
#endif

  printf("Start execute\n");
  if (myTransaction->execute(NdbTransaction::NoCommit) != 0 ||
      myQuery->getNdbError().code)
  {
    APIERROR(myQuery->getNdbError());
  }

  int cnt = 0;
  while (true) {
    memset (&managerRow,  0, sizeof(managerRow));
    memset (&employeeRow, 0, sizeof(employeeRow));

    // All NdbQuery operations are handled as scans with cursor placed 'before'
    // first record: Fetch next to retrieve result:
    NdbQuery::NextResultOutcome res = myQuery->nextResult();

    if (res == NdbQuery::NextResult_error) {
      PRINT_APIERROR(myQuery->getNdbError());
      break;
    } else if (res!=NdbQuery::NextResult_gotRow) {
      break;
    }

#ifdef USE_RECATTR
    printf("manager  emp_no: %d, NULL:%d\n",
            value_q6[0][1]->u_32_value(), myQuery->getQueryOperation(0U)->isRowNULL());
    printf("employee emp_no: %d, NULL:%d\n",
            value_q6[1][0]->u_32_value(), myQuery->getQueryOperation(1U)->isRowNULL());
#else
    // NOW: Result is available in row buffers
    printf("manager  emp_no: %d, NULL:%d\n",
            managerRow.emp_no, myQuery->getQueryOperation(0U)->isRowNULL());
    printf("employee emp_no: %d, NULL:%d\n",
            employeeRow.emp_no, myQuery->getQueryOperation(1U)->isRowNULL());
#endif
    cnt++;
  };
  printf("EOF, %d rows\n", cnt);
  myQuery->close();

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;
}
#endif

#if 1
{
  printf("Ordered index scan + lookup\n");

  const NdbQueryDef* q6_1 = 0;
  {
    NdbQueryBuilder* qb = myBuilder; //myDict->getQueryBuilder();

    // Lookup Primary key for manager table
    const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", manager->getName());
    if (myPIndex == NULL)
      APIERROR(myDict->getNdbError());

    const NdbQueryOperand* low[] =  // Manager PK index is {"emp_no","dept_no", }
    {
       qb->paramValue(),
//     qb->constValue(110567),      // emp_no  = 110567
       qb->constValue("d005"),      // dept_no = "d005"
       0
    };
    const NdbQueryOperand* high[] =  // Manager PK index is {"emp_no","dept_no", }
    {
       qb->constValue(110567),      // emp_no  = 110567
       qb->constValue("d005"),      // dept_no = "d005"
       0
    };
    const NdbQueryIndexBound  bound        (low, high);   // emp_no = [110567, oo]
    const NdbQueryIndexBound  boundEq(low);

    // Lookup on a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryScanOperationDef* scanManager = qb->scanIndex(myPIndex, manager, &bound);
    if (scanManager == NULL) APIERROR(qb->getNdbError());

    // THEN: employee table is joined:
    //    A linked value is used to let employee lookup refer values
    //    from the parent operation on manager.

    const NdbQueryOperand* empJoinKey[] =       // Employee is indexed om {"emp_no"}
    {  qb->linkedValue(scanManager, "emp_no"),  // where '= readManger.emp_no'
       0
    };
    const NdbQueryLookupOperationDef* readEmployee = qb->readTuple(employee, empJoinKey);
    if (readEmployee == NULL) APIERROR(qb->getNdbError());

    q6_1 = qb->prepare();
    if (q6_1 == NULL) APIERROR(qb->getNdbError());
  }

  myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbQueryParamValue paramList_q6_1[] = {emp_no};
  myQuery = myTransaction->createQuery(q6_1, paramList_q6_1);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

#ifdef USE_RECATTR
  const NdbRecAttr* value_q6_1[2][2];

  for (Uint32 i=0; i<myQuery->getNoOfOperations(); ++i)
  {
    NdbQueryOperation* op = myQuery->getQueryOperation(i);
    const NdbDictionary::Table* table = op->getQueryOperationDef().getTable();

    value_q6_1[i][1] =  op->getValue(table->getColumn(1));
    value_q6_1[i][0] =  op->getValue(table->getColumn(0));
  }
#else
{
  int err;
//int mask = 0x03;
  const NdbRecord* rowManagerRecord = manager->getDefaultRecord();
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());

  assert(myQuery->getNoOfOperations()==2);
  NdbQueryOperation* op0 = myQuery->getQueryOperation(0U);
  err = op0->setResultRowBuf(rowManagerRecord, (char*)&managerRow /*, (const unsigned char*)&mask*/);
  assert (err==0);
  if (err) APIERROR(myQuery->getNdbError());

  const NdbRecord* rowEmployeeRecord = employee->getDefaultRecord();
  if (rowEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  NdbQueryOperation* op1 = myQuery->getQueryOperation(1U);
  err = op1->setResultRowBuf(rowEmployeeRecord, (char*)&employeeRow  /*, (const unsigned char*)&mask*/);
  assert (err==0);
  if (err) APIERROR(myQuery->getNdbError());
}
#endif

  printf("Start execute\n");
  if (myTransaction->execute(NdbTransaction::NoCommit) != 0 ||
      myQuery->getNdbError().code)
  {
    APIERROR(myQuery->getNdbError());
  }
  printf("Done executed\n");

  int cnt = 0;
  while (true) {
    memset (&managerRow,  0, sizeof(managerRow));
    memset (&employeeRow, 0, sizeof(employeeRow));

    // All NdbQuery operations are handled as scans with cursor placed 'before'
    // first record: Fetch next to retrieve result:
    NdbQuery::NextResultOutcome res = myQuery->nextResult();

    if (res == NdbQuery::NextResult_error) {
      PRINT_APIERROR(myQuery->getNdbError());
      break;
    } else if (res!=NdbQuery::NextResult_gotRow) {
      break;
    }

#ifdef USE_RECATTR
    printf("manager  emp_no: %d, NULL:%d\n",
            value_q6_1[0][1]->u_32_value(), myQuery->getQueryOperation(0U)->isRowNULL());
    printf("employee emp_no: %d, NULL:%d\n",
            value_q6_1[1][0]->u_32_value(), myQuery->getQueryOperation(1U)->isRowNULL());
#else
    // NOW: Result is available in row buffers
    printf("manager  emp_no: %d, NULL:%d\n",
            managerRow.emp_no, myQuery->getQueryOperation(0U)->isRowNULL());
    printf("employee emp_no: %d, NULL:%d\n",
            employeeRow.emp_no, myQuery->getQueryOperation(1)->isRowNULL());
#endif
    cnt++;
  };

  printf("EOF, %d rows\n", cnt);
  myQuery->close();

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;
}
#endif

  myBuilder->destroy();
  return 0;
}


int
main(int argc, const char** argv){
  if(argc!=4){
    std::cout << "Usage: " << argv[0] 
              << " <mysql IP address> <mysql port> <cluster connect string>" 
              << std::endl;
    exit(-1);
  }
  const char* const host=argv[1];
  const int port = atoi(argv[2]);
  const char* const connectString = argv[3];

  //extern const char *my_progname;
  //NDB_INIT(argv[0]);
  ndb_init();
  MYSQL mysql;
  if(!mysql_init(&mysql)){
    std::cout << "mysql_init() failed:" << std::endl;
  }
  if(!mysql_real_connect(&mysql, host, "root", "", "",
                         port, NULL, 0)){
    std::cout << "mysql_real_connect() failed:" << std::endl;
  }


  if (!createEmployeeDb(mysql))
  {  std::cout << "Create of employee DB failed" << std::endl;
    exit(-1);
  }
  mysql_close(&mysql);

  /**************************************************************
   * Connect to ndb cluster                                     *
   **************************************************************/
  {
    Ndb_cluster_connection cluster_connection(connectString);
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

  }  // Must call ~Ndb_cluster_connection() before ndb_end().
  ndb_end(0);
  return 0;
}



