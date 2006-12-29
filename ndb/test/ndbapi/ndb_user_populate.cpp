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


extern "C" {
#include "user_populate.h"
}

#include <ndb_global.h>
#include <NdbApi.hpp>

#include "ndb_schema.hpp"
#include "ndb_error.hpp"

int
insert_subscriber(void * obj,
		  SubscriberNumber number, 
		  SubscriberName name,
		  GroupId groupId,
		  Location l,
		  ActiveSessions activeSessions,
		  ChangedBy changedBy,
		  ChangedTime changedTime){
  Ndb * pNDB = (Ndb *)obj;
  int check;
  
  NdbConnection * MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)	  
    error_handler("startTranscation", pNDB->getNdbErrorString(), 0);
  
  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "getNdbOperation", MyTransaction);
  
  check = MyOperation->insertTuple();
  CHECK_MINUS_ONE(check, "insertTuple", MyTransaction);
  
  check = MyOperation->equal(SUBSCRIBER_NUMBER, number);
  CHECK_MINUS_ONE(check, "equal", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_NAME, name);
  CHECK_MINUS_ONE(check, "setValue name", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_GROUP, (char*)&groupId);
  CHECK_MINUS_ONE(check, "setValue group", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_LOCATION, (char*)&l);
  CHECK_MINUS_ONE(check, "setValue location", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_SESSIONS, (char*)&activeSessions);
  CHECK_MINUS_ONE(check, "setValue sessions", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_CHANGED_BY, changedBy);
  CHECK_MINUS_ONE(check, "setValue changedBy", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_CHANGED_TIME, changedTime);
  CHECK_MINUS_ONE(check, "setValue changedTime", MyTransaction);

  check = MyTransaction->execute( Commit ); 
  CHECK_MINUS_ONE(check, "commit", MyTransaction);  

  pNDB->closeTransaction(MyTransaction);
  return 0;
}

int
insert_server(void * obj,
	      ServerId serverId,
	      SubscriberSuffix suffix,
	      ServerName name,
	      Counter noOfRead,
	      Counter noOfInsert,
	      Counter noOfDelete){
  Ndb * pNDB = (Ndb *)obj;
  int check;

  NdbConnection * MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)	  
    error_handler("startTranscation", pNDB->getNdbErrorString(), 0);
  
  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
  CHECK_NULL(MyOperation, "getNdbOperation", MyTransaction);
  
  check = MyOperation->insertTuple();
  CHECK_MINUS_ONE(check, "insert tuple", MyTransaction);  
  
  check = MyOperation->equal(SERVER_ID, (char*)&serverId);
  CHECK_MINUS_ONE(check, "setValue id", MyTransaction);  
  
  check = MyOperation->setValue(SERVER_SUBSCRIBER_SUFFIX, suffix);
  CHECK_MINUS_ONE(check, "setValue suffix", MyTransaction);  

  check = MyOperation->setValue(SERVER_NAME, name);
  CHECK_MINUS_ONE(check, "setValue name", MyTransaction);  

  check = MyOperation->setValue(SERVER_READS, (char*)&noOfRead);
  CHECK_MINUS_ONE(check, "setValue reads", MyTransaction);  

  check = MyOperation->setValue(SERVER_INSERTS, (char*)&noOfInsert);
  CHECK_MINUS_ONE(check, "setValue inserts", MyTransaction);  

  check = MyOperation->setValue(SERVER_DELETES, (char*)&noOfDelete);
  CHECK_MINUS_ONE(check, "setValue deletes", MyTransaction);  

  check = MyTransaction->execute( Commit ); 
  CHECK_MINUS_ONE(check, "commit", MyTransaction);  
  
  pNDB->closeTransaction(MyTransaction);
  return 0;
}

int
insert_group(void * obj,
	     GroupId groupId, 
	     GroupName name,
	     Permission allowRead,
	     Permission allowInsert,
	     Permission allowDelete){
  Ndb * pNDB = (Ndb *)obj;
  int check;
  
  NdbConnection * MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)	  
    error_handler("startTranscation", pNDB->getNdbErrorString(), 0);
  
  NdbOperation *MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "getNdbOperation", MyTransaction);  
  
  check = MyOperation->insertTuple();
  CHECK_MINUS_ONE(check, "insertTuple", MyTransaction);  
  
  check = MyOperation->equal(GROUP_ID, (char*)&groupId);
  CHECK_MINUS_ONE(check, "equal", MyTransaction);  
  
  check = MyOperation->setValue(GROUP_NAME, name);
  CHECK_MINUS_ONE(check, "setValue name", MyTransaction);  

  check = MyOperation->setValue(GROUP_ALLOW_READ, (char*)&allowRead);
  CHECK_MINUS_ONE(check, "setValue allowRead", MyTransaction);  

  check = MyOperation->setValue(GROUP_ALLOW_INSERT, (char*)&allowInsert);
  CHECK_MINUS_ONE(check, "setValue allowInsert", MyTransaction);  

  check = MyOperation->setValue(GROUP_ALLOW_DELETE, (char*)&allowDelete);
  CHECK_MINUS_ONE(check, "setValue allowDelete", MyTransaction);  
  
  check = MyTransaction->execute( Commit ); 
  CHECK_MINUS_ONE(check, "commit", MyTransaction);  
  
  pNDB->closeTransaction(MyTransaction);
  return 0;
}

