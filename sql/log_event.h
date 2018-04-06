
/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file sql/log_event.h

  @brief Binary log event definitions.  This includes generic code
  common to all types of log events, as well as specific code for each
  type of log event.

  @addtogroup Replication
  @{
*/

#ifndef _log_event_h
#define _log_event_h

#include <atomic>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "binlog_event.h"
#include "control_events.h"
#include "lex_string.h"
#include "load_data_events.h"
#include "m_ctype.h"
#include "m_string.h"   // native_strncasecmp
#include "my_bitmap.h"  // MY_BITMAP
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/components/services/psi_stage_bits.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"  // SERVER_VERSION_LENGTH
#include "rows_event.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"  // OPTION_AUTO_IS_NULL
#include "sql/rpl_gtid.h"       // enum_gtid_type
#include "sql/rpl_utility.h"    // Hash_slave_rows
#include "sql/sql_const.h"
#include "sql_string.h"
#include "statement_events.h"
#include "typelib.h"  // TYPELIB
#include "uuid.h"

class String;
class THD;
class Table_id;
struct mysql_mutex_t;

enum class enum_row_image_type;
class Basic_ostream;

#ifdef MYSQL_SERVER
#include <stdio.h>

#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql/psi/psi_stage.h"
#include "sql/field.h"
#include "sql/key.h"
#include "sql/rpl_filter.h"  // rpl_filter
#include "sql/rpl_record.h"  // unpack_row
#include "sql/sql_class.h"   // THD
#include "sql/sql_plugin.h"
#include "sql/sql_plugin_ref.h"
#include "sql/sql_profile.h"
#include "sql/table.h"
#include "sql/xa.h"
#endif

#ifndef MYSQL_SERVER
#include "sql/rpl_tblmap.h"  // table_mapping
#endif

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_PSI_STAGE_INTERFACE
#include "mysql/psi/mysql_stage.h"
#endif

#ifndef MYSQL_SERVER
class Format_description_log_event;
#endif

extern PSI_memory_key key_memory_Incident_log_event_message;
extern PSI_memory_key key_memory_Rows_query_log_event_rows_query;
extern "C" MYSQL_PLUGIN_IMPORT ulong server_id;

/* Forward declarations */
using binary_log::Binary_log_event;
using binary_log::Format_description_event;
using binary_log::Log_event_footer;
using binary_log::Log_event_header;
using binary_log::Log_event_type;
using binary_log::checksum_crc32;
using binary_log::enum_binlog_checksum_alg;

typedef ulonglong sql_mode_t;
struct db_worker_hash_entry;

extern "C" MYSQL_PLUGIN_IMPORT char server_version[SERVER_VERSION_LENGTH];
#if defined(MYSQL_SERVER)
int ignored_error_code(int err_code);
#endif
#define PREFIX_SQL_LOAD "SQL_LOAD-"

/**
   Maximum length of the name of a temporary file
   PREFIX LENGTH - 9
   UUID          - UUID_LENGTH
   SEPARATORS    - 2
   SERVER ID     - 10 (range of server ID 1 to (2^32)-1 = 4,294,967,295)
   FILE ID       - 10 (uint)
   EXTENSION     - 7  (Assuming that the extension is always less than 7
                       characters)
*/
#define TEMP_FILE_MAX_LEN UUID_LENGTH + 38

/**
   Either assert or return an error.

   In debug build, the condition will be checked, but in non-debug
   builds, the error code given will be returned instead.

   @param COND   Condition to check
   @param ERRNO  Error number to return in non-debug builds
*/
#ifdef DBUG_OFF
#define ASSERT_OR_RETURN_ERROR(COND, ERRNO) \
  do {                                      \
    if (!(COND)) return ERRNO;              \
  } while (0)
#else
#define ASSERT_OR_RETURN_ERROR(COND, ERRNO) DBUG_ASSERT(COND)
#endif

#define LOG_EVENT_OFFSET 4

#define NUM_LOAD_DELIM_STRS 5

/*****************************************************************************

  MySQL Binary Log

  This log consists of events.  Each event has a fixed-length header,
  possibly followed by a variable length data body.

  The data body consists of an optional fixed length segment (post-header)
  and  an optional variable length segment.

  See the #defines below for the format specifics.

  The events which really update data are Query_log_event,
  Execute_load_query_log_event and old Load_log_event and
  Execute_load_log_event events (Execute_load_query is used together with
  Begin_load_query and Append_block events to replicate LOAD DATA INFILE.
  Create_file/Append_block/Execute_load (which includes Load_log_event)
  were used to replicate LOAD DATA before the 5.0.3).

 ****************************************************************************/

#define MAX_LOG_EVENT_HEADER                                                   \
  (                        /* in order of Query_log_event::write */            \
   (LOG_EVENT_HEADER_LEN + /* write_header */                                  \
    Binary_log_event::QUERY_HEADER_LEN + /* write_data */                      \
    Binary_log_event::                                                         \
        EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN) + /*write_post_header_for_derived \
                                                */                             \
   MAX_SIZE_LOG_EVENT_STATUS +                 /* status */                    \
   NAME_LEN +                                                                  \
   1)

/*
  The new option is added to handle large packets that are sent from the master
  to the slave. It is used to increase the thd(max_allowed) for both the
  DUMP thread on the master and the SQL/IO thread on the slave.
*/
#define MAX_MAX_ALLOWED_PACKET 1024 * 1024 * 1024

/* slave event post-header (this event is never written) */

#define SL_MASTER_PORT_OFFSET 8
#define SL_MASTER_POS_OFFSET 0
#define SL_MASTER_HOST_OFFSET 10

/* Intvar event post-header */

/* Intvar event data */
#define I_TYPE_OFFSET 0
#define I_VAL_OFFSET 1

/* 4 bytes which all binlogs should begin with */
#define BINLOG_MAGIC "\xfe\x62\x69\x6e"
#define BINLOG_MAGIC_SIZE 4

/**
  @addtogroup group_cs_binglog_event_header_flags Binlog Event Header Flags
  @ingroup group_cs
  @{
*/

/*
  The 2 flags below were useless :
  - the first one was never set
  - the second one was set in all Rotate events on the master, but not used for
  anything useful.
  So they are now removed and their place may later be reused for other
  flags. Then one must remember that Rotate events in 4.x have
  LOG_EVENT_FORCED_ROTATE_F set, so one should not rely on the value of the
  replacing flag when reading a Rotate event.
  I keep the defines here just to remember what they were.

  #define LOG_EVENT_TIME_F            0x1
  #define LOG_EVENT_FORCED_ROTATE_F   0x2
*/

/**
  @def LOG_EVENT_THREAD_SPECIFIC_F

  If the query depends on the thread (for example: TEMPORARY TABLE).
  Currently this is used by mysqlbinlog to know it must print
  SET @@PSEUDO_THREAD_ID=xx; before the query (it would not hurt to print it
  for every query but this would be slow).
*/
#define LOG_EVENT_THREAD_SPECIFIC_F 0x4

/**
  @def LOG_EVENT_SUPPRESS_USE_F

  Suppress the generation of 'USE' statements before the actual
  statement. This flag should be set for any events that does not need
  the current database set to function correctly. Most notable cases
  are 'CREATE DATABASE' and 'DROP DATABASE'.

  This flags should only be used in exceptional circumstances, since
  it introduce a significant change in behaviour regarding the
  replication logic together with the flags --binlog-do-db and
  --replicated-do-db.
 */
#define LOG_EVENT_SUPPRESS_USE_F 0x8

/*
  Note: this is a place holder for the flag
  LOG_EVENT_UPDATE_TABLE_MAP_VERSION_F (0x10), which is not used any
  more, please do not reused this value for other flags.
 */

/**
   @def LOG_EVENT_ARTIFICIAL_F

   Artificial events are created arbitarily and not written to binary
   log

   These events should not update the master log position when slave
   SQL thread executes them.
*/
#define LOG_EVENT_ARTIFICIAL_F 0x20

/**
   @def LOG_EVENT_RELAY_LOG_F

   Events with this flag set are created by slave IO thread and written
   to relay log
*/
#define LOG_EVENT_RELAY_LOG_F 0x40

/**
   @def LOG_EVENT_IGNORABLE_F

   For an event, 'e', carrying a type code, that a slave,
   's', does not recognize, 's' will check 'e' for
   LOG_EVENT_IGNORABLE_F, and if the flag is set, then 'e'
   is ignored. Otherwise, 's' acknowledges that it has
   found an unknown event in the relay log.
*/
#define LOG_EVENT_IGNORABLE_F 0x80

/**
   @def LOG_EVENT_NO_FILTER_F

   Events with this flag are not filtered (e.g. on the current
   database) and are always written to the binary log regardless of
   filters.
*/
#define LOG_EVENT_NO_FILTER_F 0x100

/**
   MTS: group of events can be marked to force its execution
   in isolation from any other Workers.
   So it's a marker for Coordinator to memorize and perform necessary
   operations in order to guarantee no interference from other Workers.
   The flag can be set ON only for an event that terminates its group.
   Typically that is done for a transaction that contains
   a query accessing more than OVER_MAX_DBS_IN_EVENT_MTS databases.
*/
#define LOG_EVENT_MTS_ISOLATE_F 0x200

/** @}*/

/**
  @def OPTIONS_WRITTEN_TO_BIN_LOG

  OPTIONS_WRITTEN_TO_BIN_LOG are the bits of thd->options which must
  be written to the binlog. OPTIONS_WRITTEN_TO_BIN_LOG could be
  written into the Format_description_log_event, so that if later we
  don't want to replicate a variable we did replicate, or the
  contrary, it's doable. But it should not be too hard to decide once
  for all of what we replicate and what we don't, among the fixed 32
  bits of thd->options.

  I (Guilhem) have read through every option's usage, and it looks
  like OPTION_AUTO_IS_NULL and OPTION_NO_FOREIGN_KEYS are the only
  ones which alter how the query modifies the table. It's good to
  replicate OPTION_RELAXED_UNIQUE_CHECKS too because otherwise, the
  slave may insert data slower than the master, in InnoDB.
  OPTION_BIG_SELECTS is not needed (the slave thread runs with
  max_join_size=HA_POS_ERROR) and OPTION_BIG_TABLES is not needed
  either, as the manual says (because a too big in-memory temp table
  is automatically written to disk).
*/
#define OPTIONS_WRITTEN_TO_BIN_LOG                      \
  (OPTION_AUTO_IS_NULL | OPTION_NO_FOREIGN_KEY_CHECKS | \
   OPTION_RELAXED_UNIQUE_CHECKS | OPTION_NOT_AUTOCOMMIT)

/* Shouldn't be defined before */
#define EXPECTED_OPTIONS \
  ((1ULL << 14) | (1ULL << 26) | (1ULL << 27) | (1ULL << 19))

#if OPTIONS_WRITTEN_TO_BIN_LOG != EXPECTED_OPTIONS
#error OPTIONS_WRITTEN_TO_BIN_LOG must NOT change their values!
#endif
#undef EXPECTED_OPTIONS /* You shouldn't use this one */

/**
   Maximum value of binlog logical timestamp.
*/
const int64 SEQ_MAX_TIMESTAMP = LLONG_MAX;

#ifdef MYSQL_SERVER
class Format_description_log_event;
class Item;
class MYSQL_BIN_LOG;
class Protocol;
class Slave_reporting_capability;
class Slave_worker;
class String;
class THD;
class sql_exchange;
template <class T>
class List;
#endif

class Relay_log_info;

#ifndef MYSQL_SERVER
enum enum_base64_output_mode {
  BASE64_OUTPUT_NEVER = 0,
  BASE64_OUTPUT_AUTO = 1,
  BASE64_OUTPUT_UNSPEC = 2,
  BASE64_OUTPUT_DECODE_ROWS = 3,
  /* insert new output modes here */
  BASE64_OUTPUT_MODE_COUNT
};

/*
  A structure for mysqlbinlog to know how to print events

  This structure is passed to the event's print() methods,

  There are two types of settings stored here:
  1. Last db, flags2, sql_mode etc comes from the last printed event.
     They are stored so that only the necessary USE and SET commands
     are printed.
  2. Other information on how to print the events, e.g. short_form,
     hexdump_from.  These are not dependent on the last event.
*/
struct PRINT_EVENT_INFO {
  /*
    Settings for database, sql_mode etc that comes from the last event
    that was printed.  We cache these so that we don't have to print
    them if they are unchanged.
  */
  // TODO: have the last catalog here ??
  char db[FN_REFLEN + 1];  // TODO: make this a LEX_STRING when thd->db is
  bool flags2_inited;
  uint32 flags2;
  bool sql_mode_inited;
  sql_mode_t sql_mode; /* must be same as THD.variables.sql_mode */
  ulong auto_increment_increment, auto_increment_offset;
  bool charset_inited;
  char charset[6];  // 3 variables, each of them storable in 2 bytes
  char time_zone_str[MAX_TIME_ZONE_NAME_LENGTH];
  uint lc_time_names_number;
  uint charset_database_number;
  uint default_collation_for_utf8mb4_number;
  my_thread_id thread_id;
  bool thread_id_printed;

  PRINT_EVENT_INFO();

  ~PRINT_EVENT_INFO() {
    close_cached_file(&head_cache);
    close_cached_file(&body_cache);
    close_cached_file(&footer_cache);
  }
  bool init_ok() /* tells if construction was successful */
  {
    return my_b_inited(&head_cache) && my_b_inited(&body_cache) &&
           my_b_inited(&footer_cache);
  }

  /* Settings on how to print the events */
  // True if the --short-form flag was specified
  bool short_form;
  // The X in --base64-output=X
  enum_base64_output_mode base64_output_mode;
  // True if the --skip-gtids flag was specified.
  bool skip_gtids;
  /*
    This is set whenever a Format_description_event is printed.
    Later, when an event is printed in base64, this flag is tested: if
    no Format_description_event has been seen, it is unsafe to print
    the base64 event, so an error message is generated.
  */
  bool printed_fd_event;
  my_off_t hexdump_from;
  uint8 common_header_len;
  char delimiter[16];

  uint verbose;
  table_mapping m_table_map;
  table_mapping m_table_map_ignored;

  /*
     These three caches are used by the row-based replication events to
     collect the header information and the main body of the events
     making up a statement and in footer section any verbose related details
     or comments related to the statment.
   */
  IO_CACHE head_cache;
  IO_CACHE body_cache;
  IO_CACHE footer_cache;
  /* Indicate if the body cache has unflushed events */
  bool have_unflushed_events;

  /*
     True if an event was skipped while printing the events of
     a transaction and no COMMIT statement or XID event was ever
     output (ie, was filtered out as well). This can be triggered
     by the --database option of mysqlbinlog.

     False, otherwise.
   */
  bool skipped_event_in_transaction;

  bool print_table_metadata;
};
#endif

/*
  A specific to the database-scheduled MTS type.
*/
struct Mts_db_names {
  const char *name[MAX_DBS_IN_EVENT_MTS];
  int num;
};

/**
  @class Log_event

  This is the abstract base class for binary log events.

  @section Log_event_binary_format Binary Format

  The format of the event is described @ref Binary_log_event_format "here".

  @subsection Log_event_format_of_atomic_primitives Format of Atomic Primitives

  - All numbers, whether they are 16-, 24-, 32-, or 64-bit numbers,
  are stored in little endian, i.e., the least significant byte first,
  unless otherwise specified.

*/
class Log_event {
 public:
  /**
     Enumeration of what kinds of skipping (and non-skipping) that can
     occur when the slave executes an event.

     @see shall_skip
     @see do_shall_skip
   */
  enum enum_skip_reason {
    /**
       Don't skip event.
    */
    EVENT_SKIP_NOT,

    /**
       Skip event by ignoring it.

       This means that the slave skip counter will not be changed.
    */
    EVENT_SKIP_IGNORE,

    /**
       Skip event and decrease skip counter.
    */
    EVENT_SKIP_COUNT
  };

 protected:
  enum enum_event_cache_type {
    EVENT_INVALID_CACHE = 0,
    /*
      If possible the event should use a non-transactional cache before
      being flushed to the binary log. This means that it must be flushed
      right after its correspondent statement is completed.
    */
    EVENT_STMT_CACHE,
    /*
      The event should use a transactional cache before being flushed to
      the binary log. This means that it must be flushed upon commit or
      rollback.
    */
    EVENT_TRANSACTIONAL_CACHE,
    /*
      The event must be written directly to the binary log without going
      through any cache.
    */
    EVENT_NO_CACHE,
    /*
       If there is a need for different types, introduce them before this.
    */
    EVENT_CACHE_COUNT
  };

  enum enum_event_logging_type {
    EVENT_INVALID_LOGGING = 0,
    /*
      The event must be written to a cache and upon commit or rollback
      written to the binary log.
    */
    EVENT_NORMAL_LOGGING,
    /*
      The event must be written to an empty cache and immediatly written
      to the binary log without waiting for any other event.
    */
    EVENT_IMMEDIATE_LOGGING,
    /*
       If there is a need for different types, introduce them before this.
    */
    EVENT_CACHE_LOGGING_COUNT
  };

  /**
    Writes the common header of this event to the given memory buffer.

    This does not update the checksum.

    @note This has the following form:

    +---------+---------+---------+------------+-----------+-------+
    |timestamp|type code|server_id|event_length|end_log_pos|flags  |
    |4 bytes  |1 byte   |4 bytes  |4 bytes     |4 bytes    |2 bytes|
    +---------+---------+---------+------------+-----------+-------+

    @param buf Memory buffer to write to. This must be at least
    LOG_EVENT_HEADER_LEN bytes long.

    @return The number of bytes written, i.e., always
    LOG_EVENT_HEADER_LEN.
  */
  uint32 write_header_to_memory(uchar *buf);
  /**
    Writes the common-header of this event to the given output stream and
    updates the checksum.

    @param ostream The event will be written to this output stream.

    @param data_length The length of the post-header section plus the
    length of the data section; i.e., the length of the event minus
    the common-header and the checksum.
  */
  bool write_header(Basic_ostream *ostream, size_t data_length);
  bool write_footer(Basic_ostream *ostream);
  bool need_checksum();

 public:
  /*
     A temp buffer for read_log_event; it is later analysed according to the
     event's type, and its content is distributed in the event-specific fields.
  */
  char *temp_buf;

  /*
    This variable determines whether the event is responsible for deallocating
    the memory pointed by temp_buf. When set to true temp_buf is deallocated
    and when it is set to false just make temp_buf point to NULL.
  */
  bool m_free_temp_buf_in_destructor;

  /* The number of seconds the query took to run on the master. */
  ulong exec_time;

  /*
    The master's server id (is preserved in the relay log; used to
    prevent from infinite loops in circular replication).
  */
  uint32 server_id;

  /**
    A storage to cache the global system variable's value.
    Handling of a separate event will be governed its member.
  */
  ulong rbr_exec_mode;

  /**
    Defines the type of the cache, if any, where the event will be
    stored before being flushed to disk.
  */
  enum_event_cache_type event_cache_type;

  /**
    Defines when information, i.e. event or cache, will be flushed
    to disk.
  */
  enum_event_logging_type event_logging_type;
  /**
    Placeholder for event checksum while writing to binlog.
  */
  ha_checksum crc;
  /**
    Index in @c rli->gaq array to indicate a group that this event is
    purging. The index is set by Coordinator to a group terminator
    event is checked by Worker at the event execution. The indexed
    data represent the Worker progress status.
  */
  ulong mts_group_idx;

  /**
   The Log_event_header class contains the variable present
   in the common header
  */
  binary_log::Log_event_header *common_header;

  /**
   The Log_event_footer class contains the variable present
   in the common footer. Currently, footer contains only the checksum_alg.
  */
  binary_log::Log_event_footer *common_footer;
  /**
    MTS: associating the event with either an assigned Worker or Coordinator.
    Additionally the member serves to tag deferred (IRU) events to avoid
    the event regular time destruction.
  */
  Relay_log_info *worker;

  /**
    A copy of the main rli value stored into event to pass to MTS worker rli
  */
  ulonglong future_event_relay_log_pos;

#ifdef MYSQL_SERVER
  THD *thd;
  /**
     Partition info associate with event to deliver to MTS event applier
  */
  db_worker_hash_entry *mts_assigned_partitions[MAX_DBS_IN_EVENT_MTS];

  Log_event(Log_event_header *header, Log_event_footer *footer,
            enum_event_cache_type cache_type_arg,
            enum_event_logging_type logging_type_arg);
  Log_event(THD *thd_arg, uint16 flags_arg,
            enum_event_cache_type cache_type_arg,
            enum_event_logging_type logging_type_arg, Log_event_header *header,
            Log_event_footer *footer);
  /*
    init_show_field_list() prepares the column names and types for the
    output of SHOW BINLOG EVENTS; it is used only by SHOW BINLOG
    EVENTS.
  */
  static void init_show_field_list(List<Item> *field_list);

  int net_send(Protocol *protocol, const char *log_name, my_off_t pos);

  /**
    Stores a string representation of this event in the Protocol.
    This is used by SHOW BINLOG EVENTS.

    @retval 0 success
    @retval nonzero error
  */
  virtual int pack_info(Protocol *protocol);

  virtual const char *get_db() { return thd ? thd->db().str : NULL; }
#else   // ifdef MYSQL_SERVER
  /* print*() functions are used by mysqlbinlog */
  virtual void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const = 0;
  void print_timestamp(IO_CACHE *file, time_t *ts) const;
  void print_header(IO_CACHE *file, PRINT_EVENT_INFO *print_event_info,
                    bool is_more) const;
  void print_base64(IO_CACHE *file, PRINT_EVENT_INFO *print_event_info,
                    bool is_more) const;
#endif  // ifdef MYSQL_SERVER ... else

  void *operator new(size_t size);

  static void operator delete(void *ptr, size_t) { my_free(ptr); }

  /* Placement version of the above operators */
  static void *operator new(size_t, void *ptr) { return ptr; }
  static void operator delete(void *, void *) {}
  /**
    Write the given buffer to the given output stream, updating the
    checksum if checksums are enabled.

    @param ostream The output stream to write to.
    @param buf The buffer to write.
    @param data_length The number of bytes to write.

    @retval false Success.
    @retval true Error.
  */
  bool wrapper_my_b_safe_write(Basic_ostream *ostream, const uchar *buf,
                               size_t data_length);

#ifdef MYSQL_SERVER
  virtual bool write(Basic_ostream *ostream) {
    return (write_header(ostream, get_data_size()) ||
            write_data_header(ostream) || write_data_body(ostream) ||
            write_footer(ostream));
  }

  time_t get_time();

  virtual bool write_data_header(Basic_ostream *) { return 0; }
  virtual bool write_data_body(Basic_ostream *) { return 0; }
#endif

  Log_event_type get_type_code() const { return common_header->type_code; }

  /**
    Return true if the event has to be logged using SBR for DMLs.
  */
  virtual bool is_sbr_logging_format() const { return false; }
  /**
    Return true if the event has to be logged using RBR for DMLs.
  */
  virtual bool is_rbr_logging_format() const { return false; }

  /*
   is_valid is event specific sanity checks to determine that the
    object is correctly initialized.
  */
  bool is_valid();
  void set_artificial_event() {
    common_header->flags |= LOG_EVENT_ARTIFICIAL_F;
    /*
      Artificial events are automatically generated and do not exist
      in master's binary log, so log_pos should be set to 0.
    */
    common_header->log_pos = 0;
  }
  void set_relay_log_event() { common_header->flags |= LOG_EVENT_RELAY_LOG_F; }
  bool is_artificial_event() const {
    return common_header->flags & LOG_EVENT_ARTIFICIAL_F;
  }
  bool is_relay_log_event() const {
    return common_header->flags & LOG_EVENT_RELAY_LOG_F;
  }
  bool is_ignorable_event() const {
    return common_header->flags & LOG_EVENT_IGNORABLE_F;
  }
  bool is_no_filter_event() const {
    return common_header->flags & LOG_EVENT_NO_FILTER_F;
  }
  inline bool is_using_trans_cache() const {
    return (event_cache_type == EVENT_TRANSACTIONAL_CACHE);
  }
  inline bool is_using_stmt_cache() const {
    return (event_cache_type == EVENT_STMT_CACHE);
  }
  inline bool is_using_immediate_logging() const {
    return (event_logging_type == EVENT_IMMEDIATE_LOGGING);
  }

  /*
     For the events being decoded in BAPI, common_header should
     point to the header object which is contained within the class
     Binary_log_event.
  */
  Log_event(Log_event_header *header, Log_event_footer *footer);

  virtual ~Log_event() { free_temp_buf(); }
  void register_temp_buf(char *buf, bool free_in_destructor = true) {
    m_free_temp_buf_in_destructor = free_in_destructor;
    temp_buf = buf;
  }
  void free_temp_buf() {
    if (temp_buf) {
      if (m_free_temp_buf_in_destructor) my_free(temp_buf);
      temp_buf = 0;
    }
  }
  /*
    Get event length for simple events. For complicated events the length
    is calculated during write()
  */
  virtual size_t get_data_size() { return 0; }
  /**
    Returns the human readable name of the given event type.
  */
  static const char *get_type_str(Log_event_type type);
  /**
    Returns the human readable name of this event's type.
  */
  const char *get_type_str() const;
  /* Return start of query time or current time */

#if defined(MYSQL_SERVER)
  /**
     Is called from get_mts_execution_mode() to

     @return true  if the event needs applying with synchronization
                   agaist Workers, otherwise
             false

     @note There are incompatile combinations such as referred further events
           are wrapped with BEGIN/COMMIT. Such cases should be identified
           by the caller and treats correspondingly.

           todo: to mts-support Old master Load-data related events
  */
  bool is_mts_sequential_exec() {
    switch (get_type_code()) {
      case binary_log::STOP_EVENT:
      case binary_log::ROTATE_EVENT:
      case binary_log::SLAVE_EVENT:
      case binary_log::FORMAT_DESCRIPTION_EVENT:
      case binary_log::INCIDENT_EVENT:
        return true;
      default:
        return false;
    }
  }

 private:
  /*
    possible decisions by get_mts_execution_mode().
    The execution mode can be PARALLEL or not (thereby sequential
    unless impossible at all). When it's sequential it further  breaks into
    ASYNChronous and SYNChronous.
  */
  enum enum_mts_event_exec_mode {
    /*
      Event is run by a Worker.
    */
    EVENT_EXEC_PARALLEL,
    /*
      Event is run by Coordinator.
    */
    EVENT_EXEC_ASYNC,
    /*
      Event is run by Coordinator and requires synchronization with Workers.
    */
    EVENT_EXEC_SYNC,
    /*
      Event can't be executed neither by Workers nor Coordinator.
    */
    EVENT_EXEC_CAN_NOT
  };

  /**
     MTS Coordinator finds out a way how to execute the current event.

     Besides the parallelizable case, some events have to be applied by
     Coordinator concurrently with Workers and some to require synchronization
     with Workers (@c see wait_for_workers_to_finish) before to apply them.

     @param mts_in_group      the being group parsing status, true
                              means inside the group

     @retval EVENT_EXEC_PARALLEL  if event is executed by a Worker
     @retval EVENT_EXEC_ASYNC     if event is executed by Coordinator
     @retval EVENT_EXEC_SYNC      if event is executed by Coordinator
                                  with synchronization against the Workers
  */
  enum enum_mts_event_exec_mode get_mts_execution_mode(bool mts_in_group) {
    /*
      Slave workers are unable to handle Format_description_log_event,
      Rotate_log_event and Previous_gtids_log_event correctly.
      However, when a transaction spans multiple relay logs, these
      events occur in the middle of a transaction. The way we handle
      this is by marking the events as 'ASYNC', meaning that the
      coordinator thread will handle the events without stopping the
      worker threads.

      @todo Refactor this: make Log_event::get_slave_worker handle
      transaction boundaries in a more robust way, so that it is able
      to process Format_description_log_event, Rotate_log_event, and
      Previous_gtids_log_event.  Then, when these events occur in the
      middle of a transaction, make them part of the transaction so
      that the worker that handles the transaction handles these
      events too. /Sven
    */
    if (
        /*
          When a Format_description_log_event occurs in the middle of
          a transaction, it either has the slave's server_id, or has
          end_log_pos==0.

          @todo This does not work when master and slave have the same
          server_id and replicate-same-server-id is enabled, since
          events that are not in the middle of a transaction will be
          executed in ASYNC mode in that case.
        */
        (get_type_code() == binary_log::FORMAT_DESCRIPTION_EVENT &&
         ((server_id == (uint32)::server_id) ||
          (common_header->log_pos == 0))) ||
        /*
          All Previous_gtids_log_events in the relay log are generated
          by the slave. They don't have any meaning to the applier, so
          they can always be ignored by the applier. So we can process
          them asynchronously by the coordinator. It is also important
          to not feed them to workers because that confuses
          get_slave_worker.
        */
        (get_type_code() == binary_log::PREVIOUS_GTIDS_LOG_EVENT) ||
        /*
          Rotate_log_event can occur in the middle of a transaction.
          When this happens, either it is a Rotate event generated on
          the slave which has the slave's server_id, or it is a Rotate
          event that originates from a master but has end_log_pos==0.
        */
        (get_type_code() == binary_log::ROTATE_EVENT &&
         ((server_id == (uint32)::server_id) ||
          (common_header->log_pos == 0 && mts_in_group))))
      return EVENT_EXEC_ASYNC;
    else if (is_mts_sequential_exec())
      return EVENT_EXEC_SYNC;
    else
      return EVENT_EXEC_PARALLEL;
  }

  /**
     @return index  in [0, M] range to indicate
             to be assigned worker;
             M is the max index of the worker pool.
  */
  Slave_worker *get_slave_worker(Relay_log_info *rli);

  /*
    Group of events can be marked to force its execution
    in isolation from any other Workers.
    Typically that is done for a transaction that contains
    a query accessing more than OVER_MAX_DBS_IN_EVENT_MTS databases.
    Factually that's a sequential mode where a Worker remains to
    be the applier.
  */
  virtual void set_mts_isolate_group() {
    DBUG_ASSERT(ends_group() || get_type_code() == binary_log::QUERY_EVENT ||
                get_type_code() == binary_log::EXECUTE_LOAD_QUERY_EVENT);
    common_header->flags |= LOG_EVENT_MTS_ISOLATE_F;
  }

 public:
  /**
     The method fills in pointers to event's database name c-strings
     to a supplied array.
     In other than Query-log-event case the returned array contains
     just one item.
     @param[out] arg pointer to a struct containing char* array
                     pointers to be filled in and the number
                     of filled instances.
     @param rpl_filter pointer to a replication filter.

     @return     number of the filled intances indicating how many
                 databases the event accesses.
  */
  virtual uint8 get_mts_dbs(Mts_db_names *arg,
                            Rpl_filter *rpl_filter MY_ATTRIBUTE((unused))) {
    arg->name[0] = get_db();

    return arg->num = mts_number_dbs();
  }

  /**
     @return true  if events carries partitioning data (database names).
  */
  bool contains_partition_info(bool);

  /*
    @return  the number of updated by the event databases.

    @note In other than Query-log-event case that's one.
  */
  virtual uint8 mts_number_dbs() { return 1; }

  /**
    @return true   if the terminal event of a group is marked to
                   execute in isolation from other Workers,
            false  otherwise
  */
  bool is_mts_group_isolated() {
    return common_header->flags & LOG_EVENT_MTS_ISOLATE_F;
  }

    /**
       Events of a certain type can start or end a group of events treated
       transactionally wrt binlog.

       Public access is required by implementation of recovery + skip.

       @return true  if the event starts a group (transaction)
               false otherwise
    */
#endif
  virtual bool starts_group() const { return false; }
  /**
     @return true  if the event ends a group (transaction)
             false otherwise
  */
  virtual bool ends_group() const { return false; }
#ifdef MYSQL_SERVER
  /**
     Apply the event to the database.

     This function represents the public interface for applying an
     event.

     @see do_apply_event
   */
  int apply_event(Relay_log_info *rli);

  /**
     Apply the GTID event in curr_group_data to the database.

     @param rli Pointer to coordinato's relay log info.

     @retval 0 success
     @retval 1 error
  */
  inline int apply_gtid_event(Relay_log_info *rli);

  /**
     Update the relay log position.

     This function represents the public interface for "stepping over"
     the event and will update the relay log information.

     @see do_update_pos
   */
  int update_pos(Relay_log_info *rli) { return do_update_pos(rli); }

  /**
     Decide if the event shall be skipped, and the reason for skipping
     it.

     @see do_shall_skip
   */
  enum_skip_reason shall_skip(Relay_log_info *rli) {
    DBUG_ENTER("Log_event::shall_skip");
    enum_skip_reason ret = do_shall_skip(rli);
    DBUG_PRINT("info", ("skip reason=%d=%s", ret,
                        ret == EVENT_SKIP_NOT
                            ? "NOT"
                            : ret == EVENT_SKIP_IGNORE ? "IGNORE" : "COUNT"));
    DBUG_RETURN(ret);
  }

  /**
    Primitive to apply an event to the database.

    This is where the change to the database is made.

    @note The primitive is protected instead of private, since there
    is a hierarchy of actions to be performed in some cases.

    @see Format_description_log_event::do_apply_event()

    @param rli Pointer to relay log info structure

    @retval 0     Event applied successfully
    @retval errno Error code if event application failed
  */
  virtual int do_apply_event(Relay_log_info const *rli MY_ATTRIBUTE((unused))) {
    return 0; /* Default implementation does nothing */
  }

  virtual int do_apply_event_worker(Slave_worker *w);

 protected:
  /**
     Helper function to ignore an event w.r.t. the slave skip counter.

     This function can be used inside do_shall_skip() for functions
     that cannot end a group. If the slave skip counter is 1 when
     seeing such an event, the event shall be ignored, the counter
     left intact, and processing continue with the next event.

     A typical usage is:
     @code
     enum_skip_reason do_shall_skip(Relay_log_info *rli) {
       return continue_group(rli);
     }
     @endcode

     @return Skip reason
   */
  enum_skip_reason continue_group(Relay_log_info *rli);

  /**
     Advance relay log coordinates.

     This function is called to advance the relay log coordinates to
     just after the event.  It is essential that both the relay log
     coordinate and the group log position is updated correctly, since
     this function is used also for skipping events.

     Normally, each implementation of do_update_pos() shall:

     - Update the event position to refer to the position just after
       the event.

     - Update the group log position to refer to the position just
       after the event <em>if the event is last in a group</em>

     @param rli Pointer to relay log info structure

     @retval 0     Coordinates changed successfully
     @retval errno Error code if advancing failed (usually just
                   1). Observe that handler errors are returned by the
                   do_apply_event() function, and not by this one.
   */
  virtual int do_update_pos(Relay_log_info *rli);

  /**
     Decide if this event shall be skipped or not and the reason for
     skipping it.

     The default implementation decide that the event shall be skipped
     if either:

     - the server id of the event is the same as the server id of the
       server and <code>rli->replicate_same_server_id</code> is true,
       or

     - if <code>rli->slave_skip_counter</code> is greater than zero.

     @see do_apply_event
     @see do_update_pos

     @retval Log_event::EVENT_SKIP_NOT
     The event shall not be skipped and should be applied.

     @retval Log_event::EVENT_SKIP_IGNORE
     The event shall be skipped by just ignoring it, i.e., the slave
     skip counter shall not be changed. This happends if, for example,
     the originating server id of the event is the same as the server
     id of the slave.

     @retval Log_event::EVENT_SKIP_COUNT
     The event shall be skipped because the slave skip counter was
     non-zero. The caller shall decrease the counter by one.
   */
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli);
#endif
};

