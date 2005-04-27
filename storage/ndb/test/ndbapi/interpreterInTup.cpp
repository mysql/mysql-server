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

/* ***************************************************
       TEST OF INTERPRETER IN TUP
       Verify that the interpreter in TUP is able to
       handle and execute all the commands that the
       NdbApi can isssue

       Arguments:

       operationType     1 openScanRead,
                         2 openScanExclusive,
                         3 interpretedUpdateTuple,
                         4 interpretedDirtyUpdate,
                         5 interpretedDeleteTuple
                         6 deleteTuple
                         7 insertTuple
                         8 updateTuple
                         9 writeTuple
                         10 readTuple
                         11 readTupleExclusive
                         12 simpleRead
                         13 dirtyRead
                         14 dirtyUpdate
                         15 dirtyWrite

       tupTest           1 exitMethods
                         2 incValue
                         3 subValue
                         4 readAttr
                         5 writeAttr
                         6 loadConst
                         7 branch
                         8 branchIfNull
                         9 addReg
                         10 subReg
                         11 subroutineWithBranchLabel

        scanTest         Number of the test within each tupTest

* *************************************************** */

#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbApi.hpp>
#include <NdbSchemaCon.hpp>
#include <NDBT.hpp>

#define MAXATTR 3
#define MAXTABLES 12
#define MAXSTRLEN 8
#define NUMBEROFRECORDS 1000

typedef enum {
    FAIL = 0,
    NO_FAIL,
	UNDEF
} TTYPE;

inline void  setAttrNames() ;
inline void  setTableNames() ;
void  error_handler(const NdbError & err, TTYPE);
void  create_table(Ndb*);
void  write_rows(Ndb*);
void  update_rows(Ndb*, int, int);
void  delete_rows(Ndb*, int, int);
void  verify_deleted(Ndb*);
void  read_and_verify_rows(Ndb*, bool pre);
void  scan_rows(Ndb*, int, int, int);
TTYPE t_exitMethods(int, NdbOperation*, int);
TTYPE t_incValue(int, NdbOperation*);
TTYPE t_subValue(int, NdbOperation*);
TTYPE t_readAttr(int, NdbOperation*);
TTYPE t_writeAttr(int, NdbOperation*);
TTYPE t_loadConst(int, NdbOperation*, int);
TTYPE t_branch(int, NdbOperation*);
TTYPE t_branchIfNull(int, NdbOperation*);
TTYPE t_addReg(int, NdbOperation*);
TTYPE t_subReg(int, NdbOperation*);
TTYPE t_subroutineWithBranchLabel(int, NdbOperation*);

char        tableName[MAXSTRLEN+1];
char        attrName[MAXATTR][MAXSTRLEN+1];
int         attrValue[NUMBEROFRECORDS] = {0};
int         pkValue[NUMBEROFRECORDS] = {0};
const int   tAttributeSize = 1 ;
const int   nRecords = 20 ;
int         bTestPassed = 0;
  


int main(int argc, const char** argv) {
  ndb_init();

  int operationType = 0;
  int tupTest = 0;
  int scanTest = 0;

  Ndb* pNdb = new Ndb("TEST_DB");
  pNdb->init();

  if (argc != 4  ||  sscanf(argv[1],"%d", &operationType) != 1) {
    operationType = 1 ;
  }
  if (argc != 4  ||  sscanf(argv[2],"%d", &tupTest) != 1) {
    tupTest = 1 ;
  }
  if (argc != 4  ||  sscanf(argv[3],"%d", &scanTest) != 1) {
    scanTest = 1 ;
  }

  ndbout << endl
      << "Test the interpreter in TUP using SimpleTable with\n"
      << nRecords << " records" << endl << endl ;

  if (pNdb->waitUntilReady(30) != 0) {
    ndbout << "NDB is not ready" << endl;
    return -1;
  }

  // Init the pk and attr values.
  for (int i = 0; i < NUMBEROFRECORDS; i ++)
    pkValue[i] = attrValue[i] = i ;

  setAttrNames() ;
  setTableNames() ;
  
  const void * p = NDBT_Table::discoverTableFromDb(pNdb, tableName);
  if (p != 0){
    create_table(pNdb);
  }
  
  write_rows(pNdb);

  ndbout << endl << "Starting interpreter in TUP test." << endl << "Operation type: " ;

  switch(operationType) {
  case 1:
    ndbout << "openScanRead" << endl;
    scan_rows(pNdb,  operationType,  tupTest,  scanTest);
    break;
  case 2:
    ndbout << "openScanExclusive" << endl;
    scan_rows(pNdb,  operationType,  tupTest,  scanTest);
    break;
  case 3:
    ndbout << "interpretedUpdateTuple" << endl;
    update_rows(pNdb,  tupTest,  operationType);
    break;
  case 4:
    ndbout << "interpretedDirtyUpdate" << endl;
    update_rows(pNdb,  tupTest,  operationType);
    break;
  case 5:
    ndbout << "interpretedDeleteTuple" << endl;
    delete_rows(pNdb,  tupTest,  operationType);
    break;
  case 6:
    ndbout << "deleteTuple" << endl;
    break;
  case 7:
    ndbout << "insertTuple" << endl;
    break;
  case 8:
    ndbout << "updateTuple" << endl;
    break;
  case 9:
    ndbout << "writeTuple" << endl;
    break;
  case 10:
    ndbout << "readTuple" << endl;
    break;
  case 11:
    ndbout << "readTupleExclusive" << endl;
    break;
  case 12:
    ndbout << "simpleRead" << endl;
    break;
  case 13:
    ndbout << "dirtyRead" << endl;
    break;
  case 14:
    ndbout << "dirtyUpdate" << endl;
    break;
  case 15:
    ndbout << "dirtyWrite" << endl;
    break;
  default:
    break ;

  }

//  read_and_verify_rows(pNdb, false);
	
//  delete_rows(pNdb, 0, 0) ;
  delete pNdb ;

  if (bTestPassed == 0) {
      ndbout << "OK: test passed" << endl;
      exit(0);
  }else{
      ndbout << "FAIL: test failed" << endl;
      exit(-1);
  }
}

