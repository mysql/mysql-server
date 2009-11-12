/* 
Copyright (c) 2008, Patrick Galbraith 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

    * Neither the name of Patrick Galbraith nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


class federatedx_io;

/*
  FEDERATEDX_SERVER will eventually be a structure that will be shared among
  all FEDERATEDX_SHARE instances so that the federated server can minimise
  the number of open connections. This will eventually lead to the support
  of reliable XA federated tables.
*/
typedef struct st_fedrated_server {
  MEM_ROOT mem_root;
  uint use_count, io_count;
  
  uchar *key;
  uint key_length;

  const char *scheme;
  const char *hostname;
  const char *username;
  const char *password;
  const char *database;
  const char *socket;
  ushort port;

  const char *csname;

  pthread_mutex_t mutex;
  federatedx_io *idle_list;
} FEDERATEDX_SERVER;

/*
  Please read ha_exmple.cc before reading this file.
  Please keep in mind that the federatedx storage engine implements all methods
  that are required to be implemented. handler.h has a full list of methods
  that you can implement.
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <mysql.h>

/* 
  handler::print_error has a case statement for error numbers.
  This value is (10000) is far out of range and will envoke the 
  default: case.  
  (Current error range is 120-159 from include/my_base.h)
*/
#define HA_FEDERATEDX_ERROR_WITH_REMOTE_SYSTEM 10000

#define FEDERATEDX_QUERY_BUFFER_SIZE STRING_BUFFER_USUAL_SIZE * 5
#define FEDERATEDX_RECORDS_IN_RANGE 2
#define FEDERATEDX_MAX_KEY_LENGTH 3500 // Same as innodb

/*
  FEDERATEDX_SHARE is a structure that will be shared amoung all open handlers
  The example implements the minimum of what you will probably need.
*/
typedef struct st_federatedx_share {
  MEM_ROOT mem_root;

  bool parsed;
  /* this key is unique db/tablename */
  const char *share_key;
  /*
    the primary select query to be used in rnd_init
  */
  char *select_query;
  /*
    remote host info, parse_url supplies
  */
  char *server_name;
  char *connection_string;
  char *scheme;
  char *hostname;
  char *username;
  char *password;
  char *database;
  char *table_name;
  char *table;
  char *socket;
  char *sport;
  int share_key_length;
  ushort port;

  uint table_name_length, server_name_length, connect_string_length;
  uint use_count;
  THR_LOCK lock;
  FEDERATEDX_SERVER *s;
} FEDERATEDX_SHARE;


typedef struct st_federatedx_result FEDERATEDX_IO_RESULT;
typedef struct st_federatedx_row FEDERATEDX_IO_ROW;
typedef ptrdiff_t FEDERATEDX_IO_OFFSET;

class federatedx_io
{
  friend class federatedx_txn;
  FEDERATEDX_SERVER * const server;
  federatedx_io **owner_ptr;
  federatedx_io *txn_next;
  federatedx_io *idle_next;
  bool active;  /* currently participating in a transaction */
  bool busy;    /* in use by a ha_federated instance */
  bool readonly;/* indicates that no updates have occurred */

protected:
  void set_active(bool new_active)
  { active= new_active; }
public:
  federatedx_io(FEDERATEDX_SERVER *);
  virtual ~federatedx_io();

  bool is_readonly() const { return readonly; }
  bool is_active() const { return active; }

  const char * get_charsetname() const
  { return server->csname ? server->csname : "latin1"; }

  const char * get_hostname() const { return server->hostname; }
  const char * get_username() const { return server->username; }
  const char * get_password() const { return server->password; }
  const char * get_database() const { return server->database; }
  ushort       get_port() const     { return server->port; }
  const char * get_socket() const   { return server->socket; }
  
