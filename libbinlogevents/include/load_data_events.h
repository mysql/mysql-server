/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @addtogroup Replication
  @{

  @file load_data_events.h

  @brief LOAD DATA INFILE is not written to the binary log like other
  statements. It is written as one or more events in a packed format,
  not as a cleartext statement in the binary log. The events indicate
  what options are present in the statement and how to process the data file.
*/

#ifndef LOAD_DATA_EVENTS_INCLUDED
#define	LOAD_DATA_EVENTS_INCLUDED

#include "statement_events.h"
#include "table_id.h"

/*
  These are flags and structs to handle all the LOAD DATA INFILE options (LINES
  TERMINATED etc).
  DUMPFILE_FLAG is probably not used (DUMPFILE is a clause of SELECT,
  not of LOAD DATA).
*/
#define DUMPFILE_FLAG           0x1
#define OPT_ENCLOSED_FLAG       0x2
#define REPLACE_FLAG            0x4
#define IGNORE_FLAG             0x8

#define FIELD_TERM_EMPTY        0x1
#define ENCLOSED_EMPTY          0x2
#define LINE_TERM_EMPTY         0x4
#define LINE_START_EMPTY        0x8
#define ESCAPED_EMPTY           0x10

namespace binary_log
{
/**
  @struct old_sql_ex

  This structure holds the single character field/line options in the
  LOAD_DATA_INFILE statement. It is used for server versions prior to
  5.0.3, where a single character separator was supported for
  LOAD_DATA_INFILE statements.

  The structure contains the foloowing components.
  <table>
  <caption>old_sql_ex members for Load_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>field_term</td>
    <td>a single character</td>
    <td>field terminating character spcified by the subclause
        'FIELDS TERMINATED BY'
    </td>
  </tr>

  <tr>
    <td>enclosed</td>
    <td>a single character</td>
    <td>character used for enclosing field data, specified by
        the subclause 'FIELDS ENCLOSED BY'
    </td>
  </tr>

  <tr>
    <td>line_term</td>
    <td>a single character</td>
    <td>line terminating character, specified by the subclause
        'LINES TERMINATED BY'
    </td>
  </tr>

  <tr>
    <td>line_start</td>
    <td>a single character</td>
    <td>character indicating the start of a line, specified by
       the subclause 'LINES STARTING BY'
    </td>
  </tr>

  <tr>
    <td>escaped</td>
    <td>a single character</td>
    <td>escape character for a field, specified by the subclause
       'FIELD ESCAPED BY'
    </td>
  </tr>

  <tr>
    <td>opt_flags</td>
    <td>8 bit bitfield value </td>
    <td>bitfield indicating the presence of the keywords REPLACE,
        IGNORE, and OPTIONALLY
    </td>
  </tr>

  <tr>
    <td>empty_flags</td>
    <td>8 bit bitfield value </td>
    <td>The low 5 bits of empty_flags indicate which of the five strings
        have length 0.  For each of the following flags that is set, the
        corresponding string has length 0; for the flags that are not set,
        the string has length 1: FIELD_TERM_EMPTY==0x1,
        ENCLOSED_EMPTY==0x2, LINE_TERM_EMPTY==0x4, LINE_START_EMPTY==0x8,
        ESCAPED_EMPTY==0x10.
    </td>
  </tr>
*/
struct old_sql_ex
{
  char field_term;
  char enclosed;
  char line_term;
  char line_start;
  char escaped;
  char opt_flags;
  char empty_flags;
};

/**
  @struct sql_ex_data_info

  This structure holds the multi character field/line options in the
  LOAD_DATA_INFILE statement. It is used for server versions newer than
  5.0.3, where multicharacter separators were supported for
  LOAD_DATA_INFILE SQL Query.

  The structure is simlar to old_sql_ex defined above.
  The new and old format differ in the way the five strings indicating the
  terminating characters in the query are stored.

  To know more, read comments in the class Load_event and struct
  old_sql_ex.
*/
struct sql_ex_data_info
{
  sql_ex_data_info()
  : cached_new_format(0)
  {
  }
  const char* field_term;
  const char* enclosed;
  const char* line_term;
  const char* line_start;
  const char* escaped;
  uint8_t field_term_len, enclosed_len, line_term_len,
          line_start_len, escaped_len;

  char opt_flags;
  char empty_flags;

  int cached_new_format;

  /** store in new format even if old is possible */
  void force_new_format() { cached_new_format = 1;}
  int data_size()
  {
    return (new_format() ?
            field_term_len + enclosed_len + line_term_len +
            line_start_len + escaped_len + 6 : 7);
  }
  const char* init(const char* buf, const char* buf_end, bool use_new_format);
  bool new_format()
  {
    return ((cached_new_format != -1) ? cached_new_format :
            (cached_new_format= (field_term_len > 1 ||
                                 enclosed_len > 1 ||
                                 line_term_len > 1 || line_start_len > 1 ||
                                 escaped_len > 1)));
  }
};

/**
  Elements of this enum describe how LOAD DATA handles duplicates.
*/
enum enum_load_dup_handling
{
  LOAD_DUP_ERROR= 0,
  LOAD_DUP_IGNORE,
  LOAD_DUP_REPLACE
};

/**
  @class Execute_load_query_event

  Event responsible for LOAD DATA execution, it similar to Query_event
  but before executing the query it substitutes original filename in LOAD DATA
  query with name of temporary file.

  The first 13 bytes of the Post-Header for this event are the same as for
  Query_event, as is the initial status variable block in the Body.

  @section Execute_load_query_event_binary_format Binary Format

  The additional members of the events are the following:

   <table>
   <caption>Body for Execute_load_query_event</caption>

   <tr>
     <th>Name</th>
     <th>Format</th>
     <th>Description</th>
   </tr>

   <tr>
     <td>file_id</td>
     <td>4 byte unsigned integer</td>
     <td>ID of the temporary file to load</td>
   </tr>

   <tr>
     <td>fn_pos_start</td>
     <td>4 byte unsigned integer</td>
     <td>The start position within the statement for filename substitution</td>
   </tr>
   <tr>

     <td>fn_pos_end</td>
     <td>4 byte unsigned integer</td>
     <td>The end position within the statement for filename substitution</td>
   </tr>

   <tr>
     <td>dup_handling</td>
     <td>enum_load_dup_handling</td>
     <td>Represents information on how to handle duplicates:
          LOAD_DUP_ERROR= 0, LOAD_DUP_IGNORE= 1, LOAD_DUP_REPLACE= 2</td>
   </tr>
*/
class Execute_load_query_event : public virtual Query_event
{
public:
  enum Execute_load_query_event_offset{
   /** ELQ = "Execute Load Query" */
   ELQ_FILE_ID_OFFSET= QUERY_HEADER_LEN,
   ELQ_FN_POS_START_OFFSET= ELQ_FILE_ID_OFFSET + 4,
   ELQ_FN_POS_END_OFFSET= ELQ_FILE_ID_OFFSET + 8,
   ELQ_DUP_HANDLING_OFFSET= ELQ_FILE_ID_OFFSET + 12
  };

  int32_t file_id;        /** file_id of temporary file */
  uint32_t fn_pos_start;  /** pointer to the part of the query that should
                             be substituted */
  uint32_t fn_pos_end;    /** pointer to the end of this part of query */

  /**
    We have to store type of duplicate handling explicitly, because
    for LOAD DATA it also depends on LOCAL option. And this part
    of query will be rewritten during replication so this information
    may be lost...
  */
  enum_load_dup_handling dup_handling;

  Execute_load_query_event(uint32_t file_id_arg, uint32_t fn_pos_start,
                           uint32_t fn_pos_end, enum_load_dup_handling dup);

  /**
    The constructor receives a buffer and instantiates a Execute_load_uery_event
    filled in with the data from the buffer.

    <pre>
    The fixed event data part buffer layout is as follows:
    +---------------------------------------------------------------------+
    | thread_id | query_exec_time | db_len | error_code | status_vars_len |
    +---------------------------------------------------------------------+
    +----------------------------------------------------+
    | file_id | fn_pos_start | fn_pos_end | dup_handling |
    +----------------------------------------------------+
    </pre>

    <pre>
    The fixed event data part buffer layout is as follows:
    +------------------------------------------------------------------+
    | Zero or more status variables | db |  LOAD DATA INFILE statement |
    +------------------------------------------------------------------+
    </pre>

    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
    @param event_type         Required to determine whether the event type is
                              QUERY_EVENT or EXECUTE_LOAD_QUERY_EVENT
  */
  Execute_load_query_event(const char* buf, unsigned int event_len,
                           const Format_description_event *description_event);

  ~Execute_load_query_event() {}

};

/**
  @class Load_event

  This event corresponds to a "LOAD DATA INFILE" SQL query in the
  following form:

  @verbatim
   (1)    USE db;
   (2)    LOAD DATA [CONCURRENT] [LOCAL] INFILE 'file_name'
   (3)    [REPLACE | IGNORE]
   (4)    INTO TABLE 'table_name'
   (5)    [FIELDS
   (6)      [TERMINATED BY 'field_term']
   (7)      [[OPTIONALLY] ENCLOSED BY 'enclosed']
   (8)      [ESCAPED BY 'escaped']
   (9)    ]
  (10)    [LINES
  (11)      [TERMINATED BY 'line_term']
  (12)      [LINES STARTING BY 'line_start']
  (13)    ]
  (14)    [IGNORE skip_lines LINES]
  (15)    (field_1, field_2, ..., field_n)
  @endverbatim

  @section Load_event_binary_format Binary Format

  The Post-Header consists of the following six components.

  <table>
  <caption>Post-Header for Load_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>slave_proxy_id</td>
    <td>4 byte unsigned integer</td>
    <td>An integer identifying the client thread that issued the
    query.  The id is unique per server.  (Note, however, that two
    threads on different servers may have the same slave_proxy_id.)
    This is used when a client thread creates a temporary table local
    to the client.  The slave_proxy_id is used to distinguish
    temporary tables that belong to different clients.
    </td>
  </tr>

  <tr>
    <td>load_exec_time</td>
    <td>4 byte unsigned integer</td>
    <td>The time from when the query started to when it was logged in
    the binlog, in seconds.</td>
  </tr>

  <tr>
    <td>skip_lines</td>
    <td>4 byte unsigned integer</td>
    <td>The number on line (14) above, if present, or 0 if line (14)
    is left out.
    </td>
  </tr>

  <tr>
    <td>table_name_len</td>
    <td>1 byte unsigned integer</td>
    <td>The length of 'table_name' on line (4) above.</td>
  </tr>

  <tr>
    <td>db_len</td>
    <td>1 byte unsigned integer</td>
    <td>The length of 'db' on line (1) above.</td>
  </tr>

  <tr>
    <td>num_fields</td>
    <td>4 byte unsigned integer</td>
    <td>The number n of fields on line (15) above.</td>
  </tr>
  </table>

  The Body contains the following components.

  <table>
  <caption>Body of Load_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>sql_ex</td>
    <td>variable length</td>

    <td>Describes the part of the query on lines (3) and
    (5)&ndash;(13) above.  More precisely, it stores the five strings
    (on lines) field_term (6), enclosed (7), escaped (8), line_term
    (11), and line_start (12); as well as a bitfield indicating the
    presence of the keywords REPLACE (3), IGNORE (3), and OPTIONALLY
    (7).

    The data is stored in one of two formats, called "old" and "new".
    The type field of Common-Header determines which of these two
    formats is used: type LOAD_EVENT means that the old format is
    used, and type NEW_LOAD_EVENT means that the new format is used.
    When MySQL writes a Load_event, it uses the new format if at
    least one of the five strings is two or more bytes long.
    Otherwise (i.e., if all strings are 0 or 1 bytes long), the old
    format is used.

    The new and old format differ in the way the five strings are
    stored.

    <ul>
    <li> In the new format, the strings are stored in the order
    field_term, enclosed, escaped, line_term, line_start. Each string
    consists of a length (1 byte), followed by a sequence of
    characters (0-255 bytes).  Finally, a boolean combination of the
    following flags is stored in 1 byte: REPLACE_FLAG==0x4,
    IGNORE_FLAG==0x8, and OPT_ENCLOSED_FLAG==0x2.  If a flag is set,
    it indicates the presence of the corresponding keyword in the SQL
    query.

    <li> In the old format, we know that each string has length 0 or
    1.  Therefore, only the first byte of each string is stored.  The
    order of the strings is the same as in the new format.  These five
    bytes are followed by the same 1 byte bitfield as in the new
    format.  Finally, a 1 byte bitfield called empty_flags is stored.
    The low 5 bits of empty_flags indicate which of the five strings
    have length 0.  For each of the following flags that is set, the
    corresponding string has length 0; for the flags that are not set,
    the string has length 1: FIELD_TERM_EMPTY==0x1,
    ENCLOSED_EMPTY==0x2, LINE_TERM_EMPTY==0x4, LINE_START_EMPTY==0x8,
    ESCAPED_EMPTY==0x10.
    </ul>

    Thus, the size of the new format is 6 bytes + the sum of the sizes
    of the five strings.  The size of the old format is always 7
    bytes.
    </td>
  </tr>

  <tr>
    <td>field_lens</td>
    <td>num_fields 1 byte unsigned integers</td>
    <td>An array of num_fields integers representing the length of
    each field in the query.  (num_fields is from the Post-Header).
    </td>
  </tr>

  <tr>
    <td>fields</td>
    <td>num_fields null-terminated strings</td>
    <td>An array of num_fields null-terminated strings, each
    representing a field in the query.  (The trailing zero is
    redundant, since the length are stored in the num_fields array.)
    The total length of all strings equals to the sum of all
    field_lens, plus num_fields bytes for all the trailing zeros.
    </td>
  </tr>

  <tr>
    <td>table_name</td>
    <td>null-terminated string of length table_len + 1 bytes</td>
    <td>The 'table_name' from the query, as a null-terminated string.
    (The trailing zero is actually redundant since the table_len is
    known from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>db</td>
    <td>null-terminated string of length db_len + 1 bytes</td>
    <td>The 'db' from the query, as a null-terminated string.
    (The trailing zero is actually redundant since the db_len is known
    from Post-Header.)
    </td>
  </tr>

  <tr>
    <td>fname</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>The 'file_name' from the query.
    </td>
  </tr>

  </table>

  @subsection Load_event_notes_on_previous_versions Notes on Previous Versions

  This event type is understood by current versions, but only
  generated by MySQL 3.23 and earlier.
*/
class Load_event: public Binary_log_event
{
protected:
  int copy_load_event(const char *buf, unsigned long event_len,
                      int body_offset,
                      const Format_description_event* description_event);
  // Required by Load_event(THD* ...) in the server
  explicit Load_event(Log_event_type type_code_arg= NEW_LOAD_EVENT)
    : Binary_log_event(type_code_arg),
      num_fields(0),
      fields(0),
      field_lens(0),
      field_block_len(0)
  {}
public:
  enum Load_event_offset {
    L_THREAD_ID_OFFSET= 0,
    L_EXEC_TIME_OFFSET= 4,
    L_SKIP_LINES_OFFSET= 8,
    L_TBL_LEN_OFFSET= 12,
    L_DB_LEN_OFFSET= 13,
    L_NUM_FIELDS_OFFSET= 14,
    L_SQL_EX_OFFSET= 18,
    L_DATA_OFFSET= LOAD_HEADER_LEN
  };

  /**
   This is the execution time stored in the post header.
   Make sure to use it to set the exec_time variable in class Log_event
  */
  uint32_t load_exec_time;

  uint32_t slave_proxy_id;
  size_t table_name_len;

  /**
    No need to have a catalog, as these events can only come from 4.x.
  */
  size_t db_len;
  size_t fname_len;
  uint32_t num_fields;

  const char* fields;
  const unsigned char* field_lens;
  uint32_t field_block_len;

  const char* table_name;
  const char* db;
  const char* fname;
  uint32_t skip_lines;
  sql_ex_data_info sql_ex_data;
  bool local_fname;

  /**
    Indicates that this event corresponds to LOAD DATA CONCURRENT,

    @note Since Load_event event coming from the binary log
          lacks information whether LOAD DATA on master was concurrent
          or not, this flag is only set to TRUE for an auxiliary
          Load_event object which is used in mysql_load() to
          re-construct LOAD DATA statement from function parameters,
          for logging.
  */
  bool is_concurrent;

  /**
    Note that for all the events related to LOAD DATA (Load_event,
    Create_file/Append/Exec/Delete, we pass description_event; however as
    logging of LOAD DATA is changed, this is only used
    for the common_header_len (post_header_len is not changed).

    <pre>
    The fixed event data part buffer layout is as follows:
    +--------------------------------------------------------------------------------+
    | thread_id | load_exec_time | skip_lines | table_name_len | db_len | num_fields |
    +--------------------------------------------------------------------------------+
    </pre>

    <pre>
    Variable data part
    +---------------------------------------------------------------------+
    | sql_ex_data struct | len of col names to load | col_names | tb_name |
    +-------------------------------------------------------------- ------+
    +----------------+
    |db_name | fname |
    +----------------+
    </pre>
    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Load_event(const char* buf, unsigned int event_len,
             const Format_description_event* description_event);

  ~Load_event()
  {
  }

  size_t get_data_size()
  {
    return (table_name_len + db_len + 2 + fname_len
            + static_cast<unsigned int>(LOAD_HEADER_LEN)
            + sql_ex_data.data_size() + field_block_len + num_fields);
  }
#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info for
  //            all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info) {};
#endif

};

/* the classes below are for the new LOAD DATA INFILE logging */

/**
  @class Create_file_event

  The Create_file_event contains the options to LOAD DATA INFILE.
  This was a design flaw since the file cannot be loaded until the
  Exec_load_event is seen. The use of this event was deprecated from
  MySQL server version 5.0.3 and above.

  @section Create_file_event_binary_format Binary Format

  The post header contains the following:

  <table>
  <caption>Post header for Create_file_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>file_id</td>
    <td>32 bit integer</td>
    <td>The ID of the temporary file created by the slave to which
        the first data block is copied</td>
  </tr>
  </table>
*/
class Create_file_event: public virtual Load_event
{
protected:
  /**
    Pretend we are Load event, so we can write out just
    our Load part - used on the slave when writing event out to
    SQL_LOAD-*.info file
  */
  bool fake_base;
public:
  enum Create_file_offset {
   /** CF = "Create File" */
   CF_FILE_ID_OFFSET= 0,
   CF_DATA_OFFSET= CREATE_FILE_HEADER_LEN
  };

  unsigned char* block;
  const char *event_buf;
  unsigned int block_len;
  unsigned int file_id;
  bool inited_from_old;

  /**
    The buffer layout for variable data part is as follows:
    <pre>
    +-------------------------------------------------------------------+
    | name_len | name | is_null | type | charset_number | val_len | val |
    +-------------------------------------------------------------------+
    </pre>

    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Create_file_event(const char* buf, unsigned int event_len,
                    const Format_description_event* description_event);


  ~Create_file_event()
  {
     bapi_free(const_cast<char*>(event_buf));
  }
  size_t get_data_size()
  {
    return (fake_base ? Load_event::get_data_size() :
            Load_event::get_data_size() +
            4 + 1 + block_len);
  }


};

/**
  @class Delete_file_event

  DELETE_FILE_EVENT occurs when the LOAD DATA failed on the master.
  This event notifies the slave not to do the load and to delete
  the temporary file.

  @section Delete_file_event_binary_format Binary Format

  The variable data part is empty. The post header contains the following:

  <table>
  <caption>Post header for Delete_file_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>file_id</td>
    <td>32 bit integer</td>
    <td>The ID of the file to be deleted</td>
  </tr>
  </table>
*/
class Delete_file_event: public Binary_log_event
{
protected:
  // Required by Delete_file_log_event(THD* ..)
  Delete_file_event(uint32_t file_id_arg, const char* db_arg)
    : Binary_log_event(DELETE_FILE_EVENT),
      file_id(file_id_arg),
      db(db_arg)
  {}
public:
  enum Delete_file_offset {
    /** DF = "Delete File" */
    DF_FILE_ID_OFFSET= 0
  };

  uint32_t file_id;
  const char* db; /** see comment in Append_block_event */

   /**
    The buffer layout for fixed data part is as follows:
    <pre>
    +---------+
    | file_id |
    +---------+
    </pre>

    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Delete_file_event(const char* buf, unsigned int event_len,
                    const Format_description_event* description_event);

  ~Delete_file_event() {}

#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info for
  //            all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info) {};
#endif
};

/**
  @class Execute_load_event

  Execute_load_event is created when the LOAD_DATA query succeeds on
  the master. The slave should be notified to load the temporary file into
  the table. For server versions > 5.0.3, the temporary files that stores
  the parameters to LOAD DATA INFILE is not needed anymore, since they are
  stored in this event. There is still a temp file containing all the data
  to be loaded.

  @section Execute_load_event_binary_format Binary Format

  The variable data part is empty. The post header contains the following:

  <table>
  <caption>Post header for Execute_load_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>file_id</td>
    <td>32 bit integer</td>
    <td>The ID of the file to load</td>
  </tr>
*/
class Execute_load_event: public Binary_log_event
{
protected:
  Execute_load_event(const uint32_t file_id, const char* db_arg);
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}

public:
  enum Execute_load_offset {
    /** EL = "Execute Load" */
    EL_FILE_ID_OFFSET= 0
  };

  uint32_t file_id;
  const char* db; /** see comment in Append_block_event */

  /**
    The buffer layout for fixed data part is as follows:
    <pre>
    +---------+
    | file_id |
    +---------+
    </pre>

    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Execute_load_event(const char* buf, unsigned int event_len,
                     const Format_description_event *description_event);

  ~Execute_load_event() {}

#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info for
  //            all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info) {};
#endif
};

/**
  @class Append_block_event

  This event is created to contain the file data. One LOAD_DATA_INFILE
  can have 0 or more instances of this event written to the binary log
  depending on the size of the file. If the file to be loaded is greater
  than the threshold value, which is roughly 2^17 bytes, the file is
  divided into blocks of size equal to the threshold, and each block
  is sent across as a separate event.

  @section Append_block_event_binary_format Binary Format

  The post header contains the following:

  <table>
  <caption>Post header for Append_block_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>file_id</td>
    <td>32 bit integer</td>
    <td>The ID of the file to append the block to</td>
  </tr>
  </table>

  The body of the event contains the raw data to load. The raw data
  size is the event size minus the size of all the fixed event parts.
*/
class Append_block_event: public Binary_log_event
{
protected:
  /**
    This constructor is used by the MySQL server.
  */
  Append_block_event(const char* db_arg,
                     unsigned char* block_arg,
                     unsigned int block_len_arg,
                     uint32_t file_id_arg)
  : Binary_log_event(APPEND_BLOCK_EVENT),
    block(block_arg), block_len(block_len_arg),
    file_id(file_id_arg), db(db_arg)
  {}

  Append_block_event(Log_event_type type_arg= APPEND_BLOCK_EVENT)
    : Binary_log_event(type_arg)
  {}

public:
  enum Append_block_offset
  {
    /** AB = "Append Block" */
    AB_FILE_ID_OFFSET= 0,
    AB_DATA_OFFSET=  APPEND_BLOCK_HEADER_LEN
  };

  unsigned char* block;
  unsigned int block_len;
  uint32_t file_id;
  /**
    'db' is filled when the event is created in mysql_load() (the
    event needs to have a 'db' member to be well filtered by
    binlog-*-db rules). 'db' is not written to the binlog (it's not
    used by Append_block_log_event::write()), so it can't be read in
    the Append_block_event(const char* buf, int event_len)
    constructor.  In other words, 'db' is used only for filtering by
    binlog-*-db rules.  Create_file_event is different: it's 'db'
    (which is inherited from Load_event) is written to the binlog
    and can be re-read.
  */
  const char* db;


  /**
    The buffer layout for fixed data part is as follows:
    <pre>
    +---------+
    | file_id |
    +---------+
    </pre>

    The buffer layout for variabl data part is as follows:
    <pre>
    +-------------------+
    | block | block_len |
    +-------------------+
    </pre>

    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Append_block_event(const char* buf, unsigned int event_len,
                     const Format_description_event *description_event);
  ~Append_block_event() {}

#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info for
  //            all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info) {};
#endif
};

/**
  @class Begin_load_query_event

  Event for the first block of file to be loaded, its only difference from
  Append_block event is that this event creates or truncates existing file
  before writing data.

  @section Begin_load_query_event_binary_format Binary Format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/
class Begin_load_query_event: public virtual Append_block_event
{
protected:
  Begin_load_query_event()
    : Append_block_event(BEGIN_LOAD_QUERY_EVENT)
  {}

public:

  /**
    The buffer layout for fixed data part is as follows:
    <pre>
    +---------+
    | file_id |
    +---------+
    </pre>

    The buffer layout for variabl data part is as follows:
    <pre>
    +-------------------+
    | block | block_len |
    +-------------------+
    </pre>

    @param buf                Contains the serialized event.
    @param length             Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Begin_load_query_event(const char* buf, unsigned int event_len,
                         const Format_description_event *description_event);

  ~Begin_load_query_event() {}

#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info for
  //            all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info) {};
#endif
};
} // end namespace binary_log
/**
  @} (end of group Replication)
*/
#endif	/* LOAD_DATA_EVENTS_INCLUDED */