void error_handler(const NdbError & err, TTYPE type_expected) {

  ndbout << err << endl ;

  switch (type_expected){
      case NO_FAIL:
          bTestPassed = -1 ;
          break ;
      case FAIL:
          ndbout << "OK: error is expected" << endl;
          break ;
      case UNDEF:
          ndbout << "assumed OK: expected result undefined" << endl ;
          break ;
      default:
          break ;
  }
}

void  create_table(Ndb* pMyNdb) {

  /****************************************************************
   *    Create SimpleTable and Attributes.
   *
   *    create table SimpleTable1(
   *        col0 int,
   *        col1 int not null,
   *        col2 int not null,
   *        col3 int not null ... 129)
   *
   ***************************************************************/

  int               check = -1 ;
  NdbSchemaOp       *MySchemaOp = NULL ;

  ndbout << endl << "Creating " << tableName << " ... " << endl;

   NdbSchemaCon* MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pMyNdb);
   if(!MySchemaTransaction) error_handler(MySchemaTransaction->getNdbError(), NO_FAIL);
   
   MySchemaOp = MySchemaTransaction->getNdbSchemaOp();
   if( !MySchemaOp ) error_handler(MySchemaTransaction->getNdbError(), NO_FAIL);
   // Create table
   check = MySchemaOp->createTable( tableName,
                                     8,         // Table size
                                     TupleKey,  // Key Type
                                     40         // Nr of Pages
                                   );

   if( check == -1 ) error_handler(MySchemaTransaction->getNdbError(), NO_FAIL);

   ndbout << "Creating attributes ... " << flush;

   // Create first column, primary key
   check = MySchemaOp->createAttribute( attrName[0],
                                        TupleKey,
                                        32,
                                        1/*3, tAttributeSize */,
                                        UnSigned,
                                        MMBased,
                                        NotNullAttribute );

   if( check == -1 ) error_handler(MySchemaTransaction->getNdbError(), NO_FAIL);

   // create the 2 .. n columns.
   for ( int i = 1; i < MAXATTR; i++ ){
       check = MySchemaOp->createAttribute( attrName[i],
                                            NoKey,
                                            32,
                                            tAttributeSize,
                                            UnSigned,
                                            MMBased,
                                            NotNullAttribute );

      if( check == -1 ) error_handler(MySchemaTransaction->getNdbError(), NO_FAIL);
   }

   ndbout << "OK" << endl;

   if( MySchemaTransaction->execute() == -1 ) {
       ndbout << MySchemaTransaction->getNdbError() << endl;
   }else{
       ndbout << tableName[0] << " created" << endl;
   }


   NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);

   return;

}



void write_rows (Ndb* pMyNdb) {

  /****************************************************************
   *    Insert rows into SimpleTable
   *
   ***************************************************************/

  int check = -1 ;
  int loop_count_ops = nRecords ;
  NdbOperation      *MyOperation = NULL ;
  NdbConnection     *MyTransaction = NULL ;

  ndbout << endl << "Writing records ..."  << flush;

  for (int count=0 ; count < loop_count_ops ; count++){
      MyTransaction = pMyNdb->startTransaction();
      if (!MyTransaction) {
          error_handler(pMyNdb->getNdbError(), NO_FAIL);
      }//if

      MyOperation = MyTransaction->getNdbOperation(tableName);
      if (!MyOperation) {
        error_handler(pMyNdb->getNdbError(), NO_FAIL);
      }//if

      check = MyOperation->writeTuple();
      if( check == -1 ) error_handler(MyTransaction->getNdbError(), NO_FAIL);

      check = MyOperation->equal( attrName[0],(char*)&pkValue[count] );
      if( check == -1 ) error_handler(MyTransaction->getNdbError(), NO_FAIL);

      // Update the columns, index column already ok.
      for (int i = 1 ; i < MAXATTR; i++){
          if ((i == 2) && (count > 4)){
              check = MyOperation->setValue(attrName[i], (char*)&attrValue[count + 1]);
          }else{
              check = MyOperation->setValue(attrName[i], (char*)&attrValue[count]);
          }
        if( check == -1 ) error_handler(MyTransaction->getNdbError(), NO_FAIL);
      }
      check = MyTransaction->execute( Commit );
      if(check == -1 ) error_handler(MyTransaction->getNdbError(), NO_FAIL);
      
      pMyNdb->closeTransaction(MyTransaction);
    }
   ndbout <<" \tOK" << endl;
   return;
}

