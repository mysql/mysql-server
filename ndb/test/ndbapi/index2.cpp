/* Copyright (C) 2003 MySQL AB

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

/* ***************************************************
       INDEX TEST 1
       Test index functionality of NDB

       Arguments:
        -T create table
        -L include a long attribute in key or index
        -2 define primary key with two attributes
        -c create index
	-p make index unique (include primary key attribute)
        -r read using index
	-u update using index
	-d delete using index
	-n<no operations> do n operations (for -I -r -u -d -R -U -D)
	-o<no parallel operations> (for -I -r -u -d -R -U -D)
	-m<no indexes>

       Returns:
        0 - Test passed
       -1 - Test failed
        1 - Invalid arguments
 * *************************************************** */

#include <ndb_global.h>

#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbTest.hpp>
#include <NDBT_Error.hpp>

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

#define MAX_NO_PARALLEL_OPERATIONS 100

bool testPassed = true;

static void
error_handler(const char* errorText)
{
  // Test failed 
  ndbout << endl << "ErrorMessage: " << errorText << endl;
  testPassed = false;
}

static void
error_handler4(int line, int status, int classif, int errNo, const char* errorText)
{
  ndbout << endl << "Line " << line << endl;
  // Test failed 
  ndbout << "Status " << status << ", Classification " << classif<< ", Error code " << errNo << "\n" << errorText << endl;
  testPassed = false;
}

static char *longName, *sixtysix, *ninetynine, *hundred;

static void createTable(Ndb &myNdb, bool storeInACC, bool twoKey, bool longKey)
{
  NdbDictionary::Dictionary* dict = myNdb.getDictionary();
  NdbDictionary::Table table("THE_TABLE");
  NdbDictionary::Column column;
  int res;

  column.setName("X");
  column.setType(NdbDictionary::Column::Unsigned);
  column.setLength(1);
  column.setPrimaryKey(true);
  column.setNullable(false);
  table.addColumn(column);

  column.setName("Y");
  column.setType(NdbDictionary::Column::Unsigned);
  column.setLength(1);
  column.setPrimaryKey(false);
  column.setNullable(false);
  table.addColumn(column);

  if ((res = dict->createTable(table)) == -1) {
    error_handler(dict->getNdbError().message);
  }
  else
      ndbout << "Created table" << ((longKey)?" with long key":"") <<endl;
}

static void createIndex(Ndb &myNdb, bool includePrimary, unsigned int noOfIndexes)
{
  Uint64 before, after;
  NdbDictionary::Dictionary* dict = myNdb.getDictionary();
  char indexName[] = "INDEX0000";
  int res;

  for(unsigned int indexNum = 0; indexNum < noOfIndexes; indexNum++) {
    sprintf(indexName, "INDEX%.4u", indexNum);
    NdbDictionary::Index index(indexName);
    index.setTable("THE_TABLE");
    index.setType(NdbDictionary::Index::UniqueHashIndex);
    if (includePrimary) {
      const char* attr_arr[] = {"X", "Y"};
      index.addIndexColumns(2, attr_arr);
    }
    else {
      const char* attr_arr[] = {"Y"};
      index.addIndexColumns(2, attr_arr);
    }
    before = NdbTick_CurrentMillisecond();
    if ((res = dict->createIndex(index)) == -1) {
      error_handler(dict->getNdbError().message);
    }    
    after = NdbTick_CurrentMillisecond();
    ndbout << "Created index " << indexName << ", " << after - before << " msec"<< endl;
  }
}
  
static void insertTable(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool oneTrans, bool twoKey, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbOperation   *myOp;   
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    if (!oneTrans) myTrans = myNdb.startTransaction();
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (myTrans == NULL) 
	error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
		       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbOperation("THE_TABLE"); 
      if (myOp == NULL) 
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
		       myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->insertTuple();
      if (myOp->equal("X", i) == -1) {
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
		       myTrans->getNdbError().code, myTrans->getNdbError().message);
	myNdb.closeTransaction(myTrans);
	break;
      }      
      if (myOp->setValue("Y", i+1) == -1) {
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
	myNdb.closeTransaction(myTrans);
	break;
      }      
    }
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }
    after = NdbTick_CurrentMillisecond();
    if (noOfOperations == 1)
      printf("Inserted 1 tuple, %u msec\n", (Uint32) after - before);
    else
      printf("Inserted %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);

    if (!oneTrans) myNdb.closeTransaction(myTrans);
  }
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();

  ndbout << "Inserted "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}
 
