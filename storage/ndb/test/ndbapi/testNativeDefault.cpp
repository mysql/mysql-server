/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>
#include <NdbError.hpp>

static Ndb_cluster_connection *g_cluster_connection= 0;
static Ndb* g_ndb = 0;
static const char* g_tablename1 = "T_DEF1"; //The normal table with default values
static const char* g_tablename2 = "T_DEF2"; //The table for Test that maximum length defaults work
//The table for Test that an attempt to insert to a table containing defaults 
//without supplying a value for a not-null, non-defaulted column still fails
static const char* g_tablename3 = "T_DEF3"; 
static NdbDictionary::Dictionary* g_dict = 0;
static const unsigned int column_count_table1 = 8;
static const unsigned int column_count_table2 = 2;
static const unsigned int column_count_table3 = 2;

static struct NdbError g_ndberror;
static int create_table();

static bool
connect_ndb()
{
  g_cluster_connection = new Ndb_cluster_connection();
  if(g_cluster_connection->connect(12, 5, 1) != 0)
    return false;

  g_ndb = new Ndb(g_cluster_connection, "TEST");
  g_ndb->init();
  if(g_ndb->waitUntilReady(30) != 0)
    return false;

  return true;
}

static void
disconnect_ndb()                                            {
  delete g_ndb;
  delete g_cluster_connection;
  g_ndb = 0;
//  g_table = 0;
  g_cluster_connection= 0;
}

#define PRINT_ERROR(error) \
   ndbout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << error.code \
            << ", msg: " << error.message << "." << endl
#define FAIL(error_msg) \
  ndbout << error_msg << " at line " << __LINE__ << endl; \
  return NDBT_FAILED

static const int            tab1_c1_default= 6; 
static const float          tab1_c2_default= float(1234.56);
static const double         tab1_c3_default= 4567.89;
static const char           tab1_c4_default[]= "aaaaaa      ";
static const unsigned int   tab1_c4_default_siglen= 12;
static const char           tab1_c5_default[]= "\x6" "aaaaaa\0\0\0\0";
static const unsigned int   tab1_c5_default_siglen= 7;
static const char           tab1_c6_default[]= "aaaaaa      ";
static const unsigned int   tab1_c6_default_siglen= 0;
static const char           tab1_c7_default[]= "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
static const char           tab1_c7_default_siglen= 1;

static const int            tab2_c1_default_len= 8052 - 4 - 2;
/* Max row length minus 4 bytes for key, minus 2 bytes for length info */
static const char           tab2_c1_default_char= 'V';

static int 
create_table()
{
  g_dict = g_ndb->getDictionary();
  if ((g_dict->getTable(g_tablename1) != 0) &&
      (g_dict->dropTable(g_tablename1) != 0))
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  if ((g_dict->getTable(g_tablename2) != 0) &&
      (g_dict->dropTable(g_tablename2) != 0))
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  if ((g_dict->getTable(g_tablename3) != 0) &&
      (g_dict->dropTable(g_tablename3) != 0))
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  NdbDictionary::Table tab(g_tablename1);
  tab.setLogging(false);
  
  NdbDictionary::Table tab2(g_tablename2);
  tab2.setLogging(false);

  NdbDictionary::Table tab3(g_tablename3);
  tab3.setLogging(false);

  // col PK - Uint32
  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(true);
    col.setDefaultValue(NULL);
    tab.addColumn(col);
  }
 
  { 
    NdbDictionary::Column col("C1");
    col.setType(NdbDictionary::Column::Int);
    col.setDefaultValue(&tab1_c1_default,sizeof(int));
    tab.addColumn(col);
  }
  
  { 
    NdbDictionary::Column col("C2");
    col.setType(NdbDictionary::Column::Float);
    col.setDefaultValue(&tab1_c2_default, sizeof(float));
    tab.addColumn(col);
  }

  { 
    NdbDictionary::Column col("C3");
    col.setType(NdbDictionary::Column::Double);
    col.setDefaultValue(&tab1_c3_default, sizeof(double));
    tab.addColumn(col);
  }

  { 
    NdbDictionary::Column col("C4");
    col.setType(NdbDictionary::Column::Char);
    col.setLength(12);
    col.setDefaultValue(tab1_c4_default, 12);
    tab.addColumn(col);
  }

  { 
    NdbDictionary::Column col("C5");
    col.setType(NdbDictionary::Column::Varchar);
    col.setLength(199);
    col.setDefaultValue(tab1_c5_default, tab1_c5_default_siglen);
    tab.addColumn(col);
  }


  { 
    /* Test non-null pointer passed, but with zero length? */
    NdbDictionary::Column col("C6");
    col.setType(NdbDictionary::Column::Char);
    col.setLength(12);
    col.setNullable(true);
    col.setDefaultValue(tab1_c6_default, tab1_c6_default_siglen);
    tab.addColumn(col);
  }

  //Test that a zero-length VARCHAR default works
  { 
    NdbDictionary::Column col("C7");
    col.setType(NdbDictionary::Column::Varchar);
    col.setLength(10);
    col.setDefaultValue(tab1_c7_default, tab1_c7_default_siglen);
    tab.addColumn(col);
  }

  //create table T_DEF2
  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(false);
    col.setDefaultValue(NULL, 0);
    tab2.addColumn(col);
  }

  //Test that maximum length defaults work
  { 
    char default_data[tab2_c1_default_len + 2];
    default_data[0] = (tab2_c1_default_len >> 0) & 0xff;
    default_data[1] = (tab2_c1_default_len >> 8) & 0xff;
    memset(default_data + 2, tab2_c1_default_char, tab2_c1_default_len);
    NdbDictionary::Column col("C1");
    col.setType(NdbDictionary::Column::Longvarchar);
    col.setLength(tab2_c1_default_len);
    col.setDefaultValue(default_data, tab2_c1_default_len + 2);
    tab2.addColumn(col);
  }

  //Create table T_DEF3
  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(false);
    col.setDefaultValue(NULL, 0);
    tab3.addColumn(col);
  }

  //For column without supplying a value for a not-null, non-defaulted column
  { NdbDictionary::Column col("C1");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    col.setDefaultValue(NULL, 0);
    tab3.addColumn(col);
  }
  
  // create table
  if(g_dict->createTable(tab) != 0)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  if(g_dict->createTable(tab2) != 0)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  if(g_dict->createTable(tab3) != 0)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

