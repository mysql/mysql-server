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

static void doOneTransaction(UserHandle *uh, GeneratorStatistics *gen);
static void doTransaction_T1(UserHandle *uh, GeneratorStatistics *gen);
static void doTransaction_T2(UserHandle *uh, GeneratorStatistics *gen);
static void doTransaction_T3(UserHandle *uh, GeneratorStatistics *gen);
static void doTransaction_T4(UserHandle *uh, GeneratorStatistics *gen);
static void doTransaction_T5(UserHandle *uh, GeneratorStatistics *gen);

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
   trans->benchTime        = 0.0;
   trans->count            = 0;
   trans->branchExecuted   = 0;
   trans->rollbackExecuted = 0;
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
      printf("could not set the transaction types\n");
      exit(0);
   }

   if( initSequence(&gen->rollbackSequenceT4,
                    rollbackDefinition) != 0 ) {
      printf("could not set the rollback sequence\n");
      exit(0);
   }

   if( initSequence(&gen->rollbackSequenceT5,
                    rollbackDefinition) != 0 ) {
      printf("could not set the rollback sequence\n");
      exit(0);
   }

   for(i = 0; i < NUM_TRANSACTION_TYPES; i++ )
      clearTransaction(&gen->transactions[i]);

   gen->totalTransactions = 0;

   gen->activeSessions.numberInList = 0;
   gen->activeSessions.readIndex    = 0;
   gen->activeSessions.writeIndex   = 0;
}


static void doOneTransaction(UserHandle *uh, GeneratorStatistics *gen)
{
   unsigned int transactionType;

   transactionType = getNextRandom(&gen->transactionSequence);

   switch(transactionType) {
      case 1:
         doTransaction_T1(uh, gen);
         break;
      case 2:
         doTransaction_T2(uh, gen);
         break;
      case 3:
         doTransaction_T3(uh, gen);
         break;
      case 4:
         doTransaction_T4(uh, gen);
         break;
      case 5:
         doTransaction_T5(uh, gen);
         break;
      default:
         printf("Unknown transaction type: %d\n", transactionType);
   }

   gen->totalTransactions++;
}

static void doTransaction_T1(UserHandle *uh, GeneratorStatistics *gen)
{
   SubscriberNumber number;
   Location         new_location;
   ChangedBy        changed_by; 
   ChangedTime      changed_time;

   double start_time;
   double end_time;
   double transaction_time;

   unsigned int tid = 0;

   /*----------------*/
   /* Init arguments */
   /*----------------*/
   getRandomSubscriberNumber(number);
   getRandomChangedBy(changed_by);
   getRandomChangedTime(changed_time);
   new_location = changed_by[0];

   /*-----------------*/
   /* Run transaction */
   /*-----------------*/
   start_time = userGetTimeSync();
   userTransaction_T1(uh,
                      number,
                      new_location,
                      changed_by,
                      changed_time);
   end_time = userGetTimeSync();

   /*-------------------*/
   /* Update Statistics */
   /*-------------------*/
   transaction_time = end_time - start_time;
   gen->transactions[tid].benchTime += transaction_time;
   gen->transactions[tid].count++;
}

static void doTransaction_T2(UserHandle *uh, GeneratorStatistics *gen)
{
   SubscriberNumber number;
   Location         new_location;
   ChangedBy        changed_by; 
   ChangedTime      changed_time;
   SubscriberName   subscriberName;

   double start_time;
   double end_time;
   double transaction_time;

   unsigned int tid = 1;

   /*----------------*/
   /* Init arguments */
   /*----------------*/
   getRandomSubscriberNumber(number);

   /*-----------------*/
   /* Run transaction */
   /*-----------------*/
   start_time = userGetTimeSync();
   userTransaction_T2(uh,
                      number,
                     &new_location,
                      changed_by,
                      changed_time,
                      subscriberName);
   end_time = userGetTimeSync();

   /*-------------------*/
   /* Update Statistics */
   /*-------------------*/
   transaction_time = end_time - start_time;
   gen->transactions[tid].benchTime += transaction_time;
   gen->transactions[tid].count++;
}

static void doTransaction_T3(UserHandle *uh, GeneratorStatistics *gen)
{
   SubscriberNumber number;
   ServerId         serverId;
   ServerBit        serverBit;
   SessionDetails   sessionDetails;
   unsigned int     branchExecuted;
   SessionElement  *se;

   double start_time;
   double end_time;
   double transaction_time;

   unsigned int tid = 2;

   /*----------------*/
   /* Init arguments */
   /*----------------*/
   se = getNextSession(&gen->activeSessions);
   if( se ) {
      strcpy(number, se->subscriberNumber);
      serverId = se->serverId;
   }
   else {
      getRandomSubscriberNumber(number);
      getRandomServerId(&serverId);
   }

   serverBit = 1 << serverId;

   /*-----------------*/
   /* Run transaction */
   /*-----------------*/
   start_time = userGetTimeSync();
   userTransaction_T3(uh,
                      number,
                      serverId,
                      serverBit,
                      sessionDetails,
                      &branchExecuted);
   end_time = userGetTimeSync();

   /*-------------------*/
   /* Update Statistics */
   /*-------------------*/
   transaction_time = end_time - start_time;
   gen->transactions[tid].benchTime += transaction_time;
   gen->transactions[tid].count++;

   if(branchExecuted)
      gen->transactions[tid].branchExecuted++;
}

