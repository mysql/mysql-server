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

#ifndef DBINTERFACE_H
#define DBINTERFACE_H

/***************************************************************/
/* I N C L U D E D   F I L E S                                 */
/***************************************************************/

#include "testDefinitions.h"

/***************************************************************
* M A C R O S                                                  *
***************************************************************/

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/

/*-----------------------*/
/* Default Database Name */
/*-----------------------*/
#define DEFAULTDB "TestDbClient"

/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

typedef struct {
  struct Ndb           * pNDB;
  struct NdbConnection * pCurrTrans;
} UserHandle;

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

extern double userGetTimeSync(void);

extern void userCheckpoint(UserHandle *uh);

extern UserHandle *userDbConnect(uint32 createDb, char *dbName);
extern void        userDbDisconnect(UserHandle *uh);

extern int userDbInsertServer(UserHandle      *uh,
                              ServerId         serverId,
	                      SubscriberSuffix suffix,
	                      ServerName       name);

extern int userDbInsertSubscriber(UserHandle      *uh,
	                          SubscriberNumber number,
                                  uint32           groupId,
	                          SubscriberName   name);

extern int userDbInsertGroup(UserHandle *uh,
		             GroupId     groupId, 
		             GroupName   name,
		             Permission  allowRead,
		             Permission  allowInsert,
		             Permission  allowDelete);

extern int userDbCommit(UserHandle *uh);
extern int userDbRollback(UserHandle *uh);

extern void userTransaction_T1(UserHandle      *uh,
                               SubscriberNumber number,
                               Location         new_location,
                               ChangedBy        changed_by,
                               ChangedTime      changed_time);

extern void userTransaction_T2(UserHandle       *uh,
                               SubscriberNumber  number,
                               Location         *new_location,
                               ChangedBy         changed_by,
                               ChangedTime       changed_time,
                               SubscriberName    subscriberName);

extern void userTransaction_T3(UserHandle      *uh,
                               SubscriberNumber number,
                               ServerId         server_id,
                               ServerBit        server_bit,
                               SessionDetails   session_details,
                               unsigned int    *branch_executed);

extern void userTransaction_T4(UserHandle      *uh,
                               SubscriberNumber number,
                               ServerId         server_id,
                               ServerBit        server_bit,
                               SessionDetails   session_details,
                               unsigned int     do_rollback,
                               unsigned int    *branch_executed);

extern void userTransaction_T5(UserHandle      *uh,
                               SubscriberNumber number,
                               ServerId         server_id,
                               ServerBit        server_bit,
                               unsigned int     do_rollback,
                               unsigned int    *branch_executed);


#ifdef __cplusplus
}
#endif

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/

#endif /* DBINTERFACE_H */

