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

/* Denna fil includeras i alla isam-filer */

#define ISAM_LIBRARY
#include <nisam.h>			/* Structs & some defines */
#ifdef THREAD
#include <my_pthread.h>
#include <thr_lock.h>
#else
#include <my_no_pthread.h>
#endif

#ifdef my_write
#undef my_write				/* We want test if disk full */
#endif
#undef HA_SORT_ALLOWS_SAME
#define HA_SORT_ALLOWS_SAME 128		/* Can't be > 128 in NISAM */

#ifdef __WATCOMC__
#pragma pack(2)
#define uint uint16			/* Same format as in MSDOS */
#endif
#ifdef __ZTC__
#pragma ZTC align 2
#define uint uint16			/* Same format as in MSDOS */
#endif
#if defined(__WIN__) && defined(_MSC_VER)
#pragma pack(push,isamdef,2)
#define uint uint16
#endif

typedef struct st_state_info
{
  struct {				/* Fileheader */
    uchar file_version[4];
    uchar options[2];
    uchar header_length[2];
    uchar state_info_length[2];
    uchar base_info_length[2];
    uchar base_pos[2];
    uchar not_used[2];
  } header;

  ulong records;			/* Antal record i databasen */
  ulong del;				/* Antalet borttagna poster */
  ulong dellink;			/* L{nk till n{sta borttagna */
  ulong key_file_length;
  ulong data_file_length;
  ulong splitt;				/* Antal splittrade delar */
  ulong empty;				/* Outnyttjat utrymme */
  ulong process;			/* Vem som senast uppdatera */
  ulong loop;				/* not used anymore */
  ulong uniq;				/* Unik nr i denna process */
  ulong key_root[N_MAXKEY];		/* Pekare till rootblocken */
  ulong key_del[N_MAXKEY];		/* Del-l{nkar f|r n-block */
  ulong sec_index_changed;		/* Updated when new sec_index */
  ulong sec_index_used;			/* 1 bit for each sec index in use */
  ulong version;			/* timestamp of create */
  uint	keys;				/* Keys in use for database */
} N_STATE_INFO;


typedef struct st_base_info
{
  ulong keystart;			/* Var nycklarna b|rjar */
  ulong records,reloc;			/* Parameter vid skapandet */
  ulong max_pack_length;		/* Max possibly length of packed rec.*/
  ulong max_data_file_length;
  ulong max_key_file_length;
  uint reclength;			/* length of unpacked record */
  uint options;				/* Options used */
  uint pack_reclength;			/* Length of full packed rec. */
  uint min_pack_length;
  uint min_block_length;
  uint rec_reflength;			/* = 2 or 3 or 4 */
  uint key_reflength;			/* = 2 or 3 or 4 */
  uint keys;				/* Keys defined for database */
  uint blobs;				/* Number of blobs */
  uint max_block;			/* Max blockl{ngd anv{nd */
  uint max_key_length;			/* L{ngsta nyckel-l{ngden */
  uint fields,				/* Antal f{lt i databasen */
       pack_fields,			/* Packade f{lt i databasen */
       pack_bits;			/* Length of packed bits */
  time_t create_time;			/* Time when created database */
  time_t isamchk_time;			/* Time for last recover */
  ulong rec_per_key[N_MAXKEY];		/* for sql optimizing */
  uint	sortkey;			/* sorted by this key */
} N_BASE_INFO;


#ifdef __ZTC__
#pragma ZTC align
#undef uint
#endif
#ifdef __WATCOMC__
#pragma pack()
#undef uint
#endif
#if defined(__WIN__) && defined(_MSC_VER)
#pragma pack(pop,isamdef)
#undef uint
#endif

	/* Structs used intern in database */

typedef struct st_n_blob		/* Info of record */
{
  uint offset;				/* Offset to blob in record */
  uint pack_length;			/* Type of packed length */
  uint length;				/* Calc:ed for each record */
} N_BLOB;


