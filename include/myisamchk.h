/* Copyright (C) 2006 MySQL AB

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

/* Definitions needed for myisamchk/mariachk.c */

/*
  Entries marked as "QQ to be removed" are NOT used to
  pass check/repair options to xxx_check.c. They are used
  internally by xxxchk.c or/and ha_xxxx.cc and should NOT
  be stored together with other flags. They should be removed
  from the following list to make addition of new flags possible.
*/

#ifndef _myisamchk_h
#define _myisamchk_h

#define T_AUTO_INC              1
#define T_AUTO_REPAIR           2		/* QQ to be removed */
#define T_BACKUP_DATA           4
#define T_CALC_CHECKSUM         8
#define T_CHECK                 16		/* QQ to be removed */
#define T_CHECK_ONLY_CHANGED    32		/* QQ to be removed */
#define T_CREATE_MISSING_KEYS   64
#define T_DESCRIPT              128
#define T_DONT_CHECK_CHECKSUM   256
#define T_EXTEND                512
#define T_FAST                  (1L << 10)	/* QQ to be removed */
#define T_FORCE_CREATE          (1L << 11)	/* QQ to be removed */
#define T_FORCE_UNIQUENESS      (1L << 12)
#define T_INFO                  (1L << 13)
#define T_MEDIUM                (1L << 14)
#define T_QUICK                 (1L << 15)	/* QQ to be removed */
#define T_READONLY              (1L << 16)	/* QQ to be removed */
#define T_REP                   (1L << 17)
#define T_REP_BY_SORT           (1L << 18)	/* QQ to be removed */
#define T_REP_PARALLEL          (1L << 19)	/* QQ to be removed */
#define T_RETRY_WITHOUT_QUICK   (1L << 20)
#define T_SAFE_REPAIR           (1L << 21)
#define T_SILENT                (1L << 22)
#define T_SORT_INDEX            (1L << 23)	/* QQ to be removed */
#define T_SORT_RECORDS          (1L << 24)	/* QQ to be removed */
#define T_STATISTICS            (1L << 25)
#define T_UNPACK                (1L << 26)
#define T_UPDATE_STATE          (1L << 27)
#define T_VERBOSE               (1L << 28)
#define T_VERY_SILENT           (1L << 29)
#define T_WAIT_FOREVER          (1L << 30)
#define T_WRITE_LOOP            ((ulong) 1L << 31)

#define T_REP_ANY               (T_REP | T_REP_BY_SORT | T_REP_PARALLEL)

/*
  Flags used by xxxxchk.c or/and ha_xxxx.cc that are NOT passed
  to xxxcheck.c follows:
*/

#define TT_USEFRM               1
#define TT_FOR_UPGRADE          2

#define O_NEW_INDEX	1			/* Bits set in out_flag */
#define O_NEW_DATA	2
#define O_DATA_LOST	4

typedef struct st_sort_key_blocks		/* Used when sorting */
{
  uchar *buff, *end_pos;
  uchar lastkey[HA_MAX_POSSIBLE_KEY_BUFF];
  uint last_length;
  int inited;
} SORT_KEY_BLOCKS;


/* 
  MARIA/MYISAM supports several statistics collection
  methods. Currently statistics collection method is not stored in
  MARIA file and has to be specified for each table analyze/repair
  operation in MI_CHECK::stats_method.
*/

typedef enum
{
  /* Treat NULLs as inequal when collecting statistics (default for 4.1/5.0) */
  MI_STATS_METHOD_NULLS_NOT_EQUAL,
  /* Treat NULLs as equal when collecting statistics (like 4.0 did) */
  MI_STATS_METHOD_NULLS_EQUAL,
  /* Ignore NULLs - count only tuples without NULLs in the index components */
  MI_STATS_METHOD_IGNORE_NULLS
} enum_handler_stats_method;


typedef struct st_handler_check_param
{
  char *isam_file_name;
  MY_TMPDIR *tmpdir;
  void *thd;
  const char *db_name, *table_name, *op_name;
  ulonglong auto_increment_value;
  ulonglong max_data_file_length;
  ulonglong keys_in_use;
  ulonglong max_record_length;
  /* 
     The next two are used to collect statistics, see update_key_parts for
     description.
  */
  ulonglong unique_count[HA_MAX_KEY_SEG + 1];
  ulonglong notnull_count[HA_MAX_KEY_SEG + 1];

  my_off_t search_after_block;
  my_off_t new_file_pos, key_file_blocks;
  my_off_t keydata, totaldata, key_blocks, start_check_pos;
  ha_rows total_records, total_deleted;
  ha_checksum record_checksum, glob_crc;
  ha_checksum key_crc[HA_MAX_POSSIBLE_KEY];
  ulong use_buffers, read_buffer_length, write_buffer_length;
  ulong sort_buffer_length, sort_key_blocks;
  ulong rec_per_key_part[HA_MAX_KEY_SEG * HA_MAX_POSSIBLE_KEY];
  uint out_flag, warning_printed, error_printed, verbose;
  uint opt_sort_key, total_files, max_level;
  uint testflag, key_cache_block_size;
  int tmpfile_createflag;
  myf myf_rw;
  uint8 language;
  my_bool using_global_keycache, opt_lock_memory, opt_follow_links;
  my_bool retry_repair, force_sort, calc_checksum;
  char temp_filename[FN_REFLEN];
  IO_CACHE read_cache;
  enum_handler_stats_method stats_method;
} HA_CHECK;


typedef struct st_sort_ftbuf
{
  uchar *buf, *end;
  int count;
  uchar lastkey[HA_MAX_KEY_BUFF];
} SORT_FT_BUF;


typedef struct st_buffpek {
  my_off_t file_pos;                    /* Where we are in the sort file */
  uchar *base,*key;                     /* Key pointers */
  ha_rows count;                        /* Number of rows in table */
  ulong mem_count;                      /* numbers of keys in memory */
  ulong max_keys;                       /* Max keys in buffert */
} BUFFPEK;

#endif /* _myisamchk_h */
