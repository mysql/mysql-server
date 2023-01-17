/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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

#include <stdlib.h>
#include <string.h>
// Used for cout
#include <stdio.h>
#include <iostream>
#ifdef VM_TRACE
#include <my_dbug.h>
#endif
#ifndef assert
#include <assert.h>
#endif


/**
 * Assume that there is a table which is being updated by 
 * another process (e.g. flexBench -l 0 -stdtables).
 * We want to monitor what happens with column values.
 *
 * Or using the mysql client:
 *
 * shell> mysql -u root
 * mysql> create database ndb_examples;
 * mysql> use ndb_examples;
 * mysql> create table t0
          (c0 int, c1 int, c2 char(4), c3 char(4), c4 text,
          primary key(c0, c2)) engine ndb charset latin1;
 *
 * In another window start ndbapi_event, wait until properly started
 
   insert into t0 values (1, 2, 'a', 'b', null);
   insert into t0 values (3, 4, 'c', 'd', null);
   update t0 set c3 = 'e' where c0 = 1 and c2 = 'a'; -- use pk
   update t0 set c3 = 'f'; -- use scan
   update t0 set c3 = 'F'; -- use scan update to 'same'
   update t0 set c2 = 'g' where c0 = 1; -- update pk part
   update t0 set c2 = 'G' where c0 = 1; -- update pk part to 'same'
   update t0 set c0 = 5, c2 = 'H' where c0 = 3; -- update full PK
   delete from t0;

   insert ...; update ...; -- see events w/ same pk merged (if -m option)
   delete ...; insert ...; -- there are 5 combinations ID IU DI UD UU
   update ...; update ...;

   -- text requires -m flag
   set @a = repeat('a',256); -- inline size
   set @b = repeat('b',2000); -- part size
   set @c = repeat('c',2000*30); -- 30 parts

   -- update the text field using combinations of @a, @b, @c ...
 
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
		  const int noEventColumnName,
                  bool merge_events);

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cout << "Arguments are <connect_string cluster> <timeout> [m(merge events)|d(debug)].\n";
    exit(-1);
  }
  const char *connectstring = argv[1];
  int timeout = atoi(argv[2]);
  ndb_init();
  bool merge_events = argc > 3 && strchr(argv[3], 'm') != 0;
#ifdef VM_TRACE
  bool dbug = argc > 3 && strchr(argv[3], 'd') != 0;
  if (dbug)
  {
    // Turn on dbug tracing
    DBUG_PUSH("d:t:");

    // Print signals to stdout
    static char env[100];
    strcpy(env, "API_SIGNAL_LOG=-");
    putenv(env);
  }
#endif

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(connectstring); // Object representing the cluster

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
		      "ndb_examples");  // Object representing the database

  if (myNdb->init() == -1) APIERROR(myNdb->getNdbError());

  const char *eventName= "CHNG_IN_t0";
  const char *eventTableName= "t0";
  const int noEventColumnName= 5;
  const char *eventColumnName[noEventColumnName]=
    {"c0",
     "c1",
     "c2",
     "c3",
     "c4"
    };
  
  // Create events
  myCreateEvent(myNdb,
		eventName,
		eventTableName,
		eventColumnName,
		noEventColumnName,
                merge_events);

  // Normal values and blobs are unfortunately handled differently..
  typedef union { NdbRecAttr* ra; NdbBlob* bh; } RA_BH;

  int i, j, k, l;
  j = 0;
  while (j < timeout) {

    // Start "transaction" for handling events
    NdbEventOperation* op;
    printf("create EventOperation\n");
    if ((op = myNdb->createEventOperation(eventName)) == NULL)
      APIERROR(myNdb->getNdbError());
    op->mergeEvents(merge_events);

    printf("get values\n");
    RA_BH recAttr[noEventColumnName];
    RA_BH recAttrPre[noEventColumnName];
    // primary keys should always be a part of the result
    for (i = 0; i < noEventColumnName; i++) {
      if (i < 4) {
        recAttr[i].ra    = op->getValue(eventColumnName[i]);
        recAttrPre[i].ra = op->getPreValue(eventColumnName[i]);
      } else if (merge_events) {
        recAttr[i].bh    = op->getBlobHandle(eventColumnName[i]);
        recAttrPre[i].bh = op->getPreBlobHandle(eventColumnName[i]);
      }
    }

    // set up the callbacks
    printf("execute\n");
    // This starts changes to "start flowing"
    if (op->execute())
      APIERROR(op->getNdbError());

    NdbEventOperation* the_op = op;

    i= 0;
    while (i < timeout) {
      // printf("now waiting for event...\n");
      int r = myNdb->pollEvents(1000); // wait for event or 1000 ms
      if (r > 0) {
	// printf("got data! %d\n", r);
	while ((op= myNdb->nextEvent())) {
          assert(the_op == op);
	  i++;
	  switch (op->getEventType()) {
	  case NdbDictionary::Event::TE_INSERT:
	    printf("%u INSERT", i);
	    break;
	  case NdbDictionary::Event::TE_DELETE:
	    printf("%u DELETE", i);
	    break;
	  case NdbDictionary::Event::TE_UPDATE:
	    printf("%u UPDATE", i);
	    break;
	  default:
	    abort(); // should not happen
	  }
          printf(" gci=%d\n", (int)op->getGCI());
          for (k = 0; k <= 1; k++) {
            printf(k == 0 ? "post: " : "pre : ");
            for (l = 0; l < noEventColumnName; l++) {
              if (l < 4) {
                NdbRecAttr* ra = k == 0 ? recAttr[l].ra : recAttrPre[l].ra;
                if (ra->isNULL() >= 0) { // we have a value
                  if (ra->isNULL() == 0) { // we have a non-null value
                    if (l < 2)
                      printf("%-5u", ra->u_32_value());
                    else
                      printf("%-5.4s", ra->aRef());
                  } else
                    printf("%-5s", "NULL");
                } else
                  printf("%-5s", "-"); // no value
              } else if (merge_events) {
                int isNull;
                NdbBlob* bh = k == 0 ? recAttr[l].bh : recAttrPre[l].bh;
                bh->getDefined(isNull);
                if (isNull >= 0) { // we have a value
                  if (! isNull) { // we have a non-null value
                    Uint64 length = 0;
                    bh->getLength(length);
                    // read into buffer
                    unsigned char* buf = new unsigned char [length];
                    memset(buf, 'X', length);
                    Uint32 n = length;
                    bh->readData(buf, n); // n is in/out
                    assert(n == length);
                    // pretty-print
                    bool first = true;
                    Uint32 i = 0;
                    while (i < n) {
                      unsigned char c = buf[i++];
                      Uint32 m = 1;
                      while (i < n && buf[i] == c)
                        i++, m++;
                      if (! first)
                        printf("+");
                      printf("%u%c", m, c);
                      first = false;
                    }
                    printf("[%u]", n);
                    delete [] buf;
                  } else
                    printf("%-5s", "NULL");
                } else
                  printf("%-5s", "-"); // no value
              }
            }
            printf("\n");
          }
	}
      } // else printf("timed out (%i)\n", timeout);
    }
    // don't want to listen to events anymore
    if (myNdb->dropEventOperation(the_op)) APIERROR(myNdb->getNdbError());
    the_op = 0;

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
		  const int noEventColumnNames,
                  bool merge_events)
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
  myEvent.mergeEvents(merge_events);

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
