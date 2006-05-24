/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* This file is included by all internal maria files */

#include "maria.h"				/* Structs & some defines */
#include "myisampack.h"				/* packing of keys */
#include <my_tree.h>
#ifdef THREAD
#include <my_pthread.h>
#include <thr_lock.h>
#else
#include <my_no_pthread.h>
#endif

/* undef map from my_nosys; We need test-if-disk full */
#undef my_write	

typedef struct st_maria_status_info
{
  ha_rows records;				/* Rows in table */
  ha_rows del;					/* Removed rows */
  my_off_t empty;				/* lost space in datafile */
  my_off_t key_empty;				/* lost space in indexfile */
  my_off_t key_file_length;
  my_off_t data_file_length;
  ha_checksum checksum;
} MARIA_STATUS_INFO;

typedef struct st_maria_state_info
{
  struct
  {					/* Fileheader */
    uchar file_version[4];
    uchar options[2];
    uchar header_length[2];
    uchar state_info_length[2];
    uchar base_info_length[2];
    uchar base_pos[2];
    uchar key_parts[2];			/* Key parts */
    uchar unique_key_parts[2];		/* Key parts + unique parts */
    uchar keys;				/* number of keys in file */
    uchar uniques;			/* number of UNIQUE definitions */
    uchar language;			/* Language for indexes */
    uchar max_block_size;		/* max keyblock size */
    uchar fulltext_keys;
    uchar not_used;			/* To align to 8 */
  } header;

  MARIA_STATUS_INFO state;
  ha_rows split;			/* number of split blocks */
  my_off_t dellink;			/* Link to next removed block */
  ulonglong auto_increment;
  ulong process;			/* process that updated table last */
  ulong unique;				/* Unique number for this process */
  ulong update_count;			/* Updated for each write lock */
  ulong status;
  ulong *rec_per_key_part;
  my_off_t *key_root;			/* Start of key trees */
  my_off_t *key_del;			/* delete links for trees */
  my_off_t rec_per_key_rows;		/* Rows when calculating rec_per_key */

  ulong sec_index_changed;		/* Updated when new sec_index */
  ulong sec_index_used;			/* which extra index are in use */
  ulonglong key_map;			/* Which keys are in use */
  ulong version;			/* timestamp of create */
  time_t create_time;			/* Time when created database */
  time_t recover_time;			/* Time for last recover */
  time_t check_time;			/* Time for last check */
  uint sortkey;				/* sorted by this key (not used) */
  uint open_count;
  uint8 changed;			/* Changed since mariachk */

  /* the following isn't saved on disk */
  uint state_diff_length;		/* Should be 0 */
  uint state_length;			/* Length of state header in file */
  ulong *key_info;
} MARIA_STATE_INFO;


#define MARIA_STATE_INFO_SIZE	(24+14*8+7*4+2*2+8)
#define MARIA_STATE_KEY_SIZE	8
#define MARIA_STATE_KEYBLOCK_SIZE  8
#define MARIA_STATE_KEYSEG_SIZE	4
#define MARIA_STATE_EXTRA_SIZE ((MARIA_MAX_KEY+MARIA_MAX_KEY_BLOCK_SIZE)*MARIA_STATE_KEY_SIZE + MARIA_MAX_KEY*HA_MAX_KEY_SEG*MARIA_STATE_KEYSEG_SIZE)
#define MARIA_KEYDEF_SIZE		(2+ 5*2)
#define MARIA_UNIQUEDEF_SIZE	(2+1+1)
#define HA_KEYSEG_SIZE		(6+ 2*2 + 4*2)
#define MARIA_COLUMNDEF_SIZE	(2*3+1)
#define MARIA_BASE_INFO_SIZE	(5*8 + 8*4 + 4 + 4*2 + 16)
#define MARIA_INDEX_BLOCK_MARGIN 16	/* Safety margin for .MYI tables */

typedef struct st__ma_base_info
{
  my_off_t keystart;				/* Start of keys */
  my_off_t max_data_file_length;
  my_off_t max_key_file_length;
  my_off_t margin_key_file_length;
  ha_rows records, reloc;			/* Create information */
  ulong mean_row_length;			/* Create information */
  ulong reclength;				/* length of unpacked record */
  ulong pack_reclength;				/* Length of full packed rec */
  ulong min_pack_length;
  ulong max_pack_length;			/* Max possibly length of
						   packed rec. */
  ulong min_block_length;
  ulong fields,					/* fields in table */
    pack_fields;				/* packed fields in table */
  uint rec_reflength;				/* = 2-8 */
  uint key_reflength;				/* = 2-8 */
  uint keys;					/* same as in state.header */
  uint auto_key;				/* Which key-1 is a auto key */
  uint blobs;					/* Number of blobs */
  uint pack_bits;				/* Length of packed bits */
  uint max_key_block_length;			/* Max block length */
  uint max_key_length;				/* Max key length */
  /* Extra allocation when using dynamic record format */
  uint extra_alloc_bytes;
  uint extra_alloc_procent;
  /* Info about raid */
  uint raid_type, raid_chunks;
  ulong raid_chunksize;
  /* The following are from the header */
  uint key_parts, all_key_parts;
} MARIA_BASE_INFO;


	/* Structs used intern in database */