static int
ndb_error_check(const struct NdbError& error, unsigned int line)
{
  if (error.code != 850)
  {
    PRINT_ERROR(error);
    ndbout << " at line " << line << "\n";
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

#define CHECK_ERROR(error) {                           \
  if (ndb_error_check(error, __LINE__) == NDBT_FAILED) \
    return NDBT_FAILED;                                \
  }

static int
create_table_error()
{
  g_dict = g_ndb->getDictionary();

  /*
   * 1. The following test case is for fixed columns that
   *    there are too long or too short default values.
   */
  //for too long default value
  NdbDictionary::Table tab1("T_DEF_TEST1");
  tab1.setLogging(false);

  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setDefaultValue(NULL);
    tab1.addColumn(col);
  }

  { int default_data = 6;
    NdbDictionary::Column col("C1");
    col.setType(NdbDictionary::Column::Int);
    col.setDefaultValue(&default_data, 8);
    tab1.addColumn(col);
  }

  if(g_dict->createTable(tab1) != 0)
  {
    CHECK_ERROR(g_dict->getNdbError());
  }
  else
  {
    FAIL("Create table should not have succeeded");
  }
  
  //for too short default value
  NdbDictionary::Table tab2("T_DEF_TEST2");
  tab2.setLogging(false);

  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(true);
    col.setDefaultValue(NULL);
    tab2.addColumn(col);
  }

  { const char default_data[] = "aaaaaa";
    NdbDictionary::Column col("C4");
    col.setType(NdbDictionary::Column::Char);
    col.setLength(12);
    col.setDefaultValue(default_data, 6);
    tab2.addColumn(col);
  }

  if(g_dict->createTable(tab2) != 0)
  {
    CHECK_ERROR(g_dict->getNdbError());
  }
  else
  {
    FAIL("Create table should not have succeeded");
  }

  /*
   * 2. The following test case is for Var-type columns that
   *    there are too long default values.
   */
  NdbDictionary::Table tab3("T_DEF_TEST3");
  tab3.setLogging(false);

  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(true);
    col.setDefaultValue(NULL);
    tab3.addColumn(col);
  }

  { char default_data[20];
    memset(default_data, 0, 20);
    Uint8 * p = (Uint8*)default_data;
    *p = 10;
    memcpy(default_data + 1, "aaaaaaaaaa", 10);
    NdbDictionary::Column col("C5");
    col.setType(NdbDictionary::Column::Varchar);
    col.setLength(9);
    col.setDefaultValue(default_data, 11);
    tab3.addColumn(col);
  }

  if(g_dict->createTable(tab3) != 0)
  {
    CHECK_ERROR(g_dict->getNdbError());
  }
  else
  {
    FAIL("Create table should not have succeeded");
  }


  /*
   * 3. Test attempt to set default value for primary key
   */
  NdbDictionary::Table tab4("T_DEF_TEST4");
  tab4.setLogging(false);

  { NdbDictionary::Column col("PK");
    unsigned int default_val=22;
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(true);
    col.setDefaultValue(&default_val, sizeof(default_val));
    tab4.addColumn(col);
  }

  if(g_dict->createTable(tab4) == 0)
  {
    FAIL("Create table should not have succeeded");
  }

  if(g_dict->getNdbError().code != 792)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }
 
  /*
   * 4. The following test case is for Var-type columns that
   *    there are too long default values, where the passed
   *    value is short, but the implied value is too long
   */
  NdbDictionary::Table tab5("T_DEF_TEST5");
  tab5.setLogging(false);

  { NdbDictionary::Column col("PK");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    col.setNullable(false);
    col.setAutoIncrement(true);
    col.setDefaultValue(NULL);
    tab5.addColumn(col);
  }

  { char default_data[20];
    memset(default_data, 0, 20);
    Uint8 * p = (Uint8*)default_data;
    *p = 15;
    memcpy(default_data + 1, "aaaaaaaaaaaaaaa", 15);
    NdbDictionary::Column col("C5");
    col.setType(NdbDictionary::Column::Varchar);
    col.setLength(9);
    /* Within range, but contained VARCHAR is too long */
    col.setDefaultValue(default_data, 10);
    tab5.addColumn(col);
  }

  if(g_dict->createTable(tab5) != 0)
  {
    CHECK_ERROR(g_dict->getNdbError());
  }
  else
  {
    FAIL("Create table should not have succeeded");
  }

  return NDBT_OK;
}

