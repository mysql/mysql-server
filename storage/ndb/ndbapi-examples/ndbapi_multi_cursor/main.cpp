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
#define APIERROR(error) { \
  PRINT_ERROR((error).code,(error).message); \
  exit(-1); }



/**
 * Define NDB_CONNECT_STRING if you don't connect through the default localhost:1186
 */
  #define NDB_CONNECT_STRING "fimafeng08:1"



/* Clunky statics for shared NdbRecord stuff */
static const NdbDictionary::Table  *manager;
static const NdbDictionary::Column *manager_dept_no;
static const NdbDictionary::Column *manager_emp_no;
static const NdbDictionary::Column *manager_from_date;
static const NdbDictionary::Column *manager_to_date;

static const NdbDictionary::Table  *employee;
static const NdbDictionary::Column *employee_emp_no;
static const NdbDictionary::Column *employee_birth_date;
static const NdbDictionary::Column *employee_first_name;
static const NdbDictionary::Column *employee_last_name;
static const NdbDictionary::Column *employee_gender;
static const NdbDictionary::Column *employee_hire_date;

static const NdbDictionary::Table  *salary;
static const NdbDictionary::Column *salary_emp_no;
static const NdbDictionary::Column *salary_salary;
static const NdbDictionary::Column *salary_from_date;
static const NdbDictionary::Column *salary_to_date;

static  NdbRecord *pkeyManagerRecord;
static  NdbRecord *rowManagerRecord;

static  NdbRecord *pkeyEmployeeRecord;
static  NdbRecord *rowEmployeeRecord;

static  NdbRecord *pkeySalaryRecord;
static  NdbRecord *indexSalaryRecord;
static  NdbRecord *rowSalaryRecord;


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

  ////////////////////////////////////////////////
  // Prepare NdbRecords for MANAGER table
  ////////////////////////////////////////////////
  manager_dept_no = manager->getColumn("dept_no");
  if (manager_dept_no == NULL) APIERROR(myDict->getNdbError());
  manager_emp_no = manager->getColumn("emp_no");
  if (manager_emp_no == NULL) APIERROR(myDict->getNdbError());
  manager_from_date = manager->getColumn("from_date");
  if (manager_from_date == NULL) APIERROR(myDict->getNdbError());
  manager_to_date = manager->getColumn("to_date");
  if (manager_to_date == NULL) APIERROR(myDict->getNdbError());

  const NdbDictionary::RecordSpecification mngSpec[] = {
//    NdbDictionary::RecordSpecification(manager_dept_no,   offsetof(ManagerRow, dept_no)),
//    NdbDictionary::RecordSpecification(manager_emp_no,    offsetof(ManagerRow, emp_no)),
//    NdbDictionary::RecordSpecification(manager_from_date, offsetof(ManagerRow, from_date)),
//    NdbDictionary::RecordSpecification(manager_to_date,   offsetof(ManagerRow, to_date)),
  };

  pkeyManagerRecord = 
    myDict->createRecord(manager, mngSpec, 2, sizeof(mngSpec[0]));
  if (pkeyManagerRecord == NULL) APIERROR(myDict->getNdbError());

  rowManagerRecord = 
    myDict->createRecord(manager, mngSpec, 4, sizeof(mngSpec[0]));
  if (rowManagerRecord == NULL) APIERROR(myDict->getNdbError());


  ////////////////////////////////////////////////
  // Prepare NdbRecords for EMPLOYEE table
  ////////////////////////////////////////////////
  employee_emp_no = employee->getColumn("emp_no");
  if (employee_emp_no == NULL) APIERROR(myDict->getNdbError());
  employee_birth_date = employee->getColumn("birth_date");
  if (employee_birth_date == NULL) APIERROR(myDict->getNdbError());
  employee_first_name = employee->getColumn("first_name");
  if (employee_first_name == NULL) APIERROR(myDict->getNdbError());
  employee_last_name = employee->getColumn("last_name");
  if (employee_last_name == NULL) APIERROR(myDict->getNdbError());
  employee_gender = employee->getColumn("gender");
  if (employee_gender == NULL) APIERROR(myDict->getNdbError());
  employee_hire_date = employee->getColumn("hire_date");
  if (employee_gender == NULL) APIERROR(myDict->getNdbError());

  const NdbDictionary::RecordSpecification empSpec[] = {
/****
      NdbDictionary::RecordSpecification(employee_emp_no,     offsetof(EmployeeRow, emp_no)),
      NdbDictionary::RecordSpecification(employee_birth_date, offsetof(EmployeeRow, birth_date)),
      NdbDictionary::RecordSpecification(employee_first_name, offsetof(EmployeeRow, first_name)),
      NdbDictionary::RecordSpecification(employee_last_name,  offsetof(EmployeeRow, last_name)),
      NdbDictionary::RecordSpecification(employee_gender,     offsetof(EmployeeRow, gender)),
      NdbDictionary::RecordSpecification(employee_hire_date,  offsetof(EmployeeRow, hire_date)),
****/
  };

  pkeyEmployeeRecord = 
    myDict->createRecord(employee, empSpec, 1, sizeof(empSpec[0]));
  if (pkeyEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  rowEmployeeRecord = 
    myDict->createRecord(employee, empSpec, 6, sizeof(empSpec[0]));
  if (rowEmployeeRecord == NULL) APIERROR(myDict->getNdbError());

  ////////////////////////////////////////////////
  // Prepare NdbRecords for SALARIES table
  ////////////////////////////////////////////////
  salary_emp_no = salary->getColumn("emp_no");
  if (salary_emp_no == NULL) APIERROR(myDict->getNdbError());
  salary_salary = salary->getColumn("salary");
  if (salary_salary == NULL) APIERROR(myDict->getNdbError());
  salary_from_date = salary->getColumn("from_date");
  if (salary_from_date == NULL) APIERROR(myDict->getNdbError());
  salary_to_date = salary->getColumn("to_date");
  if (salary_to_date == NULL) APIERROR(myDict->getNdbError());

  const NdbDictionary::RecordSpecification salarySpec[] = {
/****
      NdbDictionary::RecordSpecification(salary_emp_no,    offsetof(SalaryRow, emp_no)),
      NdbDictionary::RecordSpecification(salary_from_date, offsetof(SalaryRow, from_date)),
      NdbDictionary::RecordSpecification(salary_salary,    offsetof(SalaryRow, salary)),
      NdbDictionary::RecordSpecification(salary_to_date,   offsetof(SalaryRow, to_date))
***/
  };

  // Lookup Primary key for salaries table
  const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", "salaries");
  if (myPIndex == NULL)
    APIERROR(myDict->getNdbError());

  pkeySalaryRecord = 
    myDict->createRecord(salary, salarySpec, 2, sizeof(salarySpec[0]));
  if (pkeySalaryRecord == NULL) APIERROR(myDict->getNdbError());

  rowSalaryRecord = 
    myDict->createRecord(salary, salarySpec, 4, sizeof(salarySpec[0]));
  if (rowSalaryRecord == NULL) APIERROR(myDict->getNdbError());

  indexSalaryRecord = 
    myDict->createRecord(myPIndex, salarySpec, 2, sizeof(salarySpec[0]));
  if (indexSalaryRecord == NULL) APIERROR(myDict->getNdbError());
}
  

