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

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

#include <ndb_global.h>

#include "dbGenerator.h"
#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>

/***************************************************************
* L O C A L   C O N S T A N T S                                *
***************************************************************/

/***************************************************************
* L O C A L   D A T A   S T R U C T U R E S                    *
***************************************************************/

/***************************************************************
* L O C A L   F U N C T I O N S                                *
***************************************************************/

static void getRandomSubscriberNumber(SubscriberNumber number);
static void getRandomServerId(ServerId *serverId);
static void getRandomChangedBy(ChangedBy changedBy);
static void getRandomChangedTime(ChangedTime changedTime);

static void clearTransaction(TransactionDefinition *trans);
static void initGeneratorStatistics(GeneratorStatistics *gen);

static void doOneTransaction(ThreadData * td, 
			     int parallellism,
			     int millisSendPoll,
			     int minEventSendPoll,
			     int forceSendPoll);
static void doTransaction_T1(Ndb * pNDB, ThreadData * td, int async);
static void doTransaction_T2(Ndb * pNDB, ThreadData * td, int async);
static void doTransaction_T3(Ndb * pNDB, ThreadData * td, int async);
static void doTransaction_T4(Ndb * pNDB, ThreadData * td, int async);
static void doTransaction_T5(Ndb * pNDB, ThreadData * td, int async);

/***************************************************************
* L O C A L   D A T A                                          *
***************************************************************/

static SequenceValues transactionDefinition[] = {
   {25, 1},
   {25, 2},
   {20, 3},
   {15, 4},
   {15, 5},
   {0,  0}
};

static SequenceValues rollbackDefinition[] = {
   {98, 0},
   {2 , 1},
   {0,  0}
};

static int maxsize = 0;

/***************************************************************
* P U B L I C   D A T A                                        *
***************************************************************/

/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

static void getRandomSubscriberNumber(SubscriberNumber number)
{
   uint32 tmp;
   char sbuf[SUBSCRIBER_NUMBER_LENGTH + 1];
   tmp = myRandom48(NO_OF_SUBSCRIBERS);
   sprintf(sbuf, "%.*d", SUBSCRIBER_NUMBER_LENGTH, tmp);
   memcpy(number, sbuf, SUBSCRIBER_NUMBER_LENGTH);
}

static void getRandomServerId(ServerId *serverId)
{
   *serverId = myRandom48(NO_OF_SERVERS);
}

static void getRandomChangedBy(ChangedBy changedBy)
{
   memset(changedBy, myRandom48(26)+'A', CHANGED_BY_LENGTH);
   changedBy[CHANGED_BY_LENGTH] = 0;
}

static void getRandomChangedTime(ChangedTime changedTime)
{
   memset(changedTime, myRandom48(26)+'A', CHANGED_TIME_LENGTH);
   changedTime[CHANGED_TIME_LENGTH] = 0;
}

static void clearTransaction(TransactionDefinition *trans)
{
  trans->count            = 0;
  trans->branchExecuted   = 0;
  trans->rollbackExecuted = 0;
  trans->latencyCounter   = myRandom48(127);
  trans->latency.reset();
}

static int listFull(SessionList *list)
{
   return(list->numberInList == SESSION_LIST_LENGTH);
}

static int listEmpty(SessionList *list)
{
   return(list->numberInList == 0);
}

static void insertSession(SessionList     *list, 
                          SubscriberNumber number,
                          ServerId         serverId)
{
   SessionElement *e;
   if( listFull(list) ) return;

   e = &list->list[list->writeIndex];

   strcpy(e->subscriberNumber, number);
   e->serverId = serverId;

   list->writeIndex = (list->writeIndex + 1) % SESSION_LIST_LENGTH;
   list->numberInList++;

   if( list->numberInList > maxsize )
     maxsize = list->numberInList;
}

static SessionElement *getNextSession(SessionList *list)
{
   if( listEmpty(list) ) return(0);

   return(&list->list[list->readIndex]);
}

static void deleteSession(SessionList *list)
{
   if( listEmpty(list) ) return;

   list->readIndex = (list->readIndex + 1) % SESSION_LIST_LENGTH;
   list->numberInList--;
}

static void initGeneratorStatistics(GeneratorStatistics *gen)
{
   int i;

   if( initSequence(&gen->transactionSequence,
                    transactionDefinition) != 0 ) {
      ndbout_c("could not set the transaction types");
      exit(0);
   }

   if( initSequence(&gen->rollbackSequenceT4,
                    rollbackDefinition) != 0 ) {
      ndbout_c("could not set the rollback sequence");
      exit(0);
   }

   if( initSequence(&gen->rollbackSequenceT5,
                    rollbackDefinition) != 0 ) {
      ndbout_c("could not set the rollback sequence");
      exit(0);
   }

   for(i = 0; i < NUM_TRANSACTION_TYPES; i++ )
      clearTransaction(&gen->transactions[i]);

   gen->totalTransactions = 0;

   gen->activeSessions.numberInList = 0;
   gen->activeSessions.readIndex    = 0;
   gen->activeSessions.writeIndex   = 0;
}


