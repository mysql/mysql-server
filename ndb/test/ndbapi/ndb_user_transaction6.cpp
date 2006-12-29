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

//#define DEBUG_ON

#include <string.h>
#include "userHandle.h"
#include "userInterface.h"

#include "macros.h"
#include "ndb_schema.hpp"
#include "ndb_error.hpp"

#include <NdbApi.hpp>


void
userCheckpoint(UserHandle *uh){
}

inline
NdbConnection *
startTransaction(Ndb * pNDB, ServerId inServerId, const SubscriberNumber inNumber){
  
  const int keyDataLenBytes    = sizeof(ServerId)+SUBSCRIBER_NUMBER_LENGTH;
  const int keyDataLen_64Words = keyDataLenBytes >> 3;

  Uint64 keyDataBuf[keyDataLen_64Words+1]; // The "+1" is for rounding...
  
  char     * keyDataBuf_charP = (char *)&keyDataBuf[0];
  Uint32  * keyDataBuf_wo32P = (Uint32 *)&keyDataBuf[0];
  
  // Server Id comes first
  keyDataBuf_wo32P[0] = inServerId;
  // Then subscriber number
  memcpy(&keyDataBuf_charP[sizeof(ServerId)], inNumber, SUBSCRIBER_NUMBER_LENGTH);

  return pNDB->startTransaction(0, keyDataBuf_charP, keyDataLenBytes);
}

/**
 * Transaction 1 - T1 
 *
 * Update location and changed by/time on a subscriber
 *
 * Input: 
 *   SubscriberNumber,
 *   Location,
 *   ChangedBy,
 *   ChangedTime
 *
 * Output:
 */
void
userTransaction_T1(UserHandle * uh,
		   SubscriberNumber number, 
		   Location new_location, 
		   ChangedBy changed_by, 
		   ChangedTime changed_time){
  Ndb * pNDB = uh->pNDB;

  DEBUG2("T1(%.*s):\n", SUBSCRIBER_NUMBER_LENGTH, number);

  int check;
  NdbRecAttr * check2;

  NdbConnection * MyTransaction = pNDB->startTransaction();
  if (MyTransaction != NULL) {
    NdbOperation *MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
    if (MyOperation != NULL) {  
      MyOperation->updateTuple();  
      MyOperation->equal(IND_SUBSCRIBER_NUMBER,
                         number);
      MyOperation->setValue(IND_SUBSCRIBER_LOCATION, 
		            (char *)&new_location);
      MyOperation->setValue(IND_SUBSCRIBER_CHANGED_BY, 
			    changed_by);
      MyOperation->setValue(IND_SUBSCRIBER_CHANGED_TIME, 
			    changed_time);
      check = MyTransaction->execute( Commit );
      if (check != -1) {
        pNDB->closeTransaction(MyTransaction);
        return;
      } else {
        CHECK_MINUS_ONE(check, "T1: Commit", 
		        MyTransaction);
      }//if
    } else {
      CHECK_NULL(MyOperation, "T1: getNdbOperation", MyTransaction);
    }//if
  } else {
    error_handler("T1-1: startTranscation", pNDB->getNdbErrorString(), pNDB->getNdbError());
  }//if
}

/**
 * Transaction 2 - T2
 *
 * Read from Subscriber:
 *
 * Input: 
 *   SubscriberNumber
 *
 * Output:
 *   Location
 *   Changed by
 *   Changed Timestamp
 *   Name
 */
void
userTransaction_T2(UserHandle * uh,
		   SubscriberNumber number, 
		   Location * readLocation, 
		   ChangedBy changed_by, 
		   ChangedTime changed_time,
		   SubscriberName subscriberName){
  Ndb * pNDB = uh->pNDB;

  DEBUG2("T2(%.*s):\n", SUBSCRIBER_NUMBER_LENGTH, number);

  int check;
  NdbRecAttr * check2;

  NdbConnection * MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)	  
    error_handler("T2-1: startTransaction", pNDB->getNdbErrorString(), pNDB->getNdbError());

  NdbOperation *MyOperation= MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T2: getNdbOperation", 
	     MyTransaction);
  
  MyOperation->readTuple();
  MyOperation->equal(IND_SUBSCRIBER_NUMBER, 
		     number);
  MyOperation->getValue(IND_SUBSCRIBER_LOCATION, 
			(char *)readLocation);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, 
			changed_by);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
                        changed_time);
  MyOperation->getValue(IND_SUBSCRIBER_NAME, 
			subscriberName);
  check = MyTransaction->execute( Commit ); 
  CHECK_MINUS_ONE(check, "T2: Commit", 
		  MyTransaction);  
  pNDB->closeTransaction(MyTransaction);
}