typedef struct st_isam_pack {
  ulong header_length;
  uint ref_length;
} N_PACK;


typedef struct st_isam_share {		/* Shared between opens */
  N_STATE_INFO state;
  N_BASE_INFO base;
  N_KEYDEF  *keyinfo;			/* Nyckelinfo */
  N_RECINFO *rec;			/* Pointer till recdata */
  N_PACK    pack;			/* Data about packed records */
  N_BLOB    *blobs;			/* Pointer to blobs */
  char	*filename;			/* Name of indexfile */
  byte *file_map;			/* mem-map of file if possible */
  ulong this_process;			/* processid */
  ulong last_process;			/* For table-change-check */
  ulong last_version;			/* Version on start */
  uint	rec_reflength;			/* rec_reflength in use now */
  int	kfile;				/* Shared keyfile */
  int	mode;				/* mode of file on open */
  int	reopen;				/* How many times reopened */
  uint	state_length;
  uint	w_locks,r_locks;		/* Number of read/write locks */
  uint	min_pack_length;		/* Theese is used by packed data */
  uint	max_pack_length;
  uint	blocksize;			/* blocksize of keyfile */
  my_bool  changed,not_flushed;		/* If changed since lock */
  int	rnd;				/* rnd-counter */
  DECODE_TREE *decode_trees;
  uint16 *decode_tables;
  enum data_file_type data_file_type;
  int (*read_record)(struct st_isam_info*, ulong, byte*);
  int (*write_record)(struct st_isam_info*, const byte*);
  int (*update_record)(struct st_isam_info*, ulong, const byte*);
  int (*delete_record)(struct st_isam_info*);
  int (*read_rnd)(struct st_isam_info*, byte*, ulong, int);
  int (*compare_record)(struct st_isam_info*, const byte *);
#ifdef THREAD
  THR_LOCK lock;
  pthread_mutex_t intern_lock;		/* Locking for use with _locking */
#endif
} ISAM_SHARE;


typedef uint bit_type;

typedef struct st_bit_buff {		/* Used for packing of record */
  bit_type current_byte;
  uint bits;
  uchar *pos,*end;
  uint error;
} BIT_BUFF;


typedef struct st_isam_info {
  ISAM_SHARE *s;			/* Shared between open:s */
  N_BLOB     *blobs;			/* Pointer to blobs */
  int dfile;				/* The datafile */
  BIT_BUFF   bit_buff;
  uint	options;
  uint	opt_flag;			/* Optim. for space/speed */
  uint update;				/* If file changed since open */
  char *filename;			/* parameter to open filename */
  ulong this_uniq;			/* uniq filenumber or thread */
  ulong last_uniq;			/* last uniq number */
  ulong this_loop;			/* counter for this open */
  ulong last_loop;			/* last used counter */
  ulong lastpos,			/* Last record position */
	nextpos;			/* Position to next record */
  ulong int_pos;			/* Intern variabel */
  ulong dupp_key_pos;			/* Position to record with dupp key */
  ulong last_search_keypage;
  ulong save_lastpos;
  uint packed_length;			/* Length of found, packed record */
  uint	alloced_rec_buff_length;	/* Max recordlength malloced */
  uchar *buff,				/* Temp area for key */
	*lastkey;			/* Last used search key */
  byte	*rec_buff,			/* Tempbuff for recordpack */
	*rec_alloc;			/* Malloced area for record */
  uchar *int_keypos,			/* Intern variabel */
	*int_maxpos;			/* Intern variabel */
  int	lastinx;			/* Last used index */
  int	errkey;				/* Got last error on this key */
  uint	data_changed;			/* Somebody has changed data */
  int lock_type;			/* How database was locked */
  int tmp_lock_type;			/* When locked by readinfo */
  int was_locked;			/* Was locked in panic */
  myf lock_wait;			/* is 0 or MY_DONT_WAIT */
  my_bool page_changed;
  my_bool buff_used;
  uint	save_update;			/* When using KEY_READ */
  int	save_lastinx;
  int (*read_record)(struct st_isam_info*, ulong, byte*);
  LIST	open_list;
  IO_CACHE rec_cache;			/* When cacheing records */
#ifdef THREAD
  THR_LOCK_DATA lock;
#endif
} N_INFO;


	/* Some defines used by isam-funktions */

