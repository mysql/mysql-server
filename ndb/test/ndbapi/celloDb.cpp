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
       BASIC TEST 1
       Test basic functions and status of NDB

       Arguments:
        none 

       Returns:
        0 - Test passed
       -1 - Test failed
        1 - Invalid arguments
flexBench
 * *************************************************** */

#include <NdbApi.hpp>
#include <NdbMain.h>

#define MAXATTR 4
#define MAXTABLES 4
#define PAGESIZE 8192
#define OVERHEAD 0.02
#define NUMBEROFRECORDS 10
#define PKSIZE 1
#define ATTRNAMELEN 16


static void  createTable_IPACCT(Ndb*);
static void  createTable_RPACCT(Ndb*);
static void  createTable_SBMCALL(Ndb*);
static void  createTable_TODACCT(Ndb*);

static void  error_handler(const char*);
static bool  error_handler2(const char*, int) ;

static void  setAttrNames(void);
static void  setTableNames(void);
static void  create_table(Ndb*);
static void  insert_rows(Ndb*);
static void  update_rows(Ndb*);
static void  delete_rows(Ndb*);
static void  verify_deleted(Ndb*);
static void  read_and_verify_rows(Ndb*);

static void  insert_IPACCT(Ndb*, Uint32, Uint32, Uint32, Uint32, Uint32); //3 for Pk, and two data. just to test;

static void read_IPACCT(Ndb* , Uint32 , Uint32  , Uint32 );

static  int		        tAttributeSize;
static  int             bTestPassed;

static  char    		tableName[MAXTABLES][ATTRNAMELEN];static  char		    attrName[MAXATTR][ATTRNAMELEN];
static  int    		    attrValue[NUMBEROFRECORDS];
static  int                 pkValue[NUMBEROFRECORDS];
static int		    failed = 0 ;
#include <NdbOut.hpp>

NDB_COMMAND(celloDb, "celloDb", "celloDb", "celloDb", 65535)
{
  ndb_init();

  int                   tTableId;
  int                   i;
  Ndb	      		MyNdb( "CELLO-SESSION-DB" );

  MyNdb.init();

  // Assume test passed
  bTestPassed = 0;
  /*
  // Initialize global variables
  for (i = 0; i < NUMBEROFRECORDS; i ++)
     pkValue[i] = i; 
  
  for (i = 0; i < NUMBEROFRECORDS; i ++)
     attrValue[i] = i;
  */
  tAttributeSize = 1;

  // Wait for Ndb to become ready
  if (MyNdb.waitUntilReady() == 0) {
    ndbout << endl << "Cello session db - Starting " << endl;
    ndbout << "Test basic functions and status of NDB" << endl;
    

      
    createTable_SBMCALL (&MyNdb );
    createTable_RPACCT (&MyNdb );
    createTable_TODACCT (&MyNdb );
    createTable_IPACCT (&MyNdb );
    
    insert_IPACCT(&MyNdb, 1,2,1,2,2);
    read_IPACCT(&MyNdb, 1, 2 , 1);
    /*
      insert_rows(&MyNdb);

      read_and_verify_rows(&MyNdb);


      // Create some new values to use for update
      for (i = 0; i < NUMBEROFRECORDS; i++)
		  attrValue[i] = NUMBEROFRECORDS-i;

      update_rows(&MyNdb);

      read_and_verify_rows(&MyNdb);

      delete_rows(&MyNdb);

      verify_deleted(&MyNdb);
      */
    }
    else {
	bTestPassed = -1;
      }


  if (bTestPassed == 0)
    {
      // Test passed
      ndbout << "OK - Test passed" << endl;
    }
  else
    {
      // Test failed
      ndbout << "FAIL - Test failed" << endl;
      exit(-1);
    }
  return NULL;
}

static void 
error_handler(const char* errorText)
{
  // Test failed 
  ndbout << endl << "ErrorMessage: " << errorText << endl;
  bTestPassed = -1;
}