/*
   One class for each type of event.
   Two constructors for each class:
   - one to create the event for logging (when the server acts as a master),
   called after an update to the database is done,
   which accepts parameters like the query, the database, the options for LOAD
   DATA INFILE...
   - one to create the event from a packet (when the server acts as a slave),
   called before reproducing the update, which accepts parameters (like a
   buffer). Used to read from the master, from the relay log, and in
   mysqlbinlog. This constructor must be format-tolerant.
*/

/**
  A Query event is written to the binary log whenever the database is
  modified on the master, unless row based logging is used.

  Query_log_event is created for logging, and is called after an update to the
  database is done. It is used when the server acts as the master.

  Virtual inheritance is required here to handle the diamond problem in
  the class @c Execute_load_query_log_event.
  The diamond structure is explained in @c Excecute_load_query_log_event

  @internal
  The inheritance structure is as follows:

            Binary_log_event
                   ^
                   |
                   |
            Query_event  Log_event
                   \       /
         <<virtual>>\     /
                     \   /
                Query_log_event
  @endinternal
*/
class Query_log_event : public virtual binary_log::Query_event,
                        public Log_event {
 protected:
  Log_event_header::Byte *data_buf;

 public:
  /*
    For events created by Query_log_event::do_apply_event (and
    Load_log_event::do_apply_event()) we need the *original* thread
    id, to be able to log the event with the original (=master's)
    thread id (fix for BUG#1686).
  */
  my_thread_id slave_proxy_id;

  /**
   True if this is a ROLLBACK event injected by the mts coordinator to finish a
   group corresponding to a partial transaction in the relay log.
   False otherwise and by default, as it must be explicitly set to true by the
   coordinator.
  */
  bool rollback_injected_by_coord = false;

  /**
    The flag indicates whether the DDL query has been (already)
    committed or not.  It's initialized as OFF at the event instantiation,
    flips ON when the DDL transaction has been committed with
    all its possible extra statement due to replication or GTID.

    The flag status is also checked in few places to catch uncommitted
    transactions which can normally happen due to filtering out. In
    such a case the commit is deferred to @c Log_event::do_update_pos().
  */
  bool has_ddl_committed;

#ifdef MYSQL_SERVER

  Query_log_event(THD *thd_arg, const char *query_arg, size_t query_length,
                  bool using_trans, bool immediate, bool suppress_use,
                  int error, bool ignore_command = false);
  const char *get_db() override { return db; }

  /**
     @param[out] arg pointer to a struct containing char* array
                     pointers be filled in and the number of
                     filled instances.
                     In case the number exceeds MAX_DBS_IN_EVENT_MTS,
                     the overfill is indicated with assigning the number to
                     OVER_MAX_DBS_IN_EVENT_MTS.
     @param rpl_filter pointer to a replication filter.

     @return     number of databases in the array or OVER_MAX_DBS_IN_EVENT_MTS.
  */
  virtual uint8 get_mts_dbs(Mts_db_names *arg,
                            Rpl_filter *rpl_filter) override {
    if (mts_accessed_dbs == OVER_MAX_DBS_IN_EVENT_MTS) {
      // the empty string db name is special to indicate sequential applying
      mts_accessed_db_names[0][0] = 0;
    } else {
      for (uchar i = 0; i < mts_accessed_dbs; i++) {
        char *db_name = mts_accessed_db_names[i];

        // Only default database is rewritten.
        if (!rpl_filter->is_rewrite_empty() && !strcmp(get_db(), db_name)) {
          size_t dummy_len;
          const char *db_filtered =
              rpl_filter->get_rewrite_db(db_name, &dummy_len);
          // db_name != db_filtered means that db_name is rewritten.
          if (strcmp(db_name, db_filtered)) db_name = (char *)db_filtered;
        }
        arg->name[i] = db_name;
      }
    }
    return arg->num = mts_accessed_dbs;
  }

  void attach_temp_tables_worker(THD *, const Relay_log_info *);
  void detach_temp_tables_worker(THD *, const Relay_log_info *);

  virtual uchar mts_number_dbs() override { return mts_accessed_dbs; }

  int pack_info(Protocol *protocol) override;
#else
  void print_query_header(IO_CACHE *file,
                          PRINT_EVENT_INFO *print_event_info) const;
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
  static bool rewrite_db_in_buffer(char **buf, ulong *event_len,
                                   const Format_description_event &fde);
#endif

  Query_log_event();

  Query_log_event(const char *buf, uint event_len,
                  const Format_description_event *description_event,
                  Log_event_type event_type);
  ~Query_log_event() {
    if (data_buf) my_free(data_buf);
  }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
  virtual bool write_post_header_for_derived(Basic_ostream *) { return false; }
#endif

  /*
    Returns number of bytes additionally written to post header by derived
    events (so far it is only Execute_load_query event).
  */
  virtual ulong get_post_header_size_for_derived() { return 0; }
  /* Writes derived event-specific part of post header. */

 public: /* !!! Public in this patch to allow old usage */
#if defined(MYSQL_SERVER)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;

  int do_apply_event(Relay_log_info const *rli, const char *query_arg,
                     size_t q_len_arg);
#endif /* MYSQL_SERVER */
  /*
    If true, the event always be applied by slave SQL thread or be printed by
    mysqlbinlog
   */
  bool is_trans_keyword() const {
    /*
      Before the patch for bug#50407, The 'SAVEPOINT and ROLLBACK TO'
      queries input by user was written into log events directly.
      So the keywords can be written in both upper case and lower case
      together, strncasecmp is used to check both cases. they also could be
      binlogged with comments in the front of these keywords. for examples:
        / * bla bla * / SAVEPOINT a;
        / * bla bla * / ROLLBACK TO a;
      but we don't handle these cases and after the patch, both quiries are
      binlogged in upper case with no comments.
     */
    return !strncmp(query, "BEGIN", q_len) ||
           !strncmp(query, "COMMIT", q_len) ||
           !native_strncasecmp(query, "SAVEPOINT", 9) ||
           !native_strncasecmp(query, "ROLLBACK", 8) ||
           !native_strncasecmp(query, STRING_WITH_LEN("XA START")) ||
           !native_strncasecmp(query, STRING_WITH_LEN("XA END")) ||
           !native_strncasecmp(query, STRING_WITH_LEN("XA PREPARE")) ||
           !native_strncasecmp(query, STRING_WITH_LEN("XA COMMIT")) ||
           !native_strncasecmp(query, STRING_WITH_LEN("XA ROLLBACK"));
  }

  /**
    When a query log event contains a non-transaction control statement, we
    assume that it is changing database content (DML) and was logged using
    binlog_format=statement.

    @return True the event represents a statement that was logged using SBR
            that can change database content.
            False for transaction control statements.
  */
  bool is_sbr_logging_format() const override { return !is_trans_keyword(); }

  /**
     Notice, DDL queries are logged without BEGIN/COMMIT parentheses
     and identification of such single-query group
     occures within logics of @c get_slave_worker().
  */

  bool starts_group() const override {
    return !strncmp(query, "BEGIN", q_len) ||
           !strncmp(query, STRING_WITH_LEN("XA START"));
  }

  virtual bool ends_group() const override {
    return !strncmp(query, "COMMIT", q_len) ||
           (!native_strncasecmp(query, STRING_WITH_LEN("ROLLBACK")) &&
            native_strncasecmp(query, STRING_WITH_LEN("ROLLBACK TO "))) ||
           !strncmp(query, STRING_WITH_LEN("XA ROLLBACK"));
  }
  static size_t get_query(const char *buf, size_t length,
                          const Format_description_event *fd_event,
                          char **query);

  bool is_query_prefix_match(const char *pattern, uint p_len) {
    return !strncmp(query, pattern, p_len);
  }
};