  static bool handles_scheme(const char *scheme);
  static federatedx_io *construct(MEM_ROOT *server_root,
                                  FEDERATEDX_SERVER *server);

  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr, size_t size)
  { TRASH(ptr, size); }
    
  virtual int query(const char *buffer, uint length)=0;
  virtual FEDERATEDX_IO_RESULT *store_result()=0;

  virtual size_t max_query_size() const=0;

  virtual my_ulonglong affected_rows() const=0;
  virtual my_ulonglong last_insert_id() const=0;

  virtual int error_code()=0;
  virtual const char *error_str()=0;
  
  virtual void reset()=0;
  virtual int commit()=0;
  virtual int rollback()=0;
  
  virtual int savepoint_set(ulong sp)=0;
  virtual ulong savepoint_release(ulong sp)=0;
  virtual ulong savepoint_rollback(ulong sp)=0;
  virtual void savepoint_restrict(ulong sp)=0;
  
  virtual ulong last_savepoint() const=0;
  virtual ulong actual_savepoint() const=0;
  virtual bool is_autocommit() const=0;

  virtual bool table_metadata(ha_statistics *stats, const char *table_name,
                              uint table_name_length, uint flag) = 0;
  
  /* resultset operations */
  
  virtual void free_result(FEDERATEDX_IO_RESULT *io_result)=0;
  virtual unsigned int get_num_fields(FEDERATEDX_IO_RESULT *io_result)=0;
  virtual my_ulonglong get_num_rows(FEDERATEDX_IO_RESULT *io_result)=0;
  virtual FEDERATEDX_IO_ROW *fetch_row(FEDERATEDX_IO_RESULT *io_result)=0;
  virtual ulong *fetch_lengths(FEDERATEDX_IO_RESULT *io_result)=0;
  virtual const char *get_column_data(FEDERATEDX_IO_ROW *row,
                                      unsigned int column)=0;
  virtual bool is_column_null(const FEDERATEDX_IO_ROW *row,
                              unsigned int column) const=0;
};


class federatedx_txn
{
  federatedx_io *txn_list;
  ulong savepoint_level;
  ulong savepoint_stmt;
  ulong savepoint_next;
  
  void release_scan();
public:
  federatedx_txn();
  ~federatedx_txn();
  
  bool has_connections() const { return txn_list != NULL; }
  bool in_transaction() const { return savepoint_next != 0; }
  int acquire(FEDERATEDX_SHARE *share, bool readonly, federatedx_io **io);
  void release(federatedx_io **io);
  void close(FEDERATEDX_SERVER *);

  bool txn_begin();
  int txn_commit();
  int txn_rollback();

  bool sp_acquire(ulong *save);
  int sp_rollback(ulong *save);
  int sp_release(ulong *save);

  bool stmt_begin();
  int stmt_commit();
  int stmt_rollback();
  void stmt_autocommit();
};


/*
  Class definition for the storage engine
*/
class ha_federatedx: public handler
{
  friend int federatedx_db_init(void *p);

  THR_LOCK_DATA lock;      /* MySQL lock */
  FEDERATEDX_SHARE *share;    /* Shared lock info */
  federatedx_txn *txn;
  federatedx_io *io;
  FEDERATEDX_IO_RESULT *stored_result;
  uint fetch_num; // stores the fetch num
  FEDERATEDX_IO_OFFSET current_position;  // Current position used by ::position()
  int remote_error_number;
  char remote_error_buf[FEDERATEDX_QUERY_BUFFER_SIZE];
  bool ignore_duplicates, replace_duplicates;
  bool insert_dup_update;
  DYNAMIC_STRING bulk_insert;

private:
  /*
      return 0 on success
      return errorcode otherwise
  */
  uint convert_row_to_internal_format(uchar *buf, FEDERATEDX_IO_ROW *row,
                                      FEDERATEDX_IO_RESULT *result);
  bool create_where_from_key(String *to, KEY *key_info, 
                             const key_range *start_key,
                             const key_range *end_key,
                             bool records_in_range, bool eq_range);
  int stash_remote_error();

  federatedx_txn *get_txn(THD *thd, bool no_create= FALSE);

  static int disconnect(handlerton *hton, MYSQL_THD thd);
  static int savepoint_set(handlerton *hton, MYSQL_THD thd, void *sv);
  static int savepoint_rollback(handlerton *hton, MYSQL_THD thd, void *sv);
  static int savepoint_release(handlerton *hton, MYSQL_THD thd, void *sv);
  static int commit(handlerton *hton, MYSQL_THD thd, bool all);
  static int rollback(handlerton *hton, MYSQL_THD thd, bool all);

  bool append_stmt_insert(String *query);

