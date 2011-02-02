#ifndef UNIREG_INCLUDED
#define UNIREG_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


#include "my_global.h"                          /* ulonglong */
#include "mysql_version.h"                      /* FRM_VER */

/*  Extra functions used by unireg library */

typedef struct st_ha_create_information HA_CREATE_INFO;

#ifndef NO_ALARM_LOOP
#define NO_ALARM_LOOP		/* lib5 and popen can't use alarm */
#endif

/* These paths are converted to other systems (WIN95) before use */

#define LANGUAGE	"english/"
#define ERRMSG_FILE	"errmsg.sys"
#define TEMP_PREFIX	"MY"
#define LOG_PREFIX	"ML"
#define PROGDIR		"bin/"
#ifndef MYSQL_DATADIR
#define MYSQL_DATADIR		"data/"
#endif
#ifndef SHAREDIR
#define SHAREDIR	"share/"
#endif
#ifndef PLUGINDIR
#define PLUGINDIR	"lib/plugin"
#endif

#define CURRENT_THD_ERRMSGS current_thd->variables.lc_messages->errmsgs->errmsgs
#define DEFAULT_ERRMSGS     my_default_lc_messages->errmsgs->errmsgs

#define ER(X)         CURRENT_THD_ERRMSGS[(X) - ER_ERROR_FIRST]
#define ER_DEFAULT(X) DEFAULT_ERRMSGS[(X) - ER_ERROR_FIRST]
#define ER_SAFE(X) (((X) >= ER_ERROR_FIRST && (X) <= ER_ERROR_LAST) ? ER(X) : "Invalid error code")
#define ER_THD(thd,X) ((thd)->variables.lc_messages->errmsgs->errmsgs[(X) - \
                       ER_ERROR_FIRST])
#define ER_THD_OR_DEFAULT(thd,X) ((thd) ? ER_THD(thd, X) : ER_DEFAULT(X))


#define ME_INFO (ME_HOLDTANG+ME_OLDWIN+ME_NOREFRESH)
#define ME_ERROR (ME_BELL+ME_OLDWIN+ME_NOREFRESH)
#define MYF_RW MYF(MY_WME+MY_NABP)		/* Vid my_read & my_write */

#define SPECIAL_USE_LOCKS	1		/* Lock used databases */
#define SPECIAL_NO_NEW_FUNC	2		/* Skip new functions */
#define SPECIAL_SKIP_SHOW_DB    4               /* Don't allow 'show db' */
#define SPECIAL_WAIT_IF_LOCKED	8		/* Wait if locked database */
#define SPECIAL_SAME_DB_NAME   16		/* form name = file name */
#define SPECIAL_ENGLISH        32		/* English error messages */
#define SPECIAL_NO_RESOLVE     64		/* Don't use gethostname */
#define SPECIAL_NO_PRIOR	128		/* Obsolete */
#define SPECIAL_BIG_SELECTS	256		/* Don't use heap tables */
#define SPECIAL_NO_HOST_CACHE	512		/* Don't cache hosts */
#define SPECIAL_SHORT_LOG_FORMAT 1024
#define SPECIAL_SAFE_MODE	2048
#define SPECIAL_LOG_QUERIES_NOT_USING_INDEXES 4096 /* Obsolete */

	/* Extern defines */
#define store_record(A,B) memcpy((A)->B,(A)->record[0],(size_t) (A)->s->reclength)
#define restore_record(A,B) memcpy((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define cmp_record(A,B) memcmp((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define empty_record(A) { \
                          restore_record((A),s->default_values); \
                          bfill((A)->null_flags,(A)->s->null_bytes,255);\
                        }

	/* Defines for use with openfrm, openprt and openfrd */

#define READ_ALL		1	/* openfrm: Read all parameters */
#define CHANGE_FRM		2	/* openfrm: open .frm as O_RDWR */
#define READ_KEYINFO		4	/* L{s nyckeldata fr}n filen */
#define EXTRA_RECORD		8	/* Reservera plats f|r extra record */
#define DONT_OPEN_TABLES	8	/* Don't open database-files (frd) */
#define DONT_OPEN_MASTER_REG	16	/* Don't open first reg-file (prt) */
#define EXTRA_LONG_RECORD	16	/* Plats f|r dubbel s|k-record */
#define COMPUTE_TYPES		32	/* Kontrollera type f|r f{ltena */
#define SEARCH_PRG		64	/* S|k efter registret i 'prg_dev' */
#define READ_USED_NAMES		128	/* L{s anv{nda formul{rnamn */
#define DONT_GIVE_ERROR		256	/* Don't do frm_error on openfrm  */
#define READ_SCREENS		1024	/* Read screens, info and helpfile */
#define DELAYED_OPEN		4096	/* Open table later */
#define OPEN_VIEW		8192	/* Allow open on view */
#define OPEN_VIEW_NO_PARSE     16384    /* Open frm only if it's a view,
                                           but do not parse view itself */
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to open FRM file only to get necessary data.
*/
#define OPEN_FRM_FILE_ONLY     32768
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to process tables only to get necessary data.
  Views are not processed.
*/
#define OPEN_TABLE_ONLY        OPEN_FRM_FILE_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to process views only to get necessary data.
  Tables are not processed.
*/
#define OPEN_VIEW_ONLY         OPEN_TABLE_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine.
  The flag means that we need to open a view using
  open_normal_and_derived_tables() function.
*/
#define OPEN_VIEW_FULL         OPEN_VIEW_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine.
  The flag means that I_S table uses optimization algorithm.
*/
#define OPTIMIZE_I_S_TABLE     OPEN_VIEW_FULL*2

/*
  The flag means that we need to process trigger files only.
*/
#define OPEN_TRIGGER_ONLY      OPTIMIZE_I_S_TABLE*2

#define SC_INFO_LENGTH 4		/* Form format constant */
#define TE_INFO_LENGTH 3
#define MTYP_NOEMPTY_BIT 128

/*
  Minimum length pattern before Turbo Boyer-Moore is used
  for SELECT "text" LIKE "%pattern%", excluding the two
  wildcards in class Item_func_like.
*/
#define MIN_TURBOBM_PATTERN_LEN 3

/* 
   Defines for binary logging.
   Do not decrease the value of BIN_LOG_HEADER_SIZE.
   Do not even increase it before checking code.
*/

#define BIN_LOG_HEADER_SIZE    4 

#define DEFAULT_KEY_CACHE_NAME "default"

/* Include prototypes for unireg */

#include "mysqld_error.h"
#include "structs.h"				/* All structs we need */
#include "sql_list.h"                           /* List<> */
#include "field.h"                              /* Create_field */

bool mysql_create_frm(THD *thd, const char *file_name,
                      const char *db, const char *table,
		      HA_CREATE_INFO *create_info,
		      List<Create_field> &create_field,
		      uint key_count,KEY *key_info,handler *db_type);
int rea_create_table(THD *thd, const char *path,
                     const char *db, const char *table_name,
                     HA_CREATE_INFO *create_info,
  		     List<Create_field> &create_field,
                     uint key_count,KEY *key_info,
                     handler *file);
#endif
