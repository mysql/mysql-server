/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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


/*  Extra functions used by unireg library */

#ifndef _unireg_h

#ifndef NO_ALARM_LOOP
#define NO_ALARM_LOOP		/* lib5 and popen can't use alarm */
#endif

/* These paths are converted to other systems (WIN95) before use */

#define LANGUAGE	"english/"
#define ERRMSG_FILE	"errmsg.sys"
#define TEMP_PREFIX	"MY"
#define LOG_PREFIX	"ML"
#define PROGDIR		"bin/"
#ifndef DATADIR
#define DATADIR		"data/"
#endif
#ifndef SHAREDIR
#define SHAREDIR	"share/"
#endif

#define ER(X) ((X) >= 1000 && (X) < ER_ERROR_MESSAGES + 1000) ? \
 errmesg[(X)-1000] : "Invalid error code"

#define ERRMAPP 1				/* Errormap f|r my_error */
#define LIBLEN FN_REFLEN-FN_LEN			/* Max l{ngd p} dev */
#define MAX_DBKEY_LENGTH (FN_LEN*2+6)           /* extra 4 bytes for slave tmp
						 * tables */
#define MAX_FIELD_NAME 34			/* Max colum name length +2 */
#define MAX_KEY 32				/* Max used keys */
#define MAX_REF_PARTS 16			/* Max parts used as ref */
#define MAX_KEY_LENGTH 500			/* max possible key */
#if SIZEOF_OFF_T > 4
#define MAX_REFLENGTH 8				/* Max length for record ref */
#else
#define MAX_REFLENGTH 4				/* Max length for record ref */
#endif

#define MAX_FIELD_WIDTH 256			/* Max column width +1 */
#define MAX_TABLES	(sizeof(table_map)*8-1)	/* Max tables in join */
#define RAND_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-1))
#define MAX_FIELDS	4096			/* Limit in the .frm file */

#define MAX_SORT_MEMORY (2048*1024-MALLOC_OVERHEAD)
#define MIN_SORT_MEMORY (32*1024-MALLOC_OVERHEAD)
#define EXTRA_RECORDS	10			/* Extra records in sort */
#define SCROLL_EXTRA	5			/* Extra scroll-rows. */
#define FIELD_NAME_USED ((uint) 32768)		/* Bit set if fieldname used */
#define FORM_NAME_USED	((uint) 16384)		/* Bit set if formname used */
#define FIELD_NR_MASK	16383			/* To get fieldnumber */
#define FERR		-1			/* Error from my_functions */
#define CREATE_MODE	0			/* Default mode on new files */
#define NAMES_SEP_CHAR	'\377'			/* Char to sep. names */
#ifdef MSDOS
#define EXTRA_FIELD_CHAR (char) '\234'		/* Interchangebly with '#' */
#else
#define EXTRA_FIELD_CHAR '#'			/* Interchangebly with '#' */
#endif

#define READ_RECORD_BUFFER	(uint) (IO_SIZE*8) /* Pointer_buffer_size */
#define DISK_BUFFER_SIZE	(uint) (IO_SIZE*16) /* Size of diskbuffer */
#define POSTFIX_ERROR		DBL_MAX

#define ME_INFO (ME_HOLDTANG+ME_OLDWIN+ME_NOREFRESH)
#define ME_ERROR (ME_BELL+ME_OLDWIN+ME_NOREFRESH)
#define MYF_RW MYF(MY_WME+MY_NABP)		/* Vid my_read & my_write */

#define SPECIAL_USE_LOCKS	1		/* Lock used databases */
#define SPECIAL_NO_NEW_FUNC	2		/* Skipp new functions */
#define SPECIAL_NEW_FUNC	4		/* New nonstandard functions */
#define SPECIAL_WAIT_IF_LOCKED	8		/* Wait if locked database */
#define SPECIAL_SAME_DB_NAME   16		/* form name = file name */
#define SPECIAL_ENGLISH        32		/* English error messages */
#define SPECIAL_NO_RESOLVE     64		/* Don't use gethostname */
#define SPECIAL_NO_PRIOR	128		/* Don't prioritize threads */
#define SPECIAL_BIG_SELECTS	256		/* Don't use heap tables */
#define SPECIAL_NO_HOST_CACHE	512		/* Don't cache hosts */
#define SPECIAL_LONG_LOG_FORMAT 1024
#define SPECIAL_SAFE_MODE	2048
#define SPECIAL_SKIP_SHOW_DB	4096		/* Don't allow 'show db' */

	/* Extern defines */
#define store_record(A,B) bmove_allign((A)->record[B],(A)->record[0],(size_t) (A)->reclength)
#define restore_record(A,B) bmove_allign((A)->record[0],(A)->record[B],(size_t) (A)->reclength)
#define cmp_record(A,B) memcmp((A)->record[0],(A)->record[B],(size_t) (A)->reclength)
#define empty_record(A) { \
bmove_allign((A)->record[0],(A)->record[2],(size_t) (A)->reclength); \
bfill((A)->null_flags,(A)->null_bytes,255);\
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

#define SC_INFO_LENGTH 4		/* Form format constant */
#define TE_INFO_LENGTH 3
#define MTYP_NOEMPTY_BIT 128

	/* Include prototypes for unireg */

#include "mysqld_error.h"
#include "structs.h"				/* All structs we need */

#endif
