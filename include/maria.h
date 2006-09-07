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

/* This file should be included when using maria_funktions */

#ifndef _maria_h
#define _maria_h
#ifdef	__cplusplus
extern "C" {
#endif
#ifndef _my_base_h
#include <my_base.h>
#endif
#ifndef _m_ctype_h
#include <m_ctype.h>
#endif
#ifndef _keycache_h
#include "keycache.h"
#endif
#include "my_handler.h"
#include "ft_global.h"
#include <myisamchk.h>
#include <mysql/plugin.h>

/*
  Limit max keys according to HA_MAX_POSSIBLE_KEY; See myisamchk.h for details
*/

#if MAX_INDEXES > HA_MAX_POSSIBLE_KEY
#define MARIA_MAX_KEY    HA_MAX_POSSIBLE_KEY    /* Max allowed keys */
#else
#define MARIA_MAX_KEY    MAX_INDEXES            /* Max allowed keys */
#endif

#define MARIA_MAX_MSG_BUF      1024 /* used in CHECK TABLE, REPAIR TABLE */
#define MARIA_NAME_IEXT	".MAI"
#define MARIA_NAME_DEXT	".MAD"
/* Max extra space to use when sorting keys */
#define MARIA_MAX_TEMP_LENGTH	2*1024L*1024L*1024L
/* Possible values for maria_block_size (must be power of 2) */
#define MARIA_KEY_BLOCK_LENGTH	8192		/* default key block length */
#define MARIA_MIN_KEY_BLOCK_LENGTH	1024	/* Min key block length */
#define MARIA_MAX_KEY_BLOCK_LENGTH	32768
#define maria_portable_sizeof_char_ptr 8

/*
  In the following macros '_keyno_' is 0 .. keys-1.
  If there can be more keys than bits in the key_map, the highest bit
  is for all upper keys. They cannot be switched individually.
  This means that clearing of high keys is ignored, setting one high key
  sets all high keys.
*/
#define MARIA_KEYMAP_BITS      (8 * SIZEOF_LONG_LONG)
#define MARIA_KEYMAP_HIGH_MASK (ULL(1) << (MARIA_KEYMAP_BITS - 1))
#define maria_get_mask_all_keys_active(_keys_) \
                            (((_keys_) < MARIA_KEYMAP_BITS) ? \
                             ((ULL(1) << (_keys_)) - ULL(1)) : \
                             (~ ULL(0)))
#if MARIA_MAX_KEY > MARIA_KEYMAP_BITS
#define maria_is_key_active(_keymap_,_keyno_) \
                            (((_keyno_) < MARIA_KEYMAP_BITS) ? \
                             test((_keymap_) & (ULL(1) << (_keyno_))) : \
                             test((_keymap_) & MARIA_KEYMAP_HIGH_MASK))
#define maria_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (((_keyno_) < MARIA_KEYMAP_BITS) ? \
                                          (ULL(1) << (_keyno_)) : \
                                          MARIA_KEYMAP_HIGH_MASK)
#define maria_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (((_keyno_) < MARIA_KEYMAP_BITS) ? \
                                          (~ (ULL(1) << (_keyno_))) : \
                                          (~ (ULL(0))) /*ignore*/ )
#else
#define maria_is_key_active(_keymap_,_keyno_) \
                            test((_keymap_) & (ULL(1) << (_keyno_)))
#define maria_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (ULL(1) << (_keyno_))
#define maria_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (~ (ULL(1) << (_keyno_)))
#endif
#define maria_is_any_key_active(_keymap_) \
                            test((_keymap_))
#define maria_is_all_keys_active(_keymap_,_keys_) \
                            ((_keymap_) == maria_get_mask_all_keys_active(_keys_))
#define maria_set_all_keys_active(_keymap_,_keys_) \
                            (_keymap_)= maria_get_mask_all_keys_active(_keys_)
#define maria_clear_all_keys_active(_keymap_) \
                            (_keymap_)= 0
#define maria_intersect_keys_active(_to_,_from_) \
                            (_to_)&= (_from_)
#define maria_is_any_intersect_keys_active(_keymap1_,_keys_,_keymap2_) \
                            ((_keymap1_) & (_keymap2_) & \
                             maria_get_mask_all_keys_active(_keys_))
#define maria_copy_keys_active(_to_,_maxkeys_,_from_) \
                            (_to_)= (maria_get_mask_all_keys_active(_maxkeys_) & \
                                     (_from_))

	/* Param to/from maria_info */

