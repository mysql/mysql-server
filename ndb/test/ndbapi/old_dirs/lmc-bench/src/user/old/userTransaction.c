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
#include <time.h>

#include "sql.h"
#include "sqlext.h"


#include "userInterface.h"
#include "userHandle.h"

/***************************************************************
* L O C A L   C O N S T A N T S                                *
***************************************************************/

/***************************************************************
* L O C A L   D A T A   S T R U C T U R E S                    *
***************************************************************/

/***************************************************************
* L O C A L   F U N C T I O N S                                *
***************************************************************/

static int readSubscriberSessions(UserHandle        *uh,
                                  SubscriberNumber number,
                                  char            *transactionType);

/***************************************************************
* L O C A L   D A T A                                          *
***************************************************************/

extern void handle_error(SQLHDBC  hdbc,
                  SQLHENV  henv, 
                  SQLHSTMT hstmt, 
                  SQLRETURN rc,
                  char *filename,
                  int lineno);

/***************************************************************
* P U B L I C   D A T A                                        *
***************************************************************/


/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

static int readSubscriberSessions(UserHandle      *uh,
                                  SubscriberNumber number,
                                  char            *transactionType)
{
   SQLRETURN rc;

   /*-----------------------------------------------------*/
   /* SELECT activeSessions,groupId,changedBy,changedTime */
   /* FROM SUBSCRIBER                                     */
   /* WHERE subscriberNumber=x;                           */
   /*-----------------------------------------------------*/
   strcpy(uh->readSubscriberSession.values.number,number);

   rc = SQLExecute(uh->readSubscriberSession.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("%s %s\n",
             transactionType, 
             "Unable to execute read subscriber session");
      return(-1);
   }

   rc = SQLFetch(uh->readSubscriberSession.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("%s %s\n",
             transactionType, 
             "Unable to fetch read subscriber session");
      return(-1);
   }

   return(0);
}

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/

void userTransaction_T1(UserHandle        *uh,
                        SubscriberNumber number,
                        Location         new_location,
                        ChangedBy        changed_by,
                        ChangedTime      changed_time)
{
   SQLRETURN rc;

   if(!uh) return;

   /*---------------------------------------------*/
   /* Update the subscriber information           */
   /*                                             */
   /* UPDATE SUBSCRIBER                           */
   /* SET location=x, changedBy=x, changedTime=x  */
   /* WHERE subscriberNumber=x;                   */             
   /*---------------------------------------------*/
   strcpy(uh->updateSubscriber.values.number, number);
   uh->updateSubscriber.values.location = new_location;
   strcpy(uh->updateSubscriber.values.changedBy, changed_by);
   strcpy(uh->updateSubscriber.values.changedTime, changed_time);

   rc = SQLExecute(uh->updateSubscriber.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T1 Unable to execute update subscriber\n");
      return;
   }

   userDbCommit(uh);
}

void userTransaction_T2(UserHandle        *uh,
                        SubscriberNumber  number,
                        Location         *new_location,
                        ChangedBy         changed_by,
                        ChangedTime       changed_time,
                        SubscriberName    subscriberName)
{
   SQLRETURN rc;

   if(!uh) return;

   /*------------------------------------------------------*/
   /* Read the information from the subscriber table       */
   /*                                                      */
   /* SELECT location,subscriberName,changedBy,changedTime */
   /* FROM SUBSCRIBER                                      */
   /* WHERE subscriberNumber=x;                            */
   /*------------------------------------------------------*/
   strcpy(uh->readSubscriber.values.number,number);

   rc = SQLExecute(uh->readSubscriber.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T2 Unable to execute read subscriber\n");
      return;
   }

   rc = SQLFetch(uh->readSubscriber.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T2 Unable to fetch read subscriber\n");
      return;
   }

   userDbCommit(uh);

   strcpy(subscriberName, uh->readSubscriber.values.name);
   *new_location = uh->readSubscriber.values.location;
   strcpy(changed_by, uh->readSubscriber.values.changedBy);
   strcpy(changed_time, uh->readSubscriber.values.changedTime);
}

