/* Copyright (C) 2000-2003 MySQL AB

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

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

/* The class defining a handle to an NDB Cluster table */

#ifdef __GNUC__
#pragma interface                       /* gcc class implementation */
#endif

#include <ndbapi_limits.h>
#include <ndb_types.h>

class Ndb;             // Forward declaration
class NdbOperation;    // Forward declaration
class NdbConnection;   // Forward declaration
class NdbRecAttr;      // Forward declaration
class NdbResultSet;    // Forward declaration
class NdbScanOperation; 
class NdbIndexScanOperation; 
class NdbBlob;

// connectstring to cluster if given by mysqld
extern const char *ndbcluster_connectstring;

typedef enum ndb_index_type {
  UNDEFINED_INDEX = 0,
  PRIMARY_KEY_INDEX = 1,
  PRIMARY_KEY_ORDERED_INDEX = 2,
  UNIQUE_INDEX = 3,
  UNIQUE_ORDERED_INDEX = 4,
  ORDERED_INDEX = 5
} NDB_INDEX_TYPE;

typedef struct ndb_index_data {
  NDB_INDEX_TYPE type;
  void *index;
  const char * unique_name;
  void *unique_index;
} NDB_INDEX_DATA;

typedef struct st_ndbcluster_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} NDB_SHARE;

/*
  Place holder for ha_ndbcluster thread specific data
*/

class Thd_ndb {
 public:
  Thd_ndb();
  ~Thd_ndb();
  Ndb *ndb;
  ulong count;
  uint lock_count;
  int error;
};

