/*
   Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* This file is included by all internal myisam files */

/**
  @file storage/myisam/myisamdef.h
*/

#ifndef STORAGE_MYISAM_MYISAMDEF_INCLUDED
#define STORAGE_MYISAM_MYISAMDEF_INCLUDED 1

#include "my_alloc.h"
#include "my_checksum.h"  // ha_checksum
#include "my_icp.h"
#include "my_pointer_arithmetic.h"
#include "myisam.h"     /* Structs & some defines */
#include "myisampack.h" /* packing of keys */
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/mysql_thread.h"
#include "thr_lock.h"

struct HA_KEYSEG;
struct KEY_CACHE;
struct MI_INFO;
struct MYSQL_FTPARSER_PARAM;
struct TREE;
struct st_keycache_thread_var;

/* undef map from my_nosys; We need test-if-disk full */
#if defined(my_write)
#undef my_write
#endif

struct MI_STATUS_INFO {
  ha_rows records;    /* Rows in table */
  ha_rows del;        /* Removed rows */
  my_off_t empty;     /* lost space in datafile */
  my_off_t key_empty; /* lost space in indexfile */
  my_off_t key_file_length;
  my_off_t data_file_length;
  ha_checksum checksum;
  bool uncacheable; /* Active concurrent insert */
};

struct MI_STATE_INFO {
  struct { /* Fileheader */
    uchar file_version[4];
    uchar options[2];
    uchar header_length[2];
    uchar state_info_length[2];
    uchar base_info_length[2];
    uchar base_pos[2];
    uchar key_parts[2];         /* Key parts */
    uchar unique_key_parts[2];  /* Key parts + unique parts */
    uchar keys;                 /* number of keys in file */
    uchar uniques;              /* number of UNIQUE definitions */
    uchar language;             /* Language (collation) for indexes */
    uchar max_block_size_index; /* max keyblock size */
    uchar fulltext_keys;
    uchar not_used; /* To align to 8 */
  } header;

  MI_STATUS_INFO state;
  ha_rows split;    /* number of split blocks */
  my_off_t dellink; /* Link to next removed block */
  ulonglong auto_increment;
  ulong process;      /* process that updated table last */
  ulong unique;       /* Unique number for this process */
  ulong update_count; /* Updated for each write lock */
  ulong status;
  ulong *rec_per_key_part;
  my_off_t *key_root;        /* Start of key trees */
  my_off_t *key_del;         /* delete links for trees */
  my_off_t rec_per_key_rows; /* Rows when calculating rec_per_key */

  ulong sec_index_changed; /* Updated when new sec_index */
  ulong sec_index_used;    /* which extra index are in use */
  ulonglong key_map;       /* Which keys are in use */
  ha_checksum checksum;    /* Table checksum */
  ulong version;           /* timestamp of create */
  time_t create_time;      /* Time when created database */
  time_t recover_time;     /* Time for last recover */
  time_t check_time;       /* Time for last check */
  uint sortkey;            /* sorted by this key  (not used) */
  uint open_count;
  uint8 changed; /* Changed since myisamchk */

  /* the following isn't saved on disk */
  uint state_diff_length; /* Should be 0 */
  uint state_length;      /* Length of state header in file */
  ulong *key_info;
};

#define MI_STATE_INFO_SIZE (24 + 14 * 8 + 7 * 4 + 2 * 2 + 8)
#define MI_STATE_KEY_SIZE 8
#define MI_STATE_KEYBLOCK_SIZE 8
#define MI_STATE_KEYSEG_SIZE 4
#define MI_STATE_EXTRA_SIZE                                   \
  ((MI_MAX_KEY + MI_MAX_KEY_BLOCK_SIZE) * MI_STATE_KEY_SIZE + \
   MI_MAX_KEY * MI_MAX_KEY_SEG * MI_STATE_KEYSEG_SIZE)
#define MI_KEYDEF_SIZE (2 + 5 * 2)
#define MI_UNIQUEDEF_SIZE (2 + 1 + 1)
#define HA_KEYSEG_SIZE (6 + 2 * 2 + 4 * 2)
#define MI_COLUMNDEF_SIZE (2 * 3 + 1)
#define MI_BASE_INFO_SIZE (5 * 8 + 8 * 4 + 4 + 4 * 2 + 16)
#define MI_INDEX_BLOCK_MARGIN 16 /* Safety margin for .MYI tables */

struct MI_BASE_INFO {
  my_off_t keystart; /* Start of keys */
  my_off_t max_data_file_length;
  my_off_t max_key_file_length;
  my_off_t margin_key_file_length;
  ha_rows records, reloc; /* Create information */
  ulong mean_row_length;  /* Create information */
  ulong reclength;        /* length of unpacked record */
  ulong pack_reclength;   /* Length of full packed rec. */
  ulong min_pack_length;
  ulong max_pack_length; /* Max possibly length of packed rec.*/
  ulong min_block_length;
  ulong fields,              /* fields in table */
      pack_fields;           /* packed fields in table */
  uint rec_reflength;        /* = 2-8 */
  uint key_reflength;        /* = 2-8 */
  uint keys;                 /* same as in state.header */
  uint auto_key;             /* Which key-1 is a auto key */
  uint blobs;                /* Number of blobs */
  uint pack_bits;            /* Length of packed bits */
  uint max_key_block_length; /* Max block length */
  uint max_key_length;       /* Max key length */
  /* Extra allocation when using dynamic record format */
  uint extra_alloc_bytes;
  uint extra_alloc_procent;
  /* The following are from the header */
  uint key_parts, all_key_parts;
};