/**
 * Simple example of intended usage of the new (SPJ) QueryBuilder API.
 *
 * STATUS:
 *   Compilable code, but neither link nor execute.
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
  NdbQueryBuilder myBuilder(&myNdb);

  /* qt1 is 'const defined' */
  NdbQueryDef* q1 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    const NdbQueryOperand* managerKey[] =  // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->constValue("d005"),             // dept_no = "d005"
       qb->constValue(110567),             // emp_no  = 110567
       0
    };
    const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, managerKey);
    if (readManager == NULL) APIERROR(myNdb.getNdbError());

    q1 = qb->prepare();
    if (q1 == NULL) APIERROR(qb->getNdbError());
  }

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
    if (readManager == NULL) APIERROR(myNdb.getNdbError());

    q2 = qb->prepare();
    if (q2 == NULL) APIERROR(qb->getNdbError());
  }

/**** UNFINISHED...
  NdbQueryDef* q3 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    const NdbQueryIndexBound* managerBound =       // Manager is indexed om {"dept_no", "emp_no"}
    {  ....
    };
    // Lookup on a single tuple with key define by 'managerKey' param. tuple
    const NdbQueryScanNode *scanManager = qb->scanIndex(manager, managerKey);
    if (scanManager == NULL) APIERROR(myNdb.getNdbError());

    q3 = qb->prepare();
    if (q3 == NULL) APIERROR(qb->getNdbError());
  }
*****/


  ///////////////////////////////////////////////////
  // q2 may later be executed as:
  // (Possibly multiple ::execute() or multiple NdbQueryDef instances 
  // within the same NdbTransaction::execute(). )
  ////////////////////////////////////////////////////
  char* dept_no = "d005";
  Uint32 emp_no = 132323;
  void* paramList[] = {&dept_no, &emp_no};

  NdbTransaction* myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbQuery* myQuery = myTransaction->createQuery(q2, paramList);
  if (myQuery == NULL)
    APIERROR(myTransaction->getNdbError());

  ManagerRow managerRow;
  memset (&managerRow, 0, sizeof(managerRow));

  // Specify result handling NdbRecord style - need the (single) NdbQueryOperation:
  assert(myQuery->getNoOfOperations()==1);
  assert (myQuery->getQueryOperation((Uint32)0) == myQuery->getRootOperation());
  NdbQueryOperation* op = myQuery->getQueryOperation((Uint32)0);

  op->setResultRowBuf(rowManagerRecord, (char*)&managerRow);

  if (myTransaction->execute( NdbTransaction::NoCommit ) == -1)
    APIERROR(myTransaction->getNdbError());

  // All NdbQuery operations are handled as scans with cursor placed 'before'
  // first record: Fetch next to retrieve result:
  if (myQuery->nextResult() != 0)
    APIERROR(myQuery->getNdbError());

  // NOW: Result is available in 'managerRow' buffer

  myNdb.closeTransaction(myTransaction);
  myTransaction = 0;


  /* Composite operations building real *trees* aka. linked operations.
   * (First part is identical to building 'qt2' above)
   *
   * The related SQL query which this simulates would be something like:
   *
   * select * from dept_manager join employees using(emp_no)
   *  where dept_no = 'd005' and emp_no = 110567;
   */
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
    if (readManager == NULL) APIERROR(myNdb.getNdbError());

    // THEN: employee table is joined:
    //    A linked value is used to let employee lookup refer values
    //    from the parent operation on manger.

    const NdbQueryOperand* empJoinKey[] =         // Employee is indexed om {"emp_no"}
    {  qb->linkedValue(readManager, "emp_no"),  // where '= readManger.emp_no'
       0
    };
    const NdbQueryLookupOperationDef* readEmployee = qb->readTuple(employee, empJoinKey);
    if (readEmployee == NULL) APIERROR(myNdb.getNdbError());

    q4 = qb->prepare();
    if (q4 == NULL) APIERROR(qb->getNdbError());
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

  /*******************************************
   * Check table existence                   *
   *******************************************/
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

  init_ndbrecord_info(myNdb);

  testQueryBuilder(myNdb);

  return 0;
}



