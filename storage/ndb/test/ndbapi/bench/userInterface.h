/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#ifndef DBINTERFACE_H
#define DBINTERFACE_H

/***************************************************************/
/* I N C L U D E D   F I L E S                                 */
/***************************************************************/

#include "testDefinitions.h"
#include "testData.h"

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

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

class Ndb;

#ifdef __cplusplus
extern "C" {
#endif
  extern void showTime();
  extern double userGetTime(void);
  extern Ndb   *asyncDbConnect(int parallellism);
  extern void    asyncDbDisconnect(Ndb* pNDB);

  extern void getRandomSubscriberNumber(SubscriberNumber number);

  extern void start_T1(Ndb * uh, ThreadData * data, int async);
  extern void start_T2(Ndb * uh, ThreadData * data, int async);
  extern void start_T3(Ndb * uh, ThreadData * data, int async);
  extern void start_T4(Ndb * uh, ThreadData * data, int async);
  extern void start_T5(Ndb * uh, ThreadData * data, int async);
  
  extern void complete_T1(ThreadData * data);
  extern void complete_T2(ThreadData * data);
  extern void complete_T3(ThreadData * data);
  extern void complete_T4(ThreadData * data);
  extern void complete_T5(ThreadData * data);



#ifdef __cplusplus
}
#endif



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
  class Ndb_cluster_connection* pNCC;
  class Ndb           * pNDB;
  class NdbTransaction * pCurrTrans;
} UserHandle;

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

extern double userGetTimeSync(void);

extern void userCheckpoint(UserHandle *uh);

extern UserHandle *userDbConnect(uint32 createDb, const char *dbName);
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
  
#ifdef __cplusplus
}
#endif

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/

#endif /* DBINTERFACE_H */