/**
 * Transaction 3 - T3
 *
 * Read session details
 *
 * Input:
 *   SubscriberNumber
 *   ServerId
 *   ServerBit
 *
 * Output:
 *   BranchExecuted
 *   SessionDetails
 *   ChangedBy
 *   ChangedTime
 *   Location
 */
void
userTransaction_T3(UserHandle * uh,
		   SubscriberNumber   inNumber,
		   ServerId           inServerId,
		   ServerBit          inServerBit,
		   SessionDetails     outSessionDetails,
		   BranchExecuted   * outBranchExecuted){
  Ndb * pNDB = uh->pNDB;

  char               outChangedBy   [sizeof(ChangedBy)  +(4-(sizeof(ChangedBy)   & 3))];
  char               outChangedTime [sizeof(ChangedTime)+(4-(sizeof(ChangedTime) & 3))];
  Location           outLocation;
  GroupId            groupId;
  ActiveSessions     sessions;
  Permission         permission;
  SubscriberSuffix   inSuffix;

  DEBUG3("T3(%.*s, %.2d): ", SUBSCRIBER_NUMBER_LENGTH, inNumber, inServerId);

  int check;
  NdbRecAttr * check2;

  NdbConnection * MyTransaction = startTransaction(pNDB, inServerId, inNumber);
  if (MyTransaction == NULL)	  
    error_handler("T3-1: startTranscation", pNDB->getNdbErrorString(), pNDB->getNdbError());

  NdbOperation *MyOperation= MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T3-1: getNdbOperation", 
	     MyTransaction);
    
  MyOperation->readTuple();
  MyOperation->equal(IND_SUBSCRIBER_NUMBER, 
			     inNumber);
  MyOperation->getValue(IND_SUBSCRIBER_LOCATION, 
			(char *)&outLocation);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, 
			outChangedBy);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
                        outChangedTime);
  MyOperation->getValue(IND_SUBSCRIBER_GROUP,
			(char *)&groupId);
  MyOperation->getValue(IND_SUBSCRIBER_SESSIONS,
			(char *)&sessions);
  check = MyTransaction->execute( NoCommit ); 
  CHECK_MINUS_ONE(check, "T3-1: NoCommit", 
		  MyTransaction);
  
    /* Operation 2 */

  MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "T3-2: getNdbOperation", 
	     MyTransaction);
  
  
  MyOperation->readTuple();
  MyOperation->equal(IND_GROUP_ID,
		     (char*)&groupId);
  MyOperation->getValue(IND_GROUP_ALLOW_READ, 
			(char *)&permission);
  check = MyTransaction->execute( NoCommit ); 
  CHECK_MINUS_ONE(check, "T3-2: NoCommit", 
		  MyTransaction);
  
  if(((permission & inServerBit) == inServerBit) &&
     ((sessions   & inServerBit) == inServerBit)){

    memcpy(inSuffix,
	   &inNumber[SUBSCRIBER_NUMBER_LENGTH-SUBSCRIBER_NUMBER_SUFFIX_LENGTH], SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    DEBUG2("reading(%.*s) - ", SUBSCRIBER_NUMBER_SUFFIX_LENGTH, inSuffix);
    
    /* Operation 3 */
    MyOperation = MyTransaction->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOperation, "T3-3: getNdbOperation", 
	       MyTransaction);
    
    MyOperation->simpleRead();
  
    MyOperation->equal(IND_SESSION_SUBSCRIBER,
		       (char*)inNumber);
    MyOperation->equal(IND_SESSION_SERVER,
		       (char*)&inServerId);
    MyOperation->getValue(IND_SESSION_DATA, 
			  (char *)outSessionDetails);
    /* Operation 4 */
    MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOperation, "T3-4: getNdbOperation", 
	       MyTransaction);
    
    MyOperation->interpretedUpdateTuple();
    MyOperation->equal(IND_SERVER_ID,
		       (char*)&inServerId);
    MyOperation->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
		        (char*)inSuffix);
    MyOperation->incValue(IND_SERVER_READS, (uint32)1);
    (* outBranchExecuted) = 1;
  } else {
    (* outBranchExecuted) = 0;
  }
  DEBUG("commit...");
  check = MyTransaction->execute( Commit ); 
  CHECK_MINUS_ONE(check, "T3: Commit", 
		  MyTransaction);
  
  pNDB->closeTransaction(MyTransaction);
  
  DEBUG("done\n");
}


/**
 * Transaction 4 - T4
 * 
 * Create session
 *
 * Input:
 *   SubscriberNumber
 *   ServerId
 *   ServerBit
 *   SessionDetails,
 *   DoRollback
 * Output:
 *   ChangedBy
 *   ChangedTime
 *   Location
 *   BranchExecuted
 */
