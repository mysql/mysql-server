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

#ifndef TESTDATA_H
#define TESTDATA_H

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

#include "testDefinitions.h"
#include <random.h>

/***************************************************************
* M A C R O S                                                  *
***************************************************************/

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/

#define NUM_TRANSACTION_TYPES    5
#define SESSION_LIST_LENGTH   1000

/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

typedef struct {
   SubscriberNumber subscriberNumber;
   ServerId         serverId;
} SessionElement;

typedef struct {
   SessionElement list[SESSION_LIST_LENGTH];
   unsigned int readIndex;
   unsigned int writeIndex;
   unsigned int numberInList;
} SessionList;  

typedef struct {
   double        benchTime;
   unsigned int  count;
   double        tps;
   unsigned int  branchExecuted;
   unsigned int  rollbackExecuted;
}TransactionDefinition;

typedef struct {
   RandomSequence transactionSequence;
   RandomSequence rollbackSequenceT4;
   RandomSequence rollbackSequenceT5;

   TransactionDefinition transactions[NUM_TRANSACTION_TYPES];

   unsigned int totalTransactions;

   double       innerLoopTime;
   double       innerTps;

   double       outerLoopTime;
   double       outerTps;

   SessionList  activeSessions;
} GeneratorStatistics;

typedef struct {
   unsigned long threadId;
   unsigned long randomSeed;

   unsigned int warmUpSeconds;
   unsigned int testSeconds;
   unsigned int coolDownSeconds;
   unsigned int numTransactions;

   GeneratorStatistics generator;
}ThreadData;

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/



#endif /* TESTDATA_H */