/* Structs used intern in database */

struct MI_BLOB /* Info of record */
{
  ulong offset;     /* Offset to blob in record */
  uint pack_length; /* Type of packed length */
  ulong length;     /* Calc:ed for each record */
};

struct MI_PACK {
  ulong header_length;
  uint ref_length;
  uchar version;
};

#define MAX_NONMAPPED_INSERTS 1000

struct MYISAM_SHARE { /* Shared between opens */
  MI_STATE_INFO state;
  MI_BASE_INFO base;
  MI_KEYDEF ft2_keyinfo;    /* Second-level ft-key definition */
  MI_KEYDEF *keyinfo;       /* Key definitions */
  MI_UNIQUEDEF *uniqueinfo; /* unique definitions */
  HA_KEYSEG *keyparts;      /* key part info */
  MI_COLUMNDEF *rec;        /* Pointer to field information */
  MI_PACK pack;             /* Data about packed records */
  MI_BLOB *blobs;           /* Pointer to blobs */
  LIST *in_use;             /* List of threads using this table */
  char *unique_file_name;   /* realpath() of index file */
  char *data_file_name,     /* Resolved path names from symlinks */
      *index_file_name;
  uchar *file_map;      /* mem-map of file if possible */
  KEY_CACHE *key_cache; /* ref to the current key cache */
  MI_DECODE_TREE *decode_trees;
  uint16 *decode_tables;
  int (*read_record)(MI_INFO *, my_off_t, uchar *);
  int (*write_record)(MI_INFO *, const uchar *);
  int (*update_record)(MI_INFO *, my_off_t, const uchar *);
  int (*delete_record)(MI_INFO *);
  int (*read_rnd)(MI_INFO *, uchar *, my_off_t, bool);
  int (*compare_record)(MI_INFO *, const uchar *);
  /* Function to use for a row checksum. */
  ha_checksum (*calc_checksum)(MI_INFO *, const uchar *);
  int (*compare_unique)(MI_INFO *, MI_UNIQUEDEF *, const uchar *record,
                        my_off_t pos);
  size_t (*file_read)(MI_INFO *, uchar *, size_t, my_off_t, myf);
  size_t (*file_write)(MI_INFO *, const uchar *, size_t, my_off_t, myf);
  ulong this_process;    /* processid */
  ulong last_process;    /* For table-change-check */
  ulong last_version;    /* Version on start */
  ulong options;         /* Options used */
  ulong min_pack_length; /* These are used by packed data */
  ulong max_pack_length;
  ulong state_diff_length;
  uint rec_reflength; /* rec_reflength in use now */
  uint unique_name_length;
  uint32 ftkeys;                    /* Number of full-text keys + 1 */
  File kfile;                       /* Shared keyfile */
  File data_file;                   /* Shared data file */
  int mode;                         /* mode of file on open */
  uint reopen;                      /* How many times reopened */
  uint w_locks, r_locks, tot_locks; /* Number of read/write locks */
  uint blocksize;                   /* blocksize of keyfile */
  myf write_flag;
  enum data_file_type data_file_type;
  /* Below flag is needed to make log tables work with concurrent insert */
  bool is_log_table;

  bool changed,       /* If changed since lock */
      global_changed, /* If changed since open */
      not_flushed, temporary, delay_key_write, concurrent_insert, have_rtree;

  THR_LOCK lock;
  mysql_mutex_t intern_lock; /* Locking for use with _locking */
  mysql_rwlock_t *key_root_lock;
  my_off_t mmaped_length;
  uint nonmmaped_inserts; /* counter of writing in non-mmaped
                             area */
  mysql_rwlock_t mmap_lock;
};

typedef uint mi_bit_type;

struct MI_BIT_BUFF { /* Used for packing of record */
  mi_bit_type current_byte;
  uint bits;
  uchar *pos, *end, *blob_pos, *blob_end;
  uint error;
};

typedef ICP_RESULT (*index_cond_func_t)(void *param);

struct MI_INFO {
  MYISAM_SHARE *s; /* Shared between open:s */
  MI_STATUS_INFO *state, save_state;
  MI_BLOB *blobs; /* Pointer to blobs */
  MI_BIT_BUFF bit_buff;
  /* accumulate indexfile changes between write's */
  TREE *bulk_insert;
  DYNAMIC_ARRAY *ft1_to_ft2;            /* used only in ft1->ft2 conversion */
  MEM_ROOT ft_memroot;                  /* used by the parser               */
  MYSQL_FTPARSER_PARAM *ftparser_param; /* share info between init/deinit   */
  LIST in_use;                          /* Thread using this table          */
  char *filename;                       /* parameter to open filename       */
  uchar *buff,                          /* Temp area for key                */
      *lastkey, *lastkey2;              /* Last used search key             */

