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


/* The old structures from unireg */

struct st_table;
class Field;

typedef struct st_date_format {		/* How to print date */
  uint pos[6];				/* Positions to YY.MM.DD HH:MM:SS */
} DATE_FORMAT;


typedef struct st_keyfile_info {	/* used with ha_info() */
  byte ref[MAX_REFLENGTH];		/* Pointer to current row */
  byte dupp_ref[MAX_REFLENGTH];		/* Pointer to dupp row */
  uint ref_length;			/* Length of ref (1-8) */
  uint block_size;			/* index block size */
  File filenr;				/* (uniq) filenr for table */
  ha_rows records;			/* Records i datafilen */
  ha_rows deleted;			/* Deleted records */
  ulonglong data_file_length;		/* Length off data file */
  ulonglong max_data_file_length;	/* Length off data file */
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;		/* Free bytes */
  ulonglong auto_increment_value;
  int errkey,sortkey;			/* Last errorkey and sorted by */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;
  ulong mean_rec_length;		/* physical reclength */
} KEYFILE_INFO;


typedef struct st_key_part_info {	/* Info about a key part */
  Field *field;
  uint	offset;				/* offset in record (from 0) */
  uint	null_offset;			// Offset to null_bit in record
  uint16 length;			/* Length of key_part */
  uint16 store_length;
  uint16 key_type;
  uint16 fieldnr;			/* Fieldnum in UNIREG */
  uint8 key_part_flag;			/* 0 or HA_REVERSE_SORT */
  uint8 type;
  uint8 null_bit;			// Position to null_bit
} KEY_PART_INFO ;


typedef struct st_key {
  uint	key_length;			/* Tot length of key */
  uint	flags;				/* dupp key and pack flags */
  uint	key_parts;			/* How many key_parts */
  uint  extra_length;
  uint	usable_key_parts;		/* Should normally be = key_parts */
  KEY_PART_INFO *key_part;
  char	*name;				/* Name of key */
  ulong *rec_per_key;			/* Key part distribution */
} KEY;


struct st_join_table;

typedef struct st_reginfo {		/* Extra info about reg */
  struct st_join_table *join_tab;	/* Used by SELECT() */
  enum thr_lock_type lock_type;		/* How database is used */
  bool not_exists_optimize;
  bool impossible_range;
} REGINFO;


struct st_read_record;				/* For referense later */
class SQL_SELECT;
class THD;
class handler;

typedef struct st_read_record {			/* Parameter to read_record */
  struct st_table *table;			/* Head-form */
  handler *file;
  struct st_table **forms;			/* head and ref forms */
  int (*read_record)(struct st_read_record *);
  THD *thd;
  SQL_SELECT *select;
  uint cache_records;
  uint ref_length,struct_length,reclength,rec_cache_size,error_offset;
  uint index;
  byte *ref_pos;				/* pointer to form->refpos */
  byte *record;
  byte	*cache,*cache_pos,*cache_end,*read_positions;
  IO_CACHE *io_cache;
  bool print_error;
} READ_RECORD;

enum timestamp_type { TIMESTAMP_NONE, TIMESTAMP_DATE, TIMESTAMP_FULL,
		      TIMESTAMP_TIME };

typedef struct st_time {
  uint year,month,day,hour,minute,second,second_part;
  bool neg;
  timestamp_type time_type;
} TIME;

typedef struct {
  long year,month,day,hour,minute,second,second_part;
  bool neg;
} INTERVAL;


enum SHOW_TYPE { SHOW_LONG,SHOW_CHAR,SHOW_INT,SHOW_CHAR_PTR,SHOW_BOOL,
		 SHOW_MY_BOOL,SHOW_OPENTABLES,SHOW_STARTTIME,SHOW_QUESTION,
		 SHOW_LONG_CONST, SHOW_INT_CONST};

struct show_var_st {
  const char *name;
  char *value;
  SHOW_TYPE type;
};

typedef struct lex_string {
  char *str;
  uint length;
} LEX_STRING;

typedef struct	st_lex_user {
  LEX_STRING user, host, password;
} LEX_USER;

	/* Bits in form->update */
#define REG_MAKE_DUPP		1	/* Make a copy of record when read */
#define REG_NEW_RECORD		2	/* Write a new record if not found */
#define REG_UPDATE		4	/* Uppdate record */
#define REG_DELETE		8	/* Delete found record */
#define REG_PROG		16	/* User is updateing database */
#define REG_CLEAR_AFTER_WRITE	32
#define REG_MAY_BE_UPDATED	64
#define REG_AUTO_UPDATE		64	/* Used in D-forms for scroll-tables */
#define REG_OVERWRITE		128
#define REG_SKIPP_DUPP		256

	/* Bits in form->status */
#define STATUS_NO_RECORD	(1+2)	/* Record isn't usably */
#define STATUS_GARBAGE		1
#define STATUS_NOT_FOUND	2	/* No record in database when neaded */
#define STATUS_NO_PARENT	4	/* Parent record wasn't found */
#define STATUS_NOT_READ		8	/* Record isn't read */
#define STATUS_UPDATED		16	/* Record is updated by formula */
