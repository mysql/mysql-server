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

#ifndef NDB_TRIGGER_DEFINITIONS_H
#define NDB_TRIGGER_DEFINITIONS_H

#include <ndb_global.h>
#include "ndb_limits.h"

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

#define ILLEGAL_TRIGGER_ID ((Uint32)(~0))

struct TriggerType {
  enum Value {
    CONSTRAINT            = 0,
    SECONDARY_INDEX       = 1,
    FOREIGN_KEY           = 2,
    SCHEMA_UPGRADE        = 3,
    API_TRIGGER           = 4,
    SQL_TRIGGER           = 5,
    SUBSCRIPTION          = 6,
    READ_ONLY_CONSTRAINT  = 7,
    ORDERED_INDEX         = 8,
    SUBSCRIPTION_BEFORE   = 9
  };
};

struct TriggerActionTime {
  enum Value {
    TA_BEFORE   = 0, /* Immediate, before operation */
    TA_AFTER    = 1, /* Immediate, after operation */
    TA_DEFERRED = 2, /* Before commit */
    TA_DETACHED = 3, /* After commit in a separate transaction, NYI */
    TA_CUSTOM   = 4  /* Hardcoded per TriggerType */
  };
};

struct TriggerEvent {
  enum Value {
    TE_INSERT = 0,
    TE_DELETE = 1,
    TE_UPDATE = 2,
    TE_CUSTOM = 3    /* Hardcoded per TriggerType */
  };
};

#endif
