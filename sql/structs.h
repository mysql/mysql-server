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
  uint16 length;                        /* Length of keypart value in bytes */
  /* 
    Number of bytes required to store the keypart value. This may be
    different from the "length" field as it also counts
     - possible NULL-flag byte (see HA_KEY_NULL_LENGTH)
     - possible HA_KEY_BLOB_LENGTH bytes needed to store actual value length.
  */
  uint16 store_length;
  uint16 key_type;
  uint16 fieldnr;			/* Fieldnum in UNIREG */
  uint16 key_part_flag;			/* 0 or HA_REVERSE_SORT */
  uint8 type;
  uint8 null_bit;			/* Position to null_bit */
} KEY_PART_INFO ;


typedef struct st_key {
  uint	key_length;			/* Tot length of key */
  uint	flags;				/* dupp key and pack flags */
  uint	key_parts;			/* How many key_parts */
  uint  extra_length;
  uint	usable_key_parts;		/* Should normally be = key_parts */
  uint  block_size;
  enum  ha_key_alg algorithm;
  /*
    Note that parser is used when the table is opened for use, and
    parser_name is used when the table is being created.
  */
  union
  {
    struct st_plugin_int *parser;       /* Fulltext [pre]parser */
    LEX_STRING *parser_name;            /* Fulltext [pre]parser name */
  };
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
  struct st_table *table;
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

enum SHOW_COMP_OPTION { SHOW_OPTION_YES, SHOW_OPTION_NO, SHOW_OPTION_DISABLED};

extern const char *show_comp_option_name[];

typedef int *(*update_var)(THD *, struct st_mysql_show_var *);

typedef struct	st_lex_user {
  LEX_STRING user, host, password;
} LEX_USER;

/*
  This structure specifies the maximum amount of resources which
  can be consumed by each account. Zero value of a member means
  there is no limit.
*/
typedef struct user_resources {
  /* Maximum number of queries/statements per hour. */
  uint questions;
  /*
     Maximum number of updating statements per hour (which statements are
     updating is defined by sql_command_flags array).
  */
  uint updates;
  /* Maximum number of connections established per hour. */
  uint conn_per_hour;
  /* Maximum number of concurrent connections. */
  uint user_conn;
  /*
     Values of this enum and specified_limits member are used by the
     parser to store which user limits were specified in GRANT statement.
  */
  enum {QUERIES_PER_HOUR= 1, UPDATES_PER_HOUR= 2, CONNECTIONS_PER_HOUR= 4,
        USER_CONNECTIONS= 8};
  uint specified_limits;
} USER_RESOURCES;


/*
  This structure is used for counting resources consumed and for checking
  them against specified user limits.
*/
typedef struct  user_conn {
  /*
     Pointer to user+host key (pair separated by '\0') defining the entity
     for which resources are counted (By default it is user account thus
     priv_user/priv_host pair is used. If --old-style-user-limits option
     is enabled, resources are counted for each user+host separately).
  */
  char *user;
  /* Pointer to host part of the key. */
  char *host;
  /* Total length of the key. */
  uint len;
  /* Current amount of concurrent connections for this account. */
  uint connections;
  /*
     Current number of connections per hour, number of updating statements
     per hour and total number of statements per hour for this account.
  */
  uint conn_per_hour, updates, questions;
  /* Maximum amount of resources which account is allowed to consume. */
  USER_RESOURCES user_resources;
  /*
     The moment of time when per hour counters were reset last time
     (i.e. start of "hour" for conn_per_hour, updates, questions counters).
  */
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

/*
  Such interval is "discrete": it is the set of
  { auto_inc_interval_min + k * increment,
    0 <= k <= (auto_inc_interval_values-1) }
  Where "increment" is maintained separately by the user of this class (and is
  currently only thd->variables.auto_increment_increment).
  It mustn't derive from Sql_alloc, because SET INSERT_ID needs to
  allocate memory which must stay allocated for use by the next statement.
*/
class Discrete_interval {
private:
  ulonglong interval_min;
  ulonglong interval_values;
  ulonglong  interval_max;    // excluded bound. Redundant.
public:
  Discrete_interval *next;    // used when linked into Discrete_intervals_list
  void replace(ulonglong start, ulonglong val, ulonglong incr)
  {
    interval_min=    start;
    interval_values= val;
    interval_max=    (val == ULONGLONG_MAX) ? val : start + val * incr;
  }
  Discrete_interval(ulonglong start, ulonglong val, ulonglong incr) :
    next(NULL) { replace(start, val, incr); };
  Discrete_interval() : next(NULL) { replace(0, 0, 0); };
  ulonglong minimum() const { return interval_min;    };
  ulonglong values()  const { return interval_values; };
  ulonglong maximum() const { return interval_max;    };
  /*
    If appending [3,5] to [1,2], we merge both in [1,5] (they should have the
    same increment for that, user of the class has to ensure that). That is
    just a space optimization. Returns 0 if merge succeeded.
  */
  bool merge_if_contiguous(ulonglong start, ulonglong val, ulonglong incr)
  {
    if (interval_max == start)
    {
      if (val == ULONGLONG_MAX)
      {
        interval_values=   interval_max= val;
      }
      else
      {
        interval_values+=  val;
        interval_max=      start + val * incr;
      }
      return 0;
    }
    return 1;
  };
};

/* List of Discrete_interval objects */
class Discrete_intervals_list {
private:
  Discrete_interval        *head;
  Discrete_interval        *tail;
  /*
    When many intervals are provided at the beginning of the execution of a
    statement (in a replication slave or SET INSERT_ID), "current" points to
    the interval being consumed by the thread now (so "current" goes from
    "head" to "tail" then to NULL).
  */
  Discrete_interval        *current;
  uint                  elements; // number of elements
public:
  Discrete_intervals_list() : head(NULL), current(NULL), elements(0) {};
  void empty_no_free()
  {
    head= current= NULL;
    elements= 0;
  }
  void empty()
  {
    for (Discrete_interval *i= head; i;)
    {
      Discrete_interval *next= i->next;
      delete i;
      i= next;
    }
    empty_no_free();
  }
  const Discrete_interval* get_next()
  {
    Discrete_interval *tmp= current;
    if (current != NULL)
      current= current->next;
    return tmp;
  }
  ~Discrete_intervals_list() { empty(); };
  bool append(ulonglong start, ulonglong val, ulonglong incr);
  ulonglong minimum()     const { return (head ? head->minimum() : 0); };
  ulonglong maximum()     const { return (head ? tail->maximum() : 0); };
  uint      nb_elements() const { return elements; }
};