/**
  @class Format_description_log_event

  For binlog version 4.
  This event is saved by threads which read it, as they need it for future
  use (to decode the ordinary events).
  This is the subclass of Format_description_event

  @internal
  The inheritance structure in the current design for the classes is
  as follows:

            Binary_log_event
                   ^
                   |
                   |
            Format_description_event  Log_event
                               \       /
                                \     /
                                 \   /
                    Format_description_log_event
  @endinternal
  @section Format_description_log_event_binary_format Binary Format
*/

class Format_description_log_event : public Format_description_event,
                                     public Log_event {
 public:
  /*
    MTS Workers and Coordinator share the event and that affects its
    destruction. Instantiation is always done by Coordinator/SQL thread.
    Workers are allowed to destroy only "obsolete" instances, those
    that are not actual for Coordinator anymore but needed to Workers
    that are processing queued events depending on the old instance.
    The counter of a new FD is incremented by Coordinator or Worker at
    time of {Relay_log_info,Slave_worker}::set_rli_description_event()
    execution.
    In the same methods the counter of the "old" FD event is decremented
    and when it drops to zero the old FD is deleted.
    The latest read from relay-log event is to be
    destroyed by Coordinator/SQL thread at its thread exit.
    Notice the counter is processed even in the single-thread mode where
    decrement and increment are done by the single SQL thread.
  */
  std::atomic<int32> atomic_usage_counter{0};

  Format_description_log_event();
  Format_description_log_event(
      const char *buf, const Format_description_event *description_event);
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  size_t get_data_size() override {
    /*
      The vector of post-header lengths is considered as part of the
      post-header, because in a given version it never changes (contrary to the
      query in a Query_log_event).
    */
    return Binary_log_event::FORMAT_DESCRIPTION_HEADER_LEN;
  }

 protected:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif
};

