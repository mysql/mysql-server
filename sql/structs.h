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

typedef struct st_lex_string
{
  char *str;
  uint length;
} LEX_STRING;

typedef struct st_lex_string_with_init :public st_lex_string
{
  st_lex_string_with_init(const char *str_arg, uint length_arg)
  {
    str= (char*) str_arg;
    length= length_arg;
  }
} LEX_STRING_WITH_INIT;


typedef struct st_date_time_format {
  uchar positions[8];
  char  time_separator;			/* Separator between hour and minute */
  uint flag;				/* For future */
  LEX_STRING format;
} DATE_TIME_FORMAT;


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
  uint	null_offset;			/* Offset to null_bit in record */
  uint16 length;			/* Length of key_part */
  uint16 store_length;
  uint16 key_type;
  uint16 fieldnr;			/* Fieldnum in UNIREG */
  uint8 key_part_flag;			/* 0 or HA_REVERSE_SORT */
  uint8 type;
  uint8 null_bit;			/* Position to null_bit */
} KEY_PART_INFO ;


typedef struct st_key {
  uint	key_length;			/* Tot length of key */
  uint	flags;				/* dupp key and pack flags */
  uint	key_parts;			/* How many key_parts */
  uint  extra_length;
  uint	usable_key_parts;		/* Should normally be = key_parts */
  enum  ha_key_alg algorithm;
  KEY_PART_INFO *key_part;
  char	*name;				/* Name of key */
  /*
    Array of AVG(#records with the same field value) for 1st ... Nth key part.
    0 means 'not known'.
    For temporary heap tables this member is NULL.
  */
  ulong *rec_per_key;
  union {
    int  bdb_return_if_eq;
  } handler;
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
  byte *rec_buf;                /* to read field values  after filesort */
  byte	*cache,*cache_pos,*cache_end,*read_positions;
  IO_CACHE *io_cache;
  bool print_error, ignore_not_found_rows;
} READ_RECORD;


/*
  Originally MySQL used TIME structure inside server only, but since
  4.1 it's exported to user in the new client API. Define aliases for
  new names to keep existing code simple.
*/

typedef struct st_mysql_time TIME;
typedef enum enum_mysql_timestamp_type timestamp_type;


typedef struct {
  ulong year,month,day,hour;
  ulonglong minute,second,second_part;
  bool neg;
} INTERVAL;


typedef struct st_known_date_time_format {
  const char *format_name;
  const char *date_format;
  const char *datetime_format;
  const char *time_format;
} KNOWN_DATE_TIME_FORMAT;


enum SHOW_TYPE
{
  SHOW_UNDEF,
  SHOW_LONG, SHOW_LONGLONG, SHOW_INT, SHOW_CHAR, SHOW_CHAR_PTR, SHOW_BOOL,
  SHOW_MY_BOOL, SHOW_OPENTABLES, SHOW_STARTTIME, SHOW_QUESTION,
  SHOW_LONG_CONST, SHOW_INT_CONST, SHOW_HAVE, SHOW_SYS, SHOW_HA_ROWS,
#ifdef HAVE_OPENSSL
  SHOW_SSL_CTX_SESS_ACCEPT, 	SHOW_SSL_CTX_SESS_ACCEPT_GOOD,
  SHOW_SSL_GET_VERSION, 	SHOW_SSL_CTX_GET_SESSION_CACHE_MODE,
  SHOW_SSL_CTX_SESS_CB_HITS, 	SHOW_SSL_CTX_SESS_ACCEPT_RENEGOTIATE,
  SHOW_SSL_CTX_SESS_NUMBER, 	SHOW_SSL_SESSION_REUSED,
  SHOW_SSL_CTX_SESS_GET_CACHE_SIZE, SHOW_SSL_GET_CIPHER,
  SHOW_SSL_GET_DEFAULT_TIMEOUT,	SHOW_SSL_GET_VERIFY_MODE,
  SHOW_SSL_CTX_GET_VERIFY_MODE, SHOW_SSL_GET_VERIFY_DEPTH,
  SHOW_SSL_CTX_GET_VERIFY_DEPTH, SHOW_SSL_CTX_SESS_CONNECT,
  SHOW_SSL_CTX_SESS_CONNECT_RENEGOTIATE, SHOW_SSL_CTX_SESS_CONNECT_GOOD,
  SHOW_SSL_CTX_SESS_HITS, SHOW_SSL_CTX_SESS_MISSES,
  SHOW_SSL_CTX_SESS_TIMEOUTS, SHOW_SSL_CTX_SESS_CACHE_FULL,
  SHOW_SSL_GET_CIPHER_LIST,
#endif /* HAVE_OPENSSL */
  SHOW_RPL_STATUS, SHOW_SLAVE_RUNNING,
  SHOW_KEY_CACHE_LONG, SHOW_KEY_CACHE_CONST_LONG
};

enum SHOW_COMP_OPTION { SHOW_OPTION_YES, SHOW_OPTION_NO, SHOW_OPTION_DISABLED};

extern const char *show_comp_option_name[];

typedef int *(*update_var)(THD *, struct show_var_st *);


typedef struct show_var_st {
  const char *name;
  char *value;
  SHOW_TYPE type;
} SHOW_VAR;


typedef struct	st_lex_user {
  LEX_STRING user, host, password;
} LEX_USER;


typedef struct user_resources {
  uint questions, updates, connections, bits;
} USER_RESOURCES;

typedef struct  user_conn {
  char *user, *host;
  uint len, connections, conn_per_hour, updates, questions, user_len;
  USER_RESOURCES user_resources;
  time_t intime;
} USER_CONN;
	/* Bits in form->update */
#define REG_MAKE_DUPP		1	/* Make a copy of record when read */
#define REG_NEW_RECORD		2	/* Write a new record if not found */
#define REG_UPDATE		4	/* Uppdate record */
#define REG_DELETE		8	/* Delete found record */
#define REG_PROG		16	/* User is updating database */
#define REG_CLEAR_AFTER_WRITE	32
#define REG_MAY_BE_UPDATED	64
#define REG_AUTO_UPDATE		64	/* Used in D-forms for scroll-tables */
#define REG_OVERWRITE		128
#define REG_SKIP_DUP		256

	/* Bits in form->status */
#define STATUS_NO_RECORD	(1+2)	/* Record isn't usably */
#define STATUS_GARBAGE		1
#define STATUS_NOT_FOUND	2	/* No record in database when needed */
#define STATUS_NO_PARENT	4	/* Parent record wasn't found */
#define STATUS_NOT_READ		8	/* Record isn't read */
#define STATUS_UPDATED		16	/* Record is updated by formula */
#define STATUS_NULL_ROW		32	/* table->null_row is set */
#define STATUS_DELETED		64
