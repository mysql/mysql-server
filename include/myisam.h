/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* This file should be included when using myisam_funktions */

#ifndef _myisam_h
#define _myisam_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _my_base_h
#include <my_base.h>
#endif
#ifndef _m_ctype_h
#include <m_ctype.h>
#endif

	/* defines used by myisam-funktions */

/* The following defines can be increased if necessary */
#define MI_MAX_KEY	32		/* Max allowed keys */
#define MI_MAX_KEY_SEG	16		/* Max segments for key */
#define MI_MAX_KEY_LENGTH 500

#define MI_MAX_KEY_BUFF  (MI_MAX_KEY_LENGTH+MI_MAX_KEY_SEG*6+8+8)
#define MI_MAX_POSSIBLE_KEY_BUFF (1024+6+6)	/* For myisam_chk */
#define MI_MAX_POSSIBLE_KEY	64		/* For myisam_chk */
#define MI_MAX_MSG_BUF      1024 /* used in CHECK TABLE, REPAIR TABLE */
#define MI_NAME_IEXT	".MYI"
#define MI_NAME_DEXT	".MYD"
/* Max extra space to use when sorting keys */
#define MI_MAX_TEMP_LENGTH	256*1024L*1024L

#define mi_portable_sizeof_char_ptr 8

typedef uint32 ha_checksum;

	/* Param to/from mi_info */

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
  uint  keys;				/* Number of keys in use */
  uint	options;			/* HA_OPTIONS_... used */
  int	errkey,				/* With key was dupplicated on err */
	sortkey;			/* clustered by this key */
  File	filenr;				/* (uniq) filenr for datafile */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;
  uint  reflength;
  ulong record_offset;
  ulong *rec_per_key;			/* for sql optimizing */
  uint raid_type,raid_chunks;
  ulong raid_chunksize;
} MI_ISAMINFO;


typedef struct st_mi_create_info
{
  ha_rows max_rows;
  ha_rows reloc_rows;
  ulonglong auto_increment;
  ulonglong data_file_length;
  uint raid_type,raid_chunks;
  ulong raid_chunksize;
  uint old_options;
  uint8 language;
} MI_CREATE_INFO;

struct st_myisam_info;			/* For referense */
typedef struct st_myisam_info MI_INFO;

typedef struct st_mi_keyseg		/* Key-portion */
{
  uint8  type;				/* Type of key (for sort) */
  uint8  language;
  uint8  null_bit;			/* bitmask to test for NULL */
  uint8  bit_start,bit_end;		/* if bit field */
  uint16 flag;
  uint16 length;			/* Keylength */
  uint32 start;				/* Start of key in record */
  uint32 null_pos;			/* position to NULL indicator */
  CHARSET_INFO *charset;
} MI_KEYSEG;


struct st_mi_s_param;

typedef struct st_mi_keydef		/* Key definition with open & info */
{
  uint16 keysegs;			/* Number of key-segment */
  uint16 flag;				/* NOSAME, PACK_USED */

  uint16 block_length;			/* Length of keyblock (auto) */
  uint16 underflow_block_length;	/* When to execute underflow */
  uint16 keylength;			/* Tot length of keyparts (auto) */
  uint16 minlength;			/* min length of (packed) key (auto) */
  uint16 maxlength;			/* max length of (packed) key (auto) */
  uint16 block_size;			/* block_size (auto) */
  uint32 version;			/* For concurrent read/write */

  MI_KEYSEG *seg,*end;
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
} MI_KEYDEF;


#define MI_UNIQUE_HASH_LENGTH	4

typedef struct st_unique_def		/* Segment definition of unique */
{
  uint16 keysegs;			/* Number of key-segment */
  uchar key;				/* Mapped to which key */
  uint8 null_are_equal;
  MI_KEYSEG *seg,*end;
} MI_UNIQUEDEF;

typedef struct st_mi_decode_tree	/* Decode huff-table */
{
  uint16 *table;
  uint	 quick_table_bits;
  byte	 *intervalls;
} MI_DECODE_TREE;


struct st_mi_bit_buff;

/* Note that null markers should always be first in a row !
   When creating a column, one should only specify:
   type, length, null_bit and null_pos */

typedef struct st_columndef		/* column information */
{
  int16  type;				/* en_fieldtype */
  uint16 length;			/* length of field */
  uint32 offset;			/* Offset to position in row */
  uint8  null_bit;			/* If column may be 0 */
  uint16 null_pos;			/* position for null marker */

#ifndef NOT_PACKED_DATABASES
  void (*unpack)(struct st_columndef *rec,struct st_mi_bit_buff *buff,
		 uchar *start,uchar *end);
  enum en_fieldtype base_type;
  uint space_length_bits,pack_type;
  MI_DECODE_TREE *huff_tree;
#endif
} MI_COLUMNDEF;