#define USE_HOLE_KEY	0		/* Use hole key in _nisam_search() */
#define F_EXTRA_LCK	-1

	/* bits in opt_flag */
#define MEMMAP_USED	32
#define REMEMBER_OLD_POS 64

#define getint(x)	((uint) (uint16) *((int16*) (x)) & 32767)
#define putint(x,y,nod) (*((uint16*) (x))= ((nod ? (uint16) 32768 : 0)+(uint16) (y)))
#ifdef WORDS_BIGENDIAN
#define test_if_nod(x) (x[0] & 128 ? info->s->base.key_reflength : 0)
#else
#define test_if_nod(x) (x[1] & 128 ? info->s->base.key_reflength : 0)
#endif

#define N_MIN_BLOCK_LENGTH	8	/* Because of delete-link */
#define N_EXTEND_BLOCK_LENGTH	20	/* Don't use to small record-blocks */
#define N_SPLITT_LENGTH  ((N_EXTEND_BLOCK_LENGTH+3)*2)
#define MAX_DYN_BLOCK_HEADER	11	/* Max prefix of record-block */
#define DYN_DELETE_BLOCK_HEADER 8	/* length of delete-block-header */
#define MEMMAP_EXTRA_MARGIN	7	/* Write this as a suffix for file */
#define INDEX_BLOCK_MARGIN	16	/* Safety margin for .ISM tables */

#define PACK_TYPE_SELECTED	1	/* Bits in field->pack_type */
#define PACK_TYPE_SPACE_FIELDS	2
#define PACK_TYPE_ZERO_FILL	4

#ifdef THREAD
extern pthread_mutex_t THR_LOCK_isam;
#endif

	/* Some extern variables */

extern LIST *nisam_open_list;
extern uchar NEAR nisam_file_magic[],NEAR nisam_pack_file_magic[];
extern uint NEAR nisam_read_vec[],nisam_quick_table_bits;
extern File nisam_log_file;

	/* This is used by _nisam_get_pack_key_length och _nisam_store_key */

typedef struct st_s_param
{
  uint	ref_length,key_length,
	n_ref_length,
	n_length,
	totlength,
        part_of_prev_key,prev_length;
  uchar *key, *prev_key;
} S_PARAM;

	/* Prototypes for intern functions */

extern int _nisam_read_dynamic_record(N_INFO *info,ulong filepos,byte *buf);
extern int _nisam_write_dynamic_record(N_INFO*, const byte*);
extern int _nisam_update_dynamic_record(N_INFO*, ulong, const byte*);
extern int _nisam_delete_dynamic_record(N_INFO *info);
extern int _nisam_cmp_dynamic_record(N_INFO *info,const byte *record);
extern int _nisam_read_rnd_dynamic_record(N_INFO *, byte *,ulong, int);
extern int _nisam_write_blob_record(N_INFO*, const byte*);
extern int _nisam_update_blob_record(N_INFO*, ulong, const byte*);
extern int _nisam_read_static_record(N_INFO *info,ulong filepos,byte *buf);
extern int _nisam_write_static_record(N_INFO*, const byte*);
extern int _nisam_update_static_record(N_INFO*, ulong, const byte*);
extern int _nisam_delete_static_record(N_INFO *info);
extern int _nisam_cmp_static_record(N_INFO *info,const byte *record);
extern int _nisam_read_rnd_static_record(N_INFO*, byte *,ulong, int);
extern int _nisam_ck_write(N_INFO *info,uint keynr,uchar *key);
extern int _nisam_enlarge_root(N_INFO *info,uint keynr,uchar *key);
extern int _nisam_insert(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
		      uchar *anc_buff,uchar *key_pos,uchar *key_buff,
		      uchar *father_buff, uchar *father_keypos,
		      ulong father_page);
