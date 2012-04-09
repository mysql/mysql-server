/*
   Copyright (C) 2005, 2006 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SCHEMA_H
#define NDB_SCHEMA_H

#include "testDefinitions.h"

#define SUBSCRIBER_TABLE        "SUBSCRIBER"
#define SUBSCRIBER_NUMBER       "NUMBER"
#define SUBSCRIBER_LOCATION     "LOCATION"
#define SUBSCRIBER_NAME         "NAME"
#define SUBSCRIBER_GROUP        "GROUP_ID"
#define SUBSCRIBER_SESSIONS     "SESSIONS"
#define SUBSCRIBER_CHANGED_BY   "CHANGED_BY"
#define SUBSCRIBER_CHANGED_TIME "CHANGED_TIME"

#define SERVER_TABLE             "SERVER"
#define SERVER_ID                "SERVER_ID"
#define SERVER_SUBSCRIBER_SUFFIX "SUFFIX"
#define SERVER_NAME              "NAME"
#define SERVER_READS             "NO_OF_READ"
#define SERVER_INSERTS           "NO_OF_INSERT"
#define SERVER_DELETES           "NO_OF_DELETE"

#undef GROUP_NAME /* Defined in Windows SDK (include\nb30.h), so we use NDB_GROUP_NAME */

#define GROUP_TABLE              "GROUP_T"
#define GROUP_ID                 "GROUP_ID"
#define NDB_GROUP_NAME           "GROUP_NAME"
#define GROUP_ALLOW_READ         "ALLOW_READ"
#define GROUP_ALLOW_INSERT       "ALLOW_INSERT"
#define GROUP_ALLOW_DELETE       "ALLOW_DELETE"

#define SESSION_TABLE            "SESSION"
#define SESSION_SERVER           "SERVER_ID"
#define SESSION_SUBSCRIBER       "NUMBER"
#define SESSION_DATA             "DATA"

/** Numbers */

#define IND_SUBSCRIBER_NUMBER        (unsigned)0
#define IND_SUBSCRIBER_NAME          (unsigned)1
#define IND_SUBSCRIBER_GROUP         (unsigned)2
#define IND_SUBSCRIBER_LOCATION      (unsigned)3
#define IND_SUBSCRIBER_SESSIONS      (unsigned)4
#define IND_SUBSCRIBER_CHANGED_BY    (unsigned)5
#define IND_SUBSCRIBER_CHANGED_TIME  (unsigned)6

#define IND_SERVER_SUBSCRIBER_SUFFIX (unsigned)0
#define IND_SERVER_ID                (unsigned)1
#define IND_SERVER_NAME              (unsigned)2
#define IND_SERVER_READS             (unsigned)3
#define IND_SERVER_INSERTS           (unsigned)4
#define IND_SERVER_DELETES           (unsigned)5

#define IND_GROUP_ID                 (unsigned)0
#define IND_GROUP_NAME               (unsigned)1
#define IND_GROUP_ALLOW_READ         (unsigned)2
#define IND_GROUP_ALLOW_INSERT       (unsigned)3
#define IND_GROUP_ALLOW_DELETE       (unsigned)4

#define IND_SESSION_SUBSCRIBER       (unsigned)0
#define IND_SESSION_SERVER           (unsigned)1
#define IND_SESSION_DATA             (unsigned)2

#endif