/**
  @class Intvar_log_event

  The class derives from the class Intvar_event in Binlog API,
  defined in the header binlog_event.h. An Intvar_log_event is
  created just before a Query_log_event, if the query uses one
  of the variables LAST_INSERT_ID or INSERT_ID. This class is used
  by the slave for applying the event.

  @internal
  The inheritance structure in the current design for the classes is
  as follows:

        Binary_log_event
               ^
               |
               |
           Intvar_event  Log_event
                \       /
                 \     /
                  \   /
             Intvar_log_event
  @endinternal
*/
class Intvar_log_event : public binary_log::Intvar_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  Intvar_log_event(THD *thd_arg, uchar type_arg, ulonglong val_arg,
                   enum_event_cache_type cache_type_arg,
                   enum_event_logging_type logging_type_arg)
      : binary_log::Intvar_event(type_arg, val_arg),
        Log_event(thd_arg, 0, cache_type_arg, logging_type_arg, header(),
                  footer()) {
    common_header->set_is_valid(true);
  }
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  Intvar_log_event(const char *buf,
                   const Format_description_event *description_event);
  ~Intvar_log_event() {}
  size_t get_data_size() override {
    return 9; /* sizeof(type) + sizeof(val) */
    ;
  }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
#endif

  bool is_sbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif
};

/**
  @class Rand_log_event

  Logs random seed used by the next RAND(), and by PASSWORD() in 4.1.0.
  4.1.1 does not need it (it's repeatable again) so this event needn't be
  written in 4.1.1 for PASSWORD() (but the fact that it is written is just a
  waste, it does not cause bugs).

  The state of the random number generation consists of 128 bits,
  which are stored internally as two 64-bit numbers.

  @internal
  The inheritance structure in the current design for the classes is
  as follows:
        Binary_log_event
               ^
               |
               |
           Rand_event  Log_event
                \       /
                 \     /
                  \   /
              Rand_log_event
  @endinternal
*/
class Rand_log_event : public binary_log::Rand_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  Rand_log_event(THD *thd_arg, ulonglong seed1_arg, ulonglong seed2_arg,
                 enum_event_cache_type cache_type_arg,
                 enum_event_logging_type logging_type_arg)
      : binary_log::Rand_event(seed1_arg, seed2_arg),
        Log_event(thd_arg, 0, cache_type_arg, logging_type_arg, header(),
                  footer()) {
    common_header->set_is_valid(true);
  }
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  Rand_log_event(const char *buf,
                 const Format_description_event *description_event);
  ~Rand_log_event() {}
  size_t get_data_size() override { return 16; /* sizeof(ulonglong) * 2*/ }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
#endif

  bool is_sbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif
};

/**
  @class Xid_log_event

  This is the subclass of Xid_event defined in libbinlogevent,
  An XID event is generated for a commit of a transaction that modifies one or
  more tables of an XA-capable storage engine
  Logs xid of the transaction-to-be-committed in the 2pc protocol.
  Has no meaning in replication, slaves ignore it
  The inheritance structure in the current design for the classes is
  as follows

  @internal
  The inheritance structure in the current design for the classes is
  as follows:
        Binary_log_event
               ^
               |
               |
           Xid_event  Log_event
                \       /
                 \     /
                  \   /
               Xid_log_event
  @endinternal
*/
#ifndef MYSQL_SERVER
typedef ulonglong my_xid;  // this line is the same as in handler.h
#endif

class Xid_apply_log_event : public Log_event {
 protected:
#ifdef MYSQL_SERVER
  Xid_apply_log_event(THD *thd_arg, Log_event_header *header_arg,
                      Log_event_footer *footer_arg)
      : Log_event(thd_arg, 0, Log_event::EVENT_TRANSACTIONAL_CACHE,
                  Log_event::EVENT_NORMAL_LOGGING, header_arg, footer_arg){};
#endif
  Xid_apply_log_event(Log_event_header *header_arg,
                      Log_event_footer *footer_arg)
      : Log_event(header_arg, footer_arg) {}
  ~Xid_apply_log_event() {}
  virtual bool ends_group() const override { return true; }
#if defined(MYSQL_SERVER)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_apply_event_worker(Slave_worker *rli) override;
  virtual bool do_commit(THD *thd_arg) = 0;
#endif
};

class Xid_log_event : public binary_log::Xid_event, public Xid_apply_log_event {
 public:
#ifdef MYSQL_SERVER
  Xid_log_event(THD *thd_arg, my_xid x)
      : binary_log::Xid_event(x),
        Xid_apply_log_event(thd_arg, header(), footer()) {
    common_header->set_is_valid(true);
  }
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  Xid_log_event(const char *buf,
                const Format_description_event *description_event);
  ~Xid_log_event() {}
  size_t get_data_size() override { return sizeof(xid); }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
#endif
 private:
#if defined(MYSQL_SERVER)
  bool do_commit(THD *thd_arg) override;
#endif
};

/**
  @class XA_prepare_log_event

  Similar to Xid_log_event except that
  - it is specific to XA transaction
  - it carries out the prepare logics rather than the final committing
    when @c one_phase member is off.
  From the groupping perspective the event finalizes the current "prepare" group
  started with XA START Query-log-event.
  When @c one_phase is false Commit of Rollback for XA transaction are
  logged separately to the prepare-group events so being a groups of
  their own.
*/

class XA_prepare_log_event : public binary_log::XA_prepare_event,
                             public Xid_apply_log_event {
 private:
  /* Total size of buffers to hold serialized members of XID struct */
  static const int xid_bufs_size = 12;

 public:
#ifdef MYSQL_SERVER
  XA_prepare_log_event(THD *thd_arg, XID *xid_arg, bool one_phase_arg = false)
      : binary_log::XA_prepare_event((void *)xid_arg, one_phase_arg),
        Xid_apply_log_event(thd_arg, header(), footer()) {}
#endif
  XA_prepare_log_event(const char *buf,
                       const Format_description_event *description_event)
      : binary_log::XA_prepare_event(buf, description_event),
        Xid_apply_log_event(header(), footer()) {
    DBUG_ENTER(
        "XA_prepare_log_event::XA_prepare_log_event(const char*, const "
        "Format_description_log_event *)");
    xid = NULL;
    DBUG_VOID_RETURN;
  }
  Log_event_type get_type_code() { return binary_log::XA_PREPARE_LOG_EVENT; }
  size_t get_data_size() override {
    return xid_bufs_size + my_xid.gtrid_length + my_xid.bqual_length;
  }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif
#if defined(MYSQL_SERVER)
  int pack_info(Protocol *protocol) override;
  bool do_commit(THD *thd) override;
#endif
};

/**
  @class User_var_log_event

  Every time a query uses the value of a user variable, a User_var_log_event is
  written before the Query_log_event, to set the user variable.

  @internal
  The inheritance structure in the current design for the classes is
  as follows:
        Binary_log_event
               ^
               |
               |
      User_var_event  Log_event
                \       /
                 \     /
                  \   /
            User_var_log_event
  @endinternal
*/
class User_var_log_event : public binary_log::User_var_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  bool deferred;
  query_id_t query_id;
  User_var_log_event(THD *thd_arg, const char *name_arg, uint name_len_arg,
                     char *val_arg, ulong val_len_arg, Item_result type_arg,
                     uint charset_number_arg, uchar flags_arg,
                     enum_event_cache_type cache_type_arg,
                     enum_event_logging_type logging_type_arg)
      : binary_log::User_var_event(name_arg, name_len_arg, val_arg, val_len_arg,
                                   type_arg, charset_number_arg, flags_arg),
        Log_event(thd_arg, 0, cache_type_arg, logging_type_arg, header(),
                  footer()),
        deferred(false) {
    common_header->set_is_valid(name != 0);
  }
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  User_var_log_event(const char *buf, uint event_len,
                     const Format_description_event *description_event);
  ~User_var_log_event() {}
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
  /*
     Getter and setter for deferred User-event.
     Returns true if the event is not applied directly
     and which case the applier adjusts execution path.
  */
  bool is_deferred() { return deferred; }
  /*
    In case of the deffered applying the variable instance is flagged
    and the parsing time query id is stored to be used at applying time.
  */
  void set_deferred(query_id_t qid) {
    deferred = true;
    query_id = qid;
  }
#endif

  bool is_sbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif
};

/**
  @class Stop_log_event

*/
class Stop_log_event : public binary_log::Stop_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  Stop_log_event()
      : Log_event(header(), footer(), Log_event::EVENT_INVALID_CACHE,
                  Log_event::EVENT_INVALID_LOGGING) {
    common_header->set_is_valid(true);
  }

#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  Stop_log_event(const char *buf,
                 const Format_description_event *description_event)
      : binary_log::Stop_event(buf, description_event),
        Log_event(header(), footer()) {
    DBUG_ENTER(
        "Stop_log_event::Stop_log_event(const char*, const "
        "Format_description_log_event *)");
    DBUG_VOID_RETURN;
  }

  ~Stop_log_event() {}
  Log_event_type get_type_code() { return binary_log::STOP_EVENT; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *) override {
    /*
      Events from ourself should be skipped, but they should not
      decrease the slave skip counter.
     */
    if (this->server_id == ::server_id)
      return Log_event::EVENT_SKIP_IGNORE;
    else
      return Log_event::EVENT_SKIP_NOT;
  }
#endif
};

/**
  @class Rotate_log_event

  This will be deprecated when we move to using sequence ids.
  This class is a subclass of Rotate_event, defined in binlogevent, and is used
  by the slave for updating the position in the relay log.

  It is used by the master inorder to write the rotate event in the binary log.

  @internal
  The inheritance structure in the current design for the classes is
  as follows:

        Binary_log_event
               ^
               |
               |
           Rotate_event  Log_event
                \       /
                 \     /
                  \   /
             Rotate_log_event
  @endinternal
*/
class Rotate_log_event : public binary_log::Rotate_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  Rotate_log_event(const char *new_log_ident_arg, size_t ident_len_arg,
                   ulonglong pos_arg, uint flags);
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  Rotate_log_event(const char *buf,
                   const Format_description_event *description_event);
  ~Rotate_log_event() {}
  size_t get_data_size() override {
    return ident_len + Binary_log_event::ROTATE_HEADER_LEN;
  }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
#endif

 private:
#if defined(MYSQL_SERVER)
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif
};

/**
  @class Append_block_log_event

  This event is created to contain the file data. One LOAD_DATA_INFILE
  can have 0 or more instances of this event written to the binary log
  depending on the size of the file.

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
           B_l:A_B_E  Log_event
                \         /
                 \       /
           <<vir>>\     /
                   \   /
           Append_block_log_event
  B_l: Namespace Binary_log
  A_B_E: class Append_block_event
  @endinternal

*/
class Append_block_log_event : public virtual binary_log::Append_block_event,
                               public Log_event {
 public:
#ifdef MYSQL_SERVER
  Append_block_log_event(THD *thd, const char *db_arg, uchar *block_arg,
                         uint block_len_arg, bool using_trans);
  int pack_info(Protocol *protocol) override;
  virtual int get_create_or_append() const;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

  Append_block_log_event(const char *buf, uint event_len,
                         const Format_description_event *description_event);
  ~Append_block_log_event() {}
  size_t get_data_size() override {
    return block_len + Binary_log_event::APPEND_BLOCK_HEADER_LEN;
  }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
  const char *get_db() override { return db; }
#endif

  bool is_sbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
#endif
};

/**
  @class Delete_file_log_event

  Delete_file_log_event is created when the LOAD_DATA query fails on the
  master for some reason, and the slave should be notified to abort the
  load. The event is required since the master starts writing the loaded
  block into the binary log before the statement ends. In case of error,
  the slave should abort, and delete any temporary file created while
  applying the (NEW_)LOAD_EVENT.

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
          B_l:D_F_E  Log_event
                \         /
                 \       /
                  \     /
                   \   /
           Delete_file_log_event

  B_l: Namespace Binary_log
  D_F_E: class Delete_file_event
  @endinternal

*/
class Delete_file_log_event : public binary_log::Delete_file_event,
                              public Log_event {
 public:
#ifdef MYSQL_SERVER
  Delete_file_log_event(THD *thd, const char *db_arg, bool using_trans);
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info, bool enable_local);
#endif

  Delete_file_log_event(const char *buf, uint event_len,
                        const Format_description_event *description_event);
  ~Delete_file_log_event() {}
  size_t get_data_size() override {
    return Binary_log_event::DELETE_FILE_HEADER_LEN;
  }
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override;
  const char *get_db() override { return db; }
#endif

  bool is_sbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
#endif
};

/**
  @class Begin_load_query_log_event

  Event for the first block of file to be loaded, its only difference from
  Append_block event is that this event creates or truncates existing file
  before writing data.

  @internal
  The inheritance structure is as follows

                          Binary_log_event
                                  ^
                                  |
                                  |
                                  |
                Log_event   B_l:A_B_E
                     ^            /\
                     |           /  \
                     |   <<vir>>/    \ <<vir>>
                     |         /      \
                     |        /        \
                     |       /          \
             Append_block_log_event  B_l:B_L_Q_E
                             \          /
                              \        /
                               \      /
                                \    /
                                 \  /
                       Begin_load_query_log_event

  B_l: Namespace Binary_log
  A_B_E: class Append_block_event
  B_L_Q_E: Begin_load_query_event
  @endinternal

  @section Begin_load_query_log_event_binary_format Binary Format
*/
class Begin_load_query_log_event : public Append_block_log_event,
                                   public binary_log::Begin_load_query_event {
 public:
#ifdef MYSQL_SERVER
  Begin_load_query_log_event(THD *thd_arg, const char *db_arg, uchar *block_arg,
                             uint block_len_arg, bool using_trans);
  Begin_load_query_log_event(THD *thd);
  int get_create_or_append() const override;
#endif
  Begin_load_query_log_event(const char *buf, uint event_len,
                             const Format_description_event *description_event);
  ~Begin_load_query_log_event() {}

 private:
#if defined(MYSQL_SERVER)
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif
};

/**
  @class Execute_load_query_log_event

  Event responsible for LOAD DATA execution, it similar to Query_log_event
  but before executing the query it substitutes original filename in LOAD DATA
  query with name of temporary file.

  @internal
  The inheritance structure is as follows:

                          Binary_log_event
                                  ^
                                  |
                                  |
                                  |
                Log_event   B_l:Query_event
                     ^            /\
                     |           /  \
                     |   <<vir>>/    \ <<vir>>
                     |         /      \
                     |        /        \
                     |       /          \
                  Query_log_event  B_l:E_L_Q_E
                             \          /
                              \        /
                               \      /
                                \    /
                                 \  /
                    Execute_load_query_log_event

  B_l: Namespace Binary_log
  E_L_Q_E: class Execute_load_query
  @endinternal

  @section Execute_load_query_log_event_binary_format Binary Format
*/
class Execute_load_query_log_event
    : public Query_log_event,
      public binary_log::Execute_load_query_event {
 public:
#ifdef MYSQL_SERVER
  Execute_load_query_log_event(
      THD *thd, const char *query_arg, ulong query_length,
      uint fn_pos_start_arg, uint fn_pos_end_arg,
      binary_log::enum_load_dup_handling dup_handling_arg, bool using_trans,
      bool immediate, bool suppress_use, int errcode);
  int pack_info(Protocol *protocol) override;
#else
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
  /* Prints the query as LOAD DATA LOCAL and with rewritten filename */
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info,
             const char *local_fname) const;