void
userTransaction_T4(UserHandle * uh,
		   SubscriberNumber   inNumber,
		   ServerId           inServerId,
		   ServerBit          inServerBit,
		   SessionDetails     inSessionDetails,
		   DoRollback         inDoRollback,
		   BranchExecuted   * outBranchExecuted){
  
  Ndb * pNDB = uh->pNDB;
  
  char               outChangedBy   [sizeof(ChangedBy)  +(4-(sizeof(ChangedBy)   & 3))];
  char               outChangedTime [sizeof(ChangedTime)+(4-(sizeof(ChangedTime) & 3))];
  Location         outLocation;
  GroupId          groupId;
  ActiveSessions   sessions;
  Permission       permission;
  SubscriberSuffix inSuffix;

  DEBUG3("T4(%.*s, %.2d): ", SUBSCRIBER_NUMBER_LENGTH, inNumber, inServerId);

  int check;
  NdbRecAttr * check2;

  NdbConnection * MyTransaction = startTransaction(pNDB, inServerId, inNumber);
  if (MyTransaction == NULL)	  
    error_handler("T4-1: startTranscation", pNDB->getNdbErrorString(), pNDB->getNdbError());

  NdbOperation *MyOperation= MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T4-1: getNdbOperation", 
	     MyTransaction);
  
  MyOperation->interpretedUpdateTuple();
  MyOperation->equal(IND_SUBSCRIBER_NUMBER, 
		     inNumber);
  MyOperation->getValue(IND_SUBSCRIBER_LOCATION, 
			(char *)&outLocation);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, 
		        outChangedBy);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
                        outChangedTime);
  MyOperation->getValue(IND_SUBSCRIBER_GROUP,
			(char *)&groupId);
  MyOperation->getValue(IND_SUBSCRIBER_SESSIONS,
			(char *)&sessions); 
  MyOperation->incValue(IND_SUBSCRIBER_SESSIONS, 
			(uint32)inServerBit);
  check = MyTransaction->execute( NoCommit ); 

    /* Operation 2 */

  MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "T4-2: getNdbOperation", 
	     MyTransaction);
  
  MyOperation->readTuple();
  MyOperation->equal(IND_GROUP_ID,
		     (char*)&groupId);
  MyOperation->getValue(IND_GROUP_ALLOW_INSERT, 
			(char *)&permission);
  check = MyTransaction->execute( NoCommit ); 
  CHECK_MINUS_ONE(check, "T4-2: NoCommit", 
		  MyTransaction);
  
  if(((permission & inServerBit) == inServerBit) &&
     ((sessions   & inServerBit) == 0)){
  
    memcpy(inSuffix,
	   &inNumber[SUBSCRIBER_NUMBER_LENGTH-SUBSCRIBER_NUMBER_SUFFIX_LENGTH], SUBSCRIBER_NUMBER_SUFFIX_LENGTH);

    DEBUG2("inserting(%.*s) - ", SUBSCRIBER_NUMBER_SUFFIX_LENGTH, inSuffix);
  
    /* Operation 3 */
    
    MyOperation = MyTransaction->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOperation, "T4-3: getNdbOperation", 
	       MyTransaction);
    
    MyOperation->insertTuple();
    MyOperation->equal(IND_SESSION_SUBSCRIBER,
		      (char*)inNumber);
    MyOperation->equal(IND_SESSION_SERVER,
		       (char*)&inServerId);
    MyOperation->setValue(SESSION_DATA, 
			  (char *)inSessionDetails);
    /* Operation 4 */

    /* Operation 5 */
    MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOperation, "T4-5: getNdbOperation", 
	       MyTransaction);
    
    MyOperation->interpretedUpdateTuple();
    MyOperation->equal(IND_SERVER_ID,
		       (char*)&inServerId);
    MyOperation->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
		       (char*)inSuffix);
    MyOperation->incValue(IND_SERVER_INSERTS, (uint32)1);
    (* outBranchExecuted) = 1;
  } else {
    (* outBranchExecuted) = 0;
    DEBUG1("%s", ((permission & inServerBit) ? "permission - " : "no permission - "));
    DEBUG1("%s", ((sessions   & inServerBit) ? "in session - " : "no in session - "));
  }

  if(!inDoRollback && (* outBranchExecuted)){
    DEBUG("commit\n");
    check = MyTransaction->execute( Commit ); 
    CHECK_MINUS_ONE(check, "T4: Commit", 
		    MyTransaction);
  } else {
    DEBUG("rollback\n");
    check = MyTransaction->execute(Rollback);
    CHECK_MINUS_ONE(check, "T4:Rollback", 
		    MyTransaction);
    
  }
  
  pNDB->closeTransaction(MyTransaction);
}


