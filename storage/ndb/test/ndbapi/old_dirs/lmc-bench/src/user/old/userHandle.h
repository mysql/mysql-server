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

#ifndef USERHANDLE_H
#define USERHANDLE_H

/***************************************************************/
/* I N C L U D E D   F I L E S                                 */
/***************************************************************/

#include "sql.h"
#include "sqlext.h"
#include "sqltypes.h"

#include "testDefinitions.h"

/***************************************************************
* M A C R O S                                                  *
***************************************************************/

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/

/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

struct userHandle{
   SQLHENV  henv;
   SQLHDBC  hdbc;
   SQLHSTMT stmt;

   /*----------------*/
   /* Transaction T1 */
   /*----------------*/
   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         Location         location;
         ChangedBy        changedBy;
         ChangedTime      changedTime;
      }values;
   }updateSubscriber;

   /*----------------*/
   /* Transaction T2 */
   /*----------------*/
   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         SubscriberName   name;
         Location         location;
         ChangedBy        changedBy;
         ChangedTime      changedTime;
      }values;
   }readSubscriber;

   /*----------------*/
   /* Transaction T3 */
   /*----------------*/
   struct {
      SQLHSTMT stmt;
      struct {
         GroupId          groupId;
         Permission       allowRead;
      }values;
   }readGroupAllowRead;

   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         ServerId         serverId;
         SessionDetails   details;
      }values;
   }readSessionDetails;

   struct {
      SQLHSTMT stmt;
      struct {
         ServerId         serverId;
         SubscriberSuffix suffix;
      }values;
   }updateServerNoOfRead;

   /*----------------*/
   /* Transaction T4 */
   /*----------------*/
   struct {
      SQLHSTMT stmt;
      struct {
         GroupId          groupId;
         Permission       allowInsert;
      }values;
   }readGroupAllowInsert;

   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         ServerId         serverId;
         SessionDetails   details;
      }values;
   }insertSession;

   struct {
      SQLHSTMT stmt;
      struct {
         ServerId         serverId;
         SubscriberSuffix suffix;
      }values;
   }updateServerNoOfInsert;

   /*----------------*/
   /* Transaction T5 */
   /*----------------*/
   struct {
      SQLHSTMT stmt;
      struct {
         GroupId          groupId;
         Permission       allowDelete;
      }values;
   }readGroupAllowDelete;

   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         ServerId         serverId;
      }values;
   }deleteSession;

   struct {
      SQLHSTMT stmt;
      struct {
         ServerId         serverId;
         SubscriberSuffix suffix;
      }values;
   }updateServerNoOfDelete;

   /*--------------------------*/
   /* Transaction T3 + T4 + T5 */
   /*--------------------------*/
   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         uint32           activeSessions;
         GroupId          groupId;
         ChangedBy        changedBy;
         ChangedTime      changedTime;
      }values;
   }readSubscriberSession;

   struct {
      SQLHSTMT stmt;
      struct {
         SubscriberNumber number;
         uint32           activeSessions;
      }values;
   }updateSubscriberSession;
};

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/


#endif /* USERHANDLE_H */