static int 
drop_table()
{
  if ((g_dict != 0) && ( g_dict->getTable(g_tablename1) != 0))
  {
    if(g_dict->dropTable(g_tablename1))
      PRINT_ERROR(g_dict->getNdbError());
  }

  if ((g_dict != 0) && ( g_dict->getTable(g_tablename2) != 0))
  {
    if(g_dict->dropTable(g_tablename2))
      PRINT_ERROR(g_dict->getNdbError());
  }

  if ((g_dict != 0) && ( g_dict->getTable(g_tablename3) != 0))
  {
    if(g_dict->dropTable(g_tablename3))
      PRINT_ERROR(g_dict->getNdbError());
  }

  return NDBT_OK;
}

static int do_insert()
{
  const NdbDictionary::Table *myTable= g_dict->getTable(g_tablename1);

  if (myTable == NULL)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED; 
  }

  NdbTransaction *myTransaction= g_ndb->startTransaction(); 
  if (myTransaction == NULL)
  {
    PRINT_ERROR(g_ndb->getNdbError());
    return NDBT_FAILED;
  }

  NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
  NdbOperation *myOperation1= myTransaction->getNdbOperation(myTable); 
  if (myOperation == NULL || myOperation1 == NULL) 
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  myOperation->insertTuple();
  myOperation->equal("PK", 1);
  myOperation1->insertTuple();
  myOperation1->equal("PK", 2);

  //insert the second table T_DEF2
  const NdbDictionary::Table *myTable2= g_dict->getTable(g_tablename2);

  if (myTable == NULL)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }
  NdbOperation *myOperation2 = myTransaction->getNdbOperation(myTable2);
  if (myOperation2 == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  myOperation2->insertTuple();
  myOperation2->equal("PK", 1);


  /* Test insert of max length tuple with max length default
   * Could theoretically expose kernel overflow with default 
   * + supplied value
   */
  myOperation2=myTransaction->getNdbOperation(myTable2);
  if (myOperation2 == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }
  
  myOperation2->insertTuple();
  myOperation2->equal("PK", 2);

  {
    char default_data[tab2_c1_default_len + 2];
    default_data[0] = (tab2_c1_default_len >> 0) & 0xff;
    default_data[1] = (tab2_c1_default_len >> 8) & 0xff;
    memset(default_data + 2, tab2_c1_default_char, tab2_c1_default_len);
    
    myOperation2->setValue("C1", default_data);
  }

  if (myTransaction->execute(NdbTransaction::Commit) == -1)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }
  
  g_ndb->closeTransaction(myTransaction);

  //The following insert should fail, and return error code
  const NdbDictionary::Table *myTable3= g_dict->getTable(g_tablename3);

  if (myTable3 == NULL)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  NdbTransaction *myTransaction3 = g_ndb->startTransaction();
  if (myTransaction3 == NULL)
  {
    PRINT_ERROR(g_ndb->getNdbError());
    return NDBT_FAILED;
  }

  NdbOperation *myOperation3 = myTransaction3->getNdbOperation(myTable3);
  if (myOperation3 == NULL)
  {
    PRINT_ERROR(myTransaction3->getNdbError());
    g_ndb->closeTransaction(myTransaction3);
    return NDBT_FAILED;
  }
  myOperation3->insertTuple();
  myOperation3->equal("PK", 1);

  /* It should return error code 839 ( msg: Illegal null attribute)
   * for an attempt to insert to a table containing defaults 
   * without supplying a value for a not-null, non-defaulted column
  */
  if (myTransaction3->execute(NdbTransaction::Commit) == -1)
  {
    PRINT_ERROR(myTransaction3->getNdbError());
    
    if (myTransaction3->getNdbError().code != 839)
    {
      ndbout << "Expected error 839" << endl;
      g_ndb->closeTransaction(myTransaction3);
      return NDBT_FAILED;
    }
  }
  g_ndb->closeTransaction(myTransaction3);

  return NDBT_OK;
}