/**
 * Transaction 5 - T5
 * 
 * Delete session
 *
 * Input:
 *   SubscriberNumber
 *   ServerId
 *   ServerBit
 *   DoRollback
 * Output:
 *   ChangedBy
 *   ChangedTime
 *   Location
 *   BranchExecuted
 */
void
userTransaction_T5(UserHandle * uh,
		   SubscriberNumber   inNumber,
		   ServerId           inServerId,
		   ServerBit          inServerBit,
		   DoRollback         inDoRollback,
		   BranchExecuted   * outBranchExecuted){
  Ndb * pNDB = uh->pNDB;

  DEBUG3("T5(%.*s, %.2d): ", SUBSCRIBER_NUMBER_LENGTH, inNumber, inServerId);

  NdbConnection * MyTransaction = 0;
  NdbOperation  * MyOperation = 0;

  char             outChangedBy   [sizeof(ChangedBy)  +(4-(sizeof(ChangedBy)   & 3))];
  char             outChangedTime [sizeof(ChangedTime)+(4-(sizeof(ChangedTime) & 3))];
  Location         outLocation;
  GroupId          groupId;
  ActiveSessions   sessions;
  Permission       permission;
  SubscriberSuffix inSuffix;

  int check;
  NdbRecAttr * check2;

  MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)	  
    error_handler("T5-1: startTranscation", pNDB->getNdbErrorString(), pNDB->getNdbError());
  
  MyOperation= MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T5-1: getNdbOperation", 
	     MyTransaction);
  
  MyOperation->interpretedUpdateTuple();
  MyOperation->equal(IND_SUBSCRIBER_NUMBER, 
		     inNumber);
  MyOperation->getValue(IND_SUBSCRIBER_LOCATION, 
		        (char *)&outLocation);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, 
			&outChangedBy[0]);
  MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
                        &outChangedTime[0]);
  MyOperation->getValue(IND_SUBSCRIBER_GROUP,
		        (char *)&groupId);
  MyOperation->getValue(IND_SUBSCRIBER_SESSIONS,
		        (char *)&sessions);
  MyOperation->subValue(IND_SUBSCRIBER_SESSIONS, 
		        (uint32)inServerBit);
  MyTransaction->execute( NoCommit ); 
    /* Operation 2 */

  MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "T5-2: getNdbOperation", 
	     MyTransaction);
    
  MyOperation->readTuple();
  MyOperation->equal(IND_GROUP_ID,
		     (char*)&groupId);
  MyOperation->getValue(IND_GROUP_ALLOW_DELETE, 
			(char *)&permission);
  check = MyTransaction->execute( NoCommit ); 
  CHECK_MINUS_ONE(check, "T5-2: NoCommit", 
		  MyTransaction);
  
  if(((permission & inServerBit) == inServerBit) &&
     ((sessions   & inServerBit) == inServerBit)){
  
    memcpy(inSuffix,
	   &inNumber[SUBSCRIBER_NUMBER_LENGTH-SUBSCRIBER_NUMBER_SUFFIX_LENGTH], SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    
    DEBUG2("deleting(%.*s) - ", SUBSCRIBER_NUMBER_SUFFIX_LENGTH, inSuffix);

    /* Operation 3 */
    MyOperation = MyTransaction->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOperation, "T5-3: getNdbOperation", 
	       MyTransaction);
    
    MyOperation->deleteTuple();
    MyOperation->equal(IND_SESSION_SUBSCRIBER,
		       (char*)inNumber);
    MyOperation->equal(IND_SESSION_SERVER,
		       (char*)&inServerId);
    /* Operation 4 */
        
    /* Operation 5 */
    MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOperation, "T5-5: getNdbOperation", 
	       MyTransaction);
    
    
    MyOperation->interpretedUpdateTuple();
    MyOperation->equal(IND_SERVER_ID,
		       (char*)&inServerId);
    MyOperation->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
		       (char*)inSuffix);
    MyOperation->incValue(IND_SERVER_DELETES, (uint32)1);
    (* outBranchExecuted) = 1;
  } else {
    (* outBranchExecuted) = 0;
    DEBUG1("%s", ((permission & inServerBit) ? "permission - " : "no permission - "));
    DEBUG1("%s", ((sessions   & inServerBit) ? "in session - " : "no in session - "));
  }

  if(!inDoRollback && (* outBranchExecuted)){
    DEBUG("commit\n");
    check = MyTransaction->execute( Commit ); 
    CHECK_MINUS_ONE(check, "T5: Commit", 
		    MyTransaction);
  } else {
    DEBUG("rollback\n");
    check = MyTransaction->execute(Rollback);
    CHECK_MINUS_ONE(check, "T5:Rollback", 
		    MyTransaction);
    
  }
  
  pNDB->closeTransaction(MyTransaction);
}