extern my_string myisam_log_filename;		/* Name of logfile */
extern uint myisam_block_size;
extern my_bool myisam_flush,myisam_delay_key_write;
extern my_bool myisam_concurrent_insert;
extern my_off_t myisam_max_temp_length,myisam_max_extra_temp_length;

	/* Prototypes for myisam-functions */

extern int mi_close(struct st_myisam_info *file);
extern int mi_delete(struct st_myisam_info *file,const byte *buff);
extern struct st_myisam_info *mi_open(const char *name,int mode,
				      uint wait_if_locked);
extern int mi_panic(enum ha_panic_function function);
extern int mi_rfirst(struct st_myisam_info *file,byte *buf,int inx);
extern int mi_rkey(struct st_myisam_info *file,byte *buf,int inx,
		   const byte *key,
		   uint key_len, enum ha_rkey_function search_flag);
extern int mi_rlast(struct st_myisam_info *file,byte *buf,int inx);
extern int mi_rnext(struct st_myisam_info *file,byte *buf,int inx);
extern int mi_rnext_same(struct st_myisam_info *info, byte *buf);
extern int mi_rprev(struct st_myisam_info *file,byte *buf,int inx);
extern int mi_rrnd(struct st_myisam_info *file,byte *buf, my_off_t pos);
extern int mi_scan_init(struct st_myisam_info *file);
extern int mi_scan(struct st_myisam_info *file,byte *buf);
extern int mi_rsame(struct st_myisam_info *file,byte *record,int inx);
extern int mi_rsame_with_pos(struct st_myisam_info *file,byte *record,
			     int inx, my_off_t pos);
extern int mi_update(struct st_myisam_info *file,const byte *old,
		     byte *new_record);
extern int mi_write(struct st_myisam_info *file,byte *buff);
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
		    enum ha_extra_function function);
extern ha_rows mi_records_in_range(struct st_myisam_info *info,int inx,
				   const byte *start_key,uint start_key_len,
				   enum ha_rkey_function start_search_flag,
				   const byte *end_key,uint end_key_len,
				   enum ha_rkey_function end_search_flag);
extern int mi_log(int activate_log);
extern int mi_is_changed(struct st_myisam_info *info);
extern int mi_delete_all_rows(struct st_myisam_info *info);
extern ulong _mi_calc_blob_length(uint length , const byte *pos);
extern uint mi_get_pointer_length(ulonglong file_length, uint def);

/* this is used to pass to mysql_myisamchk_table -- by Sasha Pachev */

#define   MYISAMCHK_REPAIR 1  /* equivalent to myisamchk -r*/
#define   MYISAMCHK_VERIFY 2  /* run equivalent of myisamchk -c,
			       * if corruption is detected, do myisamchk -r*/

/* definitions needed for myisamchk.c -- by Sasha Pachev */

#define T_VERBOSE	1
#define T_SILENT	2
#define T_DESCRIPT	4
#define T_EXTEND	8
#define T_INFO		16
#define T_REP		32
#define T_OPT		64		/* Not currently used */
#define T_FORCE_CREATE	128
#define T_WRITE_LOOP	256
#define T_UNPACK	512
#define T_STATISTICS	1024
#define T_VERY_SILENT	2048
#define T_SORT_RECORDS	4096
#define T_SORT_INDEX	8192
#define T_WAIT_FOREVER	16384
#define T_REP_BY_SORT	32768L
#define T_FAST		65536L
#define T_READONLY	131072L
#define T_MEDIUM	T_READONLY*2
#define T_AUTO_INC	T_MEDIUM*2
#define T_CHECK		T_AUTO_INC*2
#define T_UPDATE_STATE		T_CHECK*2
#define T_CHECK_ONLY_CHANGED	T_UPDATE_STATE*2
#define T_DONT_CHECK_CHECKSUM	T_CHECK_ONLY_CHANGED*2
#define T_TRUST_HEADER		T_DONT_CHECK_CHECKSUM*2
#define T_CREATE_MISSING_KEYS	T_TRUST_HEADER*2
#define T_SAFE_REPAIR		T_CREATE_MISSING_KEYS*2
#define T_AUTO_REPAIR   	T_SAFE_REPAIR*2
#define T_BACKUP_DATA		T_AUTO_REPAIR*2

#define O_NEW_INDEX	1		/* Bits set in out_flag */
#define O_NEW_DATA	2
#define O_DATA_LOST	4

/* these struct is used by my_check to tell it what to do */

typedef struct st_sort_key_blocks {		/* Used when sorting */
  uchar *buff,*end_pos;
  uchar lastkey[MI_MAX_POSSIBLE_KEY_BUFF];
  uint last_length;
  int inited;
} SORT_KEY_BLOCKS;