static 
void 
doOneTransaction(ThreadData * td, int p, int millis, int minEvents, int force)
{
  int i;
  unsigned int transactionType;
  int async = 1;
  if (p == 1) {
    async = 0;
  }//if
  for(i = 0; i<p; i++){
    if(td[i].runState == Runnable){
      transactionType = getNextRandom(&td[i].generator.transactionSequence);

      switch(transactionType) {
      case 1:
	doTransaction_T1(td[i].pNDB, &td[i], async);
	break;
      case 2:
	doTransaction_T2(td[i].pNDB, &td[i], async);
	break;
      case 3:
	doTransaction_T3(td[i].pNDB, &td[i], async);
	break;
      case 4:
	doTransaction_T4(td[i].pNDB, &td[i], async);
	break;
      case 5:
	doTransaction_T5(td[i].pNDB, &td[i], async);
	break;
      default:
	ndbout_c("Unknown transaction type: %d", transactionType);
      }
    }
  }
  if (async == 1) {
    td[0].pNDB->sendPollNdb(millis, minEvents, force);
  }//if
}  

static 
void 
doTransaction_T1(Ndb * pNDB, ThreadData * td, int async)
{
  /*----------------*/
  /* Init arguments */
  /*----------------*/
  getRandomSubscriberNumber(td->transactionData.number);
  getRandomChangedBy(td->transactionData.changed_by);
  BaseString::snprintf(td->transactionData.changed_time,
	   sizeof(td->transactionData.changed_time),
	   "%ld - %d", td->changedTime++, myRandom48(65536*1024));
  //getRandomChangedTime(td->transactionData.changed_time);
  td->transactionData.location = td->transactionData.changed_by[0];
  
  /*-----------------*/
  /* Run transaction */
  /*-----------------*/
  td->runState = Running;
  td->generator.transactions[0].startLatency();

  start_T1(pNDB, td, async);
}

static
void 
doTransaction_T2(Ndb * pNDB, ThreadData * td, int async)
{
  /*----------------*/
  /* Init arguments */
  /*----------------*/
  getRandomSubscriberNumber(td->transactionData.number);

  /*-----------------*/
  /* Run transaction */
  /*-----------------*/
  td->runState = Running;
  td->generator.transactions[1].startLatency();

  start_T2(pNDB, td, async);
}

static
void 
doTransaction_T3(Ndb * pNDB, ThreadData * td, int async)
{
  SessionElement  *se;
  
  /*----------------*/
  /* Init arguments */
  /*----------------*/
  se = getNextSession(&td->generator.activeSessions);
  if( se ) {
    strcpy(td->transactionData.number, se->subscriberNumber);
    td->transactionData.server_id = se->serverId;
    td->transactionData.sessionElement = 1;
  } else {
    getRandomSubscriberNumber(td->transactionData.number);
    getRandomServerId(&td->transactionData.server_id);
    td->transactionData.sessionElement = 0;
  }
  
  td->transactionData.server_bit = (1 << td->transactionData.server_id);

  /*-----------------*/
  /* Run transaction */
  /*-----------------*/
  td->runState = Running;
  td->generator.transactions[2].startLatency();
  start_T3(pNDB, td, async);
}

static 
void 
doTransaction_T4(Ndb * pNDB, ThreadData * td, int async)
{
   /*----------------*/
   /* Init arguments */
   /*----------------*/
  getRandomSubscriberNumber(td->transactionData.number);
  getRandomServerId(&td->transactionData.server_id);
  
  td->transactionData.server_bit = (1 << td->transactionData.server_id);
  td->transactionData.do_rollback = 
    getNextRandom(&td->generator.rollbackSequenceT4);

  memset(td->transactionData.session_details+2, 
	 myRandom48(26)+'A', SESSION_DETAILS_LENGTH-3);
  td->transactionData.session_details[SESSION_DETAILS_LENGTH-1] = 0;
  int2store(td->transactionData.session_details,SESSION_DETAILS_LENGTH-2);
  
  /*-----------------*/
  /* Run transaction */
  /*-----------------*/
  td->runState = Running;
  td->generator.transactions[3].startLatency();
  start_T4(pNDB, td, async);
}

static 
void 
doTransaction_T5(Ndb * pNDB, ThreadData * td, int async)
{
  SessionElement * se;
  se = getNextSession(&td->generator.activeSessions);
  if( se ) {
    strcpy(td->transactionData.number, se->subscriberNumber);
    td->transactionData.server_id = se->serverId;
    td->transactionData.sessionElement = 1;
  }
  else {
    getRandomSubscriberNumber(td->transactionData.number);
    getRandomServerId(&td->transactionData.server_id);
    td->transactionData.sessionElement = 0;
  }
  
  td->transactionData.server_bit = (1 << td->transactionData.server_id);
  td->transactionData.do_rollback  
    = getNextRandom(&td->generator.rollbackSequenceT5);
  
  /*-----------------*/
  /* Run transaction */
  /*-----------------*/
  td->runState = Running;
  td->generator.transactions[4].startLatency();
  start_T5(pNDB, td, async);
}