typedef struct st_maria_isaminfo	/* Struct from h_info */
{
  ha_rows records;			/* Records in database */
  ha_rows deleted;			/* Deleted records in database */
  my_off_t recpos;			/* Pos for last used record */
  my_off_t newrecpos;			/* Pos if we write new record */
  my_off_t dupp_key_pos;		/* Position to record with dup key */
  my_off_t data_file_length;            /* Length of data file */
  my_off_t max_data_file_length, index_file_length;
  my_off_t max_index_file_length, delete_length;
  ulong reclength;                      /* Recordlength */
  ulong mean_reclength;                 /* Mean recordlength (if packed) */
  ulonglong auto_increment;
  ulonglong key_map;                    /* Which keys are used */
  char *data_file_name, *index_file_name;
  uint keys;                            /* Number of keys in use */
  uint options;                         /* HA_OPTION_... used */
  int errkey,                           /* With key was dupplicated on err */
    sortkey;                            /* clustered by this key */
  File filenr;                          /* (uniq) filenr for datafile */
  time_t create_time;                   /* When table was created */
  time_t check_time;
  time_t update_time;
  uint reflength;
  ulong record_offset;
  ulong *rec_per_key;                   /* for sql optimizing */
} MARIA_INFO;


typedef struct st_maria_create_info
{
  const char *index_file_name, *data_file_name;	/* If using symlinks */
  ha_rows max_rows;
  ha_rows reloc_rows;
  ulonglong auto_increment;
  ulonglong data_file_length;
  ulonglong key_file_length;
  uint old_options;
  uint8 language;
  my_bool with_auto_increment;
} MARIA_CREATE_INFO;

struct st_maria_info;				/* For referense */
struct st_maria_share;
typedef struct st_maria_info MARIA_HA;
struct st_maria_s_param;

typedef struct st_maria_keydef          /* Key definition with open & info */
{
  struct st_maria_share *share;         /* Pointer to base (set in open) */
  uint16 keysegs;                       /* Number of key-segment */
  uint16 flag;                          /* NOSAME, PACK_USED */

  uint8 key_alg;                        /* BTREE, RTREE */
  uint16 block_length;                  /* Length of keyblock (auto) */
  uint16 underflow_block_length;        /* When to execute underflow */
  uint16 keylength;                     /* Tot length of keyparts (auto) */
  uint16 minlength;                     /* min length of (packed) key (auto) */
  uint16 maxlength;                     /* max length of (packed) key (auto) */
  uint16 block_size_index;              /* block_size (auto) */
  uint32 version;                       /* For concurrent read/write */
  uint32 ftparser_nr;                   /* distinct ftparser number */

  HA_KEYSEG *seg, *end;
  struct st_mysql_ftparser *parser;     /* Fulltext [pre]parser */
  int(*bin_search) (struct st_maria_info *info,
		    struct st_maria_keydef *keyinfo, uchar *page, uchar *key,
		    uint key_len, uint comp_flag, uchar **ret_pos,
		    uchar *buff, my_bool *was_last_key);
    uint(*get_key) (struct st_maria_keydef *keyinfo, uint nod_flag,
		    uchar **page, uchar *key);
  int(*pack_key) (struct st_maria_keydef *keyinfo, uint nod_flag,
		  uchar *next_key, uchar *org_key, uchar *prev_key,
		  uchar *key, struct st_maria_s_param *s_temp);
  void(*store_key) (struct st_maria_keydef *keyinfo, uchar *key_pos,
		    struct st_maria_s_param *s_temp);
  int(*ck_insert) (struct st_maria_info *inf, uint k_nr, uchar *k, uint klen);
  int(*ck_delete) (struct st_maria_info *inf, uint k_nr, uchar *k, uint klen);
} MARIA_KEYDEF;


#define MARIA_UNIQUE_HASH_LENGTH	4

typedef struct st_maria_unique_def	/* Segment definition of unique */
{
  uint16 keysegs;                       /* Number of key-segment */
  uchar key;                            /* Mapped to which key */
  uint8 null_are_equal;
  HA_KEYSEG *seg, *end;
} MARIA_UNIQUEDEF;

typedef struct st_maria_decode_tree     /* Decode huff-table */
{
  uint16 *table;
  uint quick_table_bits;
  byte *intervalls;
} MARIA_DECODE_TREE;


struct st_maria_bit_buff;

/*
  Note that null markers should always be first in a row !
  When creating a column, one should only specify:
  type, length, null_bit and null_pos
*/