struct st_mi_check_param;

typedef struct st_sort_info {
  MI_INFO *info;
  struct st_mi_check_param *param;
  enum data_file_type new_data_file_type;
  SORT_KEY_BLOCKS *key_block,*key_block_end;
  uint key,find_length;
  my_off_t pos,max_pos,filepos,start_recpos,filelength,dupp,buff_length;
  ha_rows max_records;
  ulonglong unique[MI_MAX_KEY_SEG+1];
  my_bool fix_datafile;
  char *record,*buff;
  MI_KEYDEF *keyinfo;
  MI_KEYSEG *keyseg;
} SORT_INFO;


typedef struct st_mi_check_param
{
  ulonglong auto_increment_value;
  ulonglong max_data_file_length;
  ulonglong keys_in_use;
  my_off_t search_after_block;
  my_off_t new_file_pos,key_file_blocks;
  my_off_t keydata,totaldata,key_blocks,start_check_pos;
  ha_rows total_records,total_deleted;
  ha_checksum record_checksum,glob_crc;
  ulong	use_buffers,read_buffer_length,write_buffer_length,
	sort_buffer_length,sort_key_blocks;
  uint out_flag,warning_printed,error_printed,
       opt_rep_quick,verbose;
  uint opt_sort_key,total_files,max_level;
  uint testflag;
  uint8 language;
  my_bool using_global_keycache, opt_lock_memory, opt_follow_links;
  my_bool retry_repair,retry_without_quick, force_sort;
  char temp_filename[FN_REFLEN],*isam_file_name,*tmpdir;
  int tmpfile_createflag;
  myf myf_rw;
  IO_CACHE read_cache;
  SORT_INFO sort_info;
  ulonglong unique_count[MI_MAX_KEY_SEG+1];
  ha_checksum key_crc[MI_MAX_POSSIBLE_KEY];
  ulong rec_per_key_part[MI_MAX_KEY_SEG*MI_MAX_POSSIBLE_KEY];
  void* thd;
  char* table_name;
  char* op_name;
} MI_CHECK;


typedef struct st_mi_sortinfo {
  ha_rows max_records;
  SORT_INFO *sort_info;
  char *tmpdir;
  int (*key_cmp)(SORT_INFO *info, const void *, const void *);
  int (*key_read)(SORT_INFO *info,void *buff);
  int (*key_write)(SORT_INFO *info, const void *buff);
  void (*lock_in_memory)(MI_CHECK *info);
  uint key_length;
  myf myf_rw;
} MI_SORT_PARAM;

/* functions in mi_check */
void myisamchk_init(MI_CHECK *param);
int chk_status(MI_CHECK *param, MI_INFO *info);
int chk_del(MI_CHECK *param, register MI_INFO *info, uint test_flag);
int chk_size(MI_CHECK *param, MI_INFO *info);
int chk_key(MI_CHECK *param, MI_INFO *info);
int chk_data_link(MI_CHECK *param, MI_INFO *info,int extend);
int mi_repair(MI_CHECK *param, register MI_INFO *info,
	      my_string name, int rep_quick);
int mi_sort_index(MI_CHECK *param, register MI_INFO *info, my_string name);
int mi_repair_by_sort(MI_CHECK *param, register MI_INFO *info,
		      const char * name, int rep_quick);
int change_to_newfile(const char * filename, const char * old_ext,
		      const char * new_ext, uint raid_chunks,
		      myf myflags);
int lock_file(MI_CHECK *param, File file, my_off_t start, int lock_type,
	      const char *filetype, const char *filename);
void lock_memory(MI_CHECK *param);
int flush_blocks(MI_CHECK *param, File file);
void update_auto_increment_key(MI_CHECK *param, MI_INFO *info,
			       my_bool repair);
int update_state_info(MI_CHECK *param, MI_INFO *info,uint update);
int filecopy(MI_CHECK *param, File to,File from,my_off_t start,
	     my_off_t length, const char *type);
int movepoint(MI_INFO *info,byte *record,my_off_t oldpos,
	      my_off_t newpos, uint prot_key);
int sort_write_record(SORT_INFO *sort_info);
 int write_data_suffix(MI_CHECK *param, MI_INFO *info);
int _create_index_by_sort(MI_SORT_PARAM *info,my_bool no_messages,
			  ulong);
int test_if_almost_full(MI_INFO *info);
int recreate_table(MI_CHECK *param, MI_INFO **org_info, char *filename);
void mi_disable_non_unique_index(MI_INFO *info, ha_rows rows);
my_bool mi_test_if_sort_rep(MI_INFO *info, ha_rows rows, ulonglong key_map,
			    my_bool force);

#ifdef	__cplusplus
}
#endif
#endif
