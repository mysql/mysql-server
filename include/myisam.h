/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* This file should be included when using myisam_funktions */

#ifndef _myisam_h
#define _myisam_h

#include "my_base.h"
#include "m_ctype.h"
#include "keycache.h"
#include "my_compare.h"
#include "my_check_opt.h"
#include "m_string.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
  Limit max keys according to HA_MAX_POSSIBLE_KEY
*/

#if MAX_INDEXES > HA_MAX_POSSIBLE_KEY
#define MI_MAX_KEY                  HA_MAX_POSSIBLE_KEY /* Max allowed keys */
#else
#define MI_MAX_KEY                  MAX_INDEXES         /* Max allowed keys */
#endif

#define MI_MAX_POSSIBLE_KEY_BUFF    HA_MAX_POSSIBLE_KEY_BUFF
/*
  The following defines can be increased if necessary.
  But beware the dependency of MI_MAX_POSSIBLE_KEY_BUFF and MI_MAX_KEY_LENGTH.
*/
#define MI_MAX_KEY_LENGTH           1000            /* Max length in bytes */
#define MI_MAX_KEY_SEG              16              /* Max segments for key */

#define MI_MAX_KEY_BUFF  (MI_MAX_KEY_LENGTH+MI_MAX_KEY_SEG*6+8+8)
#define MI_MAX_MSG_BUF      1024 /* used in CHECK TABLE, REPAIR TABLE */
#define MI_NAME_IEXT	".MYI"
#define MI_NAME_DEXT	".MYD"

/* Possible values for myisam_block_size (must be power of 2) */
#define MI_KEY_BLOCK_LENGTH	1024	/* default key block length */
#define MI_MIN_KEY_BLOCK_LENGTH	1024	/* Min key block length */
#define MI_MAX_KEY_BLOCK_LENGTH	16384

/*
  In the following macros '_keyno_' is 0 .. keys-1.
  If there can be more keys than bits in the key_map, the highest bit
  is for all upper keys. They cannot be switched individually.
  This means that clearing of high keys is ignored, setting one high key
  sets all high keys.
*/
#define MI_KEYMAP_BITS      (8 * SIZEOF_LONG_LONG)
#define MI_KEYMAP_HIGH_MASK (1ULL << (MI_KEYMAP_BITS - 1))
#define mi_get_mask_all_keys_active(_keys_) \
                            (((_keys_) < MI_KEYMAP_BITS) ? \
                             ((1ULL << (_keys_)) - 1ULL) : \
                             (~ 0ULL))

#if MI_MAX_KEY > MI_KEYMAP_BITS

#define mi_is_key_active(_keymap_,_keyno_) \
                            (((_keyno_) < MI_KEYMAP_BITS) ? \
                             MY_TEST((_keymap_) & (1ULL << (_keyno_))) : \
                             MY_TEST((_keymap_) & MI_KEYMAP_HIGH_MASK))
#define mi_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (((_keyno_) < MI_KEYMAP_BITS) ? \
                                          (1ULL << (_keyno_)) : \
                                          MI_KEYMAP_HIGH_MASK)
#define mi_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (((_keyno_) < MI_KEYMAP_BITS) ? \
                                          (~ (1ULL << (_keyno_))) : \
                                          (~ (0ULL)) /*ignore*/ )

#else

#define mi_is_key_active(_keymap_,_keyno_) \
                            MY_TEST((_keymap_) & (1ULL << (_keyno_)))
#define mi_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (1ULL << (_keyno_))
#define mi_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (~ (1ULL << (_keyno_)))

#endif

#define mi_is_any_key_active(_keymap_) \
                            MY_TEST((_keymap_))
#define mi_is_all_keys_active(_keymap_,_keys_) \
                            ((_keymap_) == mi_get_mask_all_keys_active(_keys_))
#define mi_set_all_keys_active(_keymap_,_keys_) \
                            (_keymap_)= mi_get_mask_all_keys_active(_keys_)
#define mi_clear_all_keys_active(_keymap_) \
                            (_keymap_)= 0
#define mi_intersect_keys_active(_to_,_from_) \
                            (_to_)&= (_from_)
