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

/* This file should be included when using nisam_funktions */
/* Author: Michael Widenius */

#ifndef _nisam_h
#define _nisam_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _my_base_h
#include <my_base.h>
#endif
	/* defines used by nisam-funktions */

#define N_MAXKEY	16		/* Max allowed keys */
#define N_MAXKEY_SEG	16		/* Max segments for key */
#define N_MAX_KEY_LENGTH 256		/* May be increased up to 500 */
#define N_MAX_KEY_BUFF	 (N_MAX_KEY_LENGTH+N_MAXKEY_SEG+sizeof(double)-1)
#define N_MAX_POSSIBLE_KEY_BUFF 500+9

#define N_NAME_IEXT	".ISM"
#define N_NAME_DEXT	".ISD"
#define NI_POS_ERROR	(~ (ulong) 0)


	/* Param to/from nisam_info */

typedef struct st_n_isaminfo		/* Struct from h_info */
{
  ulong records;			/* Records in database */
  ulong deleted;			/* Deleted records in database */
  ulong recpos;				/* Pos for last used record */
  ulong newrecpos;			/* Pos if we write new record */
  ulong dupp_key_pos;			/* Position to record with dupp key */
  ulong data_file_length,		/* Length of data file */
        max_data_file_length,
        index_file_length,
        max_index_file_length,
        delete_length;
  uint	reclength;			/* Recordlength */
  uint	mean_reclength;			/* Mean recordlength (if packed) */
  uint	keys;				/* How many keys used */
  uint	options;			/* HA_OPTION_... used */
  int	errkey,				/* With key was dupplicated on err */
	sortkey;			/* clustered by this key */
  File	filenr;				/* (uniq) filenr for datafile */
  time_t create_time;			/* When table was created */
  time_t isamchk_time;
  time_t update_time;
  ulong *rec_per_key;			/* for sql optimizing */
} N_ISAMINFO;


	/* Info saved on file for each info-part */

#ifdef __WATCOMC__
#pragma pack(2)
#define uint uint16			/* Same format as in MSDOS */
#endif

#ifdef __ZTC__
#pragma ZTC align 2
#define uint uint16			/* Same format as in MSDOS */
#endif

typedef struct st_n_save_keyseg		/* Key-portion */
{
  uint8  type;				/* Typ av nyckel (f|r sort) */
  uint8  flag;				/* HA_DIFF_LENGTH */
  uint16 start;				/* Start of key in record */
  uint16 length;			/* Keylength */
} N_SAVE_KEYSEG;

typedef struct st_n_save_keydef /* Key definition with create & info */
{
  uint8 flag;				/* NOSAME, PACK_USED */
  uint8 keysegs;			/* Number of key-segment */
  uint16 block_length;			/* Length of keyblock (auto) */
  uint16 keylength;			/* Tot length of keyparts (auto) */
  uint16 minlength;			/* min length of (packed) key (auto) */
  uint16 maxlength;			/* max length of (packed) key (auto) */
} N_SAVE_KEYDEF;

typedef struct st_n_save_recinfo	/* Info of record */
{
  int16  type;				/* en_fieldtype */
  uint16 length;			/* length of field */
} N_SAVE_RECINFO;


#ifdef __ZTC__
#pragma ZTC align
#undef uint
#endif

#ifdef __WATCOMC__
#pragma pack()
#undef uint
#endif


struct st_isam_info;			/* For referense */

#ifndef ISAM_LIBRARY
typedef struct st_isam_info N_INFO;
#endif

typedef struct st_n_keyseg		/* Key-portion */
{
  N_SAVE_KEYSEG base;
} N_KEYSEG;


typedef struct st_n_keydef		/* Key definition with open & info */
{
  N_SAVE_KEYDEF base;
  N_KEYSEG seg[N_MAXKEY_SEG+1];
  int (*bin_search)(struct st_isam_info *info,struct st_n_keydef *keyinfo,
		    uchar *page,uchar *key,
		    uint key_len,uint comp_flag,uchar * *ret_pos,
		    uchar *buff);
  uint (*get_key)(struct st_n_keydef *keyinfo,uint nod_flag,uchar * *page,
		  uchar *key);
} N_KEYDEF;


typedef struct st_decode_tree		/* Decode huff-table */
{
  uint16 *table;
  uint	 quick_table_bits;
  byte	 *intervalls;
} DECODE_TREE;


struct st_bit_buff;

typedef struct st_n_recinfo		/* Info of record */
{
  N_SAVE_RECINFO base;
#ifndef NOT_PACKED_DATABASES
  void (*unpack)(struct st_n_recinfo *rec,struct st_bit_buff *buff,
		 uchar *start,uchar *end);
  enum en_fieldtype base_type;
  uint space_length_bits,pack_type;
  DECODE_TREE *huff_tree;
#endif
} N_RECINFO;


extern my_string nisam_log_filename;		/* Name of logfile */
extern uint nisam_block_size;
extern my_bool nisam_flush;

	/* Prototypes for nisam-functions */

extern int nisam_close(struct st_isam_info *file);
extern int nisam_delete(struct st_isam_info *file,const byte *buff);
extern struct st_isam_info *nisam_open(const char *name,int mode,
				    uint wait_if_locked);
extern int nisam_panic(enum ha_panic_function function);
extern int nisam_rfirst(struct st_isam_info *file,byte *buf,int inx);
extern int nisam_rkey(struct st_isam_info *file,byte *buf,int inx,
		   const byte *key,
		   uint key_len, enum ha_rkey_function search_flag);
extern int nisam_rlast(struct st_isam_info *file,byte *buf,int inx);
extern int nisam_rnext(struct st_isam_info *file,byte *buf,int inx);
extern int nisam_rprev(struct st_isam_info *file,byte *buf,int inx);
extern int nisam_rrnd(struct st_isam_info *file,byte *buf,ulong pos);
extern int nisam_rsame(struct st_isam_info *file,byte *record,int inx);
extern int nisam_rsame_with_pos(struct st_isam_info *file,byte *record,
			     int inx,ulong pos);
extern int nisam_update(struct st_isam_info *file,const byte *old,
		     const byte *new_record);
extern int nisam_write(struct st_isam_info *file,const byte *buff);
extern int nisam_info(struct st_isam_info *file,N_ISAMINFO *x,int flag);
extern ulong nisam_position(struct st_isam_info *info);
extern int nisam_lock_database(struct st_isam_info *file,int lock_type);
extern int nisam_create(const char *name,uint keys,N_KEYDEF *keyinfo,
		     N_RECINFO *recinfo,ulong records,
		     ulong reloc,uint flags,uint options,
		     ulong data_file_length);
extern int nisam_extra(struct st_isam_info *file,
		    enum ha_extra_function function);
extern ulong nisam_records_in_range(struct st_isam_info *info,int inx,
				 const byte *start_key,uint start_key_len,
				 enum ha_rkey_function start_search_flag,
				 const byte *end_key,uint end_key_len,
				 enum ha_rkey_function end_search_flag);
extern int nisam_log(int activate_log);
extern int nisam_is_changed(struct st_isam_info *info);
extern uint _calc_blob_length(uint length , const byte *pos);

#ifdef	__cplusplus
}
#endif
#endif
