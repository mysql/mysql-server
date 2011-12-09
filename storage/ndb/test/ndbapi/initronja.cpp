/*
   Copyright (C) 2003-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

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


/* ***************************************************
       INITRONJA
       Initialise benchmark for Ronja Database
 * *************************************************** */

#include "NdbApi.hpp"
#include "NdbSchemaCon.hpp"
#include <NdbOut.hpp>
#include <NdbMain.h>
#include <NdbTest.hpp>
#include <string.h>

#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 64
#define NDB_MAXTHREADS 256
/*
  NDB_MAXTHREADS used to be just MAXTHREADS, which collides with a
  #define from <sys/thread.h> on AIX (IBM compiler).  We explicitly
  #undef it here lest someone use it by habit and get really funny
  results.  K&R says we may #undef non-existent symbols, so let's go.
*/
#undef MAXTHREADS
#define MAXATTRSIZE 8000

static unsigned int tNoOfRecords;
static unsigned int tNoOfLoops;
static unsigned int tNoOfTables;
static int tAttributeSize;
static int tNodeId;
static unsigned int tValue;
static unsigned int tNoOfOperations;
static char tableName[MAXTABLES][MAXSTRLEN];
static char attrName[MAXATTR][MAXSTRLEN];

inline int InsertRecords(Ndb*, int) ;