typedef struct st_maria_blob			/* Info of record */
{
  ulong offset;					/* Offset to blob in record */
  uint pack_length;				/* Type of packed length */
  ulong length;					/* Calc:ed for each record */
} MARIA_BLOB;


typedef struct st_maria_pack
{
  ulong header_length;
  uint ref_length;
  uchar version;
} MARIA_PACK;

#define MAX_NONMAPPED_INSERTS 1000

typedef struct st_maria_share
{					/* Shared between opens */
  MARIA_STATE_INFO state;
  MARIA_BASE_INFO base;
  MARIA_KEYDEF ft2_keyinfo;		/* Second-level ft-key
						   definition */
  MARIA_KEYDEF *keyinfo;		/* Key definitions */
  MARIA_UNIQUEDEF *uniqueinfo;		/* unique definitions */
  HA_KEYSEG *keyparts;			/* key part info */
  MARIA_COLUMNDEF *rec;			/* Pointer to field information
						*/
  MARIA_PACK pack;			/* Data about packed records */
  MARIA_BLOB *blobs;			/* Pointer to blobs */
  char *unique_file_name;		/* realpath() of index file */
  char *data_file_name,			/* Resolved path names from
						   symlinks */
   *index_file_name;
  byte *file_map;			/* mem-map of file if possible */
  KEY_CACHE *key_cache;			/* ref to the current key cache
						*/
  MARIA_DECODE_TREE *decode_trees;
  uint16 *decode_tables;
  int(*read_record) (struct st_maria_info *, my_off_t, byte *);
  int(*write_record) (struct st_maria_info *, const byte *);
  int(*update_record) (struct st_maria_info *, my_off_t, const byte *);
  int(*delete_record) (struct st_maria_info *);
  int(*read_rnd) (struct st_maria_info *, byte *, my_off_t, my_bool);
  int(*compare_record) (struct st_maria_info *, const byte *);
    ha_checksum(*calc_checksum) (struct st_maria_info *, const byte *);
  int(*compare_unique) (struct st_maria_info *, MARIA_UNIQUEDEF *,
			const byte *record, my_off_t pos);
    uint(*file_read) (MARIA_HA *, byte *, uint, my_off_t, myf);
    uint(*file_write) (MARIA_HA *, byte *, uint, my_off_t, myf);
  invalidator_by_filename invalidator;	/* query cache invalidator */
  ulong this_process;			/* processid */
  ulong last_process;			/* For table-change-check */
  ulong last_version;			/* Version on start */
  ulong options;			/* Options used */
  ulong min_pack_length;		/* Theese are used by packed
						   data */
  ulong max_pack_length;
  ulong state_diff_length;
  uint rec_reflength;			/* rec_reflength in use now */
  uint unique_name_length;
  uint32 ftparsers;			/* Number of distinct ftparsers
						   + 1 */
  File kfile;				/* Shared keyfile */
  File data_file;			/* Shared data file */
  int mode;				/* mode of file on open */
  uint reopen;				/* How many times reopened */
  uint w_locks, r_locks, tot_locks;	/* Number of read/write locks */
  uint blocksize;			/* blocksize of keyfile */
  myf write_flag;
  enum data_file_type data_file_type;
  my_bool changed,			/* If changed since lock */
    global_changed,			/* If changed since open */
    not_flushed, temporary, delay_key_write, concurrent_insert;
#ifdef THREAD
  THR_LOCK lock;
  pthread_mutex_t intern_lock;		/* Locking for use with
						   _locking */
  rw_lock_t *key_root_lock;
#endif
  my_off_t mmaped_length;
  uint nonmmaped_inserts;		/* counter of writing in
						   non-mmaped area */
  rw_lock_t mmap_lock;
} MARIA_SHARE;


typedef uint maria_bit_type;