#endif
  Execute_load_query_log_event(
      const char *buf, uint event_len,
      const Format_description_event *description_event);
  ~Execute_load_query_log_event() {}

  ulong get_post_header_size_for_derived() override;
#ifdef MYSQL_SERVER
  bool write_post_header_for_derived(Basic_ostream *ostream) override;
#endif

  bool is_sbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
#endif
};

#if defined MYSQL_SERVER
class Load_query_generator {
 public:
  Load_query_generator(THD *thd_arg, const sql_exchange *ex, const char *db_arg,
                       const char *table_name_arg, bool is_concurrent_arg,
                       bool replace, bool ignore);

  const String *generate(size_t *fn_start, size_t *fn_end);

 private:
  const size_t BUF_SIZE = 2048;
  String str;
  char *buf[2048];

  THD *thd;
  const sql_exchange *sql_ex;
  const char *db;
  const char *table_name;
  const char *fname;

  bool is_concurrent;
  bool has_replace;
  bool has_ignore;
};
#endif
#ifndef MYSQL_SERVER
/**
  @class Unknown_log_event

*/
class Unknown_log_event : public binary_log::Unknown_event, public Log_event {
 public:
  /**
    Even if this is an unknown event, we still pass description_event to
    Log_event's ctor, this way we can extract maximum information from the
    event's header (the unique ID for example).
  */
  Unknown_log_event(const char *buf,
                    const Format_description_event *description_event)
      : binary_log::Unknown_event(buf, description_event),
        Log_event(header(), footer()) {
    DBUG_ENTER(
        "Unknown_log_event::Unknown_log_event(const char *, const "
        "Format_description_log_event *)");
    if (!is_valid()) DBUG_VOID_RETURN;
    common_header->set_is_valid(true);
    DBUG_VOID_RETURN;
  }

  ~Unknown_log_event() {}
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
  Log_event_type get_type_code() { return binary_log::UNKNOWN_EVENT; }
};
#endif
char *str_to_hex(char *to, const char *from, size_t len);

/**
  @class Table_map_log_event

  Table_map_log_event which maps a table definition to a number.

  @internal
  The inheritance structure in the current design for the classes is
  as follows:

        Binary_log_event
               ^
               |
               |
    Table_map_event  Log_event
                \       /
                 \     /
                  \   /
           Table_map_log_event
  @endinternal
*/
class Table_map_log_event : public binary_log::Table_map_event,
                            public Log_event {
 public:
  /** Constants */
  enum { TYPE_CODE = binary_log::TABLE_MAP_EVENT };

  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error {
    ERR_OPEN_FAILURE = -1,        /**< Failure to open table */
    ERR_OK = 0,                   /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED = 1, /**< No more room for tables */
    ERR_OUT_OF_MEM = 2,           /**< Out of memory */
    ERR_BAD_TABLE_DEF = 3,        /**< Table definition does not match */
    ERR_RBR_TO_SBR = 4            /**< daisy-chanining RBR to SBR not allowed */
  };

  enum enum_flag {
    /**
       Nothing here right now, but the flags support is there in
       preparation for changes that are coming.  Need to add a
       constant to make it compile under HP-UX: aCC does not like
       empty enumerations.
    */
    ENUM_FLAG_COUNT
  };

  /** Special constants representing sets of flags */
  enum {
    TM_NO_FLAGS = 0U,
    TM_BIT_LEN_EXACT_F = (1U << 0),
    TM_REFERRED_FK_DB_F = (1U << 1)
  };

  flag_set get_flags(flag_set flag) const { return m_flags & flag; }

#ifdef MYSQL_SERVER
  Table_map_log_event(THD *thd_arg, TABLE *tbl, const Table_id &tid,
                      bool is_transactional);
#endif
  Table_map_log_event(const char *buf, uint event_len,
                      const Format_description_event *description_event);

  virtual ~Table_map_log_event();

#ifndef MYSQL_SERVER
  table_def *create_table_def() {
    DBUG_ASSERT(m_colcnt > 0);
    return new table_def(m_coltype, m_colcnt, m_field_metadata,
                         m_field_metadata_size, m_null_bits, m_flags);
  }
  static bool rewrite_db_in_buffer(char **buf, ulong *event_len,
                                   const Format_description_event &fde);
#endif
  const Table_id &get_table_id() const { return m_table_id; }
  const char *get_table_name() const { return m_tblnam.c_str(); }
  const char *get_db_name() const { return m_dbnam.c_str(); }

  virtual size_t get_data_size() override { return m_data_size; }
#ifdef MYSQL_SERVER
  virtual int save_field_metadata();
  virtual bool write_data_header(Basic_ostream *ostream) override;
  virtual bool write_data_body(Basic_ostream *ostream) override;
  virtual const char *get_db() override { return m_dbnam.c_str(); }
  virtual uint8 mts_number_dbs() override {
    return get_flags(TM_REFERRED_FK_DB_F) ? OVER_MAX_DBS_IN_EVENT_MTS : 1;
  }
  /**
     @param[out] arg pointer to a struct containing char* array
                     pointers be filled in and the number of filled instances.
     @param rpl_filter pointer to a replication filter.

     @return    number of databases in the array: either one or
                OVER_MAX_DBS_IN_EVENT_MTS, when the Table map event reports
                foreign keys constraint.
  */
  virtual uint8 get_mts_dbs(Mts_db_names *arg,
                            Rpl_filter *rpl_filter) override {
    const char *db_name = get_db();

    if (!rpl_filter->is_rewrite_empty() && !get_flags(TM_REFERRED_FK_DB_F)) {
      size_t dummy_len;
      const char *db_filtered = rpl_filter->get_rewrite_db(db_name, &dummy_len);
      // db_name != db_filtered means that db_name is rewritten.
      if (strcmp(db_name, db_filtered)) db_name = db_filtered;
    }

    if (!get_flags(TM_REFERRED_FK_DB_F)) arg->name[0] = db_name;

    return arg->num = mts_number_dbs();
  }

#endif

#if defined(MYSQL_SERVER)
  virtual int pack_info(Protocol *protocol) override;
#endif

#ifndef MYSQL_SERVER
  virtual void print(FILE *file,
                     PRINT_EVENT_INFO *print_event_info) const override;

  /**
    Print column metadata. Its format looks like:
    # Columns(colume_name type, colume_name type, ...)
    if colume_name field is not logged into table_map_log_event, then
    only type is printed.

    @@param[out] file the place where colume metadata is printed
    @@param[in]  The metadata extracted from optional metadata fields
 */
  void print_columns(IO_CACHE *file,
                     const Optional_metadata_fields &fields) const;
  /**
    Print primary information. Its format looks like:
    # Primary Key(colume_name, column_name(prifix), ...)
    if colume_name field is not logged into table_map_log_event, then
    colume index is printed.

    @@param[out] file the place where primary key is printed
    @@param[in]  The metadata extracted from optional metadata fields
 */
  void print_primary_key(IO_CACHE *file,
                         const Optional_metadata_fields &fields) const;
#endif

  bool is_rbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif

#ifdef MYSQL_SERVER
  TABLE *m_table;

  // Metadata fields buffer
  StringBuffer<1024> m_metadata_buf;

  /**
    Initialize the optional metadata fields should be logged into
    table_map_log_event and write them into m_metadata_buf.
  */
  void init_metadata_fields();
  bool init_signedness_field();
  bool init_charset_field();
  bool init_column_name_field();
  bool init_set_str_value_field();
  bool init_enum_str_value_field();
  bool init_geometry_type_field();
  bool init_primary_key_field();
#endif
};

#ifdef HAVE_PSI_STAGE_INTERFACE
/*
 Helper class for PSI context while applying a Rows_log_event.
 */
class Rows_applier_psi_stage {
 private:
  Rows_applier_psi_stage(const Rows_applier_psi_stage &rhs);
  Rows_applier_psi_stage &operator=(const Rows_applier_psi_stage &rhs);

  /**
   A cached pointer to this stage PSI_stage_progress.
   */
  PSI_stage_progress *m_progress;

  /**
   Counter that is unconditionally incremented on each row that is processed.
   This is helpful in case estimation is needed after started processing
   a Rows_log_event.
   */
  ulonglong m_n_rows_applied;

 public:
  Rows_applier_psi_stage() : m_progress(NULL), m_n_rows_applied(0) {}

  void set_progress(PSI_stage_progress *progress) { m_progress = progress; }

  /**
   If instrumentation is enabled this member function SHALL return true.
   @return true if instrumentation is enabled for the given stage, false
   otherwise.
   */
  bool is_enabled() { return m_progress != NULL; }

  /**
   This member function shall update the progress and reestimate the remaining
   work needed. This MUST be called after setting n_rows_applied correctly
   by calling inc_n_rows_applied beforehand.

   Cursor, begin and end are used in case estimation is needed.

   @param cursor Pointer to where we are in the buffer of rows to be processed.
   @param begin Pointer to the beginning of the rows buffer.
   @param end Pointer to the end of the rows buffer.
   */
  void update_work_estimated_and_completed(const uchar *cursor,
                                           const uchar *begin,
                                           const uchar *end) {
    if (!is_enabled()) return;

    ulonglong estimated = mysql_stage_get_work_estimated(m_progress);

    /* Estimate if need be. */
    if (estimated == 0) {
      DBUG_ASSERT(cursor > begin);
      ulonglong avg_row_change_size = (cursor - begin) / m_n_rows_applied;
      estimated = (end - begin) / avg_row_change_size;
      mysql_stage_set_work_estimated(m_progress, estimated);
    }

    /* reset estimated if done more work than estimated */
    if (m_n_rows_applied > estimated)
      mysql_stage_set_work_estimated(m_progress, m_n_rows_applied);
    mysql_stage_set_work_completed(m_progress, m_n_rows_applied);
  }

  /**
   Resets this object.
   */
  void end_work() {
    m_progress = NULL;
    m_n_rows_applied = 0;
  }

  /**
   Updates the counter of processed rows.
   @param delta the amount of increment to be done.
   */
  void inc_n_rows_applied(ulonglong delta) { m_n_rows_applied += delta; }

  /**
   Gets the value of the counter of rows that have been processed.
   @return the value of the counter of rows processed so far.
   */
  ulonglong get_n_rows_applied() { return m_n_rows_applied; }
};
#endif

/**
  @class Rows_log_event

 Common base class for all row-containing log events.

 RESPONSIBILITIES

   Encode the common parts of all events containing rows, which are:
   - Write data header and data body to an IO_CACHE.

  Virtual inheritance is required here to handle the diamond problem in
  the class Write_rows_log_event, Update_rows_log_event and
  Delete_rows_log_event.
  The diamond structure is explained in @c Write_rows_log_event,
                                        @c Update_rows_log_event,
                                        @c Delete_rows_log_event

  @internal
  The inheritance structure in the current design for the classes is
  as follows:

        Binary_log_event
               ^
               |
               |
         Rows_event  Log_event
                \       /
          <<vir>>\     /
                  \   /
              Rows_log_event
  @endinternal

*/
class Rows_log_event : public virtual binary_log::Rows_event, public Log_event {
#ifdef HAVE_PSI_STAGE_INTERFACE
 protected:
  Rows_applier_psi_stage m_psi_progress;
#endif

 public:
  typedef uint16 flag_set;

  enum row_lookup_mode {
    ROW_LOOKUP_UNDEFINED = 0,
    ROW_LOOKUP_NOT_NEEDED = 1,
    ROW_LOOKUP_INDEX_SCAN = 2,
    ROW_LOOKUP_TABLE_SCAN = 3,
    ROW_LOOKUP_HASH_SCAN = 4
  };

  /**
     Enumeration of the errors that can be returned.
   */
  enum enum_error {
    ERR_OPEN_FAILURE = -1,        /**< Failure to open table */
    ERR_OK = 0,                   /**< No error */
    ERR_TABLE_LIMIT_EXCEEDED = 1, /**< No more room for tables */
    ERR_OUT_OF_MEM = 2,           /**< Out of memory */
    ERR_BAD_TABLE_DEF = 3,        /**< Table definition does not match */
    ERR_RBR_TO_SBR = 4            /**< daisy-chanining RBR to SBR not allowed */
  };

  /* Special constants representing sets of flags */
  enum { RLE_NO_FLAGS = 0U };

  virtual ~Rows_log_event();

  void set_flags(flag_set flags_arg) { m_flags |= flags_arg; }
  void clear_flags(flag_set flags_arg) { m_flags &= ~flags_arg; }
  flag_set get_flags(flag_set flags_arg) const { return m_flags & flags_arg; }

  virtual Log_event_type
  get_general_type_code() = 0; /* General rows op type, no version */

#if defined(MYSQL_SERVER)
  virtual int pack_info(Protocol *protocol) override;
#endif

#ifndef MYSQL_SERVER
  void print_verbose(IO_CACHE *file, PRINT_EVENT_INFO *print_event_info);
  size_t print_verbose_one_row(IO_CACHE *file, table_def *td,
                               PRINT_EVENT_INFO *print_event_info,
                               MY_BITMAP *cols_bitmap, const uchar *ptr,
                               const uchar *prefix,
                               enum_row_image_type row_image_type);
#endif

#ifdef MYSQL_SERVER
  int add_row_data(uchar *data, size_t length) {
    return do_add_row_data(data, length);
  }
#endif

  /* Member functions to implement superclass interface */
  virtual size_t get_data_size() override;

  MY_BITMAP const *get_cols() const { return &m_cols; }
  MY_BITMAP const *get_cols_ai() const { return &m_cols_ai; }
  size_t get_width() const { return m_width; }
  const Table_id &get_table_id() const { return m_table_id; }

#if defined(MYSQL_SERVER)
  /**
    Compares the table's read/write_set with the columns included in
    this event's before-image and/or after-image. Each subclass
    (Write/Update/Delete) implements this function by comparing on the
    image(s) pertinent to the subclass.

    @param[in] table The table to compare this events bitmaps
                     against.

    @retval true if sets match
    @retval false otherwise (following bitmap_cmp return logic).
  */
  virtual bool read_write_bitmaps_cmp(const TABLE *table) const = 0;
#endif

#ifdef MYSQL_SERVER
  virtual bool write_data_header(Basic_ostream *ostream) override;
  virtual bool write_data_body(Basic_ostream *ostream) override;
  virtual const char *get_db() override { return m_table->s->db.str; }
#endif

  uint m_row_count; /* The number of rows added to the event */

  const uchar *get_extra_row_data() const { return m_extra_row_data; }