NDB_COMMAND(initronja, "initronja", "initronja", "initronja", 65535){
  ndb_init();

  Ndb*			 pNdb = NULL ;
  NdbSchemaCon	*MySchemaTransaction = NULL ;
  NdbSchemaOp	*MySchemaOp = NULL ;
  
  
  int check, status, i, j, cont ;
  check = status = i = j = cont = 0 ;
  tNoOfRecords = 500 ;
  tNoOfLoops = tNoOfRecords / 10;

  i = 1;
  while (argc > 1){

    if (strcmp(argv[i], "-r") == 0){
	  if( NULL == argv[i+1] ) goto error_input ; 
	  tNoOfRecords = atoi(argv[i+1]);
      tNoOfRecords = tNoOfRecords - (tNoOfRecords % 10);
      tNoOfLoops = tNoOfRecords / 10;
      if ((tNoOfRecords < 1) || (tNoOfRecords > 1000000000)) goto error_input;
    }else{
      goto error_input;
    }

    argc -= 2;
    i = i + 2; //
  }

  pNdb = new Ndb( "TEST_DB" ) ;	
  ndbout << "Initialisation started. " << endl;
  pNdb->init();
  ndbout << "Initialisation completed. " << endl;
  
  tNodeId = pNdb->getNodeId();
  ndbout << endl << "Initial loading of Ronja Database" << endl;
  ndbout << "  NdbAPI node with id = " << tNodeId << endl;
  
  if (pNdb->waitUntilReady(30) != 0) {
    ndbout << "Benchmark failed - NDB is not ready" << endl;
	delete pNdb ;
    return NDBT_ProgramExit(NDBT_FAILED) ;
  }//if
  
  ndbout << endl << "Creating the table SHORT_REC" << "..." << endl;

  MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pNdb);
  if(!MySchemaTransaction) goto error_handler;
  MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
  if(!MySchemaOp) goto error_handler;
  check = MySchemaOp->createTable( "SHORT_REC"
				   ,8		// Table Size
				   ,TupleKey	// Key Type
				   ,40		// Nr of Pages
				   );
  if (check == -1) goto error_handler;

  ndbout << "Key attribute..." ;
  check = MySchemaOp->createAttribute( (char*)"Key", TupleKey, 32, 1,
					     UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
  ndbout << "\t\tOK" << endl ;

  ndbout << "Flip attribute..." ;
  check = MySchemaOp->createAttribute("Flip", NoKey, 32, 1,
						       UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
   ndbout << "\t\tOK" << endl ;
  
  ndbout << "Count attribute..." ;
  check = MySchemaOp->createAttribute("Count", NoKey, 32, 1,
						       UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
  ndbout << "\t\tOK" << endl ;
  
  ndbout << "Placeholder attribute..." ;
  check = MySchemaOp->createAttribute("Placeholder", NoKey, 8, 90,
						       UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
  ndbout << "\tOK" << endl ;
  
  if (MySchemaTransaction->execute() == -1) {
	  if(721 == MySchemaOp->getNdbError().code){
		  ndbout << "Table SHORT_REC already exists" << endl ;
	  }else{
		  ndbout << MySchemaTransaction->getNdbError() << endl;
	  }   
  }else{
	  ndbout << "SHORT_REC created " << endl;
  }// if

  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  
  ndbout << endl << "Creating the table LONG_REC..." << endl;

  MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pNdb);
  if(!MySchemaTransaction) goto error_handler;
   
  MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
  if(!MySchemaOp) goto error_handler;
  check = MySchemaOp->createTable( "LONG_REC"
				       ,8		// Table Size
				       ,TupleKey	// Key Type
				       ,40		// Nr of Pages
				       );

  if (check == -1) goto error_handler;

  ndbout << "Key attribute..." ;
  check = MySchemaOp->createAttribute( (char*)"Key", TupleKey, 32, 1,
					     UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
  ndbout << "\t\tOK" << endl ;

  ndbout << "Flip attribute..." ;
  check = MySchemaOp->createAttribute("Flip", NoKey, 32, 1,
						       UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
   ndbout << "\t\tOK" << endl ;
  
  ndbout << "Count attribute..." ;
  check = MySchemaOp->createAttribute("Count", NoKey, 32, 1,
						       UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
   ndbout << "\t\tOK" << endl ;
  
  ndbout << "Placeholder attribute..." ;
  check = MySchemaOp->createAttribute("Placeholder", NoKey, 8, 1014,
						       UnSigned, MMBased, NotNullAttribute );
  if (check == -1) goto error_handler;
  ndbout << "\tOK" << endl ;
  
  if (MySchemaTransaction->execute() == -1) {
	  if(721 == MySchemaOp->getNdbError().code){
		  ndbout << "Table LONG_REC already exists" << endl ;
	  }else{
		  ndbout << MySchemaTransaction->getNdbError() << endl;
	  }   
  }else{
	  ndbout << "LONG_REC created" << endl;
  }// if
  
  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  

  check = InsertRecords(pNdb, tNoOfRecords);
  
  delete pNdb ;

  if(-1 == check){
	  ndbout << endl << "Initial loading of Ronja Database failed" << endl;
	  return NDBT_ProgramExit(NDBT_FAILED) ; 
  }else{
      ndbout << endl << "Initial loading of Ronja Database completed" << endl;
	  return NDBT_ProgramExit(NDBT_OK) ; 
  }
 
  
  
 

error_handler:
  ndbout << "SchemaTransaction returned error:" ;
  ndbout << MySchemaTransaction->getNdbError() << endl;
  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  delete pNdb ;
  NDBT_ProgramExit(NDBT_FAILED) ;
  exit(-1);

error_input:
  ndbout << endl << "  Ivalid parameter(s)" << endl;
  ndbout <<         "  Usage: initronja [-r n] , where 'n' is the number of records to be inserted" << endl;
  ndbout <<			"  If omitted, 500 records will be created by default" << endl;
  ndbout <<			"  Note: use this number in combination with '-r' argument when running 'benchronja'" << endl << endl;
  NDBT_ProgramExit(NDBT_WRONGARGS) ;
  exit(1);
}
////////////////////////////////////////

inline int InsertRecords(Ndb* pNdb, int nNoRecords){
	   
    NdbConnection		*MyTransaction = NULL ;
    NdbOperation*		MyOperation[10];
 
    int				Tsuccess = 0 ;
	int				loop_count_ops = 2 * tNoOfLoops;
    int				loop_count_tables = 10;
    int				loop_count_attributes = 0 ;
    int				check = 0;
    int				count = 0 ;
    int				count_tables = 0;
    int				count_attributes = 0 ;
    int				i = 0 ;
    int 			tType = 0 ;
    unsigned int	attrValue[1000];
	unsigned int	setAttrValue = 0;
	unsigned int	keyValue[3];

	for (i = 0; i < 1000; i ++) attrValue[i] = 1;
	
	for (count=0 ; count < loop_count_ops ; count++){
		  if ((((count / 100)* 100) == count) && (count != 0)){
			  ndbout << "1000 records inserted again, " << (count/100) << "000 records now inserted" << endl;
		  }
		  
		  MyTransaction = pNdb->startTransaction();
		  if(!MyTransaction){
			  ndbout << "startTransaction: " << pNdb->getNdbError();
			  ndbout << " count = " << count << endl;
			  return -1 ;
		  }
			  			  
		  for (count_tables = 0; count_tables < loop_count_tables; count_tables++) {
			  if (count < tNoOfLoops) {
				  keyValue[0] = count*10 + count_tables ;
				  MyOperation[count_tables] = MyTransaction->getNdbOperation("SHORT_REC") ;
			  }else{
				  keyValue[0] = (count - tNoOfLoops)*10 + count_tables;
				  MyOperation[count_tables] = MyTransaction->getNdbOperation("LONG_REC");
			  }//if
				  
			  if (!MyOperation[count_tables]) goto error_handler1;
			  
			  check = MyOperation[count_tables]->insertTuple();
			  if (check == -1) goto error_handler2;
          
		      check = MyOperation[count_tables]->equal("Key",(char*)&keyValue[0]);
              if (check == -1) goto error_handler4;

              check = MyOperation[count_tables]->setValue("Flip",(char*)&setAttrValue);
              if (check == -1) goto error_handler5;
          
		      check = MyOperation[count_tables]->setValue("Count",(char*)&setAttrValue);
              if (check == -1) goto error_handler5;
          
		      check = MyOperation[count_tables]->setValue("Placeholder",(char*)&attrValue[0]);
              if (check == -1) goto error_handler5;
		  }//for
     
		  if (MyTransaction->execute( Commit ) == -1){
			  ndbout << MyTransaction->getNdbError()<< endl ;
			  ndbout << "count = " << count << endl;
		  }//if
		  
		  pNdb->closeTransaction(MyTransaction) ;
	  }//for
	  return 0;

error_handler1:
   ndbout << "Error occured in getNdbOperation " << endl;
   ndbout << MyTransaction->getNdbError() << endl;
   pNdb->closeTransaction(MyTransaction);
   return -1 ;

error_handler2:
   ndbout << "Error occured in defining operation " << endl;
   ndbout << MyOperation[count_tables]->getNdbError() << endl;
   pNdb->closeTransaction(MyTransaction);
   return -1 ;

error_handler3:
   pNdb->closeTransaction(MyTransaction);
   return -1 ;
  
error_handler4:
   ndbout << "Error occured in equal " << endl;
   ndbout << MyOperation[count_tables]->getNdbError() << endl;
   pNdb->closeTransaction(MyTransaction);
   return -1 ;
  
error_handler5:
   ndbout << "Error occured in get/setValue " << endl;
   ndbout << MyOperation[count_tables]->getNdbError() << endl;
   pNdb->closeTransaction(MyTransaction);
   return -1 ;

}