typedef struct st_maria_bit_buff
{					/* Used for packing of record */
  maria_bit_type current_byte;
  uint bits;
  uchar *pos, *end, *blob_pos, *blob_end;
  uint error;
} MARIA_BIT_BUFF;

struct st_maria_info
{
  MARIA_SHARE *s;			/* Shared between open:s */
  MARIA_STATUS_INFO *state, save_state;
  MARIA_BLOB *blobs;			/* Pointer to blobs */
  MARIA_BIT_BUFF bit_buff;
  /* accumulate indexfile changes between write's */
  TREE *bulk_insert;
  DYNAMIC_ARRAY *ft1_to_ft2;		/* used only in ft1->ft2 conversion */
  MYSQL_FTPARSER_PARAM *ftparser_param;	/* share info between init/deinit */
  char *filename;			/* parameter to open filename */
  uchar *buff,				/* Temp area for key */
   *lastkey, *lastkey2;			/* Last used search key */
  uchar *first_mbr_key;			/* Searhed spatial key */
  byte *rec_buff;			/* Tempbuff for recordpack */
  uchar *int_keypos,			/* Save position for next/previous */
   *int_maxpos;				/* -""- */
  uint int_nod_flag;			/* -""- */
  uint32 int_keytree_version;		/* -""- */
  int(*read_record) (struct st_maria_info *, my_off_t, byte *);
  invalidator_by_filename invalidator;	/* query cache invalidator */
  ulong this_unique;			/* uniq filenumber or thread */
  ulong last_unique;			/* last unique number */
  ulong this_loop;			/* counter for this open */
  ulong last_loop;			/* last used counter */
  my_off_t lastpos,			/* Last record position */
    nextpos;				/* Position to next record */
  my_off_t save_lastpos;
  my_off_t pos;				/* Intern variable */
  my_off_t last_keypage;		/* Last key page read */
  my_off_t last_search_keypage;		/* Last keypage when searching */
  my_off_t dupp_key_pos;
  ha_checksum checksum;
  /*
    QQ: the folloing two xxx_length fields should be removed,
     as they are not compatible with parallel repair
  */
  ulong packed_length, blob_length;	/* Length of found, packed record */
  int dfile;				/* The datafile */
  uint opt_flag;			/* Optim. for space/speed */
  uint update;				/* If file changed since open */
  int lastinx;				/* Last used index */
  uint lastkey_length;			/* Length of key in lastkey */
  uint last_rkey_length;		/* Last length in maria_rkey() */
  enum ha_rkey_function last_key_func;	/* CONTAIN, OVERLAP, etc */
  uint save_lastkey_length;
  uint pack_key_length;			/* For MARIAMRG */
  int errkey;				/* Got last error on this key */
  int lock_type;			/* How database was locked */
  int tmp_lock_type;			/* When locked by readinfo */
  uint data_changed;			/* Somebody has changed data */
  uint save_update;			/* When using KEY_READ */
  int save_lastinx;
  LIST open_list;
  IO_CACHE rec_cache;			/* When cacheing records */
  uint preload_buff_size;		/* When preloading indexes */
  myf lock_wait;			/* is 0 or MY_DONT_WAIT */
  my_bool was_locked;			/* Was locked in panic */
  my_bool append_insert_at_end;		/* Set if concurrent insert */
  my_bool quick_mode;
  /* If info->buff can't be used for rnext */
  my_bool page_changed;
  /* If info->buff has to be reread for rnext */
  my_bool buff_used;
  my_bool once_flags;			/* For MARIAMRG */
#ifdef THREAD
  THR_LOCK_DATA lock;
#endif
  uchar *maria_rtree_recursion_state;		/* For RTREE */
  int maria_rtree_recursion_depth;
};

/* Some defines used by isam-funktions */

#define USE_WHOLE_KEY	HA_MAX_KEY_BUFF*2 /* Use whole key in _search() */
#define F_EXTRA_LCK	-1

/* bits in opt_flag */
#define MEMMAP_USED	32
#define REMEMBER_OLD_POS 64

#define WRITEINFO_UPDATE_KEYFILE	1
#define WRITEINFO_NO_UNLOCK		2

/* once_flags */
#define USE_PACKED_KEYS         1
#define RRND_PRESERVE_LASTINX   2

/* bits in state.changed */

#define STATE_CHANGED		1
#define STATE_CRASHED		2
#define STATE_CRASHED_ON_REPAIR 4
#define STATE_NOT_ANALYZED	8
#define STATE_NOT_OPTIMIZED_KEYS 16
#define STATE_NOT_SORTED_PAGES	32

/* options to maria_read_cache */

#define READING_NEXT	1
#define READING_HEADER	2