 protected:
  /*
     The constructors are protected since you're supposed to inherit
     this class, not create instances of this class.
  */
#ifdef MYSQL_SERVER
  Rows_log_event(THD *, TABLE *, const Table_id &table_id,
                 MY_BITMAP const *cols, bool is_transactional,
                 Log_event_type event_type, const uchar *extra_row_info);
#endif
  Rows_log_event(const char *row_data, uint event_len,
                 const Format_description_event *description_event);

#ifndef MYSQL_SERVER
  void print_helper(FILE *, PRINT_EVENT_INFO *) const;
#endif

#ifdef MYSQL_SERVER
  virtual int do_add_row_data(uchar *data, size_t length);
#endif

#ifdef MYSQL_SERVER
  TABLE *m_table; /* The table the rows belong to */
#endif
  MY_BITMAP m_cols; /* Bitmap denoting columns available */
#ifdef MYSQL_SERVER
  /**
     Hash table that will hold the entries for while using HASH_SCAN
     algorithm to search and update/delete rows.
   */
  Hash_slave_rows m_hash;

  /**
     The algorithm to use while searching for rows using the before
     image.
  */
  uint m_rows_lookup_algorithm;
#endif
  /*
    Bitmap for columns available in the after image, if present. These
    fields are only available for Update_rows events. Observe that the
    width of both the before image COLS vector and the after image
    COLS vector is the same: the number of columns of the table on the
    master.
  */
  MY_BITMAP m_cols_ai;

  /* Bit buffers in the same memory as the class */
  uint32 m_bitbuf[128 / (sizeof(uint32) * 8)];
  uint32 m_bitbuf_ai[128 / (sizeof(uint32) * 8)];

  /*
   is_valid depends on the value of m_rows_buf, so while changing the value
   of m_rows_buf check if is_valid also needs to be modified
  */
  uchar *m_rows_buf; /* The rows in packed format */
  uchar *m_rows_cur; /* One-after the end of the data */
  uchar *m_rows_end; /* One-after the end of the allocated space */

  /* helper functions */

#if defined(MYSQL_SERVER)
  const uchar *m_curr_row;     /* Start of the row being processed */
  const uchar *m_curr_row_end; /* One-after the end of the current row */
  uchar *m_key;                /* Buffer to keep key value during searches */
  uint m_key_index;
  KEY *m_key_info; /* Points to description of index #m_key_index */
  class Key_compare {
   public:
    /**
       @param  ki  Where to find KEY description
       @note m_distinct_keys is instantiated when Rows_log_event is constructed;
       it stores a Key_compare object internally. However at that moment, the
       index (KEY*) to use for comparisons, is not yet known. So, at
       instantiation, we indicate the Key_compare the place where it can
       find the KEY* when needed (this place is Rows_log_event::m_key_info),
       Key_compare remembers the place in member m_key_info.
       Before we need to do comparisons - i.e. before we need to insert
       elements, we update Rows_log_event::m_key_info once for all.
    */
    Key_compare(KEY **ki = NULL) : m_key_info(ki) {}
    bool operator()(uchar *k1, uchar *k2) const {
      return key_cmp2((*m_key_info)->key_part, k1, (*m_key_info)->key_length,
                      k2, (*m_key_info)->key_length) < 0;
    }

   private:
    KEY **m_key_info;
  };
  std::set<uchar *, Key_compare> m_distinct_keys;
  std::set<uchar *, Key_compare>::iterator m_itr;
  /**
    A spare buffer which will be used when saving the distinct keys
    for doing an index scan with HASH_SCAN search algorithm.
  */
  uchar *m_distinct_key_spare_buf;

  /**
    Unpack the current row image from the event into m_table->record[0].

    @param rli The applier context.

    @param cols The bitmap of columns included in the update.

    @param is_after_image Should be true if this is an after-image,
    false if it is a before-image.

    @param only_seek @see unpack_row()

    @retval 0 Success

    @retval ER_* On error, it is guaranteed that the error has been
    reported through my_error, and the corresponding ER_* code is
    returned.  Currently the error codes are: EE_OUTOFMEMORY,
    ER_SLAVE_CORRUPT_EVENT, or various JSON errors when applying JSON
    diffs (ER_COULD_NOT_APPLY_JSON_DIFF, ER_INVALID_JSON_BINARY_DATA,
    and maybe others).
  */
  int unpack_current_row(const Relay_log_info *const rli, MY_BITMAP const *cols,
                         bool is_after_image, bool only_seek = false);

  /*
    This member function is called when deciding the algorithm to be used to
    find the rows to be updated on the slave during row based replication.
    This this functions sets the m_rows_lookup_algorithm and also the
    m_key_index with the key index to be used if the algorithm is dependent on
    an index.
   */
  void decide_row_lookup_algorithm_and_key();

  /*
    Encapsulates the  operations to be done before applying
    row event for update and delete.
   */
  int row_operations_scan_and_key_setup();

  /*
   Encapsulates the  operations to be done after applying
   row event for update and delete.
  */
  int row_operations_scan_and_key_teardown(int error);

  /**
    Helper function to check whether there is an auto increment
    column on the table where the event is to be applied.

    @return true if there is an autoincrement field on the extra
            columns, false otherwise.
   */
  inline bool is_auto_inc_in_extra_columns() {
    DBUG_ASSERT(m_table);
    return (m_table->next_number_field &&
            m_table->next_number_field->field_index >= m_width);
  }
#endif

  bool is_rbr_logging_format() const override { return true; }

 private:
#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual int do_update_pos(Relay_log_info *rli) override;
  virtual enum_skip_reason do_shall_skip(Relay_log_info *rli) override;

  /*
    Primitive to prepare for a sequence of row executions.

    DESCRIPTION

      Before doing a sequence of do_prepare_row() and do_exec_row()
      calls, this member function should be called to prepare for the
      entire sequence. Typically, this member function will allocate
      space for any buffers that are needed for the two member
      functions mentioned above.

    RETURN VALUE

      The member function will return 0 if all went OK, or a non-zero
      error code otherwise.
  */
  virtual int do_before_row_operations(
      const Slave_reporting_capability *const log) = 0;

  /*
    Primitive to clean up after a sequence of row executions.

    DESCRIPTION

      After doing a sequence of do_prepare_row() and do_exec_row(),
      this member function should be called to clean up and release
      any allocated buffers.

      The error argument, if non-zero, indicates an error which happened during
      row processing before this function was called. In this case, even if
      function is successful, it should return the error code given in the
    argument.
  */
  virtual int do_after_row_operations(
      const Slave_reporting_capability *const log, int error) = 0;

  /*
    Primitive to do the actual execution necessary for a row.

    DESCRIPTION
      The member function will do the actual execution needed to handle a row.
      The row is located at m_curr_row. When the function returns,
      m_curr_row_end should point at the next row (one byte after the end
      of the current row).

    RETURN VALUE
      0 if execution succeeded, 1 if execution failed.

  */
  virtual int do_exec_row(const Relay_log_info *const rli) = 0;

  /**
    Private member function called while handling idempotent errors.

    @param rli Pointer to relay log info structure.
    @param [in,out] err the error to handle. If it is listed as
                       idempotent/ignored related error, then it is cleared.
    @returns true if the slave should stop executing rows.
   */
  int handle_idempotent_and_ignored_errors(Relay_log_info const *rli, int *err);

  /**
     Private member function called after updating/deleting a row. It
     performs some assertions and more importantly, it updates
     m_curr_row so that the next row is processed during the row
     execution main loop (@c Rows_log_event::do_apply_event()).

     @param rli Pointer to relay log info structure.
     @param err the current error code.
   */
  void do_post_row_operations(Relay_log_info const *rli, int err);

  /**
     Commodity wrapper around do_exec_row(), that deals with resetting
     the thd reference in the table.
   */
  int do_apply_row(Relay_log_info const *rli);

  /**
     Implementation of the index scan and update algorithm. It uses
     PK, UK or regular Key to search for the record to update. When
     found it updates it.
   */
  int do_index_scan_and_update(Relay_log_info const *rli);

  /**
     Implementation of the hash_scan and update algorithm. It collects
     rows positions in a hashtable until the last row is
     unpacked. Then it scans the table to update and when a record in
     the table matches the one in the hashtable, the update/delete is
     performed.
   */
  int do_hash_scan_and_update(Relay_log_info const *rli);

  /**
     Implementation of the legacy table_scan and update algorithm. For
     each unpacked row it scans the storage engine table for a
     match. When a match is found, the update/delete operations are
     performed.
   */
  int do_table_scan_and_update(Relay_log_info const *rli);

  /**
    Seek past the after-image of an update event, in case a row was processed
    without reading the after-image.

    An update event may process a row without reading the after-image,
    e.g. in case of ignored or idempotent errors.  To ensure that the
    read position for the next row is correct, we need to seek past
    the after-image.

    @param rli The applier context

    @param curr_bi_start The read position of the beginning of the
    before-image. (The function compares this with m_curr_row to know
    if the after-image has been read or not.)

    @retval 0 Success
    @retval ER_* Error code returned by unpack_current_row
  */
  virtual int skip_after_image_for_update_event(
      const Relay_log_info *rli MY_ATTRIBUTE((unused)),
      const uchar *curr_bi_start MY_ATTRIBUTE((unused))) {
    return 0;
  }

  /**
    Initializes scanning of rows. Opens an index and initailizes an iterator
    over a list of distinct keys (m_distinct_keys) if it is a HASH_SCAN
    over an index or the table if its a HASH_SCAN over the table.
  */
  int open_record_scan();

  /**
    Does the cleanup
    - closes the index if opened by open_record_scan
    - closes the table if opened for scanning.
  */
  int close_record_scan();

  /**
    Fetches next row. If it is a HASH_SCAN over an index, it populates
    table->record[0] with the next row corresponding to the index. If
    the indexes are in non-contigous ranges it fetches record corresponding
    to the key value in the next range.

    @param first_read  signifying if this is the first time we are reading a row
            over an index.
    @return  error code when there are no more records to be fetched or some
    other error occurred,
                   -  0 otherwise.
  */
  int next_record_scan(bool first_read);

  /**
    Populates the m_distinct_keys with unique keys to be modified
    during HASH_SCAN over keys.
    @returns 0 success, or the error code.
  */
  int add_key_to_distinct_keyset();

  /**
    Populates the m_hash when using HASH_SCAN. Thence, it:
    - unpacks the before image (BI)
    - saves the positions
    - saves the positions into the hash map, using the
      BI checksum as key
    - unpacks the after image (AI) if needed, so that
      m_curr_row_end gets updated correctly.

    @param rli The reference to the relay log info object.
    @returns 0 on success. Otherwise, the error code.
  */
  int do_hash_row(Relay_log_info const *rli);

  /**
    This member function scans the table and applies the changes
    that had been previously hashed. As such, m_hash MUST be filled
    by do_hash_row before calling this member function.

    @param rli The reference to the relay log info object.
    @returns 0 on success. Otherwise, the error code.
  */
  int do_scan_and_update(Relay_log_info const *rli);
#endif /* defined(MYSQL_SERVER) */

  friend class Old_rows_log_event;
};

/**
  @class Write_rows_log_event

  Log row insertions and updates. The event contain several
  insert/update rows for a table. Note that each event contains only
  rows for one table.

  @internal
  The inheritance structure is as follows

                         Binary_log_event
                                  ^
                                  |
                                  |
                                  |
                Log_event   B_l:Rows_event
                     ^            /\
                     |           /  \
                     |   <<vir>>/    \ <<vir>>
                     |         /      \
                     |        /        \
                     |       /          \
                  Rows_log_event    B_l:W_R_E
                             \          /
                              \        /
                               \      /
                                \    /
                                 \  /
                                  \/
                        Write_rows_log_event

  B_l: Namespace Binary_log
  W_R_E: class Write_rows_event
  @endinternal

*/
class Write_rows_log_event : public Rows_log_event,
                             public binary_log::Write_rows_event {
 public:
  enum {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = binary_log::WRITE_ROWS_EVENT
  };

#if defined(MYSQL_SERVER)
  Write_rows_log_event(THD *, TABLE *, const Table_id &table_id,
                       bool is_transactional, const uchar *extra_row_info);
#endif
  Write_rows_log_event(const char *buf, uint event_len,
                       const Format_description_event *description_event);
#if defined(MYSQL_SERVER)
  static bool binlog_row_logging_function(
      THD *thd, TABLE *table, bool is_transactional,
      const uchar *before_record MY_ATTRIBUTE((unused)),
      const uchar *after_record) {
    return thd->binlog_write_row(table, is_transactional, after_record, NULL);
  }
  bool read_write_bitmaps_cmp(const TABLE *table) const override {
    return bitmap_cmp(get_cols(), table->write_set);
  }
#endif

 protected:
  int write_row(const Relay_log_info *const, const bool);

 private:
  virtual Log_event_type get_general_type_code() override {
    return (Log_event_type)TYPE_CODE;
  }

#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

#if defined(MYSQL_SERVER)
  virtual int do_before_row_operations(
      const Slave_reporting_capability *const) override;
  virtual int do_after_row_operations(const Slave_reporting_capability *const,
                                      int) override;
  virtual int do_exec_row(const Relay_log_info *const) override;
#endif
};

/**
  @class Update_rows_log_event

  Log row updates with a before image. The event contain several
  update rows for a table. Note that each event contains only rows for
  one table.

  Also note that the row data consists of pairs of row data: one row
  for the old data and one row for the new data.

  @internal
  The inheritance structure is as follows

                         Binary_log_event
                                  ^
                                  |
                                  |
                                  |
                Log_event   B_l:Rows_event
                     ^            /\
                     |           /  \
                     |   <<vir>>/    \ <<vir>>
                     |         /      \
                     |        /        \
                     |       /          \
                  Rows_log_event    B_l:U_R_E
                             \          /
                              \        /
                               \      /
                                \    /
                                 \  /
                                  \/
                        Update_rows_log_event


  B_l: Namespace Binary_log
  U_R_E: class Update_rows_event
  @eninternal

*/
class Update_rows_log_event : public Rows_log_event,
                              public binary_log::Update_rows_event {
 public:
  enum {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = binary_log::UPDATE_ROWS_EVENT
  };

#ifdef MYSQL_SERVER
  Update_rows_log_event(THD *, TABLE *, const Table_id &table_id,
                        MY_BITMAP const *cols_bi, MY_BITMAP const *cols_ai,
                        bool is_transactional, const uchar *extra_row_info);

  Update_rows_log_event(THD *, TABLE *, const Table_id &table_id,
                        bool is_transactional, const uchar *extra_row_info);

  void init(MY_BITMAP const *cols);
#endif

  virtual ~Update_rows_log_event();

  Update_rows_log_event(const char *buf, uint event_len,
                        const Format_description_event *description_event);

#ifdef MYSQL_SERVER
  static bool binlog_row_logging_function(THD *thd, TABLE *table,
                                          bool is_transactional,
                                          const uchar *before_record,
                                          const uchar *after_record) {
    return thd->binlog_update_row(table, is_transactional, before_record,
                                  after_record, NULL);
  }
  bool read_write_bitmaps_cmp(const TABLE *table) const override {
    return (bitmap_cmp(get_cols(), table->read_set) &&
            bitmap_cmp(get_cols_ai(), table->write_set));
  }
#endif

 protected:
  virtual Log_event_type get_general_type_code() override {
    return (Log_event_type)TYPE_CODE;
  }

#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

#if defined(MYSQL_SERVER)
  virtual int do_before_row_operations(
      const Slave_reporting_capability *const) override;
  virtual int do_after_row_operations(const Slave_reporting_capability *const,
                                      int) override;
  virtual int do_exec_row(const Relay_log_info *const) override;

  virtual int skip_after_image_for_update_event(
      const Relay_log_info *rli, const uchar *curr_bi_start) override;

 private:
  /**
    Auxiliary function used in the (THD*, ...) constructor to
    determine the type code based on configuration options.

    @param thd_arg The THD object for the session.

    @return One of UPDATE_ROWS_EVENT_V1, PARTIAL_UPDATE_ROWS_EVENT, or
    UPDATE_ROWS_EVENT.
  */
  static binary_log::Log_event_type get_update_rows_event_type(
      const THD *thd_arg);
#endif /* defined(MYSQL_SERVER) */
};