#define mi_is_any_intersect_keys_active(_keymap1_,_keys_,_keymap2_) \
                            ((_keymap1_) & (_keymap2_) & \
                             mi_get_mask_all_keys_active(_keys_))
#define mi_copy_keys_active(_to_,_maxkeys_,_from_) \
                            (_to_)= (mi_get_mask_all_keys_active(_maxkeys_) & \
                                     (_from_))

	/* Param to/from mi_status */

typedef struct st_mi_isaminfo		/* Struct from h_info */
{
  ha_rows records;			/* Records in database */
  ha_rows deleted;			/* Deleted records in database */
  my_off_t recpos;			/* Pos for last used record */
  my_off_t newrecpos;			/* Pos if we write new record */
  my_off_t dupp_key_pos;		/* Position to record with dupp key */
  my_off_t data_file_length,		/* Length of data file */
           max_data_file_length,
           index_file_length,
           max_index_file_length,
           delete_length;
  ulong reclength;			/* Recordlength */
  ulong mean_reclength;			/* Mean recordlength (if packed) */
  ulonglong auto_increment;
  ulonglong key_map;			/* Which keys are used */
  char  *data_file_name, *index_file_name;
  uint  keys;				/* Number of keys in use */
  uint	options;			/* HA_OPTION_... used */
  int	errkey,				/* With key was dupplicated on err */
	sortkey;			/* clustered by this key */
  File	filenr;				/* (uniq) filenr for datafile */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;
  uint  reflength;
  ulong record_offset;
  ulong *rec_per_key;			/* for sql optimizing */
} MI_ISAMINFO;


typedef struct st_mi_create_info
{
  const char *index_file_name, *data_file_name;	/* If using symlinks */
  ha_rows max_rows;
  ha_rows reloc_rows;
  ulonglong auto_increment;
  ulonglong data_file_length;
  ulonglong key_file_length;
  uint old_options;
  uint16 language;
  my_bool with_auto_increment;
} MI_CREATE_INFO;

struct st_myisam_info;			/* For referense */
struct st_mi_isam_share;
typedef struct st_myisam_info MI_INFO;
struct st_mi_s_param;

typedef struct st_mi_keydef		/* Key definition with open & info */
{
  struct st_mi_isam_share *share;       /* Pointer to base (set in mi_open) */
  uint16 keysegs;			/* Number of key-segment */
  uint16 flag;				/* NOSAME, PACK_USED */

  uint8  key_alg;			/* BTREE, RTREE */
  uint16 block_length;			/* Length of keyblock (auto) */
  uint16 underflow_block_length;	/* When to execute underflow */
  uint16 keylength;			/* Tot length of keyparts (auto) */
  uint16 minlength;			/* min length of (packed) key (auto) */
  uint16 maxlength;			/* max length of (packed) key (auto) */
  uint16 block_size_index;		/* block_size (auto) */
  uint32 version;			/* For concurrent read/write */
  uint32 ftkey_nr;                      /* full-text index number */

  HA_KEYSEG *seg,*end;
  struct st_mysql_ftparser *parser;     /* Fulltext [pre]parser */
  int (*bin_search)(struct st_myisam_info *info,struct st_mi_keydef *keyinfo,
		    uchar *page,uchar *key,
		    uint key_len,uint comp_flag,uchar * *ret_pos,
		    uchar *buff, my_bool *was_last_key);
  uint (*get_key)(struct st_mi_keydef *keyinfo,uint nod_flag,uchar * *page,
		  uchar *key);
  int (*pack_key)(struct st_mi_keydef *keyinfo,uint nod_flag,uchar *next_key,
		  uchar *org_key, uchar *prev_key, uchar *key,
		  struct st_mi_s_param *s_temp);
  void (*store_key)(struct st_mi_keydef *keyinfo, uchar *key_pos,
		    struct st_mi_s_param *s_temp);
  int (*ck_insert)(struct st_myisam_info *inf, uint k_nr, uchar *k, uint klen);
  int (*ck_delete)(struct st_myisam_info *inf, uint k_nr, uchar *k, uint klen);
} MI_KEYDEF;