void verify_deleted(Ndb* pMyNdb) {

  int               check = -1 ;
  int               loop_count_ops = nRecords;
  NdbOperation*     pMyOperation = NULL ;

  ndbout << "Verifying deleted records..."<< flush;

  for (int count=0 ; count < loop_count_ops ; count++){

      NdbConnection*  pMyTransaction = pMyNdb->startTransaction();
      if (!pMyTransaction) error_handler(pMyNdb->getNdbError(), NO_FAIL);
      
      pMyOperation = pMyTransaction->getNdbOperation(tableName);
      if (!pMyOperation) error_handler(pMyNdb->getNdbError(), NO_FAIL);
      
      check = pMyOperation->readTuple();
      if( check == -1 ) error_handler( pMyTransaction->getNdbError(), NO_FAIL);

      check = pMyOperation->equal( attrName[0],(char*)&pkValue[count] );
      if( check == -1 ) error_handler( pMyTransaction->getNdbError(), NO_FAIL);

     // Exepect to receive an error
     if(pMyTransaction->execute(Commit) != -1)
         if( 626 == pMyTransaction->getNdbError().code){
             ndbout << pMyTransaction->getNdbError() << endl ;
             ndbout << "OK" << endl ;
         }else{
             error_handler(pMyTransaction->getNdbError(), NO_FAIL) ;
         }

     pMyNdb->closeTransaction(pMyTransaction);
    }

  ndbout << "OK" << endl;
  return;
};

void read_and_verify_rows(Ndb* pMyNdb, bool pre) {

  int               check = -1 ;
  int               loop_count_ops = nRecords;
  char              expectedCOL1[NUMBEROFRECORDS] = {0} ;
  char              expectedCOL2[NUMBEROFRECORDS] = {0} ;
  NdbConnection     *pMyTransaction = NULL ;
  NdbOperation      *MyOp = NULL ;
  NdbRecAttr*       tTmp = NULL ;
  int               readValue[MAXATTR] = {0} ;

  ndbout << "Verifying records...\n"<< endl;

  for (int count=0 ; count < loop_count_ops ; count++){
      
      pMyTransaction = pMyNdb->startTransaction();
      if (!pMyTransaction) error_handler(pMyNdb->getNdbError(), NO_FAIL);

      MyOp = pMyTransaction->getNdbOperation(tableName);
      if (!MyOp) error_handler( pMyTransaction->getNdbError(), NO_FAIL);


      check = MyOp->readTuple();
      if( check == -1 ) error_handler( MyOp->getNdbError(), NO_FAIL);
      
      check = MyOp->equal( attrName[0],(char*)&pkValue[count] );
      if( check == -1 ) error_handler( MyOp->getNdbError(), NO_FAIL);

      for (int count_attributes = 1; count_attributes < MAXATTR; count_attributes++){
          
          tTmp = MyOp->getValue( (char*)attrName[count_attributes], (char*)&readValue[count_attributes] );
          if(!tTmp) error_handler( MyOp->getNdbError(), NO_FAIL);
      }

      if( pMyTransaction->execute( Commit ) == -1 ) {
         error_handler(pMyTransaction->getNdbError(), NO_FAIL);
      } else {
        if (pre) {
          expectedCOL1[count] = readValue[1];
          expectedCOL2[count] = readValue[2];
        }
        
        ndbout << attrName[1] << "\t " << readValue[1] << "\t "
             << attrName[2] << "\t " << readValue[2] << endl;
      }

      pMyNdb->closeTransaction(pMyTransaction);

  }

  ndbout << "\nOK\n" << endl;

  return;

};

TTYPE t_exitMethods(int nCalls, NdbOperation * pOp, int opType) {

  ndbout << "Defining exitMethods test " << nCalls << ": " << endl ;;
  TTYPE ret_val = NO_FAIL ;

  switch(nCalls){
  case 1: // exit_nok if attr value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 14);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok() ;
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 2: // exit_ok if attr value doesn't match
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 14);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok() ;
    pOp->def_label(0);
    if (opType == 3) {
      // For update transactions use incValue to update the tuple
      Uint32 val32 = 5;
      pOp->incValue("COL2", val32);
    }
    pOp->interpret_exit_ok();
    break ;
  case 3: // Non-existent value (128): exit_ok if if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 128);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    pOp->interpret_exit_ok();
	ret_val = FAIL ;
    break;
  case 4: // Non-existent value (128): exit_nok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 128);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
	ret_val = FAIL ;
    break;
  case 5: // exit_nok of the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 2);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break ;
  case 6: // exit_ok of the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 2);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    if (opType == 3) {
      // For update transactions use incValue to update the tuple
      Uint32 val32 = 5;
      pOp->incValue("COL2", val32);
    }
    pOp->interpret_exit_ok();
    break;
  case 7: // exit_nok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 6);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 8: // exit_ok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 6);
    pOp->branch_ne(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    if (opType == 3) {
      // For update transactions use incValue to update the tuple
      Uint32 val32 = 5;
      pOp->incValue("COL2", val32);
    }
    pOp->interpret_exit_ok();
    break ;
  case 9: // exit_nok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 8);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 10: // exit_ok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 8);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    if (opType == 3) {
      // For update transactions use incValue to update the tuple
      Uint32 val32 = 5;
      pOp->incValue("COL2", val32);
    }
    pOp->interpret_exit_ok();
    break;
  case 11: // exit_nok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 10);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 12:
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 10);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    if (opType == 3) {
      // For update transactions use incValue to update the tuple
      Uint32 val32 = 5;
      pOp->incValue("COL2", val32);
    }
    pOp->interpret_exit_ok();
    break;
  case 13:
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 10);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 14: // exit_nok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 12);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 15: // exit_ok if the value matches
    pOp->read_attr("COL1", 1);
    pOp->load_const_u32(2, 12);
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    if (opType == 3) {
      // For update transactions use incValue to update the tuple
      Uint32 val32 = 5;
      pOp->incValue("COL2", val32);
    }
    pOp->interpret_exit_ok();
  case 16:
    pOp->interpret_exit_nok();
    ret_val = FAIL ;
    break;
  case 17:
    pOp->interpret_exit_ok();
    break ;
  case 18:
    pOp->interpret_exit_nok();
    ret_val = FAIL ;
    break ;
  default:
    break ;
  }
  return ret_val;
}

