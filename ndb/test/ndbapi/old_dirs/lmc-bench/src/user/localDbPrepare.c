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


/***************************************************************
* L O C A L   D A T A                                          *
***************************************************************/

/*----------------*/
/* Transaction T1 */
/*----------------*/
static char *update_subscriber_stmnt = "update subscriber set \
location = ?,changedBy = ?, changedTime = ? where subscriberNumber = ?";

/*----------------*/
/* Transaction T2 */
/*----------------*/
static char *read_subscriber_stmnt = "select subscriberName,location,\
changedBy,changedTime from subscriber where subscriberNumber = ? for update";

/*----------------*/
/* Transaction T3 */
/*----------------*/
static char *read_subscriber_session_stmnt = "select activeSessions,groupId,\
changedBy,changedTime from subscriber where subscriberNumber = ? for update";

static char *read_group_allowRead_stmnt   = "select allowRead from userGroup \
where groupId = ?";
static char *read_group_allowInsert_stmnt = "select allowInsert from userGroup \
where groupId = ?";
static char *read_group_allowDelete_stmnt = "select allowDelete from userGroup \
where groupId = ?";

static char *read_session_details_stmnt = "select sessionData from userSession \
where subscriberNumber = ? and serverId = ? for update";

static char *update_noOfRead_stmnt = "update server \
set noOfRead = noOfRead + 1 where serverId = ? and subscriberSuffix = ?";
static char *update_noOfInsert_stmnt = "update server \
set noOfInsert = noOfInsert + 1 where serverId = ? and subscriberSuffix = ?";
static char *update_noOfDelete_stmnt = "update server \
set noOfDelete = noOfDelete + 1 where serverId = ? and subscriberSuffix = ?";

static char *insert_session_stmnt = "insert into userSession values (?,?,?)";

static char *delete_session_stmnt = "delete from userSession \
where subscriberNumber = ? and serverId = ?";

static char *update_subscriber_session_stmnt = "update subscriber set \
activeSessions = ? where subscriberNumber = ?";

/***************************************************************
* P U B L I C   D A T A                                        *
***************************************************************/


/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

extern void handle_error(SQLHDBC  hdbc,
                         SQLHENV  henv, 
                         SQLHSTMT hstmt, 
                         SQLRETURN rc,
                         char *filename,
                         int lineno);

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/