static void doTransaction_T4(UserHandle *uh, GeneratorStatistics *gen)
{
   SubscriberNumber number;
   ServerId         serverId;
   ServerBit        serverBit;
   SessionDetails   sessionDetails;
   unsigned int     branchExecuted;
   unsigned int     rollback;

   double start_time;
   double end_time;
   double transaction_time;

   unsigned int tid = 3;

   /*----------------*/
   /* Init arguments */
   /*----------------*/
   getRandomSubscriberNumber(number);
   getRandomServerId(&serverId);

   serverBit = 1 << serverId;
   rollback  = getNextRandom(&gen->rollbackSequenceT4);

   memset(sessionDetails, myRandom48(26)+'A', SESSION_DETAILS_LENGTH);
   sessionDetails[SESSION_DETAILS_LENGTH] = 0;

   /*-----------------*/
   /* Run transaction */
   /*-----------------*/
   start_time = userGetTimeSync();
   userTransaction_T4(uh,
                      number,
                      serverId,
                      serverBit,
                      sessionDetails,
                      rollback,
                      &branchExecuted);
   end_time = userGetTimeSync();

   /*-------------------*/
   /* Update Statistics */
   /*-------------------*/
   transaction_time = end_time - start_time;
   gen->transactions[tid].benchTime += transaction_time;
   gen->transactions[tid].count++;

   if(branchExecuted)
      gen->transactions[tid].branchExecuted++;
   if(rollback)
      gen->transactions[tid].rollbackExecuted++;

   if( branchExecuted && !rollback ) {
      insertSession(&gen->activeSessions, number, serverId);
   }
}

static void doTransaction_T5(UserHandle *uh, GeneratorStatistics *gen)
{
   SubscriberNumber number;
   ServerId         serverId;
   ServerBit        serverBit;
   unsigned int     branchExecuted;
   unsigned int     rollback;
   SessionElement  *se;

   double start_time;
   double end_time;
   double transaction_time;

   unsigned int tid = 4;

   /*----------------*/
   /* Init arguments */
   /*----------------*/
   se = getNextSession(&gen->activeSessions);
   if( se ) {
      strcpy(number, se->subscriberNumber);
      serverId = se->serverId;
   }
   else {
      getRandomSubscriberNumber(number);
      getRandomServerId(&serverId);
   }

   serverBit = 1 << serverId;
   rollback  = getNextRandom(&gen->rollbackSequenceT5);

   /*-----------------*/
   /* Run transaction */
   /*-----------------*/
   start_time = userGetTimeSync();
   userTransaction_T5(uh,
                      number,
                      serverId,
                      serverBit,
                      rollback,
                      &branchExecuted);
   end_time = userGetTimeSync();

   /*-------------------*/
   /* Update Statistics */
   /*-------------------*/
   transaction_time = end_time - start_time;
   gen->transactions[tid].benchTime += transaction_time;
   gen->transactions[tid].count++;

   if(branchExecuted)
      gen->transactions[tid].branchExecuted++;
   if(rollback)
      gen->transactions[tid].rollbackExecuted++;

   if( se && !rollback) {
      deleteSession(&gen->activeSessions);
   }
}

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/


void dbGenerator(UserHandle *uh, ThreadData *data)
{
   GeneratorStatistics rg_warmUp;
   GeneratorStatistics rg_coolDown;
   GeneratorStatistics *st;
   double periodStop;
   double benchTimeStart;
   double benchTimeEnd;
   int i;

   myRandom48Init(data->randomSeed);

   initGeneratorStatistics(&rg_warmUp);
   initGeneratorStatistics(&data->generator);
   initGeneratorStatistics(&rg_coolDown);

   /*----------------*/
   /* warm up period */
   /*----------------*/
   periodStop = userGetTimeSync() + (double)data->warmUpSeconds;
   while(userGetTimeSync() < periodStop){
     doOneTransaction(uh, &rg_warmUp);
   }

   /*-------------------------*/
   /* normal benchmark period */
   /*-------------------------*/
   benchTimeStart = userGetTimeSync();

   if( data->numTransactions > 0 ) {
      for(i = 0; i < data->numTransactions; i++)
         doOneTransaction(uh, &data->generator);
   }
   else {
      periodStop = benchTimeStart + (double)data->testSeconds;
      while(userGetTimeSync() < periodStop)
         doOneTransaction(uh, &data->generator);
   }

   benchTimeEnd = userGetTimeSync();

   /*------------------*/
   /* cool down period */
   /*------------------*/
   periodStop = benchTimeEnd + data->coolDownSeconds;
   while(userGetTimeSync() < periodStop){
     doOneTransaction(uh, &rg_coolDown);
   }

   /*---------------------------------------------------------*/
   /* add the times for all transaction for inner loop timing */
   /*---------------------------------------------------------*/
   st = &data->generator;
   st->innerLoopTime = 0.0;
   for(i = 0 ; i < NUM_TRANSACTION_TYPES; i++) {
      st->innerLoopTime += st->transactions[i].benchTime;
      st->transactions[i].tps = getTps(st->transactions[i].count,
                                       st->transactions[i].benchTime);
   }

   st->outerLoopTime = benchTimeEnd - benchTimeStart;
   st->outerTps      = getTps(st->totalTransactions, st->outerLoopTime);
   st->innerTps      = getTps(st->totalTransactions, st->innerLoopTime);

   /* printf("maxsize = %d\n",maxsize); */
}