extern int _nisam_splitt_page(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
			   uchar *buff,uchar *key_buff);
extern uchar *_nisam_find_half_pos(N_INFO *info,N_KEYDEF *keyinfo,uchar *page,
				uchar *key);
extern uint _nisam_get_pack_key_length(N_KEYDEF *keyinfo,uint nod_flag,
				    uchar *key_pos,uchar *key_buff,
				    uchar *key, S_PARAM *s_temp);
extern void _nisam_store_key(N_KEYDEF *keyinfo,uchar *key_pos,
			  S_PARAM *s_temp);
extern int _nisam_ck_delete(N_INFO *info,uint keynr,uchar *key);
extern int _nisam_readinfo(N_INFO *info,int lock_flag,int check_keybuffer);
extern int _nisam_writeinfo(N_INFO *info, uint flag);
extern int _nisam_test_if_changed(N_INFO *info);
extern int _nisam_check_index(N_INFO *info,int inx);
extern int _nisam_search(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,uint key_len,uint nextflag,ulong pos);
extern int _nisam_bin_search(struct st_isam_info *info,N_KEYDEF *keyinfo,uchar *page,uchar *key,uint key_len,uint comp_flag,uchar * *ret_pos,uchar *buff);
extern int _nisam_seq_search(N_INFO *info,N_KEYDEF *keyinfo,uchar *page,uchar *key,uint key_len,uint comp_flag,uchar * *ret_pos,uchar *buff);
extern ulong _nisam_kpos(uint nod_flag,uchar *after_key);
extern void _nisam_kpointer(N_INFO *info,uchar *buff,ulong pos);
extern ulong _nisam_dpos(N_INFO *info, uint nod_flag,uchar *after_key);
extern void _nisam_dpointer(N_INFO *info, uchar *buff,ulong pos);
extern int _nisam_key_cmp(N_KEYSEG *keyseg,uchar *a,uchar *b,
		       uint key_length,uint nextflag);
extern uint _nisam_get_key(N_KEYDEF *keyinfo,uint nod_flag,uchar * *page,uchar *key);
extern uint _nisam_get_static_key(N_KEYDEF *keyinfo,uint nod_flag,uchar * *page,uchar *key);
extern uchar *_nisam_get_last_key(N_INFO *info,N_KEYDEF *keyinfo,uchar *keypos,uchar *lastkey,uchar *endpos);
extern uint _nisam_keylength(N_KEYDEF *keyinfo,uchar *key);
extern uchar *_nisam_move_key(N_KEYDEF *keyinfo,uchar *to,uchar *from);
extern int _nisam_search_next(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,uint nextflag,ulong pos);
extern int _nisam_search_first(N_INFO *info,N_KEYDEF *keyinfo,ulong pos);
extern int _nisam_search_last(N_INFO *info,N_KEYDEF *keyinfo,ulong pos);
extern uchar *_nisam_fetch_keypage(N_INFO *info,N_KEYDEF *keyinfo,my_off_t page,
				uchar *buff,int return_buffer);
extern int _nisam_write_keypage(N_INFO *info,N_KEYDEF *keyinfo,my_off_t page,
			     uchar *buff);
extern int _nisam_dispose(N_INFO *info,N_KEYDEF *keyinfo,my_off_t pos);
extern ulong _nisam_new(N_INFO *info,N_KEYDEF *keyinfo);
extern uint _nisam_make_key(N_INFO *info,uint keynr,uchar *key,
			 const char *record,ulong filepos);