  int read_next(uchar *buf, FEDERATEDX_IO_RESULT *result);
  int index_read_idx_with_result_set(uchar *buf, uint index,
                                     const uchar *key,
                                     uint key_len,
                                     ha_rkey_function find_flag,
                                     FEDERATEDX_IO_RESULT **result);
  int real_query(const char *query, uint length);
  int real_connect(FEDERATEDX_SHARE *my_share, uint create_flag);
public:
  ha_federatedx(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_federatedx() {}
  /* The name that will be used for display purposes */
  const char *table_type() const { return "FEDERATED"; }
  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
   */
  // perhaps get index type
  const char *index_type(uint inx) { return "REMOTE"; }
  const char **bas_ext() const;
  /*
    This is a list of flags that says what the storage engine
    implements. The current table flags are documented in
    handler.h
  */
  ulonglong table_flags() const
  {
    /* fix server to be able to get remote server table flags */
    return (HA_PRIMARY_KEY_IN_READ_INDEX | HA_FILE_BASED
            | HA_REC_NOT_IN_SEQ | HA_AUTO_PART_KEY | HA_CAN_INDEX_BLOBS |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
            HA_NO_PREFIX_CHAR_KEYS | HA_PRIMARY_KEY_REQUIRED_FOR_DELETE |
            HA_PARTIAL_COLUMN_READ | HA_NULL_IN_KEY);
  }
  /*
    This is a bitmap of flags that says how the storage engine
    implements indexes. The current index flags are documented in
    handler.h. If you do not implement indexes, just return zero
    here.

    part is the key part to check. First key part is 0
    If all_parts it's set, MySQL want to know the flags for the combined
    index up to and including 'part'.
  */
    /* fix server to be able to get remote server index flags */
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return (HA_READ_NEXT | HA_READ_RANGE | HA_READ_AFTER_KEY);
  }
  uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_supported_keys()          const { return MAX_KEY; }
  uint max_supported_key_parts()     const { return MAX_REF_PARTS; }
  uint max_supported_key_length()    const { return FEDERATEDX_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length() const { return FEDERATEDX_MAX_KEY_LENGTH; }
  /*
    Called in test_quick_select to determine if indexes should be used.
    Normally, we need to know number of blocks . For federatedx we need to
    know number of blocks on remote side, and number of packets and blocks
    on the network side (?)
    Talk to Kostja about this - how to get the
    number of rows * ...
    disk scan time on other side (block size, size of the row) + network time ...
    The reason for "records * 1000" is that such a large number forces 
    this to use indexes "
  */
  double scan_time()
  {
    DBUG_PRINT("info", ("records %lu", (ulong) stats.records));
    return (double)(stats.records*1000); 
  }
  /*
    The next method will never be called if you do not implement indexes.
  */
  double read_time(uint index, uint ranges, ha_rows rows) 
  {
    /*
      Per Brian, this number is bugus, but this method must be implemented,
      and at a later date, he intends to document this issue for handler code
    */
    return (double) rows /  20.0+1;
  }

  const key_map *keys_to_use_for_scanning() { return &key_map_full; }
  /*
    Everything below are methods that we implment in ha_federatedx.cc.

    Most of these methods are not obligatory, skip them and
    MySQL will treat them as not implemented
  */
  int open(const char *name, int mode, uint test_if_locked);    // required
  int close(void);                                              // required

  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert(bool abort);
  int write_row(uchar *buf);
  int update_row(const uchar *old_data, uchar *new_data);
  int delete_row(const uchar *buf);
  int index_init(uint keynr, bool sorted);
  ha_rows estimate_rows_upper_bound();
  int index_read(uchar *buf, const uchar *key,
                 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(uchar *buf, uint idx, const uchar *key,
                     uint key_len, enum ha_rkey_function find_flag);
  int index_next(uchar *buf);
  int index_end();
  int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range, bool sorted);
  int read_range_next();
  /*
    unlike index_init(), rnd_init() can be called two times
    without rnd_end() in between (it only makes sense if scan=1).
    then the second call should prepare for the new table scan
    (e.g if rnd_init allocates the cursor, second call should
    position it to the start of the table, no need to deallocate
    and allocate it again
  */
  int rnd_init(bool scan);                                      //required
  int rnd_end();
  int rnd_next(uchar *buf);                                      //required
  int rnd_pos(uchar *buf, uchar *pos);                            //required
  void position(const uchar *record);                            //required
  int info(uint);                                              //required
  int extra(ha_extra_function operation);

  void update_auto_increment(void);
  int repair(THD* thd, HA_CHECK_OPT* check_opt);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);

  int delete_all_rows(void);
  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info);                      //required
  ha_rows records_in_range(uint inx, key_range *start_key,
                                   key_range *end_key);
  uint8 table_cache_type() { return HA_CACHE_TBL_NOCACHE; }

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);     //required
  bool get_error_message(int error, String *buf);
  int start_stmt(THD *thd, thr_lock_type lock_type);
  int external_lock(THD *thd, int lock_type);
  int reset(void);
  int free_result(void);
};

extern const char ident_quote_char;              // Character for quoting
                                                 // identifiers
extern const char value_quote_char;              // Character for quoting
                                                 // literals

extern bool append_ident(String *string, const char *name, uint length,
                         const char quote_char);


extern federatedx_io *instantiate_io_mysql(MEM_ROOT *server_root,
                                           FEDERATEDX_SERVER *server);
extern federatedx_io *instantiate_io_null(MEM_ROOT *server_root,
                                          FEDERATEDX_SERVER *server);