void
complete_T1(ThreadData * data){
  data->generator.transactions[0].stopLatency();
  data->generator.transactions[0].count++;

  data->runState = Runnable;
  data->generator.totalTransactions++;
}

void 
complete_T2(ThreadData * data){
  data->generator.transactions[1].stopLatency();
  data->generator.transactions[1].count++;

  data->runState = Runnable;
  data->generator.totalTransactions++;
}

void 
complete_T3(ThreadData * data){

  data->generator.transactions[2].stopLatency();
  data->generator.transactions[2].count++;

  if(data->transactionData.branchExecuted)
    data->generator.transactions[2].branchExecuted++;

  data->runState = Runnable;
  data->generator.totalTransactions++;
}

void 
complete_T4(ThreadData * data){

  data->generator.transactions[3].stopLatency();
  data->generator.transactions[3].count++;

  if(data->transactionData.branchExecuted)
    data->generator.transactions[3].branchExecuted++;
  if(data->transactionData.do_rollback)
    data->generator.transactions[3].rollbackExecuted++;
  
  if(data->transactionData.branchExecuted &&
     !data->transactionData.do_rollback){
    insertSession(&data->generator.activeSessions, 
		  data->transactionData.number, 
		  data->transactionData.server_id);
  }

  data->runState = Runnable;
  data->generator.totalTransactions++;

}
void 
complete_T5(ThreadData * data){

  data->generator.transactions[4].stopLatency();
  data->generator.transactions[4].count++;

  if(data->transactionData.branchExecuted)
    data->generator.transactions[4].branchExecuted++;
  if(data->transactionData.do_rollback)
    data->generator.transactions[4].rollbackExecuted++;
  
  if(data->transactionData.sessionElement && 
     !data->transactionData.do_rollback){
    deleteSession(&data->generator.activeSessions);
  }
  
  data->runState = Runnable;
  data->generator.totalTransactions++;
}

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/
void 
asyncGenerator(ThreadData *data, 
	       int parallellism, 
	       int millisSendPoll,
	       int minEventSendPoll,
	       int forceSendPoll)
{
  ThreadData * startUp;
  
  GeneratorStatistics *st;
  double periodStop;
  double benchTimeStart;
  double benchTimeEnd;
  int i, j, done;

  myRandom48Init(data->randomSeed);
  
  for(i = 0; i<parallellism; i++){
    initGeneratorStatistics(&data[i].generator);
   }

  startUp = (ThreadData*)malloc(parallellism * sizeof(ThreadData));
  memcpy(startUp, data, (parallellism * sizeof(ThreadData)));
  
  /*----------------*/
  /* warm up period */
  /*----------------*/
  periodStop = userGetTime() + (double)data[0].warmUpSeconds;
  
  while(userGetTime() < periodStop){
    doOneTransaction(startUp, parallellism, 
		     millisSendPoll, minEventSendPoll, forceSendPoll);
  }
  
  ndbout_c("Waiting for startup to finish");

  /**
   * Wait for all transactions
   */
  done = 0;
  while(!done){
    done = 1;
    for(i = 0; i<parallellism; i++){
      if(startUp[i].runState != Runnable){
	done = 0;
	break;
      }
    }
    if(!done){
      startUp[0].pNDB->sendPollNdb();
    }
  }
  ndbout_c("Benchmark period starts");

  /*-------------------------*/
  /* normal benchmark period */
  /*-------------------------*/
  benchTimeStart = userGetTime();
  
  periodStop = benchTimeStart + (double)data[0].testSeconds;
  while(userGetTime() < periodStop)
    doOneTransaction(data, parallellism,
		     millisSendPoll, minEventSendPoll, forceSendPoll);  

  benchTimeEnd = userGetTime();
  
  ndbout_c("Benchmark period done");

  /**
   * Wait for all transactions
   */
  done = 0;
  while(!done){
    done = 1;
    for(i = 0; i<parallellism; i++){
      if(data[i].runState != Runnable){
	done = 0;
	break;
      }
    }
    if(!done){
      data[0].pNDB->sendPollNdb();
    }
  }

  /*------------------*/
  /* cool down period */
   /*------------------*/
  periodStop = userGetTime() + (double)data[0].coolDownSeconds;
  while(userGetTime() < periodStop){
    doOneTransaction(startUp, parallellism,
		     millisSendPoll, minEventSendPoll, forceSendPoll);
  }

  done = 0;
  while(!done){
    done = 1;
    for(i = 0; i<parallellism; i++){
      if(startUp[i].runState != Runnable){
	done = 0;
	break;
      }
    }
    if(!done){
      startUp[0].pNDB->sendPollNdb();
    }
  }


  /*---------------------------------------------------------*/
  /* add the times for all transaction for inner loop timing */
  /*---------------------------------------------------------*/
  for(j = 0; j<parallellism; j++){
    st = &data[j].generator;
    
    st->outerLoopTime = benchTimeEnd - benchTimeStart;
    st->outerTps      = getTps(st->totalTransactions, st->outerLoopTime);
  }
  /* ndbout_c("maxsize = %d\n",maxsize); */

  free(startUp);
}