class ha_ndbcluster: public handler
{
 public:
  ha_ndbcluster(TABLE *table);
  ~ha_ndbcluster();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int write_row(byte *buf);
  int update_row(const byte *old_data, byte *new_data);
  int delete_row(const byte *buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte *buf, const byte *key, uint key_len, 
		 enum ha_rkey_function find_flag);
  int index_read_idx(byte *buf, uint index, const byte *key, uint key_len, 
		     enum ha_rkey_function find_flag);
  int index_next(byte *buf);
  int index_prev(byte *buf);
  int index_first(byte *buf);
  int index_last(byte *buf);
  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte *buf, byte *pos);
  void position(const byte *record);
  int read_range_first(const key_range *start_key,
		       const key_range *end_key,
		       bool eq_range, bool sorted);
  int read_range_first_to_buf(const key_range *start_key,
			      const key_range *end_key,
			      bool eq_range, bool sorted,
			      byte* buf);
  int read_range_next();

  bool get_error_message(int error, String *buf);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset();
  int external_lock(THD *thd, int lock_type);
  int start_stmt(THD *thd);
  const char * table_type() const { return("ndbcluster");}
  const char ** bas_ext() const;
  ulong table_flags(void) const { return m_table_flags; }
  ulong index_flags(uint idx, uint part, bool all_parts) const;
  uint max_supported_record_length() const { return NDB_MAX_TUPLE_SIZE; };
  uint max_supported_keys() const { return MAX_KEY;  }
  uint max_supported_key_parts() const 
    { return NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY; };
  uint max_supported_key_length() const { return NDB_MAX_KEY_SIZE;};

  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  THR_LOCK_DATA **store_lock(THD *thd,
			     THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);

  bool low_byte_first() const
    { 
#ifdef WORDS_BIGENDIAN
      return false;
#else
      return true;
#endif
    }
  bool has_transactions()  { return true; }

  const char* index_type(uint key_number) {
    switch (get_index_type(key_number)) {
    case ORDERED_INDEX:
    case UNIQUE_ORDERED_INDEX:
    case PRIMARY_KEY_ORDERED_INDEX:
      return "BTREE";
    case UNIQUE_INDEX:
    case PRIMARY_KEY_INDEX:
    default:
      return "HASH";
    }
  }

  double scan_time();
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();

  static Thd_ndb* seize_thd_ndb();
  static void release_thd_ndb(Thd_ndb* thd_ndb);
  uint8 table_cache_type() { return HA_CACHE_TBL_NOCACHE; }
    
 private:
  int alter_table_name(const char *from, const char *to);
  int drop_table();
  int create_index(const char *name, KEY *key_info, bool unique);
  int create_ordered_index(const char *name, KEY *key_info);
  int create_unique_index(const char *name, KEY *key_info);
  int initialize_autoincrement(const void *table);
  enum ILBP {ILBP_CREATE = 0, ILBP_OPEN = 1}; // Index List Build Phase
  int build_index_list(TABLE *tab, enum ILBP phase);
  int get_metadata(const char* path);
  void release_metadata();
  const char* get_index_name(uint idx_no) const;
  const char* get_unique_index_name(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type_from_table(uint index_no) const;
  
  int pk_read(const byte *key, uint key_len, byte *buf);
  int complemented_pk_read(const byte *old_data, byte *new_data);
  int unique_index_read(const byte *key, uint key_len, 
			byte *buf);
  int ordered_index_scan(const key_range *start_key,
			 const key_range *end_key,
			 bool sorted, byte* buf);
  int full_table_scan(byte * buf);
  int next_result(byte *buf); 
  int define_read_attrs(byte* buf, NdbOperation* op);
  int filtered_scan(const byte *key, uint key_len, 
		    byte *buf,
		    enum ha_rkey_function find_flag);
  int close_scan();
  void unpack_record(byte *buf);
  int get_ndb_lock_type(enum thr_lock_type type);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);
  void set_tabname(const char *pathname, char *tabname);

  bool set_hidden_key(NdbOperation*,
		      uint fieldnr, const byte* field_ptr);
  int set_ndb_key(NdbOperation*, Field *field,
		  uint fieldnr, const byte* field_ptr);
  int set_ndb_value(NdbOperation*, Field *field, uint fieldnr, bool *set_blob_value= 0);
  int get_ndb_value(NdbOperation*, Field *field, uint fieldnr, byte*);
  friend int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg);
  int get_ndb_blobs_value(NdbBlob *last_ndb_blob);
  int set_primary_key(NdbOperation *op, const byte *key);
  int set_primary_key(NdbOperation *op);
  int set_primary_key_from_old_data(NdbOperation *op, const byte *old_data);
  int set_bounds(NdbIndexScanOperation *ndb_op, const key_range *key,
		 int bound);
  int key_cmp(uint keynr, const byte * old_row, const byte * new_row);
  void print_results();

  longlong get_auto_increment();
  int ndb_err(NdbConnection*);
  bool uses_blob_value(bool all_fields);

 private:
  int check_ndb_connection();

  NdbConnection *m_active_trans;
  NdbResultSet *m_active_cursor;
  Ndb *m_ndb;
  void *m_table;
  void *m_table_info;
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  ulong m_table_flags;
  THR_LOCK_DATA m_lock;
  NDB_SHARE *m_share;
  NDB_INDEX_DATA  m_index[MAX_KEY];
  // NdbRecAttr has no reference to blob
  typedef union { NdbRecAttr *rec; NdbBlob *blob; void *ptr; } NdbValue;
  NdbValue m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  bool m_use_write;
  bool m_ignore_dup_key_not_supported;
  bool retrieve_all_fields;
  ha_rows rows_to_insert;
  ha_rows rows_inserted;
  ha_rows bulk_insert_rows;
  bool bulk_insert_not_flushed;
  ha_rows ops_pending;
  bool skip_auto_increment;
  bool blobs_pending;
  // memory for blobs in one tuple
  char *blobs_buffer;
  uint32 blobs_buffer_size;
  uint dupkey;

  void records_update();
  void no_uncommitted_rows_execute_failure();
  void no_uncommitted_rows_update(int);
  void no_uncommitted_rows_init(THD *);
  void no_uncommitted_rows_reset(THD *);

  friend int execute_no_commit(ha_ndbcluster*, NdbConnection*);
};

bool ndbcluster_init(void);
bool ndbcluster_end(void);

int ndbcluster_commit(THD *thd, void* ndb_transaction);
int ndbcluster_rollback(THD *thd, void* ndb_transaction);

void ndbcluster_close_connection(THD *thd);

int ndbcluster_discover(const char* dbname, const char* name,
			const void** frmblob, uint* frmlen);
int ndbcluster_drop_database(const char* path);

void ndbcluster_print_error(int error, const NdbOperation *error_op);