#define maria_getint(x)	((uint) mi_uint2korr(x) & 32767)
#define maria_putint(x,y,nod) { uint16 boh=(nod ? (uint16) 32768 : 0) + (uint16) (y);\
			  mi_int2store(x,boh); }
#define _ma_test_if_nod(x) (x[0] & 128 ? info->s->base.key_reflength : 0)
#define maria_mark_crashed(x) (x)->s->state.changed|=STATE_CRASHED
#define maria_mark_crashed_on_repair(x) { (x)->s->state.changed|=STATE_CRASHED|STATE_CRASHED_ON_REPAIR ; (x)->update|= HA_STATE_CHANGED; }
#define maria_is_crashed(x) ((x)->s->state.changed & STATE_CRASHED)
#define maria_is_crashed_on_repair(x) ((x)->s->state.changed & STATE_CRASHED_ON_REPAIR)
#define maria_print_error(SHARE, ERRNO)                     \
        _ma_report_error((ERRNO), (SHARE)->index_file_name)

/* Functions to store length of space packed keys, VARCHAR or BLOB keys */

#define store_key_length(key,length) \
{ if ((length) < 255) \
  { *(key)=(length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); } \
}

#define get_key_full_length(length,key) \
{ if ((uchar) *(key) != 255) \
    length= ((uint) (uchar) *((key)++))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; (key)+=3; } \
}

#define get_key_full_length_rdonly(length,key) \
{ if ((uchar) *(key) != 255) \
    length= ((uint) (uchar) *((key)))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; } \
}

#define get_pack_length(length) ((length) >= 255 ? 3 : 1)

#define MARIA_MIN_BLOCK_LENGTH	20		/* Because of delete-link */
/* Don't use to small record-blocks */
#define MARIA_EXTEND_BLOCK_LENGTH	20
#define MARIA_SPLIT_LENGTH	((MARIA_EXTEND_BLOCK_LENGTH+4)*2)
	/* Max prefix of record-block */
#define MARIA_MAX_DYN_BLOCK_HEADER	20
#define MARIA_BLOCK_INFO_HEADER_LENGTH 20
#define MARIA_DYN_DELETE_BLOCK_HEADER 20    /* length of delete-block-header */
#define MARIA_DYN_MAX_BLOCK_LENGTH	((1L << 24)-4L)
#define MARIA_DYN_MAX_ROW_LENGTH	(MARIA_DYN_MAX_BLOCK_LENGTH - MARIA_SPLIT_LENGTH)
#define MARIA_DYN_ALIGN_SIZE	  4	/* Align blocks on this */
#define MARIA_MAX_DYN_HEADER_BYTE 13	/* max header byte for dynamic rows */
#define MARIA_MAX_BLOCK_LENGTH	((((ulong) 1 << 24)-1) & (~ (ulong) (MARIA_DYN_ALIGN_SIZE-1)))
#define MARIA_REC_BUFF_OFFSET      ALIGN_SIZE(MARIA_DYN_DELETE_BLOCK_HEADER+sizeof(uint32))

#define MEMMAP_EXTRA_MARGIN	7	/* Write this as a suffix for file */

#define PACK_TYPE_SELECTED	1	/* Bits in field->pack_type */
#define PACK_TYPE_SPACE_FIELDS	2
#define PACK_TYPE_ZERO_FILL	4
#define MARIA_FOUND_WRONG_KEY 32738	/* Impossible value from ha_key_cmp */

#define MARIA_MAX_KEY_BLOCK_SIZE	(MARIA_MAX_KEY_BLOCK_LENGTH/MARIA_MIN_KEY_BLOCK_LENGTH)
#define MARIA_BLOCK_SIZE(key_length,data_pointer,key_pointer) (((((key_length)+(data_pointer)+(key_pointer))*4+(key_pointer)+2)/maria_block_size+1)*maria_block_size)
#define MARIA_MAX_KEYPTR_SIZE	5	/* For calculating block lengths */
#define MARIA_MIN_KEYBLOCK_LENGTH 50	/* When to split delete blocks */

#define MARIA_MIN_SIZE_BULK_INSERT_TREE 16384	/* this is per key */
#define MARIA_MIN_ROWS_TO_USE_BULK_INSERT 100
#define MARIA_MIN_ROWS_TO_DISABLE_INDEXES 100
#define MARIA_MIN_ROWS_TO_USE_WRITE_CACHE 10

/* The UNIQUE check is done with a hashed long key */

#define MARIA_UNIQUE_HASH_TYPE	HA_KEYTYPE_ULONG_INT
#define maria_unique_store(A,B)    mi_int4store((A),(B))

