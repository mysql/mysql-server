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

extern int localDbPrepare(UserHandle *uh);

static int dbCreate(UserHandle *uh);

/***************************************************************
* L O C A L   D A T A                                          *
***************************************************************/

static char *create_subscriber_table = 
"CREATE TABLE subscriber(\
subscriberNumber CHAR(12) NOT NULL primary key,\
subscriberName   CHAR(32) NOT NULL,\
groupId          INT      NOT NULL,\
location         INT      NOT NULL,\
activeSessions   INT      NOT NULL,\
changedBy        CHAR(32) NOT NULL,\
changedTime      CHAR(32) NOT NULL)";

static char *create_group_table = 
"CREATE TABLE userGroup(\
groupId          INT      NOT NULL primary key,\
groupName        CHAR(32) NOT NULL,\
allowRead        INT      NOT NULL,\
allowInsert      INT      NOT NULL,\
allowDelete      INT      NOT NULL)";

static char *create_server_table = "CREATE TABLE server(\
serverId         INT      NOT NULL,\
subscriberSuffix CHAR(2)  NOT NULL,\
serverName       CHAR(32) NOT NULL,\
noOfRead         INT      NOT NULL,\
noOfInsert       INT      NOT NULL,\
noOfDelete       INT      NOT NULL,\
PRIMARY KEY(serverId,subscriberSuffix))";

static char *create_session_table = 
"CREATE TABLE userSession(\
subscriberNumber CHAR(12) NOT NULL,\
serverId         INT      NOT NULL,\
sessionData      CHAR(2000) NOT NULL,\
PRIMARY KEY(subscriberNumber,serverId))";

/***************************************************************
* P U B L I C   D A T A                                        *
***************************************************************/


/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/

/*-----------------------------------*/
/* Time related Functions            */
/*                                   */
/* Returns a double value in seconds */
/*-----------------------------------*/
double userGetTime(void)
{
   static int initialized = 0;
   static struct timeval initTime;
   double timeValue;

   if( !initialized ) {
      initialized = 1;
      gettimeofday(&initTime, 0);
      timeValue = 0.0;
   }
   else {
      struct timeval tv;
      double s;
      double us;

      gettimeofday(&tv, 0);
      s  = (double)tv.tv_sec  - (double)initTime.tv_sec;
      us = (double)tv.tv_usec - (double)initTime.tv_usec;

      timeValue = s + (us / 1000000.0);
   }

   return(timeValue);
}


void handle_error(SQLHDBC  hdbc,
                  SQLHENV  henv, 
                  SQLHSTMT hstmt, 
                  SQLRETURN rc,
                  char *filename,
                  int lineno)
{
#define MSG_LNG 512

   int isError = 0; 
   SQLRETURN     ret = SQL_SUCCESS;
   SQLCHAR       szSqlState[MSG_LNG];    /* SQL state string  */
   SQLCHAR       szErrorMsg[MSG_LNG];    /* Error msg text buffer pointer */
   SQLINTEGER    pfNativeError;          /* Native error code */
   SQLSMALLINT   pcbErrorMsg;            /* Error msg text Available bytes */

   if ( rc == SQL_SUCCESS || rc == SQL_NO_DATA_FOUND )
      return;
   else if ( rc == SQL_INVALID_HANDLE ) {
      printf("ERROR in %s, line %d: invalid handle\n",
               filename, lineno);
      isError = 1;
   }
   else if ( rc == SQL_SUCCESS_WITH_INFO ) {
      printf("WARNING in %s, line %d\n",
               filename, lineno);
      isError = 0;
   }
   else if ( rc == SQL_ERROR ) {
      printf("ERROR in %s, line %d\n",
               filename, lineno);
      isError = 1;
   }

   fflush(stdout);

   while ( ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO ) {
      ret = SQLError(henv, hdbc, hstmt, szSqlState, &pfNativeError, szErrorMsg,
                     MSG_LNG, &pcbErrorMsg);

      switch (ret) {
         case SQL_SUCCESS:
         case SQL_SUCCESS_WITH_INFO:
            printf("%s\n*** ODBC Error/Warning = %s, "
                    "Additional Error/Warning = %d\n",
                    szErrorMsg, szSqlState, pfNativeError);

            if(ret == SQL_SUCCESS_WITH_INFO)
               printf("(Note: error message was truncated.\n");
            break;

         case SQL_INVALID_HANDLE:
           printf("Call to SQLError failed with return code of "
                    "SQL_INVALID_HANDLE.\n");
           break;

         case SQL_ERROR:
            printf("Call to SQLError failed with return code of SQL_ERROR.\n");
            break;

         case SQL_NO_DATA_FOUND:
            break;

         default:
           printf("Call to SQLError failed with return code of %d.\n", ret);
      }
   }

   if ( isError )
      exit(1);
}

static int dbCreate(UserHandle *uh)
{
   SQLRETURN rc;
   SQLHSTMT  creatstmt;

   if(!uh) return(-1);

   rc = SQLAllocStmt(uh->hdbc, &creatstmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate create statement\n");
      return(-1);
   }

   rc = SQLExecDirect(creatstmt,(SQLCHAR *)create_subscriber_table, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to create subscriber table\n");
      return(-1);
   }

   rc = SQLExecDirect(creatstmt,(SQLCHAR *)create_group_table, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to create group table\n");
      return(-1);
   }

   rc = SQLExecDirect(creatstmt,(SQLCHAR *)create_server_table, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to create server table\n");
      return(-1);
   }

   rc = SQLExecDirect(creatstmt,(SQLCHAR *)create_session_table, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to create session table\n");
      return(-1);
   }

   rc = SQLTransact(uh->henv, uh->hdbc, SQL_COMMIT);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to commit all create table\n");
      return(-1);
   }

   rc = SQLFreeStmt(creatstmt, SQL_DROP);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to free create statement\n");
      return(-1);
   }

   return(0);
}