void userTransaction_T3(UserHandle        *uh,
                        SubscriberNumber number,
                        ServerId         server_id,
                        ServerBit        server_bit,
                        SessionDetails   session_details,
                        unsigned int    *branch_executed)
{
   SQLRETURN rc;

   if(!uh) return;

   *branch_executed = 0;

   /*--------------------------------------*/
   /* Read active sessions from subscriber */
   /*--------------------------------------*/
   if( readSubscriberSessions(uh, number, "T3") < 0 )
      return;

   /*-----------------------------------------------*/
   /* Read the 'read' Permissions for the userGroup */
   /*                                               */
   /* SELECT allowRead                              */
   /* FROM USERGROUP                                */
   /* WHERE groupId=x                               */
   /*-----------------------------------------------*/
   uh->readGroupAllowRead.values.groupId = uh->readSubscriberSession.values.groupId;

   rc = SQLExecute(uh->readGroupAllowRead.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T3 Unable to execute read group allow read\n");
      return;
   }

   rc = SQLFetch(uh->readGroupAllowRead.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T3 Unable to fetch read group allow read\n");
      return;
   }

   if( uh->readGroupAllowRead.values.allowRead & server_bit &&
       uh->readSubscriberSession.values.activeSessions & server_bit ) {

      /*----------------------------------------------------*/
      /* Read the sessionDetails from the userSession table */
      /*                                                    */
      /* SELECT sessionData                                 */
      /* FROM userSession                                   */
      /* WHERE subscriberNumber=x, serverId=x               */
      /*----------------------------------------------------*/
      strcpy(uh->readSessionDetails.values.number,number);
      uh->readSessionDetails.values.serverId = server_id;

      rc = SQLExecute(uh->readSessionDetails.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T3 Unable to execute read session details\n");
         return;
      }

      rc = SQLFetch(uh->readSessionDetails.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T3 Unable to fetch read session details\n");
         return;
      }

      strcpy(session_details, uh->readSessionDetails.values.details);

      /*----------------------------------------*/
      /* Increment noOfRead field in the server */
      /*                                        */
      /* UPDATE server                          */
      /* SET noOfRead=noOfRead+1                */
      /* WHERE serverId=x,subscriberSuffix=x    */
      /*----------------------------------------*/
      uh->updateServerNoOfRead.values.serverId = server_id;
      strcpy(uh->updateServerNoOfRead.values.suffix, 
             &number[SUBSCRIBER_NUMBER_LENGTH-SUBSCRIBER_NUMBER_SUFFIX_LENGTH]);

      rc = SQLExecute(uh->updateServerNoOfRead.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T3 Unable to execute read no of read\n");
         return;
      }

      *branch_executed = 1;
   }

   userDbCommit(uh);
}

void userTransaction_T4(UserHandle        *uh,
                        SubscriberNumber number,
                        ServerId         server_id,
                        ServerBit        server_bit,
                        SessionDetails   session_details,
                        unsigned int     do_rollback,
                        unsigned int    *branch_executed)
{
   SQLRETURN rc;

   if(!uh) return;

   *branch_executed = 0;

   /*--------------------------------------*/
   /* Read active sessions from subscriber */
   /*--------------------------------------*/
   if( readSubscriberSessions(uh, number, "T4") < 0 )
      return;

   /*-------------------------------------------------*/
   /* Read the 'insert' Permissions for the userGroup */
   /*                                                 */
   /* SELECT allowInsert                              */
   /* FROM USERGROUP                                  */
   /* WHERE groupId=x                                 */
   /*-------------------------------------------------*/
   uh->readGroupAllowInsert.values.groupId = uh->readSubscriberSession.values.groupId;

   rc = SQLExecute(uh->readGroupAllowInsert.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T4 Unable to execute read group allow insert\n");
      return;
   }

   rc = SQLFetch(uh->readGroupAllowInsert.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T4 Unable to fetch read group allow insert\n");
      return;
   }

   if( uh->readGroupAllowInsert.values.allowInsert & server_bit &&
       !(uh->readSubscriberSession.values.activeSessions & server_bit) ) {

      /*---------------------------------------------*/
      /* Insert the session to the userSession table */
      /*                                             */
      /* INSERT INTO userSession                     */
      /* VALUES (x,x,x)                              */
      /*---------------------------------------------*/
      strcpy(uh->insertSession.values.number, number);
      uh->insertSession.values.serverId = server_id;
      strcpy(uh->insertSession.values.details, session_details);

      rc = SQLExecute(uh->insertSession.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
handle_error(uh->hdbc, uh->henv, uh->insertSession.stmt, rc, __FILE__, __LINE__);
         printf("T4 Unable to execute insert session \n");
         return;
      }

      /*----------------------------------------*/
      /* Update subscriber activeSessions field */
      /*                                        */
      /* UPDATE subscriber                      */
      /* SET activeSessions=x                   */
      /* WHERE subscriberNumber=x               */
      /*----------------------------------------*/
      strcpy(uh->updateSubscriberSession.values.number, number);
      uh->updateSubscriberSession.values.activeSessions = 
         uh->readSubscriberSession.values.activeSessions | server_bit;

      rc = SQLExecute(uh->updateSubscriberSession.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T4 Unable to execute update session \n");
         return;
      }

      /*------------------------------------------*/
      /* Increment noOfInsert field in the server */
      /*                                          */
      /* UPDATE server                            */
      /* SET noOfInsert=noOfInsert+1              */
      /* WHERE serverId=x,subscriberSuffix=x      */
      /*------------------------------------------*/
      uh->updateServerNoOfInsert.values.serverId = server_id;
      strcpy(uh->updateServerNoOfInsert.values.suffix, 
             &number[SUBSCRIBER_NUMBER_LENGTH-SUBSCRIBER_NUMBER_SUFFIX_LENGTH]);

      rc = SQLExecute(uh->updateServerNoOfInsert.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T4 Unable to execute update no of read\n");
         return;
      }

      *branch_executed = 1;
   }

   if(do_rollback)
      userDbRollback(uh);
   else
      userDbCommit(uh);
}