static void updateTable(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool oneTrans, bool twoKey, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbOperation   *myOp;   
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    if (!oneTrans) myTrans = myNdb.startTransaction();
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (myTrans == NULL) 
	error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
		       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbOperation("THE_TABLE"); 
      if (myOp == NULL) 
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
		       myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->updateTuple();
      if (myOp->equal("X", i) == -1) {
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
		       myTrans->getNdbError().code, myTrans->getNdbError().message);
	myNdb.closeTransaction(myTrans);
	break;
      }      
      if (myOp->setValue("Y", i+2) == -1) {
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
	myNdb.closeTransaction(myTrans);
	break;
      }      
    }
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
	error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
		       myTrans->getNdbError().code, myTrans->getNdbError().message);
	myNdb.closeTransaction(myTrans);
	break;
      }
      after = NdbTick_CurrentMillisecond();
      if (noOfOperations == 1)
	printf("Updated 1 tuple, %u msec\n", (Uint32) after - before);
      else
	printf("Update %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);

      if (!oneTrans) myNdb.closeTransaction(myTrans);
  }    
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
		     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();
  
  ndbout << "Updated "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}

static void deleteTable(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool oneTrans, bool twoKey, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbOperation   *myOp;   
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    if (!oneTrans) myTrans = myNdb.startTransaction();
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (myTrans == NULL) 
        error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
                       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbOperation("THE_TABLE"); 
      if (myOp == NULL) 
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->deleteTuple();
      if (myOp->equal("X", i) == -1) {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }      
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }
    }
    after = NdbTick_CurrentMillisecond();
    if (noOfOperations == 1)
      printf("Deleted 1 tuple, %u msec\n", (Uint32) after - before);
    else
      printf("Deleted %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);
    
    if (!oneTrans) myNdb.closeTransaction(myTrans);
  }
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();

  ndbout << "Deleted "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}

static void readTable(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool oneTrans, bool twoKey, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbOperation   *myOp;   
  NdbRecAttr* myRecAttrArr[MAX_NO_PARALLEL_OPERATIONS];
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    if (!oneTrans) myTrans = myNdb.startTransaction();
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (myTrans == NULL) 
        error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
                       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbOperation("THE_TABLE"); 
      if (myOp == NULL) 
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->readTuple();
      if (myOp->equal("X", i) == -1) {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }         
      myRecAttrArr[j-1] = myOp->getValue("Y", NULL);
    }
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }
    after = NdbTick_CurrentMillisecond();
    if (noOfOperations == 1)
      printf("Read 1 tuple, %u msec\n", (Uint32) after - before);
    else
      printf("Read %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);
    for(unsigned int j = 0; j<noOfOperations; j++)
      printf("Y = %u\n", myRecAttrArr[j]->u_32_value());
    if (!oneTrans) myNdb.closeTransaction(myTrans);
  }
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();

  ndbout << "Read "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}

static void readIndex(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool includePrimary, bool oneTrans, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbIndexOperation   *myOp; 
  char indexName[] = "INDEX0000";
  NdbRecAttr* myRecAttrArr[MAX_NO_PARALLEL_OPERATIONS];
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    if (!oneTrans) myTrans = myNdb.startTransaction();
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (myTrans == NULL) 
        error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
                       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbIndexOperation(indexName, "THE_TABLE"); 
      if (myOp == NULL) 
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->readTuple();
      if (includePrimary) {
        if (myOp->equal("X", i) == -1) {
          error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                         myTrans->getNdbError().code, myTrans->getNdbError().message);
          myNdb.closeTransaction(myTrans);
          break;
        }         
      } 
      if (myOp->equal("Y", i+1) == -1) {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }          
      myRecAttrArr[j-1] = myOp->getValue("Y", NULL);
    }
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }
    after = NdbTick_CurrentMillisecond();
    if (noOfOperations == 1)
      printf("Read 1 tuple, %u msec\n", (Uint32) after - before);
    else
      printf("Read %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);
    for(unsigned int j = 0; j<noOfOperations; j++)
      printf("Y = %u\n", myRecAttrArr[j]->u_32_value());
    if (!oneTrans) myNdb.closeTransaction(myTrans);
  }
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();

  ndbout << "Read "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}

static void updateIndex(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool includePrimary, bool oneTrans, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbIndexOperation   *myOp;   
  char indexName[] = "INDEX0000";
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    if (!oneTrans) myTrans = myNdb.startTransaction();
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (myTrans == NULL) 
        error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
                       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbIndexOperation(indexName, "THE_TABLE"); 
      if (myOp == NULL) 
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->updateTuple();
      if (includePrimary) {
        if (myOp->equal("X", i) == -1) {
          error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                         myTrans->getNdbError().code, myTrans->getNdbError().message);
          myNdb.closeTransaction(myTrans);
          break;
        }         
      } 
      if (myOp->equal("Y", i+1) == -1) {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      } 
      // Update index itself, should be possible
      if (myOp->setValue("Y", i+2) == -1) {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }      
    }
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }

    after = NdbTick_CurrentMillisecond();
    if (noOfOperations == 1)
      printf("Updated 1 tuple, %u msec\n", (Uint32) after - before);
    else
      printf("Updated %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);
    if (!oneTrans) myNdb.closeTransaction(myTrans);
  }
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();
  
  ndbout << "Updated "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}