#ifdef THREAD
extern pthread_mutex_t THR_LOCK_maria;
#endif
#if !defined(THREAD) || defined(DONT_USE_RW_LOCKS)
#define rw_wrlock(A) {}
#define rw_rdlock(A) {}
#define rw_unlock(A) {}
#endif

	/* Some extern variables */

extern LIST *maria_open_list;
extern uchar NEAR maria_file_magic[], NEAR maria_pack_file_magic[];
extern uint NEAR maria_read_vec[], NEAR maria_readnext_vec[];
extern uint maria_quick_table_bits;

	/* This is used by _ma_calc_xxx_key_length och _ma_store_key */

typedef struct st_maria_s_param
{
  uint ref_length, key_length,
    n_ref_length,
    n_length, totlength, part_of_prev_key, prev_length, pack_marker;
  uchar *key, *prev_key, *next_key_pos;
  bool store_not_null;
} MARIA_KEY_PARAM;

	/* Prototypes for intern functions */

extern int _ma_read_dynamic_record(MARIA_HA *info, my_off_t filepos,
                                   byte *buf);
extern int _ma_write_dynamic_record(MARIA_HA *, const byte *);
extern int _ma_update_dynamic_record(MARIA_HA *, my_off_t, const byte *);
extern int _ma_delete_dynamic_record(MARIA_HA *info);
extern int _ma_cmp_dynamic_record(MARIA_HA *info, const byte *record);
extern int _ma_read_rnd_dynamic_record(MARIA_HA *, byte *, my_off_t,
                                       my_bool);
extern int _ma_write_blob_record(MARIA_HA *, const byte *);
extern int _ma_update_blob_record(MARIA_HA *, my_off_t, const byte *);
extern int _ma_read_static_record(MARIA_HA *info, my_off_t filepos,
                                  byte *buf);
extern int _ma_write_static_record(MARIA_HA *, const byte *);
extern int _ma_update_static_record(MARIA_HA *, my_off_t, const byte *);
extern int _ma_delete_static_record(MARIA_HA *info);
extern int _ma_cmp_static_record(MARIA_HA *info, const byte *record);
extern int _ma_read_rnd_static_record(MARIA_HA *, byte *, my_off_t, my_bool);
extern int _ma_ck_write(MARIA_HA *info, uint keynr, uchar *key,
                        uint length);
extern int _ma_ck_real_write_btree(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                                   uchar *key, uint key_length,
                                   my_off_t *root, uint comp_flag);
extern int _ma_enlarge_root(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                            uchar *key, my_off_t *root);
extern int _ma_insert(MARIA_HA *info, MARIA_KEYDEF *keyinfo, uchar *key,
                      uchar *anc_buff, uchar *key_pos, uchar *key_buff,
                      uchar *father_buff, uchar *father_keypos,
                      my_off_t father_page, my_bool insert_last);
extern int _ma_split_page(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                          uchar *key, uchar *buff, uchar *key_buff,
                          my_bool insert_last);
extern uchar *_ma_find_half_pos(uint nod_flag, MARIA_KEYDEF *keyinfo,
                                uchar *page, uchar *key,
                                uint *return_key_length,
                                uchar ** after_key);
extern int _ma_calc_static_key_length(MARIA_KEYDEF *keyinfo, uint nod_flag,
                                      uchar *key_pos, uchar *org_key,
                                      uchar *key_buff, uchar *key,
                                      MARIA_KEY_PARAM *s_temp);
extern int _ma_calc_var_key_length(MARIA_KEYDEF *keyinfo, uint nod_flag,
                                   uchar *key_pos, uchar *org_key,
                                   uchar *key_buff, uchar *key,
                                   MARIA_KEY_PARAM *s_temp);
extern int _ma_calc_var_pack_key_length(MARIA_KEYDEF *keyinfo,
                                        uint nod_flag, uchar *key_pos,
                                        uchar *org_key, uchar *prev_key,
                                        uchar *key,
                                        MARIA_KEY_PARAM *s_temp);
extern int _ma_calc_bin_pack_key_length(MARIA_KEYDEF *keyinfo,
                                        uint nod_flag, uchar *key_pos,
                                        uchar *org_key, uchar *prev_key,
                                        uchar *key,
                                        MARIA_KEY_PARAM *s_temp);
