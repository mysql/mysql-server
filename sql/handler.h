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


/* Definitions for parameters to do with handler-routines */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#ifndef NO_HASH
#define NO_HASH				/* Not yet implemented */
#endif

// the following is for checking tables

#define HA_ADMIN_ALREADY_DONE	  1
#define HA_ADMIN_OK               0
#define HA_ADMIN_NOT_IMPLEMENTED -1
#define HA_ADMIN_FAILED		 -2
#define HA_ADMIN_CORRUPT         -3
#define HA_ADMIN_INTERNAL_ERROR  -4
#define HA_ADMIN_INVALID         -5

/* Bits in bas_flag to show what database can do */

#define HA_READ_NEXT		1	/* Read next record with same key */
#define HA_READ_PREV		2	/* Read prev. record with same key */
#define HA_READ_ORDER		4	/* Read through record-keys in order */
#define HA_READ_RND_SAME	8	/* Read RND-record to KEY-record
					   (To update with RND-read)	   */
#define HA_KEYPOS_TO_RNDPOS	16	/* ha_info gives pos to record */
#define HA_LASTKEY_ORDER	32	/* Next record gives next record
					  according last record read (even
					  if database is updated after read) */
#define HA_REC_NOT_IN_SEQ	64	/* ha_info don't return recnumber;
					   It returns a position to ha_r_rnd */
#define HA_ONLY_WHOLE_INDEX	128	/* Can't use part key searches */
#define HA_RSAME_NO_INDEX	256	/* RSAME can't restore index */
#define HA_WRONG_ASCII_ORDER	512	/* Can't use sorting through key */
#define HA_HAVE_KEY_READ_ONLY	1024	/* Can read only keys (no record) */
#define HA_READ_NOT_EXACT_KEY	2048	/* Can read record after/before key */
#define HA_NO_INDEX		4096	/* No index needed for next/prev */
#define HA_LONGLONG_KEYS	8192	/* Can have longlong as key */
#define HA_KEY_READ_WRONG_STR	16384	/* keyread returns converted strings */
#define HA_NULL_KEY		32768	/* One can have keys with NULL */
#define HA_DUPP_POS		65536	/* ha_position() gives dupp row */
#define HA_NO_BLOBS		131072	/* Doesn't support blobs */
#define HA_BLOB_KEY		(HA_NO_BLOBS*2) /* key on blob */
#define HA_AUTO_PART_KEY	(HA_BLOB_KEY*2)
#define HA_REQUIRE_PRIMARY_KEY	(HA_AUTO_PART_KEY*2)
#define HA_NOT_EXACT_COUNT	(HA_REQUIRE_PRIMARY_KEY*2)
#define HA_NO_WRITE_DELAYED	(HA_NOT_EXACT_COUNT*2)
#define HA_PRIMARY_KEY_IN_READ_INDEX (HA_NO_WRITE_DELAYED*2)
#define HA_DROP_BEFORE_CREATE	(HA_PRIMARY_KEY_IN_READ_INDEX*2)

	/* Parameters for open() (in register form->filestat) */
	/* HA_GET_INFO does a implicit HA_ABORT_IF_LOCKED */

#define HA_OPEN_KEYFILE		1
#define HA_OPEN_RNDFILE		2
#define HA_GET_INDEX		4
#define HA_GET_INFO		8	/* do a ha_info() after open */
#define HA_READ_ONLY		16	/* File opened as readonly */
#define HA_TRY_READ_ONLY	32	/* Try readonly if can't */
					/* open with read and write */
#define HA_WAIT_IF_LOCKED	64	/* Wait if locked on open */
#define HA_ABORT_IF_LOCKED	128	/* skip if locked on open.*/
#define HA_BLOCK_LOCK		256	/* unlock when reading some records */
#define HA_OPEN_TEMPORARY	512

	/* Error on write which is recoverable  (Key exist) */

#define HA_WRITE_SKIPP 121		/* Duplicate key on write */
#define HA_READ_CHECK 123		/* Update with is recoverable */
#define HA_CANT_DO_THAT 131		/* Databasehandler can't do it */

	/* Some key definitions */
#define HA_KEY_NULL_LENGTH	1
#define HA_KEY_BLOB_LENGTH	2

#define HA_LEX_CREATE_TMP_TABLE	1
#define HA_LEX_CREATE_IF_NOT_EXISTS 2
#define HA_OPTION_NO_CHECKSUM	(1L << 17)
#define HA_OPTION_NO_DELAY_KEY_WRITE (1L << 18)
#define HA_MAX_REC_LENGTH	65535

