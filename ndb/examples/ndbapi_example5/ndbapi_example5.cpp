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
 * ndbapi_example5.cpp: Using API level events in NDB API
 */

#include <NdbApi.hpp>
#include <NdbEventOperation.hpp>

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
 * or together with the mysqlcluster client;
 *
 * shell> mysqlcluster -u root
 * mysql> create database TEST_DB;
 * mysql> use TEST_DB;
 * mysql> create table TAB0 (COL0 int primary key, COL1 int, COL11 int);
 *
 * In another window start ndbapi_example5, wait until properly started
 *
 * mysql> insert into TAB0 values (1,2,3);
 * mysql> insert into TAB0 values (2,2,3);
 * mysql> insert into TAB0 values (3,2,9);
 * mysql> 
 *
 * you should see the data popping up in the example window
 *
 */

#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }

Ndb* myCreateNdb();
int myCreateEvent(Ndb* myNdb,
		  const char *eventName,
		  const char *eventTableName,
		  const char **eventComlumnName,
		  const int noEventComlumnName);

int main()
{
  ndb_init();
  Ndb* myNdb = myCreateNdb();
  NdbDictionary::Dictionary *myDict;

  const char *eventName = "CHNG_IN_TAB0";
  const char *eventTableName = "TAB0";
  const int noEventColumnName = 3;
  const char *eventColumnName[noEventColumnName] =
    {"COL0",
     "COL1",
     "COL11"};
  
  myDict = myNdb->getDictionary();

  // Create events
  myCreateEvent(myNdb,
		eventName,
		eventTableName,
		eventColumnName,
		noEventColumnName);
  int j = 0;
  while (j < 5) {

    // Start "transaction" for handling events
    NdbEventOperation* op;
    printf("create EventOperation\n");
    if ((op = myNdb->createEventOperation(eventName,100)) == NULL) {
      printf("Event operation creation failed\n");
      exit(-1);
    }

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
    if (op->execute()) { // This starts changes to "start flowing"
      printf("operationd execution failed\n");
      exit(-1);
    }

    int i = 0;

    while(i < 40) {
      //printf("now waiting for event...\n");
      int r = myNdb->pollEvents(1000); // wait for event or 1000 ms
      if (r>0) {
	//printf("got data! %d\n", r);
	int overrun;
	while (op->next(&overrun) > 0) {
	  i++;
	  if (!op->isConsistent())
	    printf("A node failiure has occured and events might be missing\n");
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
	  }
	  printf("overrun %u pk %u: ", overrun, recAttr[0]->u_32_value());
	  for (int i = 1; i < noEventColumnName; i++) {
	    if (recAttr[i]->isNULL() >= 0) { // we have a value
	      printf(" post[%u]=", i);
	      if (recAttr[i]->isNULL() == 0) // we have a non-null value
		printf("%u", recAttr[i]->u_32_value());
	      else                           // we have a null value
		printf("NULL");
	    }
	    if (recAttrPre[i]->isNULL() >= 0) { // we have a value
	      printf(" post[%u]=", i);
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
    // don't want to listen to eventsanymore
    myNdb->dropEventOperation(op);

    j++;
  }

  myDict->dropEvent(eventName); // remove event from database

  delete myNdb;
}

Ndb* myCreateNdb()
{
  Ndb* myNdb = new Ndb("TEST_DB");

  /********************************************
   * Initialize NDB and wait until it's ready *
   ********************************************/
  if (myNdb->init() == -1) { 
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  if (myNdb->waitUntilReady(30) != 0) {
    std::cout << "NDB was not ready within 30 secs." << std::endl;
    exit(-1);
  }

  return myNdb;
}

int myCreateEvent(Ndb* myNdb,
		  const char *eventName,
		  const char *eventTableName,
		  const char **eventColumnName,
		  const int noEventColumnName)
{
  NdbDictionary::Dictionary *myDict = myNdb->getDictionary();

  if (!myDict) {
    printf("Event Creation failedDictionary not found");
    exit(-1);
  }

  NdbDictionary::Event myEvent(eventName);
  myEvent.setTable(eventTableName);
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL); 
  //  myEvent.addTableEvent(NdbDictionary::Event::TE_INSERT); 
  //  myEvent.addTableEvent(NdbDictionary::Event::TE_UPDATE); 
  //  myEvent.addTableEvent(NdbDictionary::Event::TE_DELETE);

  for (int i = 0; i < noEventColumnName; i++)
    myEvent.addEventColumn(eventColumnName[i]);

  int res = myDict->createEvent(myEvent); // Add event to database

  if (res == 0)
    myEvent.print();
  else {
    printf("Event creation failed\n");
    printf("trying drop Event, maybe event exists\n");
    res = myDict->dropEvent(eventName);
    if (res)
      exit(-1);
    // try again
    res = myDict->createEvent(myEvent); // Add event to database
    if (res)
      exit(-1);
  }

  return res;
}