typedef struct st_maria_columndef		/* column information */
{
  int16 type;					/* en_fieldtype */
  uint16 length;				/* length of field */
  uint32 offset;				/* Offset to position in row */
  uint8 null_bit;				/* If column may be 0 */
  uint16 null_pos;				/* position for null marker */

#ifndef NOT_PACKED_DATABASES
  void(*unpack) (struct st_maria_columndef *rec,
                 struct st_maria_bit_buff *buff,
		 uchar *start, uchar *end);
  enum en_fieldtype base_type;
  uint space_length_bits, pack_type;
  MARIA_DECODE_TREE *huff_tree;
#endif
} MARIA_COLUMNDEF;


extern ulong maria_block_size;
extern ulong maria_concurrent_insert;
extern my_bool maria_flush, maria_delay_key_write, maria_single_user;
extern my_off_t maria_max_temp_length;
extern ulong maria_bulk_insert_tree_size, maria_data_pointer_size;
extern KEY_CACHE maria_key_cache_var, *maria_key_cache;


	/* Prototypes for maria-functions */

extern int maria_init(void);
extern void maria_end(void);
extern int maria_close(struct st_maria_info *file);
extern int maria_delete(struct st_maria_info *file, const byte *buff);
extern struct st_maria_info *maria_open(const char *name, int mode,
					uint wait_if_locked);
extern int maria_panic(enum ha_panic_function function);
extern int maria_rfirst(struct st_maria_info *file, byte *buf, int inx);
extern int maria_rkey(struct st_maria_info *file, byte *buf, int inx,
		      const byte *key,
		      uint key_len, enum ha_rkey_function search_flag);
extern int maria_rlast(struct st_maria_info *file, byte *buf, int inx);
extern int maria_rnext(struct st_maria_info *file, byte *buf, int inx);
extern int maria_rnext_same(struct st_maria_info *info, byte *buf);
extern int maria_rprev(struct st_maria_info *file, byte *buf, int inx);
extern int maria_rrnd(struct st_maria_info *file, byte *buf, my_off_t pos);
extern int maria_scan_init(struct st_maria_info *file);
extern int maria_scan(struct st_maria_info *file, byte *buf);
extern int maria_rsame(struct st_maria_info *file, byte *record, int inx);
extern int maria_rsame_with_pos(struct st_maria_info *file, byte *record,
				int inx, my_off_t pos);
extern int maria_update(struct st_maria_info *file, const byte *old,
			byte *new_record);
extern int maria_write(struct st_maria_info *file, byte *buff);
extern my_off_t maria_position(struct st_maria_info *file);
extern int maria_status(struct st_maria_info *info, MARIA_INFO *x, uint flag);
extern int maria_lock_database(struct st_maria_info *file, int lock_type);
extern int maria_create(const char *name, uint keys, MARIA_KEYDEF *keydef,
			uint columns, MARIA_COLUMNDEF *columndef,
			uint uniques, MARIA_UNIQUEDEF *uniquedef,
			MARIA_CREATE_INFO *create_info, uint flags);
extern int maria_delete_table(const char *name);
extern int maria_rename(const char *from, const char *to);
extern int maria_extra(struct st_maria_info *file,
		       enum ha_extra_function function, void *extra_arg);
extern int maria_reset(struct st_maria_info *file);
extern ha_rows maria_records_in_range(struct st_maria_info *info, int inx,
				      key_range *min_key, key_range *max_key);
extern int maria_is_changed(struct st_maria_info *info);
extern int maria_delete_all_rows(struct st_maria_info *info);
extern uint maria_get_pointer_length(ulonglong file_length, uint def);


/* this is used to pass to mysql_mariachk_table */

#define MARIA_CHK_REPAIR 1              /* equivalent to mariachk -r */
#define MARIA_CHK_VERIFY 2              /* Verify, run repair if failure */

typedef struct st_maria_sort_info
{
#ifdef THREAD
  /* sync things */
  pthread_mutex_t mutex;
  pthread_cond_t cond;
#endif
  MARIA_HA *info;
  HA_CHECK *param;
  char *buff;
  SORT_KEY_BLOCKS *key_block, *key_block_end;
  SORT_FT_BUF *ft_buf;

  my_off_t filelength, dupp, buff_length;
  ha_rows max_records;
  uint current_key, total_keys;
  uint got_error, threads_running;
  myf myf_rw;
  enum data_file_type new_data_file_type;
} MARIA_SORT_INFO;