enum db_type { DB_TYPE_UNKNOWN=0,DB_TYPE_DIAB_ISAM=1,
	       DB_TYPE_HASH,DB_TYPE_MISAM,DB_TYPE_PISAM,
	       DB_TYPE_RMS_ISAM, DB_TYPE_HEAP, DB_TYPE_ISAM,
	       DB_TYPE_MRG_ISAM, DB_TYPE_MYISAM, DB_TYPE_MRG_MYISAM,
	       DB_TYPE_BERKELEY_DB, DB_TYPE_INNOBASE,
	       DB_TYPE_DEFAULT };

enum row_type { ROW_TYPE_DEFAULT, ROW_TYPE_FIXED, ROW_TYPE_DYNAMIC,
	        ROW_TYPE_COMPRESSED };

/* struct to hold information about the table that should be created */

/* Bits in used_fields */
#define HA_CREATE_USED_AUTO 1
#define HA_CREATE_USED_RAID 2

typedef struct st_ha_create_information
{
  ulong table_options;
  enum db_type db_type;
  enum row_type row_type;
  ulong avg_row_length;
  ulonglong max_rows,min_rows;
  ulonglong auto_increment_value;
  char *comment,*password;
  uint options;					/* OR of HA_CREATE_ options */
  uint raid_type,raid_chunks;
  ulong raid_chunksize;
  bool if_not_exists;
  ulong used_fields;
  SQL_LIST merge_list;
} HA_CREATE_INFO;


/* The handler for a table type.  Will be included in the TABLE structure */

struct st_table;
typedef struct st_table TABLE;
extern ulong myisam_sort_buffer_size;

typedef struct st_ha_check_opt
{
  ulong sort_buffer_size;
  uint flags;
  bool quick;
  bool changed_files;
  bool optimize;
  bool retry_without_quick;
  inline void init()
  {
    flags= 0; quick= optimize= retry_without_quick=0;
    sort_buffer_size = myisam_sort_buffer_size;
  }
} HA_CHECK_OPT;

class handler :public Sql_alloc
{
 protected:
  struct st_table *table;		/* The table definition */
  uint  active_index;

public:
  byte *ref;				/* Pointer to current row */
  byte *dupp_ref;			/* Pointer to dupp row */
  uint ref_length;			/* Length of ref (1-8) */
  uint block_size;			/* index block size */
  ha_rows records;			/* Records i datafilen */
  ha_rows deleted;			/* Deleted records */
  ulonglong data_file_length;		/* Length off data file */
  ulonglong max_data_file_length;	/* Length off data file */
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;		/* Free bytes */
  ulonglong auto_increment_value;
  uint raid_type,raid_chunks;
  ulong raid_chunksize;
  uint errkey;				/* Last dup key */
  uint sortkey, key_used_on_scan;
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;
  ulong mean_rec_length;		/* physical reclength */
  void  *ft_handler;

  handler(TABLE *table_arg) : table(table_arg),active_index(MAX_REF_PARTS),
    ref(0),ref_length(sizeof(my_off_t)), block_size(0),records(0),deleted(0),
    data_file_length(0), max_data_file_length(0), index_file_length(0),
    delete_length(0), auto_increment_value(0), raid_type(0),
    key_used_on_scan(MAX_KEY),
    create_time(0), check_time(0), update_time(0), mean_rec_length(0),
    ft_handler(0)
    {}
  virtual ~handler(void) {}
  int ha_open(const char *name, int mode, int test_if_locked);
  void update_timestamp(byte *record);
  void update_auto_increment();
  void print_error(int error, myf errflag);
  uint get_dup_key(int error);
  void change_table_ptr(TABLE *table_arg) { table=table_arg; }
  virtual double scan_time()
    { return ulonglong2double(data_file_length) / IO_SIZE + 1; }
  virtual double read_time(ha_rows rows) { return rows; }
  virtual bool fast_key_read() { return 0;}
  virtual bool has_transactions(){ return 0;}
  virtual uint extra_rec_buf_length() { return 0; }
  virtual ha_rows estimate_number_of_rows() { return records+EXTRA_RECORDS; }