  /* Key used in mi_rnext_same and filled by mi_rkey. */
  uchar *rnext_same_key;
  uchar *first_mbr_key;       /* Searhed spatial key              */
  uchar *rec_buff;            /* Tempbuff for recordpack          */
  uchar *int_keypos,          /* Save position for next/previous  */
      *int_maxpos;            /*  -""-  */
  uint int_nod_flag;          /*  -""-  */
  uint32 int_keytree_version; /*  -""-  */
  int (*read_record)(MI_INFO *, my_off_t, uchar *);
  ulong this_unique; /* uniq filenumber or thread */
  ulong last_unique; /* last unique number */
  ulong this_loop;   /* counter for this open */
  ulong last_loop;   /* last used counter */
  my_off_t lastpos,  /* Last record position */
      nextpos;       /* Position to next record */
  my_off_t save_lastpos;
  my_off_t pos;                 /* Intern variable */
  my_off_t last_keypage;        /* Last key page read */
  my_off_t last_search_keypage; /* Last keypage when searching */
  my_off_t dupp_key_pos;
  ha_checksum checksum; /* Temp storage for row checksum */
  /* QQ: the following two xxx_length fields should be removed,
     as they are not compatible with parallel repair */
  ulong packed_length, blob_length;    /* Length of found, packed record */
  int dfile;                           /* The datafile */
  uint opt_flag;                       /* Optim. for space/speed */
  uint update;                         /* If file changed since open */
  int lastinx;                         /* Last used index */
  uint lastkey_length;                 /* Length of key in lastkey */
  uint last_rkey_length;               /* Last length in mi_rkey() */
  enum ha_rkey_function last_key_func; /* CONTAIN, OVERLAP, etc */
  uint save_lastkey_length;
  uint pack_key_length;    /* For MYISAMMRG */
  uint16 last_used_keyseg; /* For MyISAMMRG */
  int errkey;              /* Got last error on this key */
  int lock_type;           /* How database was locked */
  int tmp_lock_type;       /* When locked by readinfo */
  uint data_changed;       /* Somebody has changed data */
  uint save_update;        /* When using KEY_READ */
  int save_lastinx;
  LIST open_list;
  IO_CACHE rec_cache;        /* When caching records */
  uint preload_buff_size;    /* When preloading indexes */
  myf lock_wait;             /* is 0 or MY_DONT_WAIT */
  bool was_locked;           /* Was locked in panic */
  bool append_insert_at_end; /* Set if concurrent insert */
  bool quick_mode;
  bool page_changed; /* If info->buff can't be used for rnext */
  bool buff_used;    /* If info->buff has to be reread for rnext */
  uint8 once_flags;  /* For MYISAMMRG */

  /* Used in mi_rnext_same to fill rnext_same_key for the first time. */
  bool set_rnext_same_key;

  index_cond_func_t index_cond_func; /* Index condition function */
  void *index_cond_func_arg;         /* parameter for the func */
#ifdef _WIN32
  bool owned_by_merge; /* This MyISAM table is part of a merge union */
#endif
  THR_LOCK_DATA lock;
  uchar *rtree_recursion_state; /* For RTREE */
  int rtree_recursion_depth;
};

struct BUFFPEK {
  my_off_t file_pos; /* Where we are in the sort file */
  uchar *base, *key; /* Key pointers */
  ha_rows count;     /* Number of rows in table */
  ulong mem_count;   /* numbers of keys in memory */
  ulong max_keys;    /* Max keys in buffert */
};

struct MI_SORT_PARAM {
  IO_CACHE read_cache, tempfile, tempfile_for_exceptions;
  DYNAMIC_ARRAY buffpek;
  MI_BIT_BUFF bit_buff; /* For sort repair of packrec. */

  /*
    The next two are used to collect statistics, see update_key_parts for
    description.
  */
  ulonglong unique[MI_MAX_KEY_SEG + 1];
  ulonglong notnull[MI_MAX_KEY_SEG + 1];
  ulonglong sortbuff_size;

  my_off_t pos, max_pos, filepos, start_recpos;
  uint key, key_length, real_key_length;
  uint maxbuffers, keys, find_length, sort_keys_length;
  bool fix_datafile, master;
  bool calc_checksum; /* calculate table checksum */
  MI_KEYDEF *keyinfo;
  HA_KEYSEG *seg;
  SORT_INFO *sort_info;
  uchar **sort_keys;
  uchar *rec_buff;
  void *wordlist, *wordptr;
  MEM_ROOT wordroot;
  uchar *record;
  MY_TMPDIR *tmpdir;
  int (*key_cmp)(void *, uchar *, uchar *);
  int (*key_read)(MI_SORT_PARAM *, void *);
  int (*key_write)(MI_SORT_PARAM *, const void *);
  int (*write_keys)(MI_SORT_PARAM *, uchar **, uint, BUFFPEK *, IO_CACHE *);
  uint (*read_to_buffer)(IO_CACHE *, BUFFPEK *, uint);
  int (*write_key)(MI_SORT_PARAM *, IO_CACHE *, uchar *, uint, uint);
};

/* Some defines used by isam-funktions */

#define USE_WHOLE_KEY MI_MAX_KEY_BUFF * 2 /* Use whole key in _mi_search() */
#define F_EXTRA_LCK -1

/* bits in opt_flag */
#define MEMMAP_USED 32
#define REMEMBER_OLD_POS 64

#define WRITEINFO_UPDATE_KEYFILE 1
#define WRITEINFO_NO_UNLOCK 2

/* once_flags */
#define USE_PACKED_KEYS 1
#define RRND_PRESERVE_LASTINX 2

/* bits in state.changed */

#define STATE_CHANGED 1
#define STATE_CRASHED 2
#define STATE_CRASHED_ON_REPAIR 4
#define STATE_NOT_ANALYZED 8
#define STATE_NOT_OPTIMIZED_KEYS 16
#define STATE_NOT_SORTED_PAGES 32