#define MI_UNIQUE_HASH_LENGTH	4

typedef struct st_unique_def		/* Segment definition of unique */
{
  uint16 keysegs;			/* Number of key-segment */
  uchar key;				/* Mapped to which key */
  uint8 null_are_equal;
  HA_KEYSEG *seg,*end;
} MI_UNIQUEDEF;

typedef struct st_mi_decode_tree	/* Decode huff-table */
{
  uint16 *table;
  uint	 quick_table_bits;
  uchar	 *intervalls;
} MI_DECODE_TREE;


struct st_mi_bit_buff;

/*
  Note that null markers should always be first in a row !
  When creating a column, one should only specify:
  type, length, null_bit and null_pos
*/

typedef struct st_columndef		/* column information */
{
  int16  type;				/* en_fieldtype */
  uint16 length;			/* length of field */
  uint32 offset;			/* Offset to position in row */
  uint8  null_bit;			/* If column may be 0 */
  uint16 null_pos;			/* position for null marker */

  void (*unpack)(struct st_columndef *rec,struct st_mi_bit_buff *buff,
		 uchar *start,uchar *end);
  enum en_fieldtype base_type;
  uint space_length_bits,pack_type;
  MI_DECODE_TREE *huff_tree;
} MI_COLUMNDEF;


extern char * myisam_log_filename;		/* Name of logfile */
extern ulong myisam_block_size;
extern ulong myisam_concurrent_insert;
extern my_bool myisam_flush,myisam_delay_key_write,myisam_single_user;
extern my_off_t myisam_max_temp_length;
extern ulong myisam_data_pointer_size;

/* usually used to check if a symlink points into the mysql data home */
/* which is normally forbidden                                        */
extern int (*myisam_test_invalid_symlink)(const char *filename);
extern ulonglong myisam_mmap_size, myisam_mmap_used;
extern mysql_mutex_t THR_LOCK_myisam_mmap;

	/* Prototypes for myisam-functions */

extern int mi_close_share(struct st_myisam_info *file, my_bool *closed_share);
#define mi_close(file) mi_close_share(file, NULL)
extern int mi_delete(struct st_myisam_info *file,const uchar *buff);
extern struct st_myisam_info *mi_open_share(const char *name,
                                            struct st_mi_isam_share *old_share,
                                            int mode,
                                            uint wait_if_locked);
#define mi_open(name, mode, wait_if_locked) \
  mi_open_share(name, NULL, mode, wait_if_locked)
extern int mi_panic(enum ha_panic_function function);
extern int mi_rfirst(struct st_myisam_info *file,uchar *buf,int inx);
extern int mi_rkey(MI_INFO *info, uchar *buf, int inx, const uchar *key,
                   key_part_map keypart_map, enum ha_rkey_function search_flag);
extern int mi_rlast(struct st_myisam_info *file,uchar *buf,int inx);
extern int mi_rnext(struct st_myisam_info *file,uchar *buf,int inx);
extern int mi_rnext_same(struct st_myisam_info *info, uchar *buf);
extern int mi_rprev(struct st_myisam_info *file,uchar *buf,int inx);
extern int mi_rrnd(struct st_myisam_info *file,uchar *buf, my_off_t pos);
extern int mi_scan_init(struct st_myisam_info *file);
extern int mi_scan(struct st_myisam_info *file,uchar *buf);
extern int mi_rsame(struct st_myisam_info *file,uchar *record,int inx);
extern int mi_rsame_with_pos(struct st_myisam_info *file,uchar *record,
			     int inx, my_off_t pos);
extern int mi_update(struct st_myisam_info *file,const uchar *old,
		     uchar *new_record);
extern int mi_write(struct st_myisam_info *file,uchar *buff);
extern my_off_t mi_position(struct st_myisam_info *file);
extern int mi_status(struct st_myisam_info *info, MI_ISAMINFO *x, uint flag);
extern int mi_lock_database(struct st_myisam_info *file,int lock_type);
extern int mi_create(const char *name,uint keys,MI_KEYDEF *keydef,
		     uint columns, MI_COLUMNDEF *columndef,
		     uint uniques, MI_UNIQUEDEF *uniquedef,
		     MI_CREATE_INFO *create_info, uint flags);