int localDbPrepare(UserHandle *uh)
{
   SQLRETURN rc;

   if(!uh) return(-1);

   /*-----------------------------*/
   /* Update Subscriber Statement */
   /*-----------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->updateSubscriber.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate insert group statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->updateSubscriber.stmt,(SQLCHAR *) update_subscriber_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
/*
handle_error(uh->hdbc, uh->henv, uh->updateSubscriber.stmt, rc, __FILE__, __LINE__);
*/
      printf("Unable to prepare update subscriber statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateSubscriber.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->updateSubscriber.values.location,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update subscriber statement param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateSubscriber.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         CHANGED_BY_LENGTH+1,0,
                         uh->updateSubscriber.values.changedBy,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update subscriber statement param 2\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateSubscriber.stmt,
                         3,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         CHANGED_TIME_LENGTH+1,0,
                         uh->updateSubscriber.values.changedTime,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update subscriber statement param 3\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateSubscriber.stmt,
                         4,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->updateSubscriber.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update subscriber statement param 3\n");
      return(-1);
   }

   /*---------------------------*/
   /* Read Subscriber Statement */
   /*---------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->readSubscriber.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate read subscriber statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->readSubscriber.stmt,(SQLCHAR *) read_subscriber_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read subscriber statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readSubscriber.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->readSubscriber.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read subscriber statement param 1\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriber.stmt, 1, 
                   SQL_C_CHAR,
                   uh->readSubscriber.values.name, SUBSCRIBER_NAME_LENGTH+1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 1 to read subscriber statement\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriber.stmt, 2, 
                   SQL_C_DEFAULT,
                   &uh->readSubscriber.values.location, 1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 2 to read subscriber statement\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriber.stmt, 3, 
                   SQL_C_CHAR,
                   uh->readSubscriber.values.changedBy, CHANGED_BY_LENGTH+1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 3 to read subscriber statement\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriber.stmt, 4, 
                   SQL_C_CHAR,
                   uh->readSubscriber.values.changedTime, CHANGED_TIME_LENGTH+1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 4 to read subscriber statement\n");
      return(-1);
   }

   /*------------------------------------*/
   /* Read Subscriber Sessions Statement */
   /*------------------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->readSubscriberSession.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate read subscriber session statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->readSubscriberSession.stmt,(SQLCHAR *) read_subscriber_session_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read subscriber sessions statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readSubscriberSession.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->readSubscriberSession.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read subscriber statement param 1\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriberSession.stmt, 1, 
                   SQL_C_DEFAULT,
                   &uh->readSubscriberSession.values.activeSessions, 0,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 1 to read subscriber sessions statement\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriberSession.stmt, 2, 
                   SQL_C_DEFAULT,
                   &uh->readSubscriberSession.values.groupId, 0,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 2 to read subscriber sessions statement\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriberSession.stmt, 3, 
                   SQL_C_CHAR,
                   uh->readSubscriberSession.values.changedBy, CHANGED_BY_LENGTH+1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 3 to read subscriber sessions statement\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSubscriberSession.stmt, 4, 
                   SQL_C_CHAR,
                   uh->readSubscriberSession.values.changedTime, CHANGED_TIME_LENGTH+1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 4 to read subscriber sessions statement\n");
      return(-1);
   }

   /*--------------------------------*/
   /* Read Group AllowRead Statement */
   /*--------------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->readGroupAllowRead.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate read subscriber session statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->readGroupAllowRead.stmt,(SQLCHAR *) read_group_allowRead_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read group allow read statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readGroupAllowRead.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->readGroupAllowRead.values.groupId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read allow read statement param 1\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readGroupAllowRead.stmt, 1, 
                   SQL_C_DEFAULT,
                   &uh->readGroupAllowRead.values.allowRead, 0,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 1 to read group allow read statement\n");
      return(-1);
   }

   /*----------------------------------*/
   /* Read Group AllowInsert Statement */
   /*----------------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->readGroupAllowInsert.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate read subscriber session statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->readGroupAllowInsert.stmt,(SQLCHAR *) read_group_allowInsert_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read group allow read statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readGroupAllowInsert.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->readGroupAllowInsert.values.groupId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read allow read statement param 1\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readGroupAllowInsert.stmt, 1, 
                   SQL_C_DEFAULT,
                   &uh->readGroupAllowInsert.values.allowInsert, 0,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 1 to read group allow read statement\n");
      return(-1);
   }

   /*----------------------------------*/
   /* Read Group AllowDelete Statement */
   /*----------------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->readGroupAllowDelete.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate read subscriber session statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->readGroupAllowDelete.stmt,(SQLCHAR *) read_group_allowDelete_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read group allow read statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readGroupAllowDelete.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->readGroupAllowDelete.values.groupId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read allow read statement param 1\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readGroupAllowDelete.stmt, 1, 
                   SQL_C_DEFAULT,
                   &uh->readGroupAllowDelete.values.allowDelete, 0,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 1 to read group allow read statement\n");
      return(-1);
   }

   /*----------------------*/
   /* read session details */
   /*----------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->readSessionDetails.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate read session details statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->readSessionDetails.stmt,(SQLCHAR *) read_session_details_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read session details statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readSessionDetails.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->readSessionDetails.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read sessions param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->readSessionDetails.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->readSessionDetails.values.serverId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read sessions param 2\n");
      return(-1);
   }

   rc = SQLBindCol(uh->readSessionDetails.stmt, 1, 
                   SQL_C_CHAR,
                   uh->readSessionDetails.values.details, SESSION_DETAILS_LENGTH+1,
                   NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to bind column 1 to read group allow read statement\n");
      return(-1);
   }

   /*-------------------*/
   /* Update no of Read */
   /*-------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->updateServerNoOfRead.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate update noOfRead statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->updateServerNoOfRead.stmt,(SQLCHAR *) update_noOfRead_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update noOfRead statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateServerNoOfRead.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->updateServerNoOfRead.values.serverId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateServerNoOfRead.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_SUFFIX_LENGTH+1,0,
                         uh->updateServerNoOfRead.values.suffix,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 2\n");
      return(-1);
   }

   /*----------------*/
   /* Insert Session */
   /*----------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->insertSession.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate update noOfRead statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->insertSession.stmt,(SQLCHAR *) insert_session_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare insert session statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->insertSession.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->insertSession.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read sessions param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->insertSession.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->insertSession.values.serverId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 2\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->insertSession.stmt,
                         3,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SESSION_DETAILS_LENGTH+1,0,
                         uh->insertSession.values.details,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read sessions param 1\n");
      return(-1);
   }

   /*----------------------------*/
   /* Update subscriber sessions */
   /*----------------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->updateSubscriberSession.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate update noOfRead statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->updateSubscriberSession.stmt,(SQLCHAR *) update_subscriber_session_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update subscriber session statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateSubscriberSession.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->updateSubscriberSession.values.activeSessions,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 2\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateSubscriberSession.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->updateSubscriberSession.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read sessions param 1\n");
      return(-1);
   }

   /*---------------------*/
   /* Update no of Insert */
   /*---------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->updateServerNoOfInsert.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate update noOfRead statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->updateServerNoOfInsert.stmt,(SQLCHAR *) update_noOfInsert_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update noOfRead statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateServerNoOfInsert.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->updateServerNoOfInsert.values.serverId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateServerNoOfInsert.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_SUFFIX_LENGTH+1,0,
                         uh->updateServerNoOfInsert.values.suffix,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 2\n");
      return(-1);
   }

   /*----------------*/
   /* Delete Session */
   /*----------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->deleteSession.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate update noOfRead statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->deleteSession.stmt,(SQLCHAR *) delete_session_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare insert session statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->deleteSession.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_LENGTH+1,0,
                         uh->deleteSession.values.number,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read sessions param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->deleteSession.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->deleteSession.values.serverId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 2\n");
      return(-1);
   }

   /*---------------------*/
   /* Update no of Delete */
   /*---------------------*/
   rc = SQLAllocStmt(uh->hdbc, &uh->updateServerNoOfDelete.stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate update noOfRead statement\n");
      return(-1);
   }

   rc = SQLPrepare(uh->updateServerNoOfDelete.stmt,(SQLCHAR *) update_noOfDelete_stmnt, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare update noOfRead statement\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateServerNoOfDelete.stmt,
                         1,SQL_PARAM_INPUT,SQL_C_DEFAULT,SQL_INTEGER,
                         0,0,
                         &uh->updateServerNoOfDelete.values.serverId,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 1\n");
      return(-1);
   }

   rc = SQLBindParameter(uh->updateServerNoOfDelete.stmt,
                         2,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,
                         SUBSCRIBER_NUMBER_SUFFIX_LENGTH+1,0,
                         uh->updateServerNoOfInsert.values.suffix,0,NULL);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to prepare read noOfRead param 2\n");
      return(-1);
   }

   /*-------------------------------*/
   /* Commit all prepare statements */
   /*-------------------------------*/
   rc = SQLTransact(uh->henv, uh->hdbc, SQL_COMMIT);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to commit all prepare insert statement\n");
      return(-1);
   }

   return(0);
}