/* options to mi_read_cache */

#define READING_NEXT 1
#define READING_HEADER 2

#define mi_getint(x) ((uint)mi_uint2korr(x) & 32767)
#define mi_putint(x, y, nod)                              \
  {                                                       \
    uint16 boh = (nod ? (uint16)32768 : 0) + (uint16)(y); \
    mi_int2store(x, boh);                                 \
  }
#define mi_test_if_nod(x) (x[0] & 128 ? info->s->base.key_reflength : 0)
#define mi_report_crashed(A, B) _mi_report_crashed((A), (B), __FILE__, __LINE__)
#define mi_mark_crashed(x)                         \
  do {                                             \
    (x)->s->state.changed |= STATE_CRASHED;        \
    DBUG_PRINT("error", ("Marked table crashed")); \
    mi_report_crashed((x), 0);                     \
  } while (0)
#define mi_mark_crashed_on_repair(x)                                  \
  do {                                                                \
    (x)->s->state.changed |= STATE_CRASHED | STATE_CRASHED_ON_REPAIR; \
    (x)->update |= HA_STATE_CHANGED;                                  \
    DBUG_PRINT("error", ("Marked table crashed"));                    \
  } while (0)
#define mi_is_crashed(x) ((x)->s->state.changed & STATE_CRASHED)
#define mi_is_crashed_on_repair(x) \
  ((x)->s->state.changed & STATE_CRASHED_ON_REPAIR)
#define mi_print_error(SHARE, ERRNO) \
  mi_report_error((ERRNO), (SHARE)->index_file_name)

/* Functions to store length of space packed keys, VARCHAR or BLOB keys */

#define store_key_length(key, length)    \
  {                                      \
    if ((length) < 255) {                \
      *(key) = (length);                 \
    } else {                             \
      *(key) = 255;                      \
      mi_int2store((key) + 1, (length)); \
    }                                    \
  }

#define get_key_full_length(length, key)        \
  {                                             \
    if ((uchar) * (key) != 255)                 \
      length = ((uint)(uchar) * ((key)++)) + 1; \
    else {                                      \
      length = mi_uint2korr((key) + 1) + 3;     \
      (key) += 3;                               \
    }                                           \
  }

#define get_key_full_length_rdonly(length, key) \
  {                                             \
    if ((uchar) * (key) != 255)                 \
      length = ((uint)(uchar) * ((key))) + 1;   \
    else {                                      \
      length = mi_uint2korr((key) + 1) + 3;     \
    }                                           \
  }

#define get_pack_length(length) ((length) >= 255 ? 3 : 1)

#define MI_MIN_BLOCK_LENGTH 20U   /* Because of delete-link */
#define MI_EXTEND_BLOCK_LENGTH 20 /* Don't use to small record-blocks */
#define MI_SPLIT_LENGTH ((MI_EXTEND_BLOCK_LENGTH + 4) * 2)
#define MI_MAX_DYN_BLOCK_HEADER 20 /* Max prefix of record-block */
#define MI_BLOCK_INFO_HEADER_LENGTH 20
#define MI_DYN_DELETE_BLOCK_HEADER 20 /* length of delete-block-header */
#define MI_DYN_MAX_BLOCK_LENGTH ((1L << 24) - 4L)
#define MI_DYN_MAX_ROW_LENGTH (MI_DYN_MAX_BLOCK_LENGTH - MI_SPLIT_LENGTH)
#define MI_DYN_ALIGN_SIZE 4       /* Align blocks on this */
#define MI_MAX_DYN_HEADER_BYTE 13 /* max header byte for dynamic rows */
#define MI_MAX_BLOCK_LENGTH \
  ((((ulong)1 << 24) - 1) & (~(ulong)(MI_DYN_ALIGN_SIZE - 1)))
#define MI_REC_BUFF_OFFSET \
  ALIGN_SIZE(MI_DYN_DELETE_BLOCK_HEADER + sizeof(uint32))

#define PACK_TYPE_SELECTED 1 /* Bits in field->pack_type */
#define PACK_TYPE_SPACE_FIELDS 2
#define PACK_TYPE_ZERO_FILL 4
#define MI_FOUND_WRONG_KEY 32738 /* Impossible value from ha_key_cmp */

#define MI_MAX_KEY_BLOCK_SIZE \
  (MI_MAX_KEY_BLOCK_LENGTH / MI_MIN_KEY_BLOCK_LENGTH)
#define MI_BLOCK_SIZE(key_length, data_pointer, key_pointer, block_size)   \
  (((((key_length) + (data_pointer) + (key_pointer)) * 4 + (key_pointer) + \
     2) /                                                                  \
        (block_size) +                                                     \
    1) *                                                                   \
   (block_size))
#define MI_MAX_KEYPTR_SIZE 5      /* For calculating block lengths */
#define MI_MIN_KEYBLOCK_LENGTH 50 /* When to split delete blocks */

#define MI_MIN_SIZE_BULK_INSERT_TREE 16384 /* this is per key */
#define MI_MIN_ROWS_TO_USE_BULK_INSERT 100
#define MI_MIN_ROWS_TO_DISABLE_INDEXES 100
#define MI_MIN_ROWS_TO_USE_WRITE_CACHE 10

/* The UNIQUE check is done with a hashed long key */

#define MI_UNIQUE_HASH_TYPE HA_KEYTYPE_ULONG_INT
#define mi_unique_store(A, B) mi_int4store((A), (B))