static void deleteIndex(Ndb &myNdb, unsigned int noOfTuples, unsigned int noOfOperations, bool includePrimary, bool oneTrans, bool longKey)
{
  Uint64 tbefore, tafter, before, after;
  NdbConnection  *myTrans;  
  NdbIndexOperation   *myOp;   
  char indexName[] = "INDEX0000";
  
  tbefore = NdbTick_CurrentMillisecond();
  if (oneTrans) myTrans = myNdb.startTransaction();
  for (unsigned int i = 0; i<noOfTuples; i++) {
    for(unsigned int j = 1; 
	((j<=noOfOperations)&&(i<noOfTuples)); 
	(++j<=noOfOperations)?i++:i) { 
      if (!oneTrans) myTrans = myNdb.startTransaction();
      if (myTrans == NULL) 
        error_handler4(__LINE__, myNdb.getNdbError().status, myNdb.getNdbError().classification,
                       myNdb.getNdbError().code, myNdb.getNdbError().message);
      
      myOp = myTrans->getNdbIndexOperation(indexName, "THE_TABLE"); 
      if (myOp == NULL) 
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
      
      myOp->deleteTuple();
      if (includePrimary) {
        if (myOp->equal("X", i) == -1) {
          error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
          myNdb.closeTransaction(myTrans);
          break;
        }         
      } 
      if (myOp->equal("Y", i+1) == -1) {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
      myNdb.closeTransaction(myTrans);
      break;
      }          
    }
    before = NdbTick_CurrentMillisecond();
    if (myTrans->execute( (oneTrans) ? NoCommit : Commit ) == -1)
      {
        error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                       myTrans->getNdbError().code, myTrans->getNdbError().message);
        myNdb.closeTransaction(myTrans);
        break;
      }
    after = NdbTick_CurrentMillisecond();
    if (noOfOperations == 1)
      printf("Deleted 1 tuple, %u msec\n", (Uint32) after - before);
    else
      printf("Deleted %u tuples, %u msec\n", noOfOperations, (Uint32) after - before);
    if (!oneTrans) myNdb.closeTransaction(myTrans);
  }
  if (oneTrans) {
    if (myTrans->execute( Commit ) == -1) {
      error_handler4(__LINE__, myTrans->getNdbError().status, myTrans->getNdbError().classification, 
                     myTrans->getNdbError().code, myTrans->getNdbError().message);
    }
    myNdb.closeTransaction(myTrans);
  }
  tafter = NdbTick_CurrentMillisecond();

  ndbout << "Deleted "<< noOfTuples << " tuples in " << ((oneTrans) ? 1 : noOfTuples) << " transaction(s), " << tafter - tbefore << " msec" << endl;        
}

static void dropIndex(Ndb &myNdb, unsigned int noOfIndexes)
{
  for(unsigned int indexNum = 0; indexNum < noOfIndexes; indexNum++) {
    char indexName[255];
    sprintf(indexName, "INDEX%.4u", indexNum);
    const Uint64 before = NdbTick_CurrentMillisecond();
    const int retVal = myNdb.getDictionary()->dropIndex(indexName,"THE_TABLE");
    const Uint64 after = NdbTick_CurrentMillisecond();
    
    if(retVal == 0){
      ndbout << "Dropped index " << indexName << ", " 
	     << after - before << " msec" << endl;
    } else {
      ndbout << "Failed to drop index " << indexName << endl;
      ndbout << myNdb.getDictionary()->getNdbError() << endl;
    }
  }
}