extern int mi_delete_table(const char *name);
extern int mi_rename(const char *from, const char *to);
extern int mi_extra(struct st_myisam_info *file,
		    enum ha_extra_function function,
		    void *extra_arg);
extern int mi_reset(struct st_myisam_info *file);
extern ha_rows mi_records_in_range(MI_INFO *info, int inx,
                                   key_range *min_key, key_range *max_key);
extern int mi_log(int activate_log);
extern int mi_is_changed(struct st_myisam_info *info);
extern int mi_delete_all_rows(struct st_myisam_info *info);
extern ulong _mi_calc_blob_length(uint length , const uchar *pos);
extern uint mi_get_pointer_length(ulonglong file_length, uint def);

#define MEMMAP_EXTRA_MARGIN     7       /* Write this as a suffix for mmap file */
/* this is used to pass to mysql_myisamchk_table */

#define   MYISAMCHK_REPAIR 1  /* equivalent to myisamchk -r */
#define   MYISAMCHK_VERIFY 2  /* Verify, run repair if failure */

/*
  Flags used by myisamchk.c or/and ha_myisam.cc that are NOT passed
  to mi_check.c follows:
*/

#define TT_USEFRM               1
#define TT_FOR_UPGRADE          2

#define O_NEW_INDEX	1		/* Bits set in out_flag */
#define O_NEW_DATA	2
#define O_DATA_LOST	4

/* these struct is used by my_check to tell it what to do */

typedef struct st_sort_key_blocks		/* Used when sorting */
{
  uchar *buff,*end_pos;
  uchar lastkey[MI_MAX_POSSIBLE_KEY_BUFF];
  uint last_length;
  int inited;
} SORT_KEY_BLOCKS;


/* 
  MyISAM supports several statistics collection methods. Currently statistics 
  collection method is not stored in MyISAM file and has to be specified for 
  each table analyze/repair operation in  MI_CHECK::stats_method.
*/

typedef enum 
{
  /* Treat NULLs as inequal when collecting statistics (default for 4.1/5.0) */
  MI_STATS_METHOD_NULLS_NOT_EQUAL,
  /* Treat NULLs as equal when collecting statistics (like 4.0 did) */
  MI_STATS_METHOD_NULLS_EQUAL,
  /* Ignore NULLs - count only tuples without NULLs in the index components */
  MI_STATS_METHOD_IGNORE_NULLS
} enum_mi_stats_method;

typedef struct st_mi_check_param
{
  ulonglong auto_increment_value;
  ulonglong max_data_file_length;
  ulonglong keys_in_use;
  ulonglong max_record_length;
  ulonglong sort_buffer_length;
  my_off_t search_after_block;
  my_off_t new_file_pos,key_file_blocks;
  my_off_t keydata,totaldata,key_blocks,start_check_pos;
  ha_rows total_records,total_deleted;
  ha_checksum record_checksum,glob_crc;
  ulonglong use_buffers;
  ulong read_buffer_length, write_buffer_length, sort_key_blocks;
  uint out_flag,warning_printed,error_printed,verbose;
  uint opt_sort_key,total_files,max_level;
  uint testflag, key_cache_block_size;
  uint16 language;
  my_bool using_global_keycache, opt_lock_memory, opt_follow_links;
  my_bool retry_repair, force_sort;
  char temp_filename[FN_REFLEN],*isam_file_name;
  MY_TMPDIR *tmpdir;
  int tmpfile_createflag;
  myf myf_rw;
  IO_CACHE read_cache;
  
  /* 
    The next two are used to collect statistics, see update_key_parts for
    description.
  */
  ulonglong unique_count[MI_MAX_KEY_SEG+1];
  ulonglong notnull_count[MI_MAX_KEY_SEG+1];
  
  ha_checksum key_crc[HA_MAX_POSSIBLE_KEY];
  ulong rec_per_key_part[MI_MAX_KEY_SEG*HA_MAX_POSSIBLE_KEY];
  void *thd;
  const char *db_name, *table_name;
  const char *op_name;
  enum_mi_stats_method stats_method;
  mysql_mutex_t print_msg_mutex;
  my_bool need_print_msg_lock;
} MI_CHECK;