extern uint _nisam_pack_key(N_INFO *info,uint keynr,uchar *key,uchar *old,uint key_length);
extern int _nisam_read_key_record(N_INFO *info,ulong filepos,byte *buf);
extern int _nisam_read_cache(IO_CACHE *info,byte *buff,ulong pos,
			  uint length,int re_read_if_possibly);
extern byte *fix_rec_buff_for_blob(N_INFO *info,uint blob_length);
extern uint _nisam_rec_unpack(N_INFO *info,byte *to,byte *from,
			   uint reclength);
my_bool _nisam_rec_check(N_INFO *info,const char *from);
extern int _nisam_write_part_record(N_INFO *info,ulong filepos,uint length,
				 ulong next_filepos,byte **record,
				 uint *reclength,int *flag);
extern void _nisam_print_key(FILE *stream,N_KEYSEG *keyseg,const uchar *key);
extern my_bool _nisam_read_pack_info(N_INFO *info,pbool fix_keys);
extern int _nisam_read_pack_record(N_INFO *info,ulong filepos,byte *buf);
extern int _nisam_read_rnd_pack_record(N_INFO*, byte *,ulong, int);
extern int _nisam_pack_rec_unpack(N_INFO *info,byte *to,byte *from,
			       uint reclength);

typedef struct st_sortinfo {
  uint key_length;
  ulong max_records;
  int (*key_cmp)(const void *, const void *, const void *);
  int (*key_read)(void *buff);
  int (*key_write)(const void *buff);
  void (*lock_in_memory)(void);
} SORT_PARAM;

int _create_index_by_sort(SORT_PARAM *info,pbool no_messages,
			  uint sortbuff_size);

#define BLOCK_INFO_HEADER_LENGTH 11

typedef struct st_block_info {	/* Parameter to _nisam_get_block_info */
  uchar header[BLOCK_INFO_HEADER_LENGTH];
  uint rec_len;
  uint data_len;
  uint block_len;
  ulong filepos;				/* Must be ulong on Alpha! */
  ulong next_filepos;
  uint second_read;
} BLOCK_INFO;

	/* bits in return from _nisam_get_block_info */

#define BLOCK_FIRST	1
#define BLOCK_LAST	2
#define BLOCK_DELETED	4
#define BLOCK_ERROR	8	/* Wrong data */
#define BLOCK_SYNC_ERROR 16	/* Right data at wrong place */
#define BLOCK_FATAL_ERROR 32	/* hardware-error */

enum nisam_log_commands {
  LOG_OPEN,LOG_WRITE,LOG_UPDATE,LOG_DELETE,LOG_CLOSE,LOG_EXTRA,LOG_LOCK
};

#define nisam_log_simple(a,b,c,d) if (nisam_log_file >= 0) _nisam_log(a,b,c,d)
#define nisam_log_command(a,b,c,d,e) if (nisam_log_file >= 0) _nisam_log_command(a,b,c,d,e)
#define nisam_log_record(a,b,c,d,e) if (nisam_log_file >= 0) _nisam_log_record(a,b,c,d,e)

extern uint _nisam_get_block_info(BLOCK_INFO *,File, ulong);
extern uint _nisam_rec_pack(N_INFO *info,byte *to,const byte *from);
extern uint _nisam_pack_get_block_info(BLOCK_INFO *, uint, File, ulong);
extern uint _calc_total_blob_length(N_INFO *info,const byte *record);
extern void _nisam_log(enum nisam_log_commands command,N_INFO *info,
		       const byte *buffert,uint length);
extern void _nisam_log_command(enum nisam_log_commands command,
			       N_INFO *info, const byte *buffert,
			       uint length, int result);
extern void _nisam_log_record(enum nisam_log_commands command,N_INFO *info,
			      const byte *record,ulong filepos,
			      int result);
extern my_bool _nisam_memmap_file(N_INFO *info);
extern void _nisam_unmap_file(N_INFO *info);
