/* Copyright (C) 2007 MySQL AB

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

#ifndef NDBINFO_TABLES_H
#define NDBINFO_TABLES_H

#include <ndbinfo.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Reserved for DBINFO only */
DECLARE_NDBINFO_TABLE(ndbinfo_TABLES,3)
     = {{"TABLES",3,0},
        {
          {"TABLE_ID",  NDBINFO_TYPE_NUMBER},
          {"TABLE_NAME",NDBINFO_TYPE_STRING},
          {"CREATE_SQL",NDBINFO_TYPE_STRING},
        }};

/** Reserved for DBINFO only */
DECLARE_NDBINFO_TABLE(ndbinfo_COLUMNS,4)
     = {{"COLUMNS",4,0},
        {
          {"TABLE_ID",    NDBINFO_TYPE_NUMBER},
          {"COLUMN_ID",   NDBINFO_TYPE_NUMBER},
          {"COLUMN_NAME", NDBINFO_TYPE_STRING},
          {"COLUMN_TYPE", NDBINFO_TYPE_STRING},
        }};

DECLARE_NDBINFO_TABLE(ndbinfo_MEMUSAGE,6)
     = {{"MEMUSAGE",6,0},
        {
          {"RESOURCE_NAME",    NDBINFO_TYPE_STRING},
          {"NODE_ID",          NDBINFO_TYPE_NUMBER},
          {"PAGE_SIZE_KB",     NDBINFO_TYPE_NUMBER},
          {"PAGES_USED",       NDBINFO_TYPE_NUMBER},
          {"PAGES_TOTAL",      NDBINFO_TYPE_NUMBER},
          {"BLOCK",            NDBINFO_TYPE_STRING},
        }};

DECLARE_NDBINFO_TABLE(ndbinfo_LOGDESTINATION,5) =
{{"LOGDESTINATION",5,0},
 {
   {"NODE_ID",NDBINFO_TYPE_NUMBER},
   {"TYPE",NDBINFO_TYPE_STRING},
   {"PARAMS",NDBINFO_TYPE_STRING},
   {"CURRENT_SIZE",NDBINFO_TYPE_NUMBER},
   {"MAX_SIZE",NDBINFO_TYPE_NUMBER},
 }
};

DECLARE_NDBINFO_TABLE(ndbinfo_BACKUP_RECORDS,11)
= {{"BACKUP_RECORDS",11,0},
   {
     {"NODE_ID",          NDBINFO_TYPE_NUMBER},
     {"BACKUP_RECORD",    NDBINFO_TYPE_NUMBER},
     {"BACKUP_ID",        NDBINFO_TYPE_NUMBER},
     {"MASTER_REF",       NDBINFO_TYPE_NUMBER},
     {"CLIENT_REF",       NDBINFO_TYPE_NUMBER},
     {"STATE",            NDBINFO_TYPE_NUMBER},
     {"BYTES",            NDBINFO_TYPE_NUMBER},
     {"RECORDS",          NDBINFO_TYPE_NUMBER},
     {"LOG_BYTES",        NDBINFO_TYPE_NUMBER},
     {"LOG_RECORDS",      NDBINFO_TYPE_NUMBER},
     {"ERROR_CODE",       NDBINFO_TYPE_NUMBER},
   }
};

DECLARE_NDBINFO_TABLE(ndbinfo_BACKUP_PARAMETERS,14)
= {{"BACKUP_PARAMETERS",14,0},
   {
     {"NODE_ID",                  NDBINFO_TYPE_NUMBER},
     {"CURRENT_DISK_WRITE_SPEED", NDBINFO_TYPE_NUMBER},
     {"BYTES_WRITTEN_THIS_PERIOD",NDBINFO_TYPE_NUMBER},
     {"OVERFLOW_DISK_WRITE",      NDBINFO_TYPE_NUMBER},
     {"RESET_DELAY_USED",         NDBINFO_TYPE_NUMBER},
     {"RESET_DISK_SPEED_TIME",    NDBINFO_TYPE_NUMBER},
     {"BACKUP_POOL_SIZE",         NDBINFO_TYPE_NUMBER},
     {"BACKUP_FILE_POOL_SIZE",    NDBINFO_TYPE_NUMBER},
     {"TABLE_POOL_SIZE",          NDBINFO_TYPE_NUMBER},
     {"TRIGGER_POOL_SIZE",        NDBINFO_TYPE_NUMBER},
     {"FRAGMENT_POOL_SIZE",       NDBINFO_TYPE_NUMBER},
     {"PAGE_POOL_SIZE",           NDBINFO_TYPE_NUMBER},
     {"COMPRESSED_BACKUP",        NDBINFO_TYPE_NUMBER},
     {"COMPRESSED_LCP",           NDBINFO_TYPE_NUMBER},
   }
};

DECLARE_NDBINFO_TABLE(ndbinfo_POOLS,5)
= {{"POOLS",5 ,0},
   {
     {"NODE_ID",                  NDBINFO_TYPE_NUMBER},
     {"BLOCK",                    NDBINFO_TYPE_STRING},
     {"POOL_NAME",                NDBINFO_TYPE_STRING},
     {"FREE",                     NDBINFO_TYPE_NUMBER},
     {"SIZE",                     NDBINFO_TYPE_NUMBER},
   }
};

static Uint32 number_ndbinfo_tables= 7;

#define DBINFOTBL(x) (struct ndbinfo_table*)&(x).t

struct ndbinfo_table *ndbinfo_tables[] = {
  DBINFOTBL(ndbinfo_TABLES),
  DBINFOTBL(ndbinfo_COLUMNS),
  DBINFOTBL(ndbinfo_MEMUSAGE),
  DBINFOTBL(ndbinfo_LOGDESTINATION),
  DBINFOTBL(ndbinfo_BACKUP_RECORDS),
  DBINFOTBL(ndbinfo_BACKUP_PARAMETERS),
  DBINFOTBL(ndbinfo_POOLS),
};

#ifdef __cplusplus
}
#endif

#endif