void _ma_store_static_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                          MARIA_KEY_PARAM *s_temp);
void _ma_store_var_pack_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                            MARIA_KEY_PARAM *s_temp);
#ifdef NOT_USED
void _ma_store_pack_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                        MARIA_KEY_PARAM *s_temp);
#endif
void _ma_store_bin_pack_key(MARIA_KEYDEF *keyinfo, uchar *key_pos,
                            MARIA_KEY_PARAM *s_temp);

extern int _ma_ck_delete(MARIA_HA *info, uint keynr, uchar *key,
                         uint key_length);
extern int _ma_readinfo(MARIA_HA *info, int lock_flag, int check_keybuffer);
extern int _ma_writeinfo(MARIA_HA *info, uint options);
extern int _ma_test_if_changed(MARIA_HA *info);
extern int _ma_mark_file_changed(MARIA_HA *info);
extern int _ma_decrement_open_count(MARIA_HA *info);
extern int _ma_check_index(MARIA_HA *info, int inx);
extern int _ma_search(MARIA_HA *info, MARIA_KEYDEF *keyinfo, uchar *key,
                      uint key_len, uint nextflag, my_off_t pos);
extern int _ma_bin_search(struct st_maria_info *info, MARIA_KEYDEF *keyinfo,
                          uchar *page, uchar *key, uint key_len,
                          uint comp_flag, uchar **ret_pos, uchar *buff,
                          my_bool *was_last_key);
extern int _ma_seq_search(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                          uchar *page, uchar *key, uint key_len,
                          uint comp_flag, uchar ** ret_pos, uchar *buff,
                          my_bool *was_last_key);
extern int _ma_prefix_search(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                             uchar *page, uchar *key, uint key_len,
                             uint comp_flag, uchar ** ret_pos, uchar *buff,
                             my_bool *was_last_key);
extern my_off_t _ma_kpos(uint nod_flag, uchar *after_key);
extern void _ma_kpointer(MARIA_HA *info, uchar *buff, my_off_t pos);
extern my_off_t _ma_dpos(MARIA_HA *info, uint nod_flag, uchar *after_key);
extern my_off_t _ma_rec_pos(MARIA_SHARE *info, uchar *ptr);
extern void _ma_dpointer(MARIA_HA *info, uchar *buff, my_off_t pos);
extern uint _ma_get_static_key(MARIA_KEYDEF *keyinfo, uint nod_flag,
                               uchar **page, uchar *key);
extern uint _ma_get_pack_key(MARIA_KEYDEF *keyinfo, uint nod_flag,
                             uchar **page, uchar *key);
extern uint _ma_get_binary_pack_key(MARIA_KEYDEF *keyinfo, uint nod_flag,
                                    uchar ** page_pos, uchar *key);
extern uchar *_ma_get_last_key(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                               uchar *keypos, uchar *lastkey,
                               uchar *endpos, uint *return_key_length);
extern uchar *_ma_get_key(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                          uchar *page, uchar *key, uchar *keypos,
                          uint *return_key_length);
extern uint _ma_keylength(MARIA_KEYDEF *keyinfo, uchar *key);
extern uint _ma_keylength_part(MARIA_KEYDEF *keyinfo, register uchar *key,
                               HA_KEYSEG *end);
extern uchar *_ma_move_key(MARIA_KEYDEF *keyinfo, uchar *to, uchar *from);
extern int _ma_search_next(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                           uchar *key, uint key_length, uint nextflag,
                           my_off_t pos);
extern int _ma_search_first(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                            my_off_t pos);
extern int _ma_search_last(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                           my_off_t pos);
extern uchar *_ma_fetch_keypage(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                                my_off_t page, int level, uchar *buff,
                                int return_buffer);
extern int _ma_write_keypage(MARIA_HA *info, MARIA_KEYDEF *keyinfo,
                             my_off_t page, int level, uchar *buff);
extern int _ma_dispose(MARIA_HA *info, MARIA_KEYDEF *keyinfo, my_off_t pos,
                       int level);
extern my_off_t _ma_new(MARIA_HA *info, MARIA_KEYDEF *keyinfo, int level);
extern uint _ma_make_key(MARIA_HA *info, uint keynr, uchar *key,
                         const byte *record, my_off_t filepos);
extern uint _ma_pack_key(MARIA_HA *info, uint keynr, uchar *key,
                         uchar *old, uint key_length,
                         HA_KEYSEG ** last_used_keyseg);
extern int _ma_read_key_record(MARIA_HA *info, my_off_t filepos,
                               byte *buf);
extern int _ma_read_cache(IO_CACHE *info, byte *buff, my_off_t pos,
                          uint length, int re_read_if_possibly);
extern void _ma_update_auto_increment(MARIA_HA *info, const byte *record);