TTYPE t_incValue(int nCalls, NdbOperation* pOp) {

  ndbout << "Defining incValue test " << nCalls << ": ";
  TTYPE ret_val = NO_FAIL;

  Uint32 val32 = 5;
  Uint64 val64 = 5;
  Uint32 attr0 = 0;
  Uint32 attr1 = 1;
  Uint32 attr20 = 20;

  switch(nCalls) {
  case 1:
    pOp->incValue(attrName[1], val32);
    break;
  case 2:
    pOp->incValue(attr1, val32);
    break;
  case 3:
    pOp->incValue(attrName[1], val64);
    break;
  case 4:
    pOp->incValue(attr1, val64);
    break;
  case 5:
    pOp->incValue(attrName[0], val32);
    ret_val = FAIL ;
    break;
  case 6:
    pOp->incValue(attrName[0], val64);
    ret_val = FAIL ;
    break;
  case 7:
    pOp->incValue(attr0, val32);
    ret_val = FAIL ;
    break;
  case 8:
    pOp->incValue(attr0, val64);
    ret_val = FAIL ;
    break;
  case 9:
    pOp->incValue("COL20", val32);
    ret_val = FAIL ;
    break;
  case 10:
    pOp->incValue("COL20", val64);
    ret_val = FAIL ;
    break;
  case 11:
    pOp->incValue(attr20, val32);
    ret_val = FAIL ;
    break;
  case 12:
    pOp->incValue(attr20, val64);
    ret_val = FAIL ;
    break;
  default:
      break ;
  }
  return ret_val;
}

TTYPE t_subValue(int nCalls, NdbOperation* pOp) {

  ndbout << "Defining subValue test " << nCalls << ": ";

  Uint32 val32 = 5;
  Uint64 val64 = 5;
  Uint32 attr0 = 0;
  Uint32 attr1 = 1;
  Uint32 attr20 = 20;

  TTYPE ret_val = NO_FAIL;

  switch(nCalls) {
  case 1:
    pOp->subValue("COL2", val32);
    break;
  case 2:
    pOp->subValue(attr1, val32);
    break;
  case 3:
    pOp->subValue("COL0", val32);
    ret_val = FAIL ;
    break;
  case 4:
    pOp->subValue(attr0, val32);
    ret_val = FAIL ;
    break;
  case 5:
    pOp->subValue("COL20", val32);
    ret_val = FAIL ;
    break;
  case 6:
    pOp->subValue(attr20, val32);
    ret_val = FAIL ;
    break;
  case 7:
    // Missing implementation
    //pOp->subValue("COL20", val64);
    ndbout << "Missing implementation" << endl;
    break;
  case 8:
    // Missing implementation
    //pOp->subValue("COL2", val64);
    ndbout << "Missing implementation" << endl;
    break;
  case 9:
    // Missing implementation
    //pOp->subValue("COL0", val64);
    ndbout << "Missing implementation" << endl;
    break;
  case 10:
    // Missing implementation
    //pOp->subValue(attr1, val64);
    ndbout << "Missing implementation" << endl;
    break;
  case 11:
    // Missing implementation
    //pOp->subValue(attr0, val64);
    ndbout << "Missing implementation" << endl;
    break;
  case 12:
    // Missing implementation
    //pOp->subValue(attr20, val64);
    ndbout << "Missing implementation" << endl;
    break;
  default:
      break ;
  }
  return ret_val ;
}

TTYPE t_readAttr(int nCalls, NdbOperation* pOp) {

  ndbout << "Defining readAttr test " << nCalls << ": ";

  Uint32 attr0 = 0;
  Uint32 attr1 = 1;
  Uint32 attr20 = 20;
  TTYPE ret_val = NO_FAIL;

  switch(nCalls) {
  case 1:
    pOp->read_attr("COL1", 1);
    break;
  case 2:
    pOp->read_attr(attr1, 1);
    ret_val = NO_FAIL ;
    break;
  case 3:
    pOp->read_attr("COL0", 1);
    ret_val = NO_FAIL ;
    break;
  case 4:
    pOp->read_attr(attr0, 1);
    ret_val = NO_FAIL ;
    break;
  case 5:
    pOp->read_attr("COL20", 1);
    ret_val = FAIL ;
    break;
  case 6:
    pOp->read_attr(20, 1);
    ret_val = FAIL ;
    break;
  case 7:
    pOp->read_attr("COL1", 8);
    ret_val = FAIL ;
    break;
  case 8:
    pOp->read_attr(attr1, 8);
    ret_val = FAIL ;
    break;
  default:
      break ;
  }
  return ret_val;
}