#define CHECK_VAL_EQ(ref, test) {                \
    if ((ref) != (test)) {                       \
      ndbout << "Equality failed at line " << __LINE__ << "\n" \
             << test << " != " << ref << "\n"; \
      return NDBT_FAILED;                      \
    } }

#define CHECK_BYTES_EQ(ref, test, len) {        \
    if (memcmp((ref), (test), (len))) {         \
      ndbout << "Bytes differ at line " << __LINE__ << "\n"; \
      return NDBT_FAILED;                                    \
    }} 




static int do_read()
{
  NdbRecAttr *myRecAttr[column_count_table1];
  NdbRecAttr *myRecAttr2[column_count_table2];
  NdbRecAttr *myRecAttr3[column_count_table3];

  const NdbDictionary::Table *myTable= g_dict->getTable(g_tablename1);
  const NdbDictionary::Table *myTable2 = g_dict->getTable(g_tablename2);
  const NdbDictionary::Table *myTable3 = g_dict->getTable(g_tablename3);

  if (myTable == NULL || myTable2 == NULL || myTable3 == NULL)
  {
    PRINT_ERROR(g_dict->getNdbError());
    return NDBT_FAILED;
  }

  NdbTransaction *myTransaction= g_ndb->startTransaction();
  if (myTransaction == NULL)
  {
    PRINT_ERROR(g_ndb->getNdbError());
    return NDBT_FAILED;
  }

  //Define Scan Operation for T_DEF1
  NdbScanOperation *myScanOp = myTransaction->getNdbScanOperation(myTable);
  if (myScanOp == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  if(myScanOp->readTuples(NdbOperation::LM_CommittedRead) == -1)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  myRecAttr[0] = myScanOp->getValue("PK");
  myRecAttr[1] = myScanOp->getValue("C1");
  myRecAttr[2] = myScanOp->getValue("C2");
  myRecAttr[3] = myScanOp->getValue("C3");
  myRecAttr[4] = myScanOp->getValue("C4");
  myRecAttr[5] = myScanOp->getValue("C5");
  myRecAttr[6] = myScanOp->getValue("C6");
  myRecAttr[7] = myScanOp->getValue("C7");

  for (unsigned int i = 0; i < column_count_table1; i++)
    if (myRecAttr[i] == NULL)
    {
      PRINT_ERROR(myTransaction->getNdbError());
      g_ndb->closeTransaction(myTransaction);
      return NDBT_FAILED;
    }

  //Define Scan Operation for T_DEF2
  NdbScanOperation *myScanOp2 = myTransaction->getNdbScanOperation(myTable2);
  if (myScanOp2 == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  if(myScanOp2->readTuples(NdbOperation::LM_CommittedRead) == -1)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  myRecAttr2[0] = myScanOp2->getValue("PK");
  myRecAttr2[1] = myScanOp2->getValue("C1");
  if (myRecAttr2[0] == NULL || myRecAttr2[1] == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  //Define Scan Operation for T_DEF3  
  NdbScanOperation *myScanOp3 = myTransaction->getNdbScanOperation(myTable3);
  if (myScanOp3 == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  if(myScanOp3->readTuples(NdbOperation::LM_CommittedRead) == -1)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  myRecAttr3[0] = myScanOp3->getValue("PK");
  myRecAttr3[1] = myScanOp3->getValue("C1");
  if (myRecAttr3[0] == NULL || myRecAttr3[1] == NULL)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  //Execute the Transcation for the 3 scan operations
  if(myTransaction->execute(NdbTransaction::NoCommit) != 0)
  {
    PRINT_ERROR(myTransaction->getNdbError());
    g_ndb->closeTransaction(myTransaction);
    return NDBT_FAILED;
  }

  //The following print out the result
  int check;
  ndbout<< "Table: " << g_tablename1 << endl;
//   ndbout << "PK";
//   for (unsigned int i = 0; i < column_count_table1; i++)
//     ndbout << "\tC" << i ;
//   ndbout << endl;
  while((check = myScanOp->nextResult(true)) == 0){
    do {
//       for (Uint32 i = 0; i < column_count_table1; i++)
//         ndbout << *myRecAttr[i] << "\t";
//       ndbout << endl;
      
      for (Uint32 i = 0; i < column_count_table1; i++)
      {
        /* Check that all columns are non NULL except c6 */
        CHECK_VAL_EQ((i == 6), myRecAttr[i]->isNULL());
      }

      CHECK_VAL_EQ(tab1_c1_default, (int) myRecAttr[1]->int32_value());
      CHECK_VAL_EQ(tab1_c2_default, myRecAttr[2]->float_value());
      CHECK_VAL_EQ(tab1_c3_default, myRecAttr[3]->double_value());
      CHECK_BYTES_EQ(tab1_c4_default, (const char*) myRecAttr[4]->aRef(), tab1_c4_default_siglen);
      CHECK_BYTES_EQ(tab1_c5_default, (const char*) myRecAttr[5]->aRef(), tab1_c5_default_siglen); 
      CHECK_BYTES_EQ(tab1_c6_default, (const char*) myRecAttr[6]->aRef(), tab1_c6_default_siglen);
      CHECK_BYTES_EQ(tab1_c7_default, (const char*) myRecAttr[7]->aRef(), tab1_c7_default_siglen);
    } while((check = myScanOp->nextResult(false)) == 0);

    if(check == -1)
    {
      ndbout << "Error with transaction " << check << " " 
             << myTransaction->getNdbError().code << "\n";
      return NDBT_FAILED;
    }
  }

  ndbout<< "Table: " << g_tablename2 << endl;
  // ndbout << "PK\tC1" << endl;
  while((check = myScanOp2->nextResult(true)) == 0){
    do {
//       for (Uint32 i = 0; i < column_count_table2; i++)
//       {
//         if (i == 1)
//           ndbout << myRecAttr2[i]->get_size_in_bytes() << "  ";
//         ndbout << *myRecAttr2[i] << "\t";
//       }
//       ndbout << endl;
      const char* valPtr= (const char*)myRecAttr2[1]->aRef();
      char default_data[tab2_c1_default_len + 2];
      default_data[0] = (tab2_c1_default_len >> 0) & 0xff;
      default_data[1] = (tab2_c1_default_len >> 8) & 0xff;
      memset(default_data + 2, tab2_c1_default_char, tab2_c1_default_len);

      CHECK_BYTES_EQ(default_data, valPtr, tab2_c1_default_len + 2);

    } while((check = myScanOp2->nextResult(false)) == 0);
  }


  return NDBT_OK;
}

int main(int argc, char* argv[])
{
  int ret;
  ndb_init();

  ndbout << "testNativeDefault started" << endl;

  if (!connect_ndb())
  {
    ndbout << "Failed to connect to NDB" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  ndbout << "connected.." << endl;

  ndbout << "checking create table errors..." << endl;
  if ((ret = create_table_error()) != NDBT_OK)
    return NDBT_ProgramExit(ret);

  ndbout << "creating table..." << endl;
  if ((ret = create_table()) != NDBT_OK)
    return NDBT_ProgramExit(ret);

  ndbout << "inserting..." << endl;
  if ((ret = do_insert()) != NDBT_OK)
    return NDBT_ProgramExit(ret);

  ndbout << "reading..." << endl;
  if ((ret = do_read()) != NDBT_OK)
    return NDBT_ProgramExit(ret);

  if ((ret = drop_table()) != NDBT_OK)
    return NDBT_ProgramExit(ret);

  ndbout << "done!" << endl;

  disconnect_ndb();

  ndbout << "All tests successful" << endl;
  return NDBT_ProgramExit(NDBT_OK);
}