extern byte *_ma_alloc_rec_buff(MARIA_HA *, ulong, byte **);
#define _ma_get_rec_buff_ptr(info,buf)                        \
  ((((info)->s->options & HA_OPTION_PACK_RECORD) && (buf)) ?    \
   (buf) - MARIA_REC_BUFF_OFFSET : (buf))
#define _ma_get_rec_buff_len(info,buf)                \
  (*((uint32 *)(_ma_get_rec_buff_ptr(info,buf))))

extern ulong _ma_rec_unpack(MARIA_HA *info, byte *to, byte *from,
                            ulong reclength);
extern my_bool _ma_rec_check(MARIA_HA *info, const char *record,
                             byte *packpos, ulong packed_length,
                             my_bool with_checkum);
extern int _ma_write_part_record(MARIA_HA *info, my_off_t filepos,
                                 ulong length, my_off_t next_filepos,
                                 byte ** record, ulong *reclength,
                                 int *flag);
extern void _ma_print_key(FILE *stream, HA_KEYSEG *keyseg,
                          const uchar *key, uint length);
extern my_bool _ma_read_pack_info(MARIA_HA *info, pbool fix_keys);
extern int _ma_read_pack_record(MARIA_HA *info, my_off_t filepos,
                                byte *buf);
extern int _ma_read_rnd_pack_record(MARIA_HA *, byte *, my_off_t, my_bool);
extern int _ma_pack_rec_unpack(MARIA_HA *info, byte *to, byte *from,
                               ulong reclength);
extern ulonglong _ma_safe_mul(ulonglong a, ulonglong b);
extern int _ma_ft_update(MARIA_HA *info, uint keynr, byte *keybuf,
                         const byte *oldrec, const byte *newrec,
                         my_off_t pos);

/* Parameter to _ma_get_block_info */

typedef struct st_maria_block_info
{
  uchar header[MARIA_BLOCK_INFO_HEADER_LENGTH];
  ulong rec_len;
  ulong data_len;
  ulong block_len;
  ulong blob_len;
  my_off_t filepos;
  my_off_t next_filepos;
  my_off_t prev_filepos;
  uint second_read;
  uint offset;
} MARIA_BLOCK_INFO;

/* bits in return from _ma_get_block_info */

#define BLOCK_FIRST	1
#define BLOCK_LAST	2
#define BLOCK_DELETED	4
#define BLOCK_ERROR	8			/* Wrong data */
#define BLOCK_SYNC_ERROR 16			/* Right data at wrong place */
#define BLOCK_FATAL_ERROR 32			/* hardware-error */

#define NEED_MEM	((uint) 10*4*(IO_SIZE+32)+32) /* Nead for recursion */
#define MAXERR			20
#define BUFFERS_WHEN_SORTING	16		/* Alloc for sort-key-tree */
#define WRITE_COUNT		MY_HOW_OFTEN_TO_WRITE
#define INDEX_TMP_EXT		".TMM"
#define DATA_TMP_EXT		".TMD"

#define UPDATE_TIME		1
#define UPDATE_STAT		2
#define UPDATE_SORT		4
#define UPDATE_AUTO_INC		8
#define UPDATE_OPEN_COUNT	16

#define USE_BUFFER_INIT		(((1024L*512L-MALLOC_OVERHEAD)/IO_SIZE)*IO_SIZE)
#define READ_BUFFER_INIT	(1024L*256L-MALLOC_OVERHEAD)
#define SORT_BUFFER_INIT	(2048L*1024L-MALLOC_OVERHEAD)
#define MIN_SORT_BUFFER		(4096-MALLOC_OVERHEAD)

#define fast_ma_writeinfo(INFO) if (!(INFO)->s->tot_locks) (void) _ma_writeinfo((INFO),0)
#define fast_ma_readinfo(INFO) ((INFO)->lock_type == F_UNLCK) && _ma_readinfo((INFO),F_RDLCK,1)

extern uint _ma_get_block_info(MARIA_BLOCK_INFO *, File, my_off_t);
extern uint _ma_rec_pack(MARIA_HA *info, byte *to, const byte *from);
extern uint _ma_pack_get_block_info(MARIA_HA *, MARIA_BLOCK_INFO *, File,
                                    my_off_t);
extern void _ma_store_blob_length(byte *pos, uint pack_length, uint length);
extern void _ma_report_error(int errcode, const char *file_name);
extern my_bool _ma_memmap_file(MARIA_HA *info);
extern void _ma_unmap_file(MARIA_HA *info);
extern uint _ma_save_pack_length(uint version, byte * block_buff,
                                 ulong length);
