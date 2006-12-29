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

#ifndef TESTDEFINITIONS_H
#define TESTDEFINITIONS_H

/***************************************************************/
/* I N C L U D E D   F I L E S                                 */
/***************************************************************/

#include <ndb_types.h>

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/

#define OP_PER_TRANS                  200
#define NO_OF_SUBSCRIBERS             500000
#define NO_OF_GROUPS                     100
#define NO_OF_SERVERS                     20

#define SUBSCRIBER_NUMBER_LENGTH          12
#define SUBSCRIBER_NUMBER_SUFFIX_LENGTH    2

#define SUBSCRIBER_NAME_LENGTH            32
#define CHANGED_BY_LENGTH                 32
#define CHANGED_TIME_LENGTH               32
#define SESSION_DETAILS_LENGTH          2000
#define SERVER_NAME_LENGTH                32
#define GROUP_NAME_LENGTH                 32

/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

#define PADDING 4

typedef char   SubscriberNumber[SUBSCRIBER_NUMBER_LENGTH];
typedef char   SubscriberSuffix[SUBSCRIBER_NUMBER_SUFFIX_LENGTH + 2];
typedef char   SubscriberName[SUBSCRIBER_NAME_LENGTH];
typedef char   ServerName[SERVER_NAME_LENGTH];
typedef char   GroupName[GROUP_NAME_LENGTH];
typedef char   ChangedBy[CHANGED_BY_LENGTH];
typedef char   ChangedTime[CHANGED_TIME_LENGTH];
typedef char   SessionDetails[SESSION_DETAILS_LENGTH];
typedef Uint32 ServerId;
typedef Uint32 ServerBit;
typedef Uint32 GroupId;
typedef Uint32 Location;
typedef Uint32 Permission;

typedef Uint32 Counter;
typedef Uint32 ActiveSessions;
typedef unsigned int BranchExecuted;
typedef unsigned int DoRollback;

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/



#endif /* TESTDEFINITIONS_H */