TTYPE t_writeAttr(int nCalls, NdbOperation* pOp) {

  ndbout << "Defining writeAttr test " << nCalls << ": ";

  pOp->load_const_u32(1, 5);

  Uint32 attr0 = 0;
  Uint32 attr1 = 1;
  Uint32 attr20 = 20;
  TTYPE ret_val = NO_FAIL ;

  switch(nCalls) {
  case 1:
    pOp->write_attr("COL1", 1);
    break;
  case 2:
    pOp->write_attr(attr1, 1);
    break;
  case 3:
    pOp->write_attr("COL0", 1);
    ret_val = FAIL ;
    break;
  case 4:
    pOp->write_attr(attr0, 1);
    ret_val = FAIL ;
    break;
  case 5:
    pOp->write_attr("COL20", 1);
    ret_val = FAIL ;
    break;
  case 6:
    pOp->write_attr(20, 1);
    ret_val = FAIL ;
    break;
  case 7:
    pOp->write_attr("COL1", 2);
    ret_val = FAIL ;
    break;
  case 8:
    pOp->write_attr(attr1, 2);
    ret_val = FAIL ;
    break;
  case 9:
    pOp->write_attr("COL1", 8);
    ret_val = FAIL ;
    break;
  case 10:
    pOp->write_attr(attr1, 8);
    ret_val = FAIL ;
    break;
  default:
      break ;
  }
  return ret_val ;
}

TTYPE t_loadConst(int nCalls, NdbOperation* pOp, int opType) {

  ndbout << "Defining loadConst test " << nCalls << " : ";
  TTYPE ret_val = NO_FAIL ;

  switch(nCalls) {
  case 1:
    // Loading null into a valid register
    pOp->load_const_null(1);
    break;
  case 2:
    // Loading null into an invalid register
    pOp->load_const_null(8);
    ret_val = FAIL ;
    break;
  case 3:
    // Test loading a 32-bit value (>65536)
    pOp->load_const_u32(1, 65600);
    break;
  case 4:
    // Test loading a 16-bit value (<65536)
    pOp->load_const_u32(1, 65500);
    break;
  case 5:
    // Test by loading to a non-valid register
    pOp->load_const_u32(8, 2);
    ret_val = FAIL ;
    break;
  case 6:
    // Test by loading a 64-bit value
    pOp->load_const_u64(1, 65600);
    break;
  case 7:
    // Test by loading a non-valid register
    pOp->load_const_u64(8, 2);
    ret_val = FAIL ;
    break;
  case 8:
    // Test by loading a valid register with -1
    pOp->load_const_u64(1, -1);
    ret_val = FAIL ;
    break;

  default:
      break ;
  }

  if (opType == 3 && FAIL != ret_val)
    pOp->write_attr("COL1", 1);
  return ret_val;
}