extern uint _ma_calc_pack_length(uint version, ulong length);
extern ulong _ma_calc_blob_length(uint length, const byte *pos);
extern uint _ma_mmap_pread(MARIA_HA *info, byte *Buffer,
                           uint Count, my_off_t offset, myf MyFlags);
extern uint _ma_mmap_pwrite(MARIA_HA *info, byte *Buffer,
                            uint Count, my_off_t offset, myf MyFlags);
extern uint _ma_nommap_pread(MARIA_HA *info, byte *Buffer,
                             uint Count, my_off_t offset, myf MyFlags);
extern uint _ma_nommap_pwrite(MARIA_HA *info, byte *Buffer,
                              uint Count, my_off_t offset, myf MyFlags);

uint _ma_state_info_write(File file, MARIA_STATE_INFO *state, uint pWrite);
uchar *_ma_state_info_read(uchar *ptr, MARIA_STATE_INFO *state);
uint _ma_state_info_read_dsk(File file, MARIA_STATE_INFO *state,
                             my_bool pRead);
uint _ma_base_info_write(File file, MARIA_BASE_INFO *base);
uchar *_ma_n_base_info_read(uchar *ptr, MARIA_BASE_INFO *base);
int _ma_keyseg_write(File file, const HA_KEYSEG *keyseg);
char *_ma_keyseg_read(char *ptr, HA_KEYSEG *keyseg);
uint _ma_keydef_write(File file, MARIA_KEYDEF *keydef);
char *_ma_keydef_read(char *ptr, MARIA_KEYDEF *keydef);
uint _ma_uniquedef_write(File file, MARIA_UNIQUEDEF *keydef);
char *_ma_uniquedef_read(char *ptr, MARIA_UNIQUEDEF *keydef);
uint _ma_recinfo_write(File file, MARIA_COLUMNDEF *recinfo);
char *_ma_recinfo_read(char *ptr, MARIA_COLUMNDEF *recinfo);
ulong _ma_calc_total_blob_length(MARIA_HA *info, const byte *record);
ha_checksum _ma_checksum(MARIA_HA *info, const byte *buf);
ha_checksum _ma_static_checksum(MARIA_HA *info, const byte *buf);
my_bool _ma_check_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                         byte *record, ha_checksum unique_hash,
                         my_off_t pos);
ha_checksum _ma_unique_hash(MARIA_UNIQUEDEF *def, const byte *buf);
int _ma_cmp_static_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                          const byte *record, my_off_t pos);
int _ma_cmp_dynamic_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                           const byte *record, my_off_t pos);
int _ma_unique_comp(MARIA_UNIQUEDEF *def, const byte *a, const byte *b,
                    my_bool null_are_equal);
void _ma_get_status(void *param, int concurrent_insert);
void _ma_update_status(void *param);
void _ma_copy_status(void *to, void *from);
my_bool _ma_check_status(void *param);

extern MARIA_HA *_ma_test_if_reopen(char *filename);
my_bool _ma_check_table_is_closed(const char *name, const char *where);
int _ma_open_datafile(MARIA_HA *info, MARIA_SHARE *share, File file_to_dup);
int _ma_open_keyfile(MARIA_SHARE *share);
void _ma_setup_functions(register MARIA_SHARE *share);
my_bool _ma_dynmap_file(MARIA_HA *info, my_off_t size);
void _ma_remap_file(MARIA_HA *info, my_off_t size);

/* Functions needed by _ma_check (are overrided in MySQL) */
C_MODE_START
volatile int *_ma_killed_ptr(HA_CHECK *param);
void _ma_check_print_error _VARARGS((HA_CHECK *param, const char *fmt, ...));
void _ma_check_print_warning _VARARGS((HA_CHECK *param, const char *fmt, ...));
void _ma_check_print_info _VARARGS((HA_CHECK *param, const char *fmt, ...));
C_MODE_END

int _ma_flush_pending_blocks(MARIA_SORT_PARAM *param);
int _ma_sort_ft_buf_flush(MARIA_SORT_PARAM *sort_param);
int _ma_thr_write_keys(MARIA_SORT_PARAM *sort_param);
#ifdef THREAD
pthread_handler_t _ma_thr_find_all_keys(void *arg);
#endif
int _ma_flush_blocks(HA_CHECK *param, KEY_CACHE *key_cache, File file);

int _ma_sort_write_record(MARIA_SORT_PARAM *sort_param);
int _ma_create_index_by_sort(MARIA_SORT_PARAM *info, my_bool no_messages,
                             ulong);