static void
createTable_SBMCALL ( Ndb* pMyNdb )
{
  /****************************************************************
   *	Create table and attributes. 	
   *
   *    create table SBMCALL(
   *      for attribs, see the REQ SPEC for cello session DB
   *     )
   *  
   ***************************************************************/ 

  const char* tname = "SBMCALL";
  Uint32 recordsize = 244; //including 12 byte overhead
  Uint32 pksize = 8; //size of total prim. key. in bytes. sum of entire composite PK.
  Uint32 tTableId = pMyNdb->getTable()->openTable(tname);
  
  if (tTableId == -1) {    
    Uint32              check; 
    Uint32              i;
    NdbSchemaCon      	*MySchemaTransaction;
    NdbSchemaOp	       	*MySchemaOp;
    
    ndbout << "Creating " << tname << "..." << endl;
    
    MySchemaTransaction = pMyNdb->startSchemaTransaction();
    if( ( MySchemaTransaction == NULL ) && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
    MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
    if( ( MySchemaOp == NULL ) && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
        
    // Createtable

    Uint32 tablesize=(recordsize*NUMBEROFRECORDS + OVERHEAD * NUMBEROFRECORDS)/1024;
    Uint32 noPages=(pksize*NUMBEROFRECORDS)/PAGESIZE;
    
    ndbout << "table size " << tablesize << "for table name " << tname << endl;
    
    check = MySchemaOp->createTable( tname,
        		             tablesize,		// Table Size
				     TupleKey,	// Key Type
				     noPages,		// Nr of Pages
				     All,
				     6,
				     78,
				     80,
				     1,
				     true
				     );
    
    if( check == -1 ) {
      error_handler(MySchemaTransaction->getNdbErrorString());
      exit(-1);
    }
    

    
    // Create first column, primary key 
    check = MySchemaOp->createAttribute( "SPBBOARDID",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );

    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create second column, primary key 
    check = MySchemaOp->createAttribute( "CALLID",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Creat thrid column, RP Session info, byte[16] represented as (String, 16)
    check = MySchemaOp->createAttribute( "RPSESS",
					 NoKey,
					 32,
					 16,
					 String,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
// Creat fourth column, GRE Tunnel info, byte[16] represented as (String, 16)
    check = MySchemaOp->createAttribute( "GRETUNNEL",
					 NoKey,
					 32,
					 16,
					 String,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
// Creat fifth column, PPP Session info, byte[24] represented as (String, 24)
    check = MySchemaOp->createAttribute( "PPPSESS",
					 NoKey,
					 32,
					 24,
					 String,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

    
    if( (MySchemaTransaction->execute() == -1) && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
    pMyNdb->closeSchemaTransaction(MySchemaTransaction);
    ndbout << "done" << endl;
    
    
  } //if 
  
  //else table already created , proceed
}



static void
createTable_RPACCT(Ndb*pMyNdb)
{
  
  /****************************************************************
   *	Create table and attributes. 	
   *
   *    create table RPACCT(
   *      for attribs, see the REQ SPEC for cello session DB
   *     )
   *  
   ***************************************************************/ 
 
 const char* tname = "RPACCT";
 Uint32 recordsize = 380; //including 12 byte overhead
 Uint32 pksize = 8; //size of total prim. key. in bytes.
 Uint32 tTableId = pMyNdb->getTable()->openTable(tname);
 
 if (tTableId == -1) {    
   Uint32              check; 
   Uint32              i;
   NdbSchemaCon      	*MySchemaTransaction;
   NdbSchemaOp	       	*MySchemaOp;
   
   ndbout << "Creating " << tname << "..." << endl;
   
   MySchemaTransaction = pMyNdb->startSchemaTransaction();
   if( MySchemaTransaction == NULL )
     error_handler(MySchemaTransaction->getNdbErrorString());
   
   MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
   if( MySchemaOp == NULL ) 
      error_handler(MySchemaTransaction->getNdbErrorString());
   
   // Createtable
   
   Uint32 tablesize=(recordsize*NUMBEROFRECORDS + OVERHEAD * NUMBEROFRECORDS)/1024;
   Uint32 noPages=(pksize*NUMBEROFRECORDS)/PAGESIZE;
   
   ndbout << "table size " << tablesize << "for table name " << tname << endl;
   
   check = MySchemaOp->createTable( tname,
				    tablesize,		// Table Size
				    TupleKey,	// Key Type
				     noPages		// Nr of Pages
				    );
   
   if( check == -1 ) 
     error_handler(MySchemaTransaction->getNdbErrorString());
   
   
   
   // Create first column, primary key 
   check = MySchemaOp->createAttribute( "SPBBOARDID",
					TupleKey,
					32,
					PKSIZE,
					 UnSigned,
					MMBased,
					NotNullAttribute );
   
   if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
     exit (-1) ;
   
   
   // Create second column, primary key 
   check = MySchemaOp->createAttribute( "CALLID",
					TupleKey,
					32,
					PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Creat thrid column MS ID, 4 byte unsigned
    check = MySchemaOp->createAttribute( "MSID",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create PDSN FA Address, 4 byte unsigned
    check = MySchemaOp->createAttribute( "PDSN",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create Serving PCF, 4 byte unsigned
    check = MySchemaOp->createAttribute( "SPCF",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    


    // Create BS ID, 12 byte char, represented as String,12
    check = MySchemaOp->createAttribute( "BSID",
					 NoKey,
					 32,
					 12,
					 String,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    




    // Create User Zone, 4 byte unsigned
    check = MySchemaOp->createAttribute( "UZ",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create Forward Multiplex, 4 byte unsigned
    check = MySchemaOp->createAttribute( "FMO",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create Reverse Multiplex, 4 byte unsigned
    check = MySchemaOp->createAttribute( "RMO",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


    // Create Forward Fund rate, 4 byte unsigned
    check = MySchemaOp->createAttribute( "FFR",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

    
 // Create Reverse Fund rate, 4 byte unsigned
    check = MySchemaOp->createAttribute( "RFR",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


    
 // Create Service Option, 4 byte unsigned
    check = MySchemaOp->createAttribute( "SO",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;




 // Create Forward Traffic Type, 4 byte unsigned
    check = MySchemaOp->createAttribute( "FTT",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



// Create Reverse Traffic Type, 4 byte unsigned
    check = MySchemaOp->createAttribute( "RTT",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



// Create Fund Frame Size, 4 byte unsigned
    check = MySchemaOp->createAttribute( "FFS",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



// Create Forware Fund RC, 4 byte unsigned
    check = MySchemaOp->createAttribute( "FFRC",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



    // Create Reverse Fund RC, 4 byte unsigned
    check = MySchemaOp->createAttribute( "RFRC",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create DCCH Frame Format, 4 byte unsigned
    check = MySchemaOp->createAttribute( "DCCH",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
    
    // Create Airlink QOS, 4 byte unsigned
    check = MySchemaOp->createAttribute( "AQOS",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



// Create Bad PPP Frame Count , 4 byte unsigned
    check = MySchemaOp->createAttribute( "BPPPFC",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;




// Create Active Time , 4 byte unsigned
    check = MySchemaOp->createAttribute( "AT",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



// Create Nb Active Transitions , 4 byte unsigned
    check = MySchemaOp->createAttribute( "NBAT",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


// Create SDB Octet Count In , 4 byte unsigned
    check = MySchemaOp->createAttribute( "SDBOCI",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


    

// Create Nb SDB In, 4 byte unsigned
    check = MySchemaOp->createAttribute( "NBSDBI",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;




// Create Nb SDB Out, 4 byte unsigned
    check = MySchemaOp->createAttribute( "NBSDBO",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;




// Create HDLC Bytes received, 4 byte unsigned
    check = MySchemaOp->createAttribute( "HDLC",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    
    if( (MySchemaTransaction->execute() == -1) && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
    pMyNdb->closeSchemaTransaction(MySchemaTransaction);
    ndbout << "done" << endl;
    
 } //if 
  
 //else table already created , proceed
  }
 

static void
createTable_IPACCT(Ndb* pMyNdb)
{


  /****************************************************************
   *	Create table and attributes. 	
   *
   *    create table IPACCT(
   *      for attribs, see the REQ SPEC for cello session DB
   *     )
   *  
   ***************************************************************/ 

  const char* tname = "IPACCT";
  Uint32 recordsize = 70; //including 12 byte overhead
  Uint32 pksize = 12; //size of total prim. key. in bytes.
  Uint32 tTableId = pMyNdb->getTable()->openTable(tname);
  
  if (tTableId == -1) {    
    Uint32              check; 
    Uint32              i;
    NdbSchemaCon      	*MySchemaTransaction;
    NdbSchemaOp	       	*MySchemaOp;
    
    ndbout << "Creating " << tname << "..." << endl;
    
    MySchemaTransaction = pMyNdb->startSchemaTransaction();
    if( MySchemaTransaction == NULL )
      error_handler(MySchemaTransaction->getNdbErrorString());
    
    MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
    if( MySchemaOp == NULL ) 
      error_handler(MySchemaTransaction->getNdbErrorString());
    
    // Createtable

    Uint32 tablesize=(recordsize*NUMBEROFRECORDS + OVERHEAD * NUMBEROFRECORDS)/1024;
    Uint32 noPages=(pksize*NUMBEROFRECORDS)/PAGESIZE;
    
    ndbout << "table size " << tablesize << "for table name " << tname << endl;
    
    check = MySchemaOp->createTable( tname,
        		             tablesize,		// Table Size
				     TupleKey,	// Key Type
				     noPages		// Nr of Pages
				     );
    
    if( check == -1 ) 
      error_handler(MySchemaTransaction->getNdbErrorString());
    


    // Create first column, primary key 
    check = MySchemaOp->createAttribute( "SPBBOARDID",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );

    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create second column, primary key 
    check = MySchemaOp->createAttribute( "CALLID",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

   // Create third column, primary key 
    check = MySchemaOp->createAttribute( "IPADDR",
					 TupleKey,
					 32,
					 PKSIZE,
					 String,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


     

// Create Acct session id  4 byte unsigned
    check = MySchemaOp->createAttribute( "ASID",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


// Create Correlation ID, 4 byte unsigned
    check = MySchemaOp->createAttribute( "CID",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


// Create MIP HA Address, 4 byte unsigned
    check = MySchemaOp->createAttribute( "MIPHA",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


    

    
// Create IP technology, 4 byte unsigned
    check = MySchemaOp->createAttribute( "IPT",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



// Create Compuls Tunnel ID, 4 byte unsigned
    check = MySchemaOp->createAttribute( "CTID",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

// Create IP QOS, 4 byte unsigned
    check = MySchemaOp->createAttribute( "IPQOS",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

    
    // Create Data octet count in, 4 byte unsigned
    check = MySchemaOp->createAttribute( "DOCI",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

    // Create Data octet count out, 4 byte unsigned
    check = MySchemaOp->createAttribute( "DOCO",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

    // Create Event time, 4 byte unsigned
    check = MySchemaOp->createAttribute( "ET",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

     // Create In mip sig count, 4 byte unsigned
    check = MySchemaOp->createAttribute( "IMSC",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

// Create Out mip sig count, 4 byte unsigned
    check = MySchemaOp->createAttribute( "OMSC",
					 NoKey,
					 32,
					 1,
					 UnSigned,
					 MMBased,
					 NullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

    
    if( (MySchemaTransaction->execute() == -1) && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    
    pMyNdb->closeSchemaTransaction(MySchemaTransaction);
    ndbout << "done" << endl;
    


  } //if 

  //else table already created , proceed
}


static void
createTable_TODACCT(Ndb* pMyNdb)
{


  /****************************************************************
   *	Create table and attributes. 	
   *
   *    create table TODACCT(
   *      for attribs, see the REQ SPEC for cello session DB
   *     )
   *  
   ***************************************************************/ 
  
  const char* tname = "TODACCT";
  Uint32 recordsize = 92; //including 12 byte overhead
  Uint32 pksize = 12; //size of total prim. key. in bytes.
  Uint32 tTableId = pMyNdb->getTable()->openTable(tname);
  
  if (tTableId == -1) {    
    Uint32              check; 
    Uint32              i;
    NdbSchemaCon      	*MySchemaTransaction;
    NdbSchemaOp	       	*MySchemaOp;
    
    ndbout << "Creating " << tname << "..." << endl;
    
    MySchemaTransaction = pMyNdb->startSchemaTransaction();
    if( MySchemaTransaction == NULL )
      error_handler(MySchemaTransaction->getNdbErrorString());
    
    MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
    if( MySchemaOp == NULL ) 
      error_handler(MySchemaTransaction->getNdbErrorString());
    
    // Createtable
    
    Uint32 tablesize=(recordsize*NUMBEROFRECORDS + OVERHEAD * NUMBEROFRECORDS)/1024;
    Uint32 noPages=(pksize*NUMBEROFRECORDS)/PAGESIZE;
    
    ndbout << "table size " << tablesize << "for table name " << tname << endl;
    
    check = MySchemaOp->createTable( tname,
        		             tablesize,		// Table Size
				     TupleKey,	// Key Type
				     noPages		// Nr of Pages
				     );
    
    if( check == -1 ) 
      error_handler(MySchemaTransaction->getNdbErrorString());
    


    // Create first column, primary key 
    check = MySchemaOp->createAttribute( "SPBBOARDID",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );

    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;
    

    // Create second column, primary key 
    check = MySchemaOp->createAttribute( "CALLID",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;

   // Create third column, primary key 
    check = MySchemaOp->createAttribute( "IPADDR",
					 TupleKey,
					 32,
					 PKSIZE,
					 String,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;



   // Create third column, primary key 
    check = MySchemaOp->createAttribute( "INDEX",
					 TupleKey,
					 32,
					 PKSIZE,
					 UnSigned,
					 MMBased,
					 NotNullAttribute );
    
    if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


     

// Create Acct session id  4 byte unsigned
    check = MySchemaOp->createAttribute( "TOD",
					 NoKey,
					 32,
					 16,
					 String,
					 MMBased,
					 NullAttribute );


   if( (check == -1)  && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
      exit (-1) ;


     
   if( (MySchemaTransaction->execute() == -1) && (!error_handler2((const char*)MySchemaTransaction->getNdbErrorString(),  MySchemaTransaction->getNdbError())) )
     exit (-1) ;
   
   pMyNdb->closeSchemaTransaction(MySchemaTransaction);
   ndbout << "done" << endl;
   
  } //if 
  
 //else table already created , proceed
}






static void  read_IPACCT(Ndb* pMyNdb, Uint32 CALLID, Uint32 SPBBOARDID , Uint32 IPADDR)
{


  int                   check;
  int                   loop_count_ops;
  int					count;
  int                   count_attributes;
  char* value;
  NdbConnection			*MyTransaction;
  NdbOperation			*MyOperation;
  NdbRecAttr*           tTmp;
  
  
  
  MyTransaction = pMyNdb->startTransaction();
  if (MyTransaction == NULL)
    error_handler(pMyNdb->getNdbErrorString());
  
  MyOperation = MyTransaction->getNdbOperation("IPACCT");	
  if (MyOperation == NULL) 
    error_handler( MyTransaction->getNdbErrorString());
  
  check = MyOperation->readTuple();
  if( check == -1 ) 
    error_handler( MyTransaction->getNdbErrorString());
  
  check = MyOperation->equal( "SPBBOARDID",SPBBOARDID );
  if( check == -1 ) 
    error_handler( MyTransaction->getNdbErrorString());
  

  check = MyOperation->equal( "IPADDR","IPADDR" );
  if( check == -1 ) 
    error_handler( MyTransaction->getNdbErrorString());
  

  check = MyOperation->equal( "CALLID",CALLID );
  if( check == -1 ) 
    error_handler( MyTransaction->getNdbErrorString());




  tTmp = MyOperation->getValue("IPQOS", NULL );
  if( tTmp == NULL ) 
    error_handler( MyTransaction->getNdbErrorString());
  ndbout << " tTmp " << tTmp->isNULL() << endl;
  MyTransaction->execute(Commit);
  
  ndbout << " value read " << tTmp->int32_value() << endl;
  
}



static void  insert_IPACCT(Ndb* pMyNdb, Uint32 CALLID, Uint32 SPBBOARDID , Uint32 IPADDR, Uint32 ASID, Uint32 IPQOS)
{
  /****************************************************************
   *	Insert rows 	
   *
   ***************************************************************/

  int                   check;
  int                   loop_count_ops;
  int					count;
  int                   i;
  NdbConnection			*MyTransaction;
  NdbOperation			*MyOperation;
 
  ndbout << "Inserting records..."  << flush;
   
  MyTransaction = pMyNdb->startTransaction();
  if (MyTransaction == NULL)	  
    error_handler(pMyNdb->getNdbErrorString());
  
  MyOperation = MyTransaction->getNdbOperation("IPACCT");	
  if (MyOperation == NULL) 
    error_handler(MyTransaction->getNdbErrorString());
 

 
  check = MyOperation->insertTuple();
  if( check == -1 ) 
    error_handler(MyTransaction->getNdbErrorString());
  
  ndbout << "insertTuple"  << endl;  
  
  check = MyOperation->equal("CALLID",CALLID );
  if( check == -1 ) 
    error_handler(MyTransaction->getNdbErrorString());
  ndbout << "equal"  << endl;  
  

  check = MyOperation->equal("SPBBOARDID",SPBBOARDID );
  if( check == -1 ) 
    error_handler(MyTransaction->getNdbErrorString());
  ndbout << "equal"  << endl;  


  check = MyOperation->equal("IPADDR","IPADDR" );
  if( check == -1 ) 
    error_handler(MyTransaction->getNdbErrorString());
  ndbout << "equal"  << endl;  


  check = MyOperation->setValue( "IPQOS", IPQOS);
  if( check == -1 ) 
    error_handler(MyTransaction->getNdbErrorString());
  ndbout << "Set Value"  << endl;
  


  check = MyOperation->setValue( "ASID", ASID);
  if( check == -1 ) 
    error_handler(MyTransaction->getNdbErrorString());
  ndbout << "Set Value"  << endl;
  

  check = MyTransaction->execute( Commit ); 
  if(check == -1 ) {
    ndbout << "error at commit"  << endl;
    error_handler(MyTransaction->getNdbErrorString());
  }
  else
    ;//ndbout << ".";
  
  pMyNdb->closeTransaction(MyTransaction);
  
  
      
  ndbout << "OK" << endl;
  
  return;
}

static void  
update_rows(Ndb* pMyNdb){
 /****************************************************************
   *	Update rows in SimpleTable 	
   *
   ***************************************************************/

  int                   check;
  int                   loop_count_ops;
  int					count;
  int                   i;
  NdbConnection			*MyTransaction;
  NdbOperation			*MyOperation;

   ndbout << "Updating records..." << flush;
   
   loop_count_ops = NUMBEROFRECORDS;

   for (count=0 ; count < loop_count_ops ; count++)    {

      MyTransaction = pMyNdb->startTransaction();
      if (MyTransaction == NULL)
		  error_handler( pMyNdb->getNdbErrorString() );

      MyOperation = MyTransaction->getNdbOperation(tableName[0]);	
      if (MyOperation == NULL) 
        error_handler(MyTransaction->getNdbErrorString());

      check = MyOperation->updateTuple();
      if( check == -1 ) 
        error_handler(MyTransaction->getNdbErrorString());
	 
      check = MyOperation->equal( attrName[0], (char*)&pkValue[count] );
      if( check == -1 ) 
         error_handler(MyTransaction->getNdbErrorString());

      for (i = 1; i < MAXATTR; i++)
	{
          check = MyOperation->setValue( attrName[i], (char*)&attrValue[count]);
          if( check == -1 ) 
             error_handler(MyTransaction->getNdbErrorString());
	}

      if( MyTransaction->execute( Commit ) == -1 )
         error_handler(MyTransaction->getNdbErrorString());
      else
	;//ndbout << ".";
        
      pMyNdb->closeTransaction(MyTransaction);

    }

   ndbout << "OK" << endl;
  return;

};

static void  
delete_rows(Ndb* pMyNdb){

  /****************************************************************
   *	Delete rows from SimpleTable 	
   *
   ***************************************************************/

  int                   check;
  int                   loop_count_ops;
  int					count;
  NdbConnection			*MyTransaction;
  NdbOperation			*MyOperation;

   ndbout << "Deleting records..."<< flush;
   
   loop_count_ops = NUMBEROFRECORDS;

   for (count=0 ; count < loop_count_ops ; count++)    {

      MyTransaction = pMyNdb->startTransaction();
      if (MyTransaction == NULL)
		  error_handler( pMyNdb->getNdbErrorString() );

      MyOperation = MyTransaction->getNdbOperation(tableName[0]);	
      if (MyOperation == NULL) 
        error_handler(MyTransaction->getNdbErrorString());


      check = MyOperation->deleteTuple();
      if( check == -1 ) 
        error_handler(MyTransaction->getNdbErrorString());
	 
      check = MyOperation->equal( attrName[0], (char*)&pkValue[count] );
      if( check == -1 ) 
         error_handler(MyTransaction->getNdbErrorString());


      if( MyTransaction->execute( Commit ) == -1 )
         error_handler(MyTransaction->getNdbErrorString());
      else
	;// ndbout << ".";
        
      pMyNdb->closeTransaction(MyTransaction);

    }

   ndbout << "OK" << endl;
  return;

};

static void 
verify_deleted(Ndb* pMyNdb){
  int                   check;
  int                   loop_count_ops;
  int					count;
  NdbConnection			*MyTransaction;
  NdbOperation			*MyOperation;

  ndbout << "Verifying deleted records..."<< flush;
    
  loop_count_ops = NUMBEROFRECORDS;

  for (count=0 ; count < loop_count_ops ; count++)
  {
     MyTransaction = pMyNdb->startTransaction();
     if (MyTransaction == NULL)
        error_handler(pMyNdb->getNdbErrorString());

     MyOperation = MyTransaction->getNdbOperation(tableName[0]);	
     if (MyOperation == NULL) 
        error_handler( MyTransaction->getNdbErrorString());
       
     check = MyOperation->readTuple();
     if( check == -1 ) 
       error_handler( MyTransaction->getNdbErrorString());

     check = MyOperation->equal( attrName[0],(char*)&pkValue[count] );
     if( check == -1 ) 
        error_handler( MyTransaction->getNdbErrorString());

     // Exepect to receive an error
      if( MyTransaction->execute( Commit ) != -1 ) 
         error_handler(MyTransaction->getNdbErrorString());
      else
      {
        ;//ndbout << ".";
      }
     
      pMyNdb->closeTransaction(MyTransaction);

    }

   ndbout << "OK" << endl;
  return;
};

static void 
read_and_verify_rows(Ndb* pMyNdb)
{

  int                   check;
  int                   loop_count_ops;
  int					count;
  int                   count_attributes;

  NdbConnection			*MyTransaction;
  NdbOperation			*MyOperation;
  NdbRecAttr*           tTmp;

  int					readValue[MAXATTR];

  ndbout << "Verifying records..."<< flush;
    
  loop_count_ops = NUMBEROFRECORDS;

  for (count=0 ; count < loop_count_ops ; count++)
  {
     MyTransaction = pMyNdb->startTransaction();
     if (MyTransaction == NULL)
        error_handler(pMyNdb->getNdbErrorString());

     MyOperation = MyTransaction->getNdbOperation(tableName[0]);	
     if (MyOperation == NULL) 
        error_handler( MyTransaction->getNdbErrorString());
       
     check = MyOperation->readTuple();
     if( check == -1 ) 
        error_handler( MyTransaction->getNdbErrorString());

     check = MyOperation->equal( attrName[0],(char*)&pkValue[count] );
     if( check == -1 ) 
        error_handler( MyTransaction->getNdbErrorString());

     for (count_attributes = 1; count_attributes < MAXATTR; count_attributes++)
     {
        tTmp = MyOperation->getValue( (char*)attrName[count_attributes], (char*)&readValue[count_attributes] );
        if( tTmp == NULL ) 
           error_handler( MyTransaction->getNdbErrorString());
      }

      if( MyTransaction->execute( Commit ) == -1 ) 
         error_handler(MyTransaction->getNdbErrorString());
      else
      {
	// Check value in db against value in mem     

	//ndbout << readValue[1] << " == " << attrValue[count] << endl;
           
	   if ( readValue[1]!=attrValue[count] )
             error_handler("Verification error!");
           else
	   if ( readValue[2]!=attrValue[count] )
             error_handler("Verification error!");
           else
	   if ( readValue[3]!=attrValue[count] )
             error_handler("Verification error!");
           else
	   {
	     ;//ndbout << ".";
	   }
	}
        pMyNdb->closeTransaction(MyTransaction);

    }

   ndbout << "OK" << endl;
  return;

 

};


static void
setAttrNames()
{
  int i;

  for (i = 0; i < MAXATTR ; i++)
  {
    sprintf(&attrName[i][0], "Col%d", i);
  }
}

static void
setTableNames()
{
  int i;

  sprintf(&tableName[0][0], "SBMCALL", 0);
  sprintf(&tableName[1][0], "RPACCT", 0);
  sprintf(&tableName[2][0], "IPACCT", 0);
  sprintf(&tableName[3][0], "TODACCT", 0);

}


bool error_handler2(const char* error_string, int error_int) {
  failed++ ;
  ndbout << error_string << endl ;
  if ( 4008==error_int || 721==error_int || 266==error_int ){
    ndbout << endl << "Attempting to recover and continue now..." << endl ;
    return true ; // return true to retry
  }
  return false ; // return false to abort
}