extern mysql_mutex_t THR_LOCK_myisam;

#ifdef __cplusplus
extern "C" {
#endif

/* Some extern variables */

extern LIST *myisam_open_list;
extern uchar myisam_file_magic[], myisam_pack_file_magic[];
extern uint myisam_read_vec[], myisam_readnext_vec[];
extern uint myisam_quick_table_bits;
extern File myisam_log_file;
extern ulong myisam_pid;

/* This is used by _mi_calc_xxx_key_length och _mi_store_key */

struct MI_KEY_PARAM {
  uint ref_length, key_length, n_ref_length, n_length, totlength,
      part_of_prev_key, prev_length, pack_marker;
  const uchar *key;
  uchar *prev_key, *next_key_pos;
  bool store_not_null;
};

/* Prototypes for intern functions */

extern int _mi_read_dynamic_record(MI_INFO *info, my_off_t filepos, uchar *buf);
extern int _mi_write_dynamic_record(MI_INFO *, const uchar *);
extern int _mi_update_dynamic_record(MI_INFO *, my_off_t, const uchar *);
extern int _mi_delete_dynamic_record(MI_INFO *info);
extern int _mi_cmp_dynamic_record(MI_INFO *info, const uchar *record);
extern int _mi_read_rnd_dynamic_record(MI_INFO *, uchar *, my_off_t, bool);
extern int _mi_write_blob_record(MI_INFO *, const uchar *);
extern int _mi_update_blob_record(MI_INFO *, my_off_t, const uchar *);
extern int _mi_read_static_record(MI_INFO *info, my_off_t filepos, uchar *buf);
extern int _mi_write_static_record(MI_INFO *, const uchar *);
extern int _mi_update_static_record(MI_INFO *, my_off_t, const uchar *);
extern int _mi_delete_static_record(MI_INFO *info);
extern int _mi_cmp_static_record(MI_INFO *info, const uchar *record);
extern int _mi_read_rnd_static_record(MI_INFO *, uchar *, my_off_t, bool);
extern int _mi_ck_write(MI_INFO *info, uint keynr, uchar *key, uint length);
extern int _mi_ck_real_write_btree(MI_INFO *info, MI_KEYDEF *keyinfo,
                                   uchar *key, uint key_length, my_off_t *root,
                                   uint comp_flag);
extern int _mi_enlarge_root(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                            my_off_t *root);
extern int _mi_insert(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                      uchar *anc_buff, uchar *key_pos, uchar *key_buff,
                      uchar *father_buff, uchar *father_keypos,
                      my_off_t father_page, bool insert_last);
extern int _mi_split_page(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                          uchar *buff, uchar *key_buff, bool insert_last);
extern uchar *_mi_find_half_pos(uint nod_flag, MI_KEYDEF *keyinfo, uchar *page,
                                uchar *key, uint *return_key_length,
                                uchar **after_key);
extern int _mi_calc_static_key_length(MI_KEYDEF *keyinfo, uint nod_flag,
                                      const uchar *key_pos, uchar *org_key,
                                      uchar *key_buff, const uchar *key,
                                      MI_KEY_PARAM *s_temp);
extern int _mi_calc_var_key_length(MI_KEYDEF *keyinfo, uint nod_flag,
                                   const uchar *key_pos, uchar *org_key,
                                   uchar *key_buff, const uchar *key,
                                   MI_KEY_PARAM *s_temp);
extern int _mi_calc_var_pack_key_length(MI_KEYDEF *keyinfo, uint nod_flag,
                                        const uchar *key_pos, uchar *org_key,
                                        uchar *prev_key, const uchar *key,
                                        MI_KEY_PARAM *s_temp);
extern int _mi_calc_bin_pack_key_length(MI_KEYDEF *keyinfo, uint nod_flag,
                                        const uchar *key_pos, uchar *org_key,
                                        uchar *prev_key, const uchar *key,
                                        MI_KEY_PARAM *s_temp);
void _mi_store_static_key(MI_KEYDEF *keyinfo, uchar *key_pos,
                          MI_KEY_PARAM *s_temp);
void _mi_store_var_pack_key(MI_KEYDEF *keyinfo, uchar *key_pos,
                            MI_KEY_PARAM *s_temp);
void _mi_store_bin_pack_key(MI_KEYDEF *keyinfo, uchar *key_pos,
                            MI_KEY_PARAM *s_temp);

extern int _mi_ck_delete(MI_INFO *info, uint keynr, uchar *key,
                         uint key_length);
extern int _mi_readinfo(MI_INFO *info, int lock_flag, int check_keybuffer);
extern int _mi_writeinfo(MI_INFO *info, uint options);
extern int _mi_test_if_changed(MI_INFO *info);
extern int _mi_mark_file_changed(MI_INFO *info);
extern int _mi_decrement_open_count(MI_INFO *info);
extern int _mi_check_index(MI_INFO *info, int inx);
extern int _mi_search(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                      uint key_len, uint nextflag, my_off_t pos);
extern int _mi_bin_search(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                          uchar *key, uint key_len, uint comp_flag,
                          uchar **ret_pos, uchar *buff, bool *was_last_key);
extern int _mi_seq_search(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                          uchar *key, uint key_len, uint comp_flag,
                          uchar **ret_pos, uchar *buff, bool *was_last_key);
extern int _mi_prefix_search(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                             uchar *key, uint key_len, uint comp_flag,
                             uchar **ret_pos, uchar *buff, bool *was_last_key);
extern my_off_t _mi_kpos(uint nod_flag, uchar *after_key);
extern void _mi_kpointer(MI_INFO *info, uchar *buff, my_off_t pos);
extern my_off_t _mi_dpos(MI_INFO *info, uint nod_flag, const uchar *after_key);
extern my_off_t _mi_rec_pos(MYISAM_SHARE *info, uchar *ptr);
extern void _mi_dpointer(MI_INFO *info, uchar *buff, my_off_t pos);
extern uint _mi_get_static_key(MI_KEYDEF *keyinfo, uint nod_flag, uchar **page,
                               uchar *key);
extern uint _mi_get_pack_key(MI_KEYDEF *keyinfo, uint nod_flag, uchar **page,
                             uchar *key);
extern uint _mi_get_binary_pack_key(MI_KEYDEF *keyinfo, uint nod_flag,
                                    uchar **page_pos, uchar *key);
extern uchar *_mi_get_last_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *keypos,
                               uchar *lastkey, uchar *endpos,
                               uint *return_key_length);