/**
  @class Delete_rows_log_event

  Log row deletions. The event contain several delete rows for a
  table. Note that each event contains only rows for one table.

  RESPONSIBILITIES

    - Act as a container for rows that has been deleted on the master
      and should be deleted on the slave.

  COLLABORATION

    Row_writer
      Create the event and add rows to the event.
    Row_reader
      Extract the rows from the event.

  @internal
  The inheritance structure is as follows

                         Binary_log_event
                                  ^
                                  |
                                  |
                                  |
                Log_event   B_l:Rows_event
                     ^            /\
                     |           /  \
                     |   <<vir>>/    \ <<vir>>
                     |         /      \
                     |        /        \
                     |       /          \
                  Rows_log_event    B_l:D_R_E
                             \          /
                              \        /
                               \      /
                                \    /
                                 \  /
                                  \/
                        Delete_rows_log_event

  B_l: Namespace Binary_log
  D_R_E: class Delete_rows_event
  @endinternal

*/
class Delete_rows_log_event : public Rows_log_event,
                              public binary_log::Delete_rows_event {
 public:
  enum {
    /* Support interface to THD::binlog_prepare_pending_rows_event */
    TYPE_CODE = binary_log::DELETE_ROWS_EVENT
  };

#ifdef MYSQL_SERVER
  Delete_rows_log_event(THD *, TABLE *, const Table_id &, bool is_transactional,
                        const uchar *extra_row_info);
#endif
  Delete_rows_log_event(const char *buf, uint event_len,
                        const Format_description_event *description_event);
#ifdef MYSQL_SERVER
  static bool binlog_row_logging_function(
      THD *thd, TABLE *table, bool is_transactional, const uchar *before_record,
      const uchar *after_record MY_ATTRIBUTE((unused))) {
    return thd->binlog_delete_row(table, is_transactional, before_record, NULL);
  }
  bool read_write_bitmaps_cmp(const TABLE *table) const override {
    return bitmap_cmp(get_cols(), table->read_set);
  }
#endif

 protected:
  virtual Log_event_type get_general_type_code() override {
    return (Log_event_type)TYPE_CODE;
  }

#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

#if defined(MYSQL_SERVER)
  virtual int do_before_row_operations(
      const Slave_reporting_capability *const) override;
  virtual int do_after_row_operations(const Slave_reporting_capability *const,
                                      int) override;
  virtual int do_exec_row(const Relay_log_info *const) override;
#endif
};

/**
  @class Incident_log_event

   Class representing an incident, an occurance out of the ordinary,
   that happened on the master.

   The event is used to inform the slave that something out of the
   ordinary happened on the master that might cause the database to be
   in an inconsistent state.
   Its the derived class of Incident_event

   @internal
   The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
     B_l:Incident_event     Log_event
                \         /
                 \       /
                  \     /
                   \   /
             Incident_log_event

  B_l: Namespace Binary_log
  @endinternal

*/
class Incident_log_event : public binary_log::Incident_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  Incident_log_event(THD *thd_arg, enum_incident incident)
      : binary_log::Incident_event(incident),
        Log_event(thd_arg, LOG_EVENT_NO_FILTER_F, Log_event::EVENT_NO_CACHE,
                  Log_event::EVENT_IMMEDIATE_LOGGING, header(), footer()) {
    DBUG_ENTER("Incident_log_event::Incident_log_event");
    DBUG_PRINT("enter", ("incident: %d", incident));
    common_header->set_is_valid(incident > INCIDENT_NONE &&
                                incident < INCIDENT_COUNT);
    DBUG_ASSERT(message == NULL && message_length == 0);
    DBUG_VOID_RETURN;
  }

  Incident_log_event(THD *thd_arg, enum_incident incident, LEX_STRING const msg)
      : binary_log::Incident_event(incident),
        Log_event(thd_arg, LOG_EVENT_NO_FILTER_F, Log_event::EVENT_NO_CACHE,
                  Log_event::EVENT_IMMEDIATE_LOGGING, header(), footer()) {
    DBUG_ENTER("Incident_log_event::Incident_log_event");
    DBUG_PRINT("enter", ("incident: %d", incident));
    common_header->set_is_valid(incident > INCIDENT_NONE &&
                                incident < INCIDENT_COUNT);
    DBUG_ASSERT(message == NULL && message_length == 0);
    if (!(message = (char *)my_malloc(key_memory_Incident_log_event_message,
                                      msg.length + 1, MYF(MY_WME)))) {
      /*
        If the incident is not recognized, this binlog event is
        invalid.  If we set incident_number to INCIDENT_NONE, the
        invalidity will be detected by is_valid in both the ctors.
      */
      incident = INCIDENT_NONE;
      DBUG_VOID_RETURN;
    }
    strmake(message, msg.str, msg.length);
    message_length = msg.length;
    DBUG_VOID_RETURN;
  }
#endif

#ifdef MYSQL_SERVER
  int pack_info(Protocol *) override;
#endif

  Incident_log_event(const char *buf,
                     const Format_description_event *description_event);

  virtual ~Incident_log_event();

#ifndef MYSQL_SERVER
  virtual void print(FILE *file,
                     PRINT_EVENT_INFO *print_event_info) const override;
#endif

#if defined(MYSQL_SERVER)
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual bool write_data_header(Basic_ostream *ostream) override;
  virtual bool write_data_body(Basic_ostream *ostream) override;
#endif

  virtual size_t get_data_size() override {
    return Binary_log_event::INCIDENT_HEADER_LEN + 1 + message_length;
  }

  virtual bool ends_group() const override { return true; }

 private:
  const char *description() const;
};

/**
  @class Ignorable_log_event

  Base class for ignorable log events is Ignorable_event.
  Events deriving from this class can be safely ignored
  by slaves that cannot recognize them.

  Its the derived class of Ignorable_event

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
 B_l:Ignorable_event     Log_event
                 \       /
       <<virtual>>\     /
                   \   /
             Ignorable_log_event

  B_l: Namespace Binary_log
  @endinternal
*/
class Ignorable_log_event : public virtual binary_log::Ignorable_event,
                            public Log_event {
 public:
#ifdef MYSQL_SERVER
  Ignorable_log_event(THD *thd_arg)
      : Log_event(thd_arg, LOG_EVENT_IGNORABLE_F, Log_event::EVENT_STMT_CACHE,
                  Log_event::EVENT_NORMAL_LOGGING, header(), footer()) {
    DBUG_ENTER("Ignorable_log_event::Ignorable_log_event");
    common_header->set_is_valid(true);
    DBUG_VOID_RETURN;
  }
#endif

  Ignorable_log_event(const char *buf,
                      const Format_description_event *descr_event);
  virtual ~Ignorable_log_event();

#ifdef MYSQL_SERVER
  int pack_info(Protocol *) override;
#endif

#ifndef MYSQL_SERVER
  virtual void print(FILE *file,
                     PRINT_EVENT_INFO *print_event_info) const override;
#endif

  virtual size_t get_data_size() override {
    return Binary_log_event::IGNORABLE_HEADER_LEN;
  }
};

/**
  @class Rows_query_log_event
  It is used to record the original query for the rows
  events in RBR.
  It is the subclass of Ignorable_log_event and Rows_query_event

  @internal
  The inheritance structure in the current design for the classes is
  as follows:
                         Binary_log_event
                                  ^
                                  |
                                  |
                                  |
                Log_event   B_l:Ignorable_event
                     ^            /\
                     |           /  \
                     |   <<vir>>/    \ <<vir>>
                     |         /      \
                     |        /        \
                     |       /          \
              Ignorable_log_event    B_l:Rows_query_event
                             \          /
                              \        /
                               \      /
                                \    /
                                 \  /
                                  \/
                       Rows_query_log_event

  B_l : namespace binary_log
  @endinternal
*/
class Rows_query_log_event : public Ignorable_log_event,
                             public binary_log::Rows_query_event {
 public:
#ifdef MYSQL_SERVER
  Rows_query_log_event(THD *thd_arg, const char *query, size_t query_len)
      : Ignorable_log_event(thd_arg) {
    DBUG_ENTER("Rows_query_log_event::Rows_query_log_event");
    common_header->type_code = binary_log::ROWS_QUERY_LOG_EVENT;
    if (!(m_rows_query =
              (char *)my_malloc(key_memory_Rows_query_log_event_rows_query,
                                query_len + 1, MYF(MY_WME))))
      return;
    snprintf(m_rows_query, query_len + 1, "%s", query);
    DBUG_PRINT("enter", ("%s", m_rows_query));
    DBUG_VOID_RETURN;
  }
#endif

#ifdef MYSQL_SERVER
  int pack_info(Protocol *) override;
  virtual int do_apply_event(Relay_log_info const *rli) override;
  virtual bool write_data_body(Basic_ostream *ostream) override;
#endif

  Rows_query_log_event(const char *buf, uint event_len,
                       const Format_description_event *descr_event);

  virtual ~Rows_query_log_event() {
    if (m_rows_query) my_free(m_rows_query);
    m_rows_query = NULL;
  }
#ifndef MYSQL_SERVER
  virtual void print(FILE *file,
                     PRINT_EVENT_INFO *print_event_info) const override;
#endif
  virtual size_t get_data_size() override {
    return Binary_log_event::IGNORABLE_HEADER_LEN + 1 + strlen(m_rows_query);
  }
};

static inline bool copy_event_cache_to_file_and_reinit(IO_CACHE *cache,
                                                       FILE *file,
                                                       bool flush_stream) {
  return my_b_copy_to_file(cache, file) ||
         (flush_stream ? (fflush(file) || ferror(file)) : 0) ||
         reinit_io_cache(cache, WRITE_CACHE, 0, false, true);
}

#ifdef MYSQL_SERVER
/*****************************************************************************

  Heartbeat Log Event class

  The class is not logged to a binary log, and is not applied on to the slave.
  The decoding of the event on the slave side is done by its superclass,
  binary_log::Heartbeat_event.

 ****************************************************************************/
class Heartbeat_log_event : public binary_log::Heartbeat_event,
                            public Log_event {
 public:
  Heartbeat_log_event(const char *buf,
                      const Format_description_event *description_event);
};

/**
   The function is called by slave applier in case there are
   active table filtering rules to force gathering events associated
   with Query-log-event into an array to execute
   them once the fate of the Query is determined for execution.
*/
bool slave_execute_deferred_events(THD *thd);
#endif

int append_query_string(THD *thd, const CHARSET_INFO *csinfo,
                        String const *from, String *to);
extern TYPELIB binlog_checksum_typelib;

/**
  @class Gtid_log_event

  This is a subclass if Gtid_event and Log_event.  It contains
  per-transaction fields, including the GTID and logical timestamps
  used by MTS.

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
         B_l:Gtid_event   Log_event
                \         /
                 \       /
                  \     /
                   \   /
               Gtid_log_event

  B_l: Namespace Binary_log
  @endinternal
*/
class Gtid_log_event : public binary_log::Gtid_event, public Log_event {
 public:
#ifdef MYSQL_SERVER
  /**
    Create a new event using the GTID owned by the given thread.
  */
  Gtid_log_event(THD *thd_arg, bool using_trans, int64 last_committed_arg,
                 int64 sequence_number_arg, bool may_have_sbr_stmts_arg,
                 ulonglong original_commit_timestamp_arg,
                 ulonglong immediate_commit_timestamp_arg);

  /**
    Create a new event using the GTID from the given Gtid_specification
    without a THD object.
  */
  Gtid_log_event(uint32 server_id_arg, bool using_trans,
                 int64 last_committed_arg, int64 sequence_number_arg,
                 bool may_have_sbr_stmts_arg,
                 ulonglong original_commit_timestamp_arg,
                 ulonglong immediate_commit_timestamp_arg,
                 const Gtid_specification spec_arg);
#endif

#ifdef MYSQL_SERVER
  int pack_info(Protocol *) override;
#endif
  Gtid_log_event(const char *buffer,
                 const Format_description_event *description_event);

  virtual ~Gtid_log_event() {}

  size_t get_data_size() override {
    DBUG_EXECUTE_IF("do_not_write_rpl_timestamps", return POST_HEADER_LENGTH;);
    return POST_HEADER_LENGTH + get_commit_timestamp_length() +
           net_length_size(transaction_length);
  }

  size_t get_event_length() { return LOG_EVENT_HEADER_LEN + get_data_size(); }

 private:
  /// Used internally by both print() and pack_info().
  size_t to_string(char *buf) const;

#ifdef MYSQL_SERVER
  /**
    Writes the post-header to the given output stream.

    This is an auxiliary function typically used by the write() member
    function.

    @param ostream The output stream to write to.

    @retval true Error.
    @retval false Success.
  */
  bool write_data_header(Basic_ostream *ostream) override;
  bool write_data_body(Basic_ostream *ostream) override;
  /**
    Writes the post-header to the given memory buffer.

    This is an auxiliary function used by write_to_memory.

    @param[in,out] buffer Buffer to which the post-header will be written.

    @return The number of bytes written, i.e., always
    Gtid_log_event::POST_HEADER_LENGTH.
  */
  uint32 write_post_header_to_memory(uchar *buffer);

  /**
    Writes the body to the given memory buffer.

    This is an auxiliary function used by write_to_memory.

    @param [in,out] buff Buffer to which the data will be written.

    @return The number of bytes written, i.e.,
            If the transaction did not originated on this server
              Gtid_event::IMMEDIATE_COMMIT_TIMESTAMP_LENGTH.
            else
              FULL_COMMIT_TIMESTAMP_LENGTH.
  */
  uint32 write_body_to_memory(uchar *buff);
#endif

 public:
#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif
#ifdef MYSQL_SERVER
  /**
    Writes this event to a memory buffer.

    @param buf The event will be written to this buffer.

    @return the number of bytes written, i.e., always
    LOG_EVENT_HEADER_LEN + Gtid_log_event::POST_HEADEr_LENGTH.
  */
  uint32 write_to_memory(uchar *buf) {
    common_header->data_written = LOG_EVENT_HEADER_LEN + get_data_size();
    uint32 len = write_header_to_memory(buf);
    len += write_post_header_to_memory(buf + len);
    len += write_body_to_memory(buf + len);
    return len;
  }
#endif

#if defined(MYSQL_SERVER)
  int do_apply_event(Relay_log_info const *rli) override;
  int do_update_pos(Relay_log_info *rli) override;
  enum_skip_reason do_shall_skip(Relay_log_info *rli) override;
#endif