NDB_COMMAND(indexTest, "indexTest", "indexTest", "indexTest", 65535)
{
  ndb_init();
  bool createTableOp, createIndexOp, dropIndexOp, insertOp, updateOp, deleteOp, readOp, readIndexOp, updateIndexOp, deleteIndexOp, twoKey, longKey;
  unsigned int noOfTuples = 1;
  unsigned int noOfOperations = 1;
  unsigned int noOfIndexes = 1;
  int i = 1;
  Ndb myNdb( "TEST_DB" );
  int check;
  bool storeInACC = false;
  bool includePrimary = false;
  bool oneTransaction = false;

  createTableOp = createIndexOp = dropIndexOp = insertOp = updateOp = deleteOp = readOp = readIndexOp = updateIndexOp = deleteIndexOp = twoKey = longKey = false;
  // Read arguments
  if (argc > 1)
    while (argc > 1)
      {
	if (strcmp(argv[i], "-T") == 0)
	  {
	    createTableOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-c") == 0)
	  {
	    createIndexOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-X") == 0)
	  {
	    dropIndexOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-I") == 0)
	  {
	    insertOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-D") == 0)
	  {
	    deleteOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-U") == 0)
	  {
	    updateOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-R") == 0)
	  {
	    readOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-r") == 0)
	  {
	    readIndexOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-u") == 0)
	  {
	    updateIndexOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-d") == 0)
	  {
	    deleteIndexOp = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-s") == 0)
	  {
	    storeInACC = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-p") == 0)
	  {
	    includePrimary = true;
	    argc -= 1;
	    i++;
	  }
	else if (strcmp(argv[i], "-L") == 0)
	  {
	    longKey = true;
	    argc -= 1;
	    i++;
	  }
 	else if (strcmp(argv[i], "-1") == 0)
	  {
	    oneTransaction = true;
	    argc -= 1;
	    i++;
	  }       
 	else if (strcmp(argv[i], "-2") == 0)
	  {
	    twoKey = true;
	    argc -= 1;
	    i++;
	  }       
        else if (strstr(argv[i], "-n") != 0)
          {
            noOfTuples = atoi(argv[i]+2);
            argc -= 1;
            i++;
          }
        else if (strstr(argv[i], "-o") != 0)
          {
            noOfOperations = MIN(MAX_NO_PARALLEL_OPERATIONS, atoi(argv[i]+2));
            argc -= 1;
            i++;
          }
        else if (strstr(argv[i], "-m") != 0)
          {
            noOfIndexes = atoi(argv[i]+2);
            argc -= 1;
            i++;
          }
	else if (strstr(argv[i], "-h") != 0)
          {
	    printf("Synopsis: \
			index\
			-T create table\
        		-L include a long attribute in key or index\
        		-2 define primary key with two attributes\
        		-c create index\
			-p make index unique (include primary key attribute)\
        		-r read using index\
			-u update using index\
			-d delete using index\
			-n<no operations> do n operations (for -I -r -u -d -R -U -D)\
			-o<no parallel operations> (for -I -r -u -d -R -U -D)\
			-m<no indexes>\n");
            argc -= 1;
            i++;
	  }
	else {
	  char errStr[256];

	  sprintf(errStr, "Illegal argument: %s", argv[i]);
	  error_handler(errStr);
	  exit(-1);
	}
      }
  else
    {
      createTableOp = createIndexOp = dropIndexOp = insertOp = updateOp = deleteOp = true;
    }
  if (longKey) {
    longName = (char *)  malloc(1024);
    for (int i = 0; i < 1023; i++)
      longName[i] = 'x'; 
    longName[1023] = '\0';
  }
  sixtysix = (char *)  malloc(256);
  for (int i = 0; i < 255; i++)
    sixtysix[i] = ' '; 
  sixtysix[255] = '\0';
  strncpy(sixtysix, "sixtysix", strlen("sixtysix"));
  ninetynine = (char *)  malloc(256);
  for (int i = 0; i < 255; i++)
    ninetynine[i] = ' '; 
  ninetynine[255] = '\0';
  strncpy(ninetynine, "ninetynine", strlen("ninetynine"));
  hundred = (char *)  malloc(256);
  for (int i = 0; i < 255; i++)
    hundred[i] = ' '; 
  hundred[255] = '\0';
  strncpy(hundred, "hundred", strlen("hundred"));
  myNdb.init();
  // Wait for Ndb to become ready
  if (myNdb.waitUntilReady(30) == 0)
  {
    if (createTableOp)
      createTable(myNdb, storeInACC, twoKey, longKey);

    if (createIndexOp)
      createIndex(myNdb, includePrimary, noOfIndexes);

    if (insertOp)
      insertTable(myNdb, noOfTuples, noOfOperations, oneTransaction, twoKey, longKey);

    if (updateOp)
      updateTable(myNdb, noOfTuples, noOfOperations, oneTransaction, twoKey, longKey);

    if (deleteOp)
      deleteTable(myNdb, noOfTuples, noOfOperations, oneTransaction, twoKey, longKey);

    if (readOp)
      readTable(myNdb, noOfTuples, noOfOperations, oneTransaction, twoKey, longKey);

    if (readIndexOp)
      readIndex(myNdb, noOfTuples, noOfOperations, includePrimary, oneTransaction, longKey);

    if (updateIndexOp)
      updateIndex(myNdb, noOfTuples, noOfOperations, includePrimary, oneTransaction, longKey);

    if (deleteIndexOp)
      deleteIndex(myNdb, noOfTuples, noOfOperations, includePrimary, oneTransaction, longKey);

    if (dropIndexOp)
      dropIndex(myNdb, noOfIndexes);
  }
  
  if (testPassed)
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