typedef struct st_sort_ft_buf
{
  uchar *buf, *end;
  int   count;
  uchar lastkey[MI_MAX_KEY_BUFF];
} SORT_FT_BUF;

typedef struct st_sort_info
{
  my_off_t filelength,dupp,buff_length;
  ha_rows max_records;
  uint current_key, total_keys;
  myf myf_rw;
  enum data_file_type new_data_file_type;
  MI_INFO *info;
  MI_CHECK *param;
  uchar *buff;
  SORT_KEY_BLOCKS *key_block,*key_block_end;
  SORT_FT_BUF *ft_buf;
  /* sync things */
  uint got_error, threads_running;
  mysql_mutex_t mutex;
  mysql_cond_t  cond;
} SORT_INFO;

/* functions in mi_check */
void myisamchk_init(MI_CHECK *param);
int chk_status(MI_CHECK *param, MI_INFO *info);
int chk_del(MI_CHECK *param, MI_INFO *info, uint test_flag);
int chk_size(MI_CHECK *param, MI_INFO *info);
int chk_key(MI_CHECK *param, MI_INFO *info);
int chk_data_link(MI_CHECK *param, MI_INFO *info,int extend);
int mi_repair(MI_CHECK *param, MI_INFO *info,
	      char * name, int rep_quick, my_bool no_copy_stat);
int mi_sort_index(MI_CHECK *param, MI_INFO *info, char * name,
                  my_bool no_copy_stat);
int mi_repair_by_sort(MI_CHECK *param, MI_INFO *info,
		      const char * name, int rep_quick, my_bool no_copy_stat);
int mi_repair_parallel(MI_CHECK *param, MI_INFO *info,
                       const char * name, int rep_quick, my_bool no_copy_stat);
int change_to_newfile(const char * filename, const char * old_ext,
		      const char * new_ext, myf myflags);
int lock_file(MI_CHECK *param, File file, my_off_t start, int lock_type,
	      const char *filetype, const char *filename);
void lock_memory(MI_CHECK *param);
void update_auto_increment_key(MI_CHECK *param, MI_INFO *info,
			       my_bool repair);
int update_state_info(MI_CHECK *param, MI_INFO *info,uint update);
void update_key_parts(MI_KEYDEF *keyinfo, ulong *rec_per_key_part,
                      ulonglong *unique, ulonglong *notnull, 
                      ulonglong records);
int filecopy(MI_CHECK *param, File to,File from,my_off_t start,
	     my_off_t length, const char *type);
int movepoint(MI_INFO *info,uchar *record,my_off_t oldpos,
	      my_off_t newpos, uint prot_key);
int write_data_suffix(SORT_INFO *sort_info, my_bool fix_datafile);
int test_if_almost_full(MI_INFO *info);
int recreate_table(MI_CHECK *param, MI_INFO **org_info, char *filename);
void mi_disable_non_unique_index(MI_INFO *info, ha_rows rows);
my_bool mi_test_if_sort_rep(MI_INFO *info, ha_rows rows, ulonglong key_map,
			    my_bool force);

int mi_init_bulk_insert(MI_INFO *info, ulong cache_size, ha_rows rows);
void mi_flush_bulk_insert(MI_INFO *info, uint inx);
void mi_end_bulk_insert(MI_INFO *info);
int mi_assign_to_key_cache(MI_INFO *info, ulonglong key_map, 
			   KEY_CACHE *key_cache);
void mi_change_key_cache(KEY_CACHE *old_key_cache,
			 KEY_CACHE *new_key_cache);
int mi_preload(MI_INFO *info, ulonglong key_map, my_bool ignore_leaves);

extern st_keycache_thread_var main_thread_keycache_var;
st_keycache_thread_var *keycache_thread_var();
#ifdef	__cplusplus
}
#endif
#endif