  /**
    Return the gtid type for this Gtid_log_event: this can be
    either ANONYMOUS_GTID, AUTOMATIC_GTID, or ASSIGNED_GTID.
  */
  enum_gtid_type get_type() const { return spec.type; }

  /**
    Return the SID for this GTID.  The SID is shared with the
    Log_event so it should not be modified.
  */
  const rpl_sid *get_sid() const { return &sid; }
  /**
    Return the SIDNO relative to the global sid_map for this GTID.

    This requires a lookup and possibly even update of global_sid_map,
    hence global_sid_lock must be held.  If global_sid_lock is not
    held, the caller must pass need_lock=true.  If there is an error
    (e.g. out of memory) while updating global_sid_map, this function
    returns a negative number.

    @param need_lock If true, the read lock on global_sid_lock is
    acquired and released inside this function; if false, the read
    lock or write lock must be held prior to calling this function.
    @retval SIDNO if successful
    @retval negative if adding SID to global_sid_map causes an error.
  */
  rpl_sidno get_sidno(bool need_lock) {
    if (spec.gtid.sidno < 0) {
      if (need_lock)
        global_sid_lock->rdlock();
      else
        global_sid_lock->assert_some_lock();
      spec.gtid.sidno = global_sid_map->add_sid(sid);
      if (need_lock) global_sid_lock->unlock();
    }
    return spec.gtid.sidno;
  }
  /**
    Return the SIDNO relative to the given Sid_map for this GTID.

    This assumes that the Sid_map is local to the thread, and thus
    does not use locks.

    @param sid_map The sid_map to use.
    @retval SIDNO if successful.
    @retval negative if adding SID to sid_map causes an error.
  */
  rpl_sidno get_sidno(Sid_map *sid_map) { return sid_map->add_sid(sid); }
  /// Return the GNO for this GTID.
  rpl_gno get_gno() const { return spec.gtid.gno; }

  /// string holding the text "SET @@GLOBAL.GTID_NEXT = '"
  static const char *SET_STRING_PREFIX;

 private:
  /// Length of SET_STRING_PREFIX
  static const size_t SET_STRING_PREFIX_LENGTH = 26;
  /// The maximal length of the entire "SET ..." query.
  static const size_t MAX_SET_STRING_LENGTH = SET_STRING_PREFIX_LENGTH +
                                              binary_log::Uuid::TEXT_LENGTH +
                                              1 + MAX_GNO_TEXT_LENGTH + 1;

 private:
  /**
    Internal representation of the GTID.  The SIDNO will be
    uninitialized (value -1) until the first call to get_sidno(bool).
  */
  Gtid_specification spec;
  /// SID for this GTID.
  rpl_sid sid;

 public:
  /**
    Set the transaction length information based on binlog cache size.

    Note that is_checksum_enabled and event_counter are optional parameters.
    When not specified, the function will assume that no checksum will be used
    and the informed cache_size is the final transaction size without
    considering the GTID event size.

    The high level formula that will be used by the function is:

    trx_length = cache_size +
                 cache_checksum_active * cache_events * CRC32_payload +
                 gtid_length +
                 cache_checksum_active * CRC32_payload; // For the GTID.

    @param cache_size The size of the binlog cache in bytes.
    @param is_checksum_enabled If checksum will be added to events on flush.
    @param event_counter The amount of events in the cache.
  */
  void set_trx_length_by_cache_size(ulonglong cache_size,
                                    bool is_checksum_enabled = false,
                                    int event_counter = 0);
};

/**
  @class Previous_gtids_log_event

  This is the subclass of Previous_gtids_event and Log_event
  It is used to record the gtid_executed in the last binary log file,
  for ex after flush logs, or at the starting of the binary log file

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
B_l:Previous_gtids_event   Log_event
                \         /
                 \       /
                  \     /
                   \   /
         Previous_gtids_log_event

  B_l: Namespace Binary_log
  @endinternal
*/
class Previous_gtids_log_event : public binary_log::Previous_gtids_event,
                                 public Log_event {
 public:
#ifdef MYSQL_SERVER
  Previous_gtids_log_event(const Gtid_set *set);
#endif

#ifdef MYSQL_SERVER
  int pack_info(Protocol *) override;
#endif

  Previous_gtids_log_event(const char *buf,
                           const Format_description_event *description_event);
  virtual ~Previous_gtids_log_event() {}

  size_t get_data_size() override { return buf_size; }

#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif
#ifdef MYSQL_SERVER
  bool write(Basic_ostream *ostream) override {
    if (DBUG_EVALUATE_IF("skip_writing_previous_gtids_log_event", 1, 0) &&
        /*
          The skip_writing_previous_gtids_log_event debug point was designed
          for skipping the writing of the previous_gtids_log_event on binlog
          files only.
        */
        !is_relay_log_event()) {
      DBUG_PRINT("info",
                 ("skip writing Previous_gtids_log_event because of"
                  "debug option 'skip_writing_previous_gtids_log_event'"));
      return false;
    }

    if (DBUG_EVALUATE_IF("write_partial_previous_gtids_log_event", 1, 0) &&
        /*
          The write_partial_previous_gtids_log_event debug point was designed
          for writing a partial previous_gtids_log_event on binlog files only.
        */
        !is_relay_log_event()) {
      DBUG_PRINT("info",
                 ("writing partial Previous_gtids_log_event because of"
                  "debug option 'write_partial_previous_gtids_log_event'"));
      return (Log_event::write_header(ostream, get_data_size()) ||
              Log_event::write_data_header(ostream));
    }

    return (Log_event::write_header(ostream, get_data_size()) ||
            Log_event::write_data_header(ostream) || write_data_body(ostream) ||
            Log_event::write_footer(ostream));
  }
  bool write_data_body(Basic_ostream *ostream) override;
#endif

  /// Return the encoded buffer, or NULL on error.
  const uchar *get_buf() { return buf; }
  /**
    Return the formatted string, or NULL on error.
    The string is allocated using my_malloc and it is the
    responsibility of the caller to free it.
  */
  char *get_str(size_t *length,
                const Gtid_set::String_format *string_format) const;
  /// Add all GTIDs from this event to the given Gtid_set.
  int add_to_set(Gtid_set *gtid_set) const;
  /*
    Previous Gtid Log events should always be skipped
    there is nothing to apply there, whether it is
    relay log's (generated on Slave) or it is binary log's
    (generated on Master, copied to slave as relay log).
    Also, we should not increment slave_skip_counter
    for this event, hence return EVENT_SKIP_IGNORE.
   */
#if defined(MYSQL_SERVER)
  enum_skip_reason do_shall_skip(Relay_log_info *) override  // 1358
  {
    return EVENT_SKIP_IGNORE;
  }

  int do_apply_event(Relay_log_info const *) override { return 0; }
  int do_update_pos(Relay_log_info *rli) override;
#endif
};

/**
  @class Transaction_context_log_event

  This is the subclass of Transaction_context_event and Log_event
  This class encodes the transaction_context_log_event.

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
B_l:Transaction_context_event   Log_event
                \                    /
                 \                  /
                  \                /
                   \              /
            Transaction_context_log_event

  B_l: Namespace Binary_log
  @endinternal
*/
class Transaction_context_log_event
    : public binary_log::Transaction_context_event,
      public Log_event {
 private:
  /// The Sid_map to use for creating the Gtid_set.
  Sid_map *sid_map;
  /// A gtid_set which is used to store the transaction set used for
  /// conflict detection.
  Gtid_set *snapshot_version;

#ifdef MYSQL_SERVER
  bool write_data_header(Basic_ostream *ostream) override;

  bool write_data_body(Basic_ostream *ostream) override;

  bool write_snapshot_version(Basic_ostream *ostream);

  bool write_data_set(Basic_ostream *ostream, std::list<const char *> *set);
#endif

  size_t get_snapshot_version_size();

  static int get_data_set_size(std::list<const char *> *set);

  size_t to_string(char *buf, ulong len) const;

 public:
#ifdef MYSQL_SERVER
  Transaction_context_log_event(const char *server_uuid_arg, bool using_trans,
                                my_thread_id thread_id_arg,
                                bool is_gtid_specified_arg);
#endif

  Transaction_context_log_event(const char *buffer,
                                const Format_description_event *descr_event);

  virtual ~Transaction_context_log_event();

  size_t get_data_size() override;

#ifdef MYSQL_SERVER
  int pack_info(Protocol *protocol) override;
#endif

#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

#if defined(MYSQL_SERVER)
  int do_apply_event(Relay_log_info const *) override { return 0; }
  int do_update_pos(Relay_log_info *rli) override;
#endif

  /**
    Add a hash which identifies a inserted/updated/deleted row on the
    ongoing transaction.

    @param[in] hash  row identifier
   */
  void add_write_set(const char *hash);

  /**
    Return a pointer to write-set list.
   */
  std::list<const char *> *get_write_set() { return &write_set; }

  /**
    Add a hash which identifies a read row on the ongoing transaction.

    @param[in] hash  row identifier
   */
  void add_read_set(const char *hash);

  /**
    Return a pointer to read-set list.
   */
  std::list<const char *> *get_read_set() { return &read_set; }

  /**
    Read snapshot version from encoded buffers.
    Cannot be executed during data read from file (event constructor),
    since its required locks will collide with the server gtid state
    initialization procedure.
   */
  bool read_snapshot_version();

  /**
    Return the transaction snapshot timestamp.
   */
  Gtid_set *get_snapshot_version() { return snapshot_version; }

  /**
    Return the server uuid.
   */
  const char *get_server_uuid() { return server_uuid; }

  /**
    Return the id of the committing thread.
   */
  my_thread_id get_thread_id() { return static_cast<my_thread_id>(thread_id); }

  /**
   Return true if transaction has GTID_NEXT specified, false otherwise.
   */
  bool is_gtid_specified() { return gtid_specified == true; };
};

/**
  @class View_change_log_event

  This is the subclass of View_change_log_event and Log_event
  This class created the view_change_log_event which is used as a marker in
  case a new node joins or leaves the group.

  @internal
  The inheritance structure is as follows

        Binary_log_event
               ^
               |
               |
B_l:   View_change_event      Log_event
                \                /
                 \              /
                  \            /
                   \          /
              View_change_log_event

  B_l: Namespace Binary_log
  @endinternal
*/

class View_change_log_event : public binary_log::View_change_event,
                              public Log_event {
 private:
  size_t to_string(char *buf, ulong len) const;

#ifdef MYSQL_SERVER
  bool write_data_header(Basic_ostream *ostream) override;

  bool write_data_body(Basic_ostream *ostream) override;

  bool write_data_map(Basic_ostream *ostream,
                      std::map<std::string, std::string> *map);
#endif

  size_t get_size_data_map(std::map<std::string, std::string> *map);

 public:
  View_change_log_event(char *view_id);

  View_change_log_event(const char *buffer,
                        const Format_description_event *descr_event);

  virtual ~View_change_log_event();

  size_t get_data_size() override;

#ifdef MYSQL_SERVER
  int pack_info(Protocol *protocol) override;
#endif

#ifndef MYSQL_SERVER
  void print(FILE *file, PRINT_EVENT_INFO *print_event_info) const override;
#endif

#if defined(MYSQL_SERVER)
  int do_apply_event(Relay_log_info const *rli) override;
  int do_update_pos(Relay_log_info *rli) override;
#endif

  /**
    Returns the view id.
  */
  char *get_view_id() { return view_id; }

  /**
    Sets the certification info
  */
  void set_certification_info(std::map<std::string, std::string> *info);

  /**
    Returns the certification info
  */
  std::map<std::string, std::string> *get_certification_info() {
    return &certification_info;
  }

  /**
    Set the certification sequence number

    @param number the sequence number
  */
  void set_seq_number(rpl_gno number) { seq_number = number; }

  /**
    Returns the certification sequence number
  */
  rpl_gno get_seq_number() { return seq_number; }
};

inline bool is_gtid_event(Log_event *evt) {
  return (evt->get_type_code() == binary_log::GTID_LOG_EVENT ||
          evt->get_type_code() == binary_log::ANONYMOUS_GTID_LOG_EVENT);
}

/**
  The function checks the argument event properties to deduce whether
  it represents an atomic DDL.

  @param  evt    a reference to Log_event
  @return true   when the DDL properties are found,
          false  otherwise
*/
inline bool is_atomic_ddl_event(Log_event *evt) {
  return evt != NULL && evt->get_type_code() == binary_log::QUERY_EVENT &&
         static_cast<Query_log_event *>(evt)->ddl_xid !=
             binary_log::INVALID_XID;
}

/**
  The function lists all DDL instances that are supported
  for crash-recovery (WL9175).
  todo: the supported feature list is supposed to grow. Once
        a feature has been readied for 2pc through WL7743,9536(7141/7016) etc
        it needs registering in the function.

  @param  thd    an Query-log-event creator thread handle
  @param  using_trans
                 The caller must specify the value accoding to the following
                 rules:
                 @c true when
                  - on master the current statement is not processing
                    a table in SE which does not support atomic DDL
                  - on slave the relay-log repository is transactional.
                 @c false otherwise.
  @return true   when the being created (master) or handled (slave) event
                 is 2pc-capable, @c false otherwise.
*/
bool is_atomic_ddl(THD *thd, bool using_trans);

#ifdef MYSQL_SERVER
/**
   Serialize an binary event to the given output stream. It is more general
   than call ev->write() directly. The caller will not be affected if any
   change happens in serialization process. For example, serializing the
   event in different format.
 */
template <class EVENT>
bool binary_event_serialize(EVENT *ev, Basic_ostream *ostream) {
  return ev->write(ostream);
}

/*
  This is an utility function that adds a quoted identifier into the a buffer.
  This also escapes any existance of the quote string inside the identifier.
 */
size_t my_strmov_quoted_identifier(THD *thd, char *buffer,
                                   const char *identifier, size_t length);
#else
size_t my_strmov_quoted_identifier(char *buffer, const char *identifier);
#endif
size_t my_strmov_quoted_identifier_helper(int q, char *buffer,
                                          const char *identifier,
                                          size_t length);

/**
  Read an integer in net_field_length format, guarding against read out of
  bounds and advancing the position.

  @param[in,out] packet Pointer to buffer to read from. On successful
  return, the buffer position will be incremented to point to the next
  byte after what was read.

  @param[in,out] max_length Pointer to the number of bytes in the
  buffer. If the function would need to look at more than *max_length
  bytes in order to decode the number, the function will do nothing
  and return true.

  @param[out] out Pointer where the value will be stored.

  @retval false Success.
  @retval true Failure, i.e., reached end of buffer.
*/
template <typename T>
bool net_field_length_checked(const uchar **packet, size_t *max_length, T *out);

/**
  @} (end of group Replication)
*/

#endif /* _log_event_h */