extern uchar *_mi_get_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
                          uchar *key, uchar *keypos, uint *return_key_length);
extern uint _mi_keylength(MI_KEYDEF *keyinfo, const uchar *key);
extern uint _mi_keylength_part(MI_KEYDEF *keyinfo, const uchar *key,
                               HA_KEYSEG *end);
extern uchar *_mi_move_key(MI_KEYDEF *keyinfo, uchar *to, const uchar *from);
extern int _mi_search_next(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *key,
                           uint key_length, uint nextflag, my_off_t pos);
extern int _mi_search_first(MI_INFO *info, MI_KEYDEF *keyinfo, my_off_t pos);
extern int _mi_search_last(MI_INFO *info, MI_KEYDEF *keyinfo, my_off_t pos);
extern uchar *_mi_fetch_keypage(MI_INFO *info, MI_KEYDEF *keyinfo,
                                my_off_t page, int level, uchar *buff,
                                int return_buffer);
extern int _mi_write_keypage(MI_INFO *info, MI_KEYDEF *keyinfo, my_off_t page,
                             int level, uchar *buff);
extern int _mi_dispose(MI_INFO *info, MI_KEYDEF *keyinfo, my_off_t pos,
                       int level);
extern my_off_t _mi_new(MI_INFO *info, MI_KEYDEF *keyinfo, int level);
extern uint _mi_make_key(MI_INFO *info, uint keynr, uchar *key,
                         const uchar *record, my_off_t filepos);
extern uint _mi_pack_key(MI_INFO *info, uint keynr, uchar *key,
                         const uchar *old, key_part_map keypart_map,
                         HA_KEYSEG **last_used_keyseg);
extern int _mi_read_key_record(MI_INFO *info, my_off_t filepos, uchar *buf);
extern int _mi_read_cache(IO_CACHE *info, uchar *buff, my_off_t pos,
                          uint length, int re_read_if_possibly);
extern ulonglong retrieve_auto_increment(MI_INFO *info, const uchar *record);

extern uchar *mi_alloc_rec_buff(MI_INFO *, ulong, uchar **);
#define mi_get_rec_buff_ptr(info, buf)                     \
  ((((info)->s->options & HA_OPTION_PACK_RECORD) && (buf)) \
       ? (buf)-MI_REC_BUFF_OFFSET                          \
       : (buf))
#define mi_get_rec_buff_len(info, buf) \
  (*((uint32 *)(mi_get_rec_buff_ptr(info, buf))))

extern size_t _mi_rec_unpack(MI_INFO *info, uchar *to, const uchar *from,
                             ulong reclength);
extern bool _mi_rec_check(MI_INFO *info, const uchar *record, uchar *packpos,
                          ulong packed_length, bool with_checkum);
extern int _mi_write_part_record(MI_INFO *info, my_off_t filepos, ulong length,
                                 my_off_t next_filepos, uchar **record,
                                 ulong *reclength, int *flag);
extern void _mi_print_key(FILE *stream, HA_KEYSEG *keyseg, const uchar *key,
                          uint length);
extern bool _mi_read_pack_info(MI_INFO *info, bool fix_keys);
extern int _mi_read_pack_record(MI_INFO *info, my_off_t filepos, uchar *buf);
extern int _mi_read_rnd_pack_record(MI_INFO *, uchar *, my_off_t, bool);
extern int _mi_pack_rec_unpack(MI_INFO *info, MI_BIT_BUFF *bit_buff, uchar *to,
                               uchar *from, ulong reclength);
extern ulonglong mi_safe_mul(ulonglong a, ulonglong b);
extern int _mi_ft_update(MI_INFO *info, uint keynr, uchar *keybuf,
                         const uchar *oldrec, const uchar *newrec,
                         my_off_t pos);

#ifdef __cplusplus
}
#endif

struct MI_BLOCK_INFO { /* Parameter to _mi_get_block_info */
  uchar header[MI_BLOCK_INFO_HEADER_LENGTH];
  ulong rec_len;
  ulong data_len;
  ulong block_len;
  ulong blob_len;
  my_off_t filepos;
  my_off_t next_filepos;
  my_off_t prev_filepos;
  uint second_read;
  uint offset;
};

/* bits in return from _mi_get_block_info */