UserHandle *userDbConnect(uint32 createDb, char *dbName)
{
   char      connStrIn[512]; /* ODBC Connection String */
   char      connStrOut[2048];
   SQLRETURN rc;
   UserHandle *uh;

   /*--------------------------*/
   /* Build the Connect string */
   /*--------------------------*/
   sprintf(connStrIn,
           "AutoCreate=%d;OverWrite=%d;DSN=%s",
           createDb ? 1 : 0,
           createDb ? 1 : 0,
           dbName);

   uh = calloc(1, sizeof(UserHandle));
   if( !uh ) {
      printf("Unable to allocate memory for Handle\n");
      return(0);
   }

   /*---------------------------------*/
   /* Allocate the Environment Handle */
   /*---------------------------------*/
   rc = SQLAllocEnv(&uh->henv);

   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate Environment Handle\n");
      return(0);
   }

   /*--------------------------------*/
   /* Allocate the DB Connect Handle */
   /*--------------------------------*/
   rc = SQLAllocConnect(uh->henv, &uh->hdbc);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate a connection handle\n");
      return(0);
   }

   /*-------------------------*/
   /* Connect to the Database */
   /*-------------------------*/
   rc = SQLDriverConnect(uh->hdbc, NULL, 
                        (SQLCHAR *)connStrIn, SQL_NTS,
                        (SQLCHAR *)connStrOut, sizeof (connStrOut), 
                         NULL, SQL_DRIVER_NOPROMPT);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
handle_error(uh->hdbc, uh->henv, NULL, rc, __FILE__, __LINE__);
      printf("Unable to connect to database server\n");
      return(0);
   }

   rc = SQLSetConnectOption(uh->hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to set connection option\n");
      return(0);
   }

   rc = SQLAllocStmt(uh->hdbc, &uh->stmt);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to allocate immediate statement\n");
      return(0);
   }

   if( createDb )
      dbCreate(uh);

   if( localDbPrepare(uh) < 0 )
      return(0);

   return(uh);
}

void userDbDisconnect(UserHandle *uh)
{
   SQLRETURN rc;

   if(!uh) return;

   rc = SQLDisconnect(uh->hdbc);

   SQLFreeConnect(uh->hdbc);
   SQLFreeEnv(uh->henv);
   free(uh);
}

int userDbInsertServer(UserHandle        *uh,
                       ServerId         serverId,
	               SubscriberSuffix suffix,
	               ServerName       name)
{
   SQLRETURN rc;
   char buf[1000];

   if(!uh) return(-1);

   sprintf(buf, "insert into server values (%d,'%.*s','%s',0,0,0)",
           serverId,
           SUBSCRIBER_NUMBER_SUFFIX_LENGTH, suffix,
           name);

   rc = SQLExecDirect(uh->stmt, (unsigned char *)buf, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to execute insert server\n");
      return(-1);
   }

   return( userDbCommit(uh) );
}

int userDbInsertSubscriber(UserHandle        *uh,
	                   SubscriberNumber number,
                           uint32           groupId,
	                   SubscriberName   name)
{
   SQLRETURN rc;
   char buf[1000];

   if(!uh) return(-1);

   sprintf(buf, "insert into subscriber values ('%s','%s',%d,0,0,'','')",
           number,
           name,
           groupId);

   rc = SQLExecDirect(uh->stmt, (unsigned char*)buf, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to execute insert subscriber\n");
      return(-1);
   }

   return( userDbCommit(uh) );
}

int userDbInsertGroup(UserHandle  *uh,
		      GroupId    groupId, 
		      GroupName  name,
		      Permission allowRead,
		      Permission allowInsert,
		      Permission allowDelete)
{
   SQLRETURN rc;
   char buf[1000];

   if(!uh) return(-1);

   sprintf(buf, "insert into usergroup values (%d,'%s',%d,%d,%d)",
           groupId,
           name,           
           allowRead,
           allowInsert,
           allowDelete);

   rc = SQLExecDirect(uh->stmt, (unsigned char*)buf, SQL_NTS);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to execute insert group\n");
      return(-1);
   }

   return( userDbCommit(uh) );
}

int userDbCommit(UserHandle *uh)
{
   SQLRETURN rc;
   if(!uh) return(-1);

   rc = SQLTransact(uh->henv, uh->hdbc, SQL_COMMIT);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
handle_error(uh->hdbc, uh->henv, 0, rc, __FILE__, __LINE__);
      printf("Unable to commit Transaction\n");
      return(-1);
   }

   return(0);
}

int userDbRollback(UserHandle *uh)
{
   SQLRETURN rc;
   if(!uh) return(-1);

   rc = SQLTransact(uh->henv, uh->hdbc, SQL_ROLLBACK);
   if( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
      printf("Unable to rollback Transaction\n");
      return(-1);
   }

   return(0);
}

void userCheckpoint(UserHandle *uh)
{
   SQLRETURN rc;
   if(!uh) return;

   rc = SQLExecDirect(uh->stmt, (SQLCHAR *)"call ttCheckpointFuzzy", SQL_NTS);
   userDbCommit(uh);
}