typedef struct st_maria_sort_param
{
  pthread_t thr;
  IO_CACHE read_cache, tempfile, tempfile_for_exceptions;
  DYNAMIC_ARRAY buffpek;
  
  MARIA_KEYDEF *keyinfo;
  MARIA_SORT_INFO *sort_info;
  HA_KEYSEG *seg;
  uchar **sort_keys;
  byte *rec_buff;
  void *wordlist, *wordptr;
  MEM_ROOT wordroot;
  char *record;
  MY_TMPDIR *tmpdir;

  /* 
    The next two are used to collect statistics, see maria_update_key_parts for
    description.
  */
  ulonglong unique[HA_MAX_KEY_SEG+1];
  ulonglong notnull[HA_MAX_KEY_SEG+1];

  my_off_t pos,max_pos,filepos,start_recpos;
  uint key, key_length,real_key_length,sortbuff_size;
  uint maxbuffers, keys, find_length, sort_keys_length;
  my_bool fix_datafile, master;

  int (*key_cmp)(struct st_maria_sort_param *, const void *, const void *);
  int (*key_read)(struct st_maria_sort_param *,void *);
  int (*key_write)(struct st_maria_sort_param *, const void *);
  void (*lock_in_memory)(HA_CHECK *);
  NEAR int (*write_keys)(struct st_maria_sort_param *, register uchar **,
                         uint , struct st_buffpek *, IO_CACHE *);
  NEAR uint (*read_to_buffer)(IO_CACHE *,struct st_buffpek *, uint);
  NEAR int (*write_key)(struct st_maria_sort_param *, IO_CACHE *,char *,
                        uint, uint);
} MARIA_SORT_PARAM;


/* functions in maria_check */
void mariachk_init(HA_CHECK *param);
int maria_chk_status(HA_CHECK *param, MARIA_HA *info);
int maria_chk_del(HA_CHECK *param, register MARIA_HA *info, uint test_flag);
int maria_chk_size(HA_CHECK *param, MARIA_HA *info);
int maria_chk_key(HA_CHECK *param, MARIA_HA *info);
int maria_chk_data_link(HA_CHECK *param, MARIA_HA *info, int extend);
int maria_repair(HA_CHECK *param, register MARIA_HA *info,
		 my_string name, int rep_quick);
int maria_sort_index(HA_CHECK *param, register MARIA_HA *info,
		     my_string name);
int maria_repair_by_sort(HA_CHECK *param, register MARIA_HA *info,
			 const char *name, int rep_quick);
int maria_repair_parallel(HA_CHECK *param, register MARIA_HA *info,
			  const char *name, int rep_quick);
int maria_change_to_newfile(const char *filename, const char *old_ext,
		      const char *new_ext, uint raid_chunks, myf myflags);
void maria_lock_memory(HA_CHECK *param);
int maria_update_state_info(HA_CHECK *param, MARIA_HA *info, uint update);
void maria_update_key_parts(MARIA_KEYDEF *keyinfo, ulong *rec_per_key_part,
		      ulonglong *unique, ulonglong *notnull,
		      ulonglong records);
int maria_filecopy(HA_CHECK *param, File to, File from, my_off_t start,
	     my_off_t length, const char *type);
int maria_movepoint(MARIA_HA *info, byte *record, my_off_t oldpos,
	      my_off_t newpos, uint prot_key);
int maria_write_data_suffix(MARIA_SORT_INFO *sort_info, my_bool fix_datafile);
int maria_test_if_almost_full(MARIA_HA *info);
int maria_recreate_table(HA_CHECK *param, MARIA_HA ** org_info, char *filename);
int maria_disable_indexes(MARIA_HA *info);
int maria_enable_indexes(MARIA_HA *info);
int maria_indexes_are_disabled(MARIA_HA *info);
void maria_disable_non_unique_index(MARIA_HA *info, ha_rows rows);
my_bool maria_test_if_sort_rep(MARIA_HA *info, ha_rows rows, ulonglong key_map,
			       my_bool force);

int maria_init_bulk_insert(MARIA_HA *info, ulong cache_size, ha_rows rows);
void maria_flush_bulk_insert(MARIA_HA *info, uint inx);
void maria_end_bulk_insert(MARIA_HA *info);
int maria_assign_to_key_cache(MARIA_HA *info, ulonglong key_map,
			      KEY_CACHE *key_cache);
void maria_change_key_cache(KEY_CACHE *old_key_cache,
			    KEY_CACHE *new_key_cache);
int maria_preload(MARIA_HA *info, ulonglong key_map, my_bool ignore_leaves);

/* fulltext functions */
FT_INFO *maria_ft_init_search(uint,void *, uint, byte *, uint,
                              CHARSET_INFO *, byte *);

/* 'Almost-internal' Maria functions */

void _ma_update_auto_increment_key(HA_CHECK *param, MARIA_HA *info,
                                  my_bool repair);


#ifdef	__cplusplus
}
#endif
#endif