#define BLOCK_FIRST 1
#define BLOCK_LAST 2
#define BLOCK_DELETED 4
#define BLOCK_ERROR 8        /* Wrong data */
#define BLOCK_SYNC_ERROR 16  /* Right data at wrong place */
#define BLOCK_FATAL_ERROR 32 /* hardware-error */

#define NEED_MEM                                               \
  ((uint)10 * 4 * (IO_SIZE + 32) + 32) /* Needed for recursion \
                                        */
#define MAXERR 20
#define BUFFERS_WHEN_SORTING 16    /* Alloc for sort-key-tree */
#define MY_HOW_OFTEN_TO_WRITE 1000 /* How often we want info on screen */
#define WRITE_COUNT MY_HOW_OFTEN_TO_WRITE
#define INDEX_TMP_EXT ".TMM"
#define DATA_TMP_EXT ".TMD"

#define UPDATE_TIME 1
#define UPDATE_STAT 2
#define UPDATE_SORT 4
#define UPDATE_AUTO_INC 8
#define UPDATE_OPEN_COUNT 16

#define USE_BUFFER_INIT (((1024L * 512L - MALLOC_OVERHEAD) / IO_SIZE) * IO_SIZE)
#define READ_BUFFER_INIT (1024L * 256L - MALLOC_OVERHEAD)
#define SORT_BUFFER_INIT (2048L * 1024L - MALLOC_OVERHEAD)
#define MIN_SORT_BUFFER (4096ULL - MALLOC_OVERHEAD)

enum myisam_log_commands {
  MI_LOG_OPEN,
  MI_LOG_WRITE,
  MI_LOG_UPDATE,
  MI_LOG_DELETE,
  MI_LOG_CLOSE,
  MI_LOG_EXTRA,
  MI_LOG_LOCK,
  MI_LOG_DELETE_ALL
};

#define myisam_log(a, b, c, d) \
  if (myisam_log_file >= 0) _myisam_log(a, b, c, d)
#define myisam_log_command(a, b, c, d, e) \
  if (myisam_log_file >= 0) _myisam_log_command(a, b, c, d, e)
#define myisam_log_record(a, b, c, d, e) \
  if (myisam_log_file >= 0) _myisam_log_record(a, b, c, d, e)

#define fast_mi_writeinfo(INFO) \
  if (!(INFO)->s->tot_locks) (void)_mi_writeinfo((INFO), 0)
#define fast_mi_readinfo(INFO) \
  ((INFO)->lock_type == F_UNLCK) && _mi_readinfo((INFO), F_RDLCK, 1)

#ifdef __cplusplus
extern "C" {
#endif

extern uint _mi_get_block_info(MI_BLOCK_INFO *, File, my_off_t);
extern uint _mi_rec_pack(MI_INFO *info, uchar *to, const uchar *from);
extern uint _mi_pack_get_block_info(MI_INFO *myisam, MI_BIT_BUFF *bit_buff,
                                    MI_BLOCK_INFO *info, uchar **rec_buff_p,
                                    File file, my_off_t filepos);
extern void _mi_store_blob_length(uchar *pos, uint pack_length, uint length);
extern void _myisam_log(enum myisam_log_commands command, MI_INFO *info,
                        const uchar *buffert, uint length);
extern void _myisam_log_command(enum myisam_log_commands command, MI_INFO *info,
                                const uchar *buffert, uint length, int result);
extern void _myisam_log_record(enum myisam_log_commands command, MI_INFO *info,
                               const uchar *record, my_off_t filepos,
                               int result);
extern void mi_report_error(int errcode, const char *file_name);
extern bool _mi_memmap_file(MI_INFO *info);
extern void _mi_unmap_file(MI_INFO *info);
extern uint save_pack_length(uint version, uchar *block_buff, ulong length);
extern uint read_pack_length(uint version, const uchar *buf, ulong *length);
extern uint calc_pack_length(uint version, ulong length);
extern size_t mi_mmap_pread(MI_INFO *info, uchar *Buffer, size_t Count,
                            my_off_t offset, myf MyFlags);
extern size_t mi_mmap_pwrite(MI_INFO *info, const uchar *Buffer, size_t Count,
                             my_off_t offset, myf MyFlags);
extern size_t mi_nommap_pread(MI_INFO *info, uchar *Buffer, size_t Count,
                              my_off_t offset, myf MyFlags);
extern size_t mi_nommap_pwrite(MI_INFO *info, const uchar *Buffer, size_t Count,
                               my_off_t offset, myf MyFlags);

uint mi_state_info_write(File file, MI_STATE_INFO *state, uint pWrite);
uchar *mi_state_info_read(uchar *ptr, MI_STATE_INFO *state);
uint mi_state_info_read_dsk(File file, MI_STATE_INFO *state, bool pRead);
uint mi_base_info_write(File file, MI_BASE_INFO *base);
uchar *my_n_base_info_read(uchar *ptr, MI_BASE_INFO *base);
int mi_keyseg_write(File file, const HA_KEYSEG *keyseg);
uchar *mi_keyseg_read(uchar *ptr, HA_KEYSEG *keyseg);
uint mi_keydef_write(File file, MI_KEYDEF *keydef);
uchar *mi_keydef_read(uchar *ptr, MI_KEYDEF *keydef);
uint mi_uniquedef_write(File file, MI_UNIQUEDEF *keydef);
uchar *mi_uniquedef_read(uchar *ptr, MI_UNIQUEDEF *keydef);
uint mi_recinfo_write(File file, MI_COLUMNDEF *recinfo);
uchar *mi_recinfo_read(uchar *ptr, MI_COLUMNDEF *recinfo);
extern int mi_disable_indexes(MI_INFO *info);
extern int mi_enable_indexes(MI_INFO *info);
extern int mi_indexes_are_disabled(MI_INFO *info);
ulong _my_calc_total_blob_length(MI_INFO *info, const uchar *record);
ha_checksum mi_checksum(MI_INFO *info, const uchar *buf);
ha_checksum mi_static_checksum(MI_INFO *info, const uchar *buf);
bool mi_check_unique(MI_INFO *info, MI_UNIQUEDEF *def, uchar *record,
                     ha_checksum unique_hash, my_off_t pos);
ha_checksum mi_unique_hash(MI_UNIQUEDEF *def, const uchar *buf);
int _mi_cmp_static_unique(MI_INFO *info, MI_UNIQUEDEF *def, const uchar *record,
                          my_off_t pos);
int _mi_cmp_dynamic_unique(MI_INFO *info, MI_UNIQUEDEF *def,
                           const uchar *record, my_off_t pos);
int mi_unique_comp(MI_UNIQUEDEF *def, const uchar *a, const uchar *b,
                   bool null_are_equal);
void mi_get_status(void *param, int concurrent_insert);
void mi_update_status(void *param);
void mi_restore_status(void *param);
void mi_copy_status(void *to, void *from);
bool mi_check_status(void *param);

extern MI_INFO *test_if_reopen(char *filename);
bool check_table_is_closed(const char *name, const char *where);
int mi_open_datafile(MI_INFO *info, MYISAM_SHARE *share, const char *orn_name,
                     File file_to_dup);

int mi_open_keyfile(MYISAM_SHARE *share);
void mi_setup_functions(MYISAM_SHARE *share);
bool mi_dynmap_file(MI_INFO *info, my_off_t size);
int mi_munmap_file(MI_INFO *info);
void mi_remap_file(MI_INFO *info, my_off_t size);
void _mi_report_crashed(MI_INFO *file, const char *message, const char *sfile,
                        uint sline);

int mi_check_index_cond(MI_INFO *info, uint keynr, uchar *record);

/* Functions needed by mi_check */
volatile int *killed_ptr(MI_CHECK *param);
void mi_check_print_error(MI_CHECK *param, const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));
void mi_check_print_warning(MI_CHECK *param, const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));
void mi_check_print_info(MI_CHECK *param, const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));
int flush_pending_blocks(MI_SORT_PARAM *param);
int sort_ft_buf_flush(MI_SORT_PARAM *sort_param);
int flush_blocks(MI_CHECK *param, KEY_CACHE *key_cache, File file);