void userTransaction_T5(UserHandle        *uh,
                        SubscriberNumber number,
                        ServerId         server_id,
                        ServerBit        server_bit,
                        unsigned int     do_rollback,
                        unsigned int    *branch_executed)
{
   SQLRETURN rc;

   if(!uh) return;

   *branch_executed = 0;

   /*--------------------------------------*/
   /* Read active sessions from subscriber */
   /*--------------------------------------*/
   if( readSubscriberSessions(uh, number, "T5") < 0 )
      return;

   /*-------------------------------------------------*/
   /* Read the 'delete' Permissions for the userGroup */
   /*                                                 */
   /* SELECT allowDelete                              */
   /* FROM USERGROUP                                  */
   /* WHERE groupId=x                                 */
   /*-------------------------------------------------*/
   uh->readGroupAllowDelete.values.groupId = uh->readSubscriberSession.values.groupId;

   rc = SQLExecute(uh->readGroupAllowDelete.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T5 Unable to execute read group allow delete\n");
      return;
   }

   rc = SQLFetch(uh->readGroupAllowDelete.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("T5 Unable to fetch read group allow delete\n");
      return;
   }

   if( uh->readGroupAllowDelete.values.allowDelete & server_bit &&
       uh->readSubscriberSession.values.activeSessions & server_bit ) {

      /*-----------------------------------------------*/
      /* Delete the session from the userSession table */
      /*                                               */
      /* DELETE FROM userSession                       */
      /* WHERE subscriberNumber=x,serverId=x           */
      /*-----------------------------------------------*/
      strcpy(uh->deleteSession.values.number,number);
      uh->deleteSession.values.serverId = server_id;

      rc = SQLExecute(uh->deleteSession.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T5 Unable to execute delete session\n");
         return;
      }

      /*----------------------------------------*/
      /* Update subscriber activeSessions field */
      /*                                        */
      /* UPDATE subscriber                      */
      /* SET activeSessions=x                   */
      /* WHERE subscriberNumber=x               */
      /*----------------------------------------*/
      strcpy(uh->updateSubscriberSession.values.number, number);
      uh->updateSubscriberSession.values.activeSessions = 
         uh->readSubscriberSession.values.activeSessions & ~server_bit;

      rc = SQLExecute(uh->updateSubscriberSession.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T5 Unable to execute update subscriber session \n");
         return;
      }

      /*------------------------------------------*/
      /* Increment noOfDelete field in the server */
      /*                                          */
      /* UPDATE server                            */
      /* SET noOfDelete=noOfDelete+1              */
      /* WHERE serverId=x,subscriberSuffix=x      */
      /*------------------------------------------*/
      uh->updateServerNoOfDelete.values.serverId = server_id;
      strcpy(uh->updateServerNoOfDelete.values.suffix, 
             &number[SUBSCRIBER_NUMBER_LENGTH-SUBSCRIBER_NUMBER_SUFFIX_LENGTH]);

      rc = SQLExecute(uh->updateServerNoOfDelete.stmt);
      if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
         printf("T5 Unable to execute update no of delete\n");
         return;
      }

      *branch_executed = 1;
   }

   if(do_rollback)
      userDbRollback(uh);
   else
      userDbCommit(uh);
}


