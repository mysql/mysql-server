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

/**
 *  ndbapi_event.cpp: Using API level events in NDB API
 *
 *  Classes and methods used in this example:
 *
 *  Ndb_cluster_connection
 *       connect()
 *       wait_until_ready()
 *
 *  Ndb
 *       init()
 *       getDictionary()
 *       createEventOperation()
 *       dropEventOperation()
 *       pollEvents()
 *       nextEvent()
 *
 *  NdbDictionary
 *       createEvent()
 *       dropEvent()
 *
 *  NdbDictionary::Event
 *       setTable()
 *       addTableEvent()
 *       addEventColumn()
 *
 *  NdbEventOperation
 *       getValue()
 *       getPreValue()
 *       execute()
 *       getEventType()
 *
 */

#include <NdbApi.hpp>

// Used for cout
#include <stdio.h>
#include <iostream>
#include <unistd.h>


/**
 *
 * Assume that there is a table TAB0 which is being updated by 
 * another process (e.g. flexBench -l 0 -stdtables).
 * We want to monitor what happens with columns COL0, COL2, COL11
 *
 * or together with the mysql client;
 *
 * shell> mysql -u root
 * mysql> create database TEST_DB;
 * mysql> use TEST_DB;
 * mysql> create table TAB0 (COL0 int primary key, COL1 int, COL11 int) engine=ndb;
 *
 * In another window start ndbapi_event, wait until properly started
 *
   insert into TAB0 values (1,2,3);
   insert into TAB0 values (2,2,3);
   insert into TAB0 values (3,2,9);
   update TAB0 set COL1=10 where COL0=1;
   delete from TAB0 where COL0=1;
 *
 * you should see the data popping up in the example window
 *
 */

#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }

int myCreateEvent(Ndb* myNdb,
		  const char *eventName,
		  const char *eventTableName,
		  const char **eventColumnName,
		  const int noEventColumnName);

int main()
{
  ndb_init();

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(); // Object representing the cluster

  int r= cluster_connection->connect(5 /* retries               */,
				     3 /* delay between retries */,
				     1 /* verbose               */);
  if (r > 0)
  {
    std::cout
      << "Cluster connect failed, possibly resolved with more retries.\n";
    exit(-1);
  }
  else if (r < 0)
  {
    std::cout
      << "Cluster connect failed.\n";
    exit(-1);
  }
					   
  if (cluster_connection->wait_until_ready(30,30))
  {
    std::cout << "Cluster was not ready within 30 secs." << std::endl;
    exit(-1);
  }

  Ndb* myNdb= new Ndb(cluster_connection,
		      "TEST_DB");  // Object representing the database

  if (myNdb->init() == -1) APIERROR(myNdb->getNdbError());

  const char *eventName= "CHNG_IN_TAB0";
  const char *eventTableName= "TAB0";
  const int noEventColumnName= 3;
  const char *eventColumnName[noEventColumnName]=
    {"COL0",
     "COL1",
     "COL11"};
  
  // Create events
  myCreateEvent(myNdb,
		eventName,
		eventTableName,
		eventColumnName,
		noEventColumnName);

  int j= 0;
  while (j < 5) {

    // Start "transaction" for handling events
    NdbEventOperation* op;
    printf("create EventOperation\n");
    if ((op = myNdb->createEventOperation(eventName)) == NULL)
      APIERROR(myNdb->getNdbError());

    printf("get values\n");
    NdbRecAttr* recAttr[noEventColumnName];
    NdbRecAttr* recAttrPre[noEventColumnName];
    // primary keys should always be a part of the result
    for (int i = 0; i < noEventColumnName; i++) {
      recAttr[i]    = op->getValue(eventColumnName[i]);
      recAttrPre[i] = op->getPreValue(eventColumnName[i]);
    }

    // set up the callbacks
    printf("execute\n");
    // This starts changes to "start flowing"
    if (op->execute())
      APIERROR(op->getNdbError());

    int i= 0;
    while(i < 40) {
      // printf("now waiting for event...\n");
      int r= myNdb->pollEvents(1000); // wait for event or 1000 ms
      if (r > 0) {
	// printf("got data! %d\n", r);
	while ((op= myNdb->nextEvent())) {
	  i++;
	  switch (op->getEventType()) {
	  case NdbDictionary::Event::TE_INSERT:
	    printf("%u INSERT: ", i);
	    break;
	  case NdbDictionary::Event::TE_DELETE:
	    printf("%u DELETE: ", i);
	    break;
	  case NdbDictionary::Event::TE_UPDATE:
	    printf("%u UPDATE: ", i);
	    break;
	  default:
	    abort(); // should not happen
	  }
	  for (int i = 1; i < noEventColumnName; i++) {
	    if (recAttr[i]->isNULL() >= 0) { // we have a value
	      printf(" post[%u]=", i);
	      if (recAttr[i]->isNULL() == 0) // we have a non-null value
		printf("%u", recAttr[i]->u_32_value());
	      else                           // we have a null value
		printf("NULL");
	    }
	    if (recAttrPre[i]->isNULL() >= 0) { // we have a value
	      printf(" pre[%u]=", i);
	      if (recAttrPre[i]->isNULL() == 0) // we have a non-null value
		printf("%u", recAttrPre[i]->u_32_value());
	      else                              // we have a null value
		printf("NULL");
	    }
	  }
	  printf("\n");
	}
      } else
	;//printf("timed out\n");
    }
    // don't want to listen to events anymore
    if (myNdb->dropEventOperation(op)) APIERROR(myNdb->getNdbError());

    j++;
  }

  {
    NdbDictionary::Dictionary *myDict = myNdb->getDictionary();
    if (!myDict) APIERROR(myNdb->getNdbError());
    // remove event from database
    if (myDict->dropEvent(eventName)) APIERROR(myDict->getNdbError());
  }

  delete myNdb;
  delete cluster_connection;
  ndb_end(0);
  return 0;
}

int myCreateEvent(Ndb* myNdb,
		  const char *eventName,
		  const char *eventTableName,
		  const char **eventColumnNames,
		  const int noEventColumnNames)
{
  NdbDictionary::Dictionary *myDict= myNdb->getDictionary();
  if (!myDict) APIERROR(myNdb->getNdbError());

  const NdbDictionary::Table *table= myDict->getTable(eventTableName);
  if (!table) APIERROR(myDict->getNdbError());

  NdbDictionary::Event myEvent(eventName, *table);
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL); 
  //  myEvent.addTableEvent(NdbDictionary::Event::TE_INSERT); 
  //  myEvent.addTableEvent(NdbDictionary::Event::TE_UPDATE); 
  //  myEvent.addTableEvent(NdbDictionary::Event::TE_DELETE);

  myEvent.addEventColumns(noEventColumnNames, eventColumnNames);

  // Add event to database
  if (myDict->createEvent(myEvent) == 0)
    myEvent.print();
  else if (myDict->getNdbError().classification ==
	   NdbError::SchemaObjectExists) {
    printf("Event creation failed, event exists\n");
    printf("dropping Event...\n");
    if (myDict->dropEvent(eventName)) APIERROR(myDict->getNdbError());
    // try again
    // Add event to database
    if ( myDict->createEvent(myEvent)) APIERROR(myDict->getNdbError());
  } else
    APIERROR(myDict->getNdbError());

  return 0;
}