int sort_write_record(MI_SORT_PARAM *sort_param);
int _create_index_by_sort(MI_SORT_PARAM *info, bool no_messages, ulonglong);

extern void mi_set_index_cond_func(MI_INFO *info, index_cond_func_t func,
                                   void *func_arg);

#ifdef __cplusplus
extern thread_local st_keycache_thread_var *keycache_tls;
}
#endif

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key mi_key_mutex_MYISAM_SHARE_intern_lock;

extern PSI_rwlock_key mi_key_rwlock_MYISAM_SHARE_key_root_lock,
    mi_key_rwlock_MYISAM_SHARE_mmap_lock;

extern PSI_cond_key mi_key_cond_MI_SORT_INFO_cond,
    mi_keycache_thread_var_suspend;

extern PSI_file_key mi_key_file_datatmp, mi_key_file_dfile, mi_key_file_kfile,
    mi_key_file_log;

extern PSI_thread_key mi_key_thread_find_all_keys;

void init_myisam_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

extern PSI_memory_key mi_key_memory_MYISAM_SHARE;
extern PSI_memory_key mi_key_memory_MI_INFO;
extern PSI_memory_key mi_key_memory_MI_INFO_ft1_to_ft2;
extern PSI_memory_key mi_key_memory_MI_INFO_bulk_insert;
extern PSI_memory_key mi_key_memory_record_buffer;
extern PSI_memory_key mi_key_memory_FTB;
extern PSI_memory_key mi_key_memory_FT_INFO;
extern PSI_memory_key mi_key_memory_FTPARSER_PARAM;
extern PSI_memory_key mi_key_memory_ft_memroot;
extern PSI_memory_key mi_key_memory_ft_stopwords;
extern PSI_memory_key mi_key_memory_MI_SORT_PARAM;
extern PSI_memory_key mi_key_memory_MI_SORT_PARAM_wordroot;
extern PSI_memory_key mi_key_memory_SORT_FT_BUF;
extern PSI_memory_key mi_key_memory_SORT_KEY_BLOCKS;
extern PSI_memory_key mi_key_memory_filecopy;
extern PSI_memory_key mi_key_memory_SORT_INFO_buffer;
extern PSI_memory_key mi_key_memory_MI_DECODE_TREE;
extern PSI_memory_key mi_key_memory_MYISAM_SHARE_decode_tables;
extern PSI_memory_key mi_key_memory_preload_buffer;
extern PSI_memory_key mi_key_memory_stPageList_pages;
extern PSI_memory_key mi_key_memory_keycache_thread_var;
extern PSI_memory_key key_memory_QUEUE;

#endif  // STORAGE_MYISAM_MYISAMDEF_INCLUDED