  virtual int index_init(uint idx) { active_index=idx; return 0;}
  virtual int index_end() {return 0; }
  uint get_index(void) const { return active_index; }
  virtual int open(const char *name, int mode, uint test_if_locked)=0;
  virtual void initialize(void) {}
  virtual int close(void)=0;
  virtual int write_row(byte * buf)=0;
  virtual int update_row(const byte * old_data, byte * new_data)=0;
  virtual int delete_row(const byte * buf)=0;
  virtual int index_read(byte * buf, const byte * key,
    			 uint key_len, enum ha_rkey_function find_flag)=0;
  virtual int index_read_idx(byte * buf, uint index, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag)=0;
  virtual int index_next(byte * buf)=0;
  virtual int index_prev(byte * buf)=0;
  virtual int index_first(byte * buf)=0;
  virtual int index_last(byte * buf)=0;
  virtual int index_next_same(byte *buf, const byte *key, uint keylen);
  virtual int ft_init()
                                 { return -1; }
  virtual void *ft_init_ext(uint inx,const byte *key, uint keylen, bool presort)
                                 { return (void *)NULL; }
  virtual int ft_read(byte *buf) { return -1; }
  virtual int rnd_init(bool scan=1)=0;
  virtual int rnd_end() { return 0; }
  virtual int rnd_next(byte *buf)=0;
  virtual int rnd_pos(byte * buf, byte *pos)=0;
  virtual int rnd_first(byte *buf);
  virtual int restart_rnd_next(byte *buf, byte *pos);
  virtual ha_rows records_in_range(int inx,
			           const byte *start_key,uint start_key_len,
			           enum ha_rkey_function start_search_flag,
			           const byte *end_key,uint end_key_len,
			           enum ha_rkey_function end_search_flag)
    { return (ha_rows) 10; }
  virtual void position(const byte *record)=0;
  virtual my_off_t row_position() { return HA_OFFSET_ERROR; }
  virtual void info(uint)=0;
  virtual int extra(enum ha_extra_function operation)=0;
  virtual int reset()=0;
  virtual int external_lock(THD *thd, int lock_type)=0;
  virtual int delete_all_rows();
  virtual longlong get_auto_increment();
  virtual void update_create_info(HA_CREATE_INFO *create_info) {}
  virtual int check(THD* thd,   HA_CHECK_OPT* check_opt );
  virtual int repair(THD* thd,  HA_CHECK_OPT* check_opt);
  virtual bool check_and_repair(THD *thd) {return 1;}
  virtual int optimize(THD* thd,HA_CHECK_OPT* check_opt);
  virtual int analyze(THD* thd, HA_CHECK_OPT* check_opt);
  virtual int backup(THD* thd, HA_CHECK_OPT* check_opt);
  virtual int restore(THD* thd, HA_CHECK_OPT* check_opt);
  // assumes .frm file must exist, and you must have already called
  // generate_table() - it will just copy the data file and run repair

  virtual int dump(THD* thd, int fd = -1) { return ER_DUMP_NOT_IMPLEMENTED; }
  virtual void deactivate_non_unique_index(ha_rows rows) {}
  virtual bool activate_all_index(THD *thd) {return 0;}
  // not implemented by default
  virtual int net_read_dump(NET* net)
  { return ER_DUMP_NOT_IMPLEMENTED; }

  /* The following can be called without an open handler */
  virtual const char *table_type() const =0;
  virtual const char **bas_ext() const =0;
  virtual ulong option_flag() const =0;
  virtual uint max_record_length() const =0;
  virtual uint max_keys() const =0;
  virtual uint max_key_parts() const =0;
  virtual uint max_key_length()const =0;
  virtual uint max_key_part_length() { return 255; }
  virtual uint min_record_length(uint options) const { return 1; }
  virtual bool low_byte_first() const { return 1; }
  virtual bool is_crashed() const  { return 0; }
  virtual bool auto_repair() const { return 0; }

  virtual int rename_table(const char *from, const char *to);
  virtual int delete_table(const char *name);
  virtual int create(const char *name, TABLE *form, HA_CREATE_INFO *info)=0;
  virtual uint lock_count(void) const { return 1; }
  virtual THR_LOCK_DATA **store_lock(THD *thd,
				     THR_LOCK_DATA **to,
				     enum thr_lock_type lock_type)=0;
};

	/* Some extern variables used with handlers */

extern const char *ha_row_type[];
extern TYPELIB ha_table_typelib;

handler *get_new_handler(TABLE *table, enum db_type db_type);
my_off_t ha_get_ptr(byte *ptr, uint pack_length);
void ha_store_ptr(byte *buff, uint pack_length, my_off_t pos);
int ha_init(void);
int ha_panic(enum ha_panic_function flag);
enum db_type ha_checktype(enum db_type database_type);
int ha_create_table(const char *name, HA_CREATE_INFO *create_info,
		    bool update_create_info);
int ha_delete_table(enum db_type db_type, const char *path);
void ha_key_cache(void);
int ha_commit(THD *thd);
int ha_rollback(THD *thd);
int ha_autocommit_or_rollback(THD *thd, int error);
bool ha_flush_logs(void);