TTYPE t_branch(int nCalls, NdbOperation* pOp) {

  ndbout << "Defining branch test " << nCalls << ": " ;
  
  TTYPE ret_val = NO_FAIL ;
  Uint32 val32=5;

  pOp->read_attr("COL1", 1);
  pOp->load_const_u32(2, val32);

  switch(nCalls) {
  case 1:
    pOp->branch_eq(1, 2, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    pOp->interpret_exit_ok();
    break;
  case 2:
    pOp->branch_eq(2, 1, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    pOp->interpret_exit_ok();
    break;
  case 3:
    pOp->branch_eq(1, 1, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    pOp->interpret_exit_ok();
    break;
  default:
      //ndbout << "t_branch: default case (no test case)" << endl ;
      //return ret_val = NO_FAIL ;
      break ;
  }
  return ret_val;
}

TTYPE t_branchIfNull(int nCalls, NdbOperation* pOp) {

  TTYPE ret_val = NO_FAIL;
  ndbout << "Defining branchIfNull test " << nCalls << ": " << endl ;

  switch(nCalls) {
  case 1:
    pOp->load_const_u32(1, 1);
    pOp->branch_ne_null(1, 0);
    pOp->interpret_exit_nok();
    pOp->def_label(0);
    pOp->interpret_exit_ok();
    break;
  case 2:
    pOp->load_const_null(1);
    pOp->branch_ne_null(1, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 3:
    pOp->load_const_u32(1, 1);
    pOp->branch_eq_null(1, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 4:
    pOp->load_const_null(1);
    pOp->branch_ne_null(1, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    break;
  case 5:
    // Test with a non-initialized register
    pOp->branch_ne_null(3, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    ret_val = FAIL ;
    break;
  case 6:
    // Test with a non-existing register
    pOp->branch_ne_null(8, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    ret_val = FAIL ;
    break;
  case 7:
    // Test with a non-initialized register
    pOp->branch_eq_null(3, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    ret_val = FAIL ;
    break;
  case 8:
    // Test with a non-existing register
    pOp->branch_ne_null(8, 0);
    pOp->interpret_exit_ok();
    pOp->def_label(0);
    pOp->interpret_exit_nok();
    ret_val = FAIL ;
    break;
  default:
      break ;
  }

  return ret_val;
}

TTYPE t_addReg(int nCalls, NdbOperation* pOp) {

  TTYPE ret_val = NO_FAIL ;
  
  ndbout << "Defining addReg test " << nCalls << ": ";

  pOp->load_const_u32(1, 65500);
  pOp->load_const_u32(2, 500);
  pOp->load_const_u64(3, 65600);
  pOp->load_const_u64(4, 95600);

  switch(nCalls) {
  case 1:
    pOp->add_reg(1, 2, 5);
    break;
  case 2:
    pOp->add_reg(1, 3, 5);
    break;
  case 3:
    pOp->add_reg(3, 1, 5);
    break;
  case 4:
    pOp->add_reg(3, 4, 5);
    break;
  case 5:
    pOp->add_reg(1, 6, 5);
    break;
  case 6:
    pOp->add_reg(6, 1, 5);
    break;
  case 7: // illegal register
    pOp->add_reg(1, 8, 5);
    ret_val = FAIL ;
    break;
  case 8: // another illegal register
    pOp->add_reg(8, 1, 5); 
    ret_val = FAIL ;
    break;
  case 9: // and another one
    pOp->add_reg(1, 2, 8);
    ret_val = FAIL ;
    break;
  default:
    break ;
  }
  pOp->load_const_u32(7, 65000);
  pOp->branch_eq(5, 7, 0);
  pOp->interpret_exit_nok();
  pOp->def_label(0);
  pOp->interpret_exit_ok();

  return ret_val ;
}

TTYPE t_subReg(int nCalls, NdbOperation* pOp) {

  TTYPE ret_val = NO_FAIL ;
  ndbout << "Defining subReg test: " << nCalls << endl;

  pOp->load_const_u32(1, 65500);
  pOp->load_const_u32(2, 500);
  pOp->load_const_u64(3, 65600);
  pOp->load_const_u64(4, 95600);

  switch(nCalls) {
  case 1:
    pOp->sub_reg(1, 2, 5);
    pOp->load_const_u32(7, 65000);
    break;
  case 2:
    pOp->sub_reg(1, 3, 5);
    pOp->load_const_u64(7, (Uint64)-100);
    break;
  case 3:
    pOp->sub_reg(3, 1, 5);
    pOp->load_const_u64(7, (Uint64)100);
    break;
  case 4:
    pOp->sub_reg(3, 4, 5);
    pOp->load_const_u64(7, (Uint64)-30000);
    break;
  case 5:
    pOp->sub_reg(1, 6, 5);
    pOp->load_const_u64(7, 0);
    ret_val = FAIL ;
    break;
  case 6:
    pOp->sub_reg(6, 1, 5);
    pOp->load_const_u64(7, 0);
    ret_val = FAIL ;
    break;
  case 7:
    pOp->sub_reg(1, 8, 5);
    pOp->load_const_u64(7, 0);
    ret_val = FAIL ;
    break;
  case 8:
    pOp->sub_reg(8, 1, 5);
    pOp->load_const_u64(7, 0);
    ret_val = FAIL ;
    break;
  case 9:
    pOp->sub_reg(1, 2, 8);
    pOp->load_const_u32(7, (Uint32)65000);
    ret_val = FAIL;
    break;
  default:
      //ndbout << "t_subReg: default case (no test case)" << endl ;
      //return ret_val = NO_FAIL ;
      break ;
  }
  pOp->branch_eq(5, 7, 0);
  pOp->interpret_exit_nok() ;
  pOp->def_label(0);
  pOp->interpret_exit_ok() ;

  return ret_val;
}

TTYPE t_subroutineWithBranchLabel(int nCalls, NdbOperation* pOp) {

  TTYPE ret_val = NO_FAIL ;
  ndbout << "Defining subroutineWithBranchLabel test:" << nCalls << endl;

  pOp->load_const_u32(1, 65500);
  pOp->load_const_u32(2, 500);
  pOp->load_const_u64(3, 65600);
  pOp->load_const_u64(4, 95600);
  pOp->load_const_u32(7, 65000);
  pOp->call_sub(0) ;

  switch(nCalls) {
  case 1:
    pOp->def_subroutine(0) ;
    pOp->add_reg(1, 2, 5);
    break;
  case 2:
    pOp->def_subroutine(0) ;
    pOp->add_reg(1, 3, 5);
    break;
  case 3:
    pOp->def_subroutine(0) ;
    pOp->add_reg(3, 1, 5);
    break;
  case 4:
    pOp->def_subroutine(0) ;
    pOp->add_reg(3, 4, 5);
    break;
  case 5:
    pOp->def_subroutine(0) ;
    pOp->add_reg(1, 6, 5);
    break;
  case 6:
    pOp->def_subroutine(0) ;
    pOp->add_reg(6, 1, 5);
    break;
  case 7: // illegal register
    pOp->def_subroutine(0) ;
    pOp->add_reg(1, 8, 5);
    ret_val = FAIL ;
    break;
  case 8: // another illegal register
    pOp->def_subroutine(0) ;
    pOp->add_reg(8, 1, 5); 
    ret_val = FAIL ;
    break;
  case 9: // and another one
    pOp->def_subroutine(0) ;
    pOp->add_reg(1, 2, 8);
    ret_val = FAIL ;
    break;
  case 10: // test subroutine nesting
      for(int sub = 0; sub < 25 ; ++sub){
          pOp->call_sub(sub) ;
          pOp->def_subroutine(sub + 1) ;
          pOp->interpret_exit_ok() ;
          pOp->ret_sub() ;
      }
    ret_val = FAIL ;
  default:
    break ;
  }
  
  pOp->branch_label(0) ;
  pOp->interpret_exit_nok() ;
  pOp->def_label(0) ;
  pOp->interpret_exit_ok() ;
  pOp->ret_sub() ;

  return ret_val ;
}


void scan_rows(Ndb* pMyNdb, int opType, int tupleType, int scanType) {

  int           check = -1 ; 
  int           loop_count_ops = nRecords ;
  int           eOf = -1 ;
  int           readValue = 0 ;
  int           readValue2 = 0 ;
  int           scanCount = 0 ;
  TTYPE         fail = NO_FAIL ;

  for (int count=0 ; count < loop_count_ops ; count++)    {
  NdbConnection* MyTransaction = pMyNdb->startTransaction();
  if (!MyTransaction) error_handler(pMyNdb->getNdbError(), NO_FAIL);

  NdbOperation*  MyOperation = MyTransaction->getNdbOperation(tableName);
  if (!MyOperation) error_handler(pMyNdb->getNdbError(), NO_FAIL);

  if (opType == 1)
    // Open for scan read, Creates the SCAN_TABREQ and if needed
    // SCAN_TABINFO signals.
    check = MyOperation->openScanRead(1);
  else if (opType == 2)
    // Open for scan with update of rows.
    check = MyOperation->openScanExclusive(1);

  // Create ATTRINFO signal(s) for interpreted program used for
  // defining search criteria and column values that should be returned.

  scanCount = count+1 ;

  switch(tupleType) {
  case 1:
    fail = t_exitMethods(scanCount, MyOperation,  opType);
    break;
  case 2:
    fail = t_incValue(scanCount, MyOperation);
    break;
  case 3:
    fail = t_subValue(scanCount, MyOperation);
    break;
  case 4:
    fail = t_readAttr(scanCount, MyOperation);
    break;
  case 5:
    fail = t_writeAttr(scanCount, MyOperation);
    break;
  case 6:
    fail = t_loadConst(scanCount, MyOperation,  opType);
    break;
  case 7:
    fail = t_branch(scanCount, MyOperation);
    break;
  case 8:
    fail = t_branchIfNull(scanCount, MyOperation);
    break;
  case 9:
    fail = t_addReg(scanCount, MyOperation);
    break;
  case 10:
    fail = t_subReg(scanCount, MyOperation);
    break;
  case 11:
    fail = t_subroutineWithBranchLabel(scanCount, MyOperation);
    break;
  default:
    break ;
  }

  if(11 != tupleType) MyOperation->getValue(attrName[1], (char*)&readValue);

  // Sends the SCAN_TABREQ, (SCAN_TABINFO) and ATTRINFO signals and then
  // reads the answer in TRANSID_AI. Confirmation is received through SCAN_TABCONF or
  // SCAN_TABREF if failure.
  check = MyTransaction->executeScan();
  if (check == -1) {
    //ndbout << endl << "executeScan returned: " << MyTransaction->getNdbError() << endl;
      error_handler(MyTransaction->getNdbError(), fail) ;
	  pMyNdb->closeTransaction(MyTransaction);
  }else{
    // Sends the SCAN_NEXTREQ signal(s) and reads the answer in TRANS_ID signals.
    // SCAN_TABCONF or SCAN_TABREF is the confirmation.
    while ((eOf = MyTransaction->nextScanResult()) == 0) {
      ndbout << readValue <<"; ";
      // Here we call takeOverScanOp for update of the tuple.
    }
    ndbout << endl ;

    pMyNdb->closeTransaction(MyTransaction);
    if (eOf == -1) {
      ndbout << endl << "nextScanResult returned: "<< MyTransaction->getNdbError() << endl;
    } else {
      ndbout << "OK" << endl;
    }
  }
  }
  return;

};


void  update_rows(Ndb* pMyNdb, int tupleType, int opType) {
   /****************************************************************
   *    Update rows in SimpleTable
   *    Only updates the first 3 cols.
   ***************************************************************/

  int check = -1 ;
  int loop_count_ops = nRecords ;
  int readValue[MAXATTR] = {0} ;
  TTYPE ret_val = NO_FAIL ;
  NdbConnection     *MyTransaction = NULL ;
  NdbOperation      *MyOperation = NULL ;

  ndbout << "Updating records ..." << endl << endl;

  for (int count=0 ; count < loop_count_ops ; count++){

    MyTransaction = pMyNdb->startTransaction();
    if (!MyTransaction) {
      error_handler(pMyNdb->getNdbError(), NO_FAIL);
      return;
    }//if

    MyOperation = MyTransaction->getNdbOperation(tableName);
    if (MyOperation == NULL) {
      error_handler(pMyNdb->getNdbError(), NO_FAIL);
      return;
    }//if

    //   if (operationType == 3)
    check = MyOperation->interpretedUpdateTuple();
    // else if (operationType == 4)
    // check = MyOperation->interpretedDirtyUpdate();
    if( check == -1 )
      error_handler(MyTransaction->getNdbError(), NO_FAIL);

    check = MyOperation->equal( attrName[0], (char*)&pkValue[count] );
    if( check == -1 )
      error_handler(MyTransaction->getNdbError(), NO_FAIL);

    switch(tupleType) {
    case 1:
      ret_val = t_exitMethods(count+1, MyOperation,  opType);
      break;
    case 2:
      ret_val = t_incValue(count+1, MyOperation);
      break;
    case 3:
      ret_val = t_subValue(count+1, MyOperation);
      break;
    case 4:
      ret_val = t_readAttr(count+1, MyOperation);
      break;
    case 5:
      ret_val = t_writeAttr(count+1, MyOperation);
      break;
    case 6:
      ret_val = t_loadConst(count+1, MyOperation,  opType);
      break;
    case 7:
      ret_val = t_branch(count+1, MyOperation);
      break;
    case 8:
      ret_val = t_branchIfNull(count+1, MyOperation);
      break;
    case 9:
      ret_val = t_addReg(count+1, MyOperation);
      break;
    case 10:
      ret_val = t_subReg(count+1, MyOperation);
      break;
    case 11:
      ret_val = t_subroutineWithBranchLabel(count+1, MyOperation);
      break;
    default:
        break ;
    }

    MyOperation->getValue("COL1", (char*)&readValue);

    if (MyTransaction->execute( Commit ) == -1 ) {
       error_handler(MyTransaction->getNdbError(), ret_val);
    }else if (NO_FAIL == ret_val ) {
      ndbout << "OK" << endl;
    } else {
      bTestPassed = -1;
      ndbout << "Test passed when expected to fail" << endl;
    }//if
    pMyNdb->closeTransaction(MyTransaction);

  }

  ndbout << "Finished updating records" << endl;
  return;
};

void delete_rows(Ndb* pMyNdb, int tupleTest, int opType) {

  /****************************************************************
   *    Delete rows from SimpleTable
   *
   ***************************************************************/

  int check = 1 ;
  int loop_count_ops = nRecords ;
  int readValue[MAXATTR] = {0};
  NdbConnection     *MyTransaction = NULL ;
  NdbOperation      *MyOperation = NULL ;
  TTYPE ret_val = NO_FAIL ;
  
  ndbout << "Deleting records ..."<< endl << endl;
 
  for (int count=0 ; count < loop_count_ops ; count++)    {

    MyTransaction = pMyNdb->startTransaction();
    if (!MyTransaction) error_handler(pMyNdb->getNdbError(), NO_FAIL) ;
    
    MyOperation = MyTransaction->getNdbOperation(tableName);
    if (!MyOperation) error_handler(pMyNdb->getNdbError(), NO_FAIL) ;

    check = MyOperation->interpretedDeleteTuple();
    if( check == -1 ) error_handler(MyTransaction->getNdbError(), NO_FAIL) ;

    check = MyOperation->equal( attrName[0], (char*)&pkValue[count] );
    if( check == -1 ) error_handler(MyTransaction->getNdbError(), NO_FAIL) ;

      switch(tupleTest) {
      case 1:
        ret_val = t_exitMethods(count+1, MyOperation,  opType);
        break;
      case 2:
        ret_val = t_incValue(count+1, MyOperation);
        break;
      case 3:
        ret_val = t_subValue(count+1, MyOperation);
        break;
      case 4:
        ret_val = t_readAttr(count+1, MyOperation);
        break;
      case 5:
        ret_val = t_writeAttr(count+1, MyOperation);
        break;
      case 6:
        ret_val = t_loadConst(count+1, MyOperation, opType);
        break;
      case 7:
        ret_val = t_branch(count+1, MyOperation);
        break;
      case 8:
        ret_val = t_branchIfNull(count+1, MyOperation);
        break;
      case 9:
        ret_val = t_addReg(count+1, MyOperation);
        break;
      case 10:
        ret_val = t_subReg(count+1, MyOperation);
        break;
      case 11:
        ret_val = t_subroutineWithBranchLabel(count+1, MyOperation);
        break;
      default:
        break ;
      }

      if(11 != tupleTest)MyOperation->getValue(attrName[1], (char*)&readValue) ;

      if (MyTransaction->execute( Commit ) == -1 ) {
         error_handler(MyTransaction->getNdbError(), ret_val);
      } else if (NO_FAIL == ret_val /*|| UNDEF == ret_val*/ ) {
        ndbout << "OK" << endl;
      } else {
        bTestPassed = -1;
        ndbout << "Test passed when expected to fail" << endl;
      }//if
      ndbout << endl;
      pMyNdb->closeTransaction(MyTransaction);
   }

   ndbout << "Finished deleting records" << endl;
   return;

};


inline void setAttrNames(){
  for (int i = 0; i < MAXATTR; i++){
      BaseString::snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
    }
}


inline void setTableNames(){
  BaseString::snprintf(tableName, MAXSTRLEN, "TAB1");
}

