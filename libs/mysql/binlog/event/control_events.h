/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file control_events.h

  @brief Contains the classes representing events operating in the replication
  stream properties. Each event is represented as a byte sequence with logical
  divisions as event header, event specific data and event footer. The header
  and footer are common to all the events and are represented as two different
  subclasses.
*/

#ifndef MYSQL_BINLOG_EVENT_CONTROL_EVENTS_H
#define MYSQL_BINLOG_EVENT_CONTROL_EVENTS_H

#include <sys/types.h>
#include <time.h>
#include <list>
#include <map>
#include <vector>

#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/compression/base.h"  // mysql::binlog::event::compression::type
#include "mysql/binlog/event/compression/buffer/buffer_sequence_view.h"  // Buffer_sequence_view
#include "mysql/gtid/gtid_constants.h"
#include "mysql/gtid/tsid.h"
#include "mysql/gtid/uuid.h"
#include "mysql/serialization/field_definition_helpers.h"
#include "mysql/serialization/field_functor.h"
#include "mysql/serialization/read_archive_binary.h"
#include "mysql/serialization/serializable.h"
#include "mysql/serialization/serializer_default.h"
#include "mysql/serialization/write_archive_binary.h"
#include "template_utils.h"

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event {
/**
  @class Rotate_event

  When a binary log file exceeds a size limit, a ROTATE_EVENT is written
  at the end of the file that points to the next file in the sequence.
  This event is information for the slave to know the name of the next
  binary log it is going to receive.

  ROTATE_EVENT is generated locally and written to the binary log
  on the master. It is written to the relay log on the slave when FLUSH LOGS
  occurs, and when receiving a ROTATE_EVENT from the master.
  In the latter case, there will be two rotate events in total originating
  on different servers.

  @section Rotate_event_binary_format Binary Format

  <table>
  <caption>Post-Header for Rotate_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>position</td>
    <td>8 byte integer</td>
    <td>The position within the binary log to rotate to.</td>
  </tr>

  </table>

  The Body has one component:

  <table>
  <caption>Body for Rotate_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>new_log_ident</td>
    <td>variable length string without trailing zero, extending to the
    end of the event (determined by the length field of the
    Common-Header)
    </td>
    <td>Name of the binlog to rotate to.</td>
  </tr>

  </table>
*/
class Rotate_event : public Binary_log_event {
 public:
  const char *new_log_ident;
  size_t ident_len;
  unsigned int flags;
  uint64_t pos;

  enum {
    /* Values taken by the flag member variable */
    DUP_NAME = 2,  // if constructor should dup the string argument
    RELAY_LOG = 4  // rotate event for the relay log
  };

  enum {
    /* Rotate event post_header */
    R_POS_OFFSET = 0,
    R_IDENT_OFFSET = 8
  };

  /**
    This is the minimal constructor, it will set the type code as ROTATE_EVENT.
  */
  Rotate_event(const char *new_log_ident_arg, size_t ident_len_arg,
               unsigned int flags_arg, uint64_t pos_arg)
      : Binary_log_event(ROTATE_EVENT),
        new_log_ident(new_log_ident_arg),
        ident_len(ident_len_arg ? ident_len_arg : strlen(new_log_ident_arg)),
        flags(flags_arg),
        pos(pos_arg) {}

  /**
    The layout of Rotate_event data part is as follows:

    <pre>
    +-----------------------------------------------------------------------+
    | common_header | post_header | position of the first event | file name |
    +-----------------------------------------------------------------------+
    </pre>

    @param buf  Contains the serialized event.
    @param fde  An FDE event, used to get the following information:
                  -binlog_version
                  -server_version
                  -post_header_len
                  -common_header_len
                The content of this object depends on the binlog-version
                currently in use.
  */
  Rotate_event(const char *buf, const Format_description_event *fde);

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &) override;
  void print_long_info(std::ostream &) override;
#endif

  ~Rotate_event() override {
    if (flags & DUP_NAME) bapi_free(const_cast<char *>(new_log_ident));
  }
};

/**
  @class Format_description_event
  For binlog version 4.
  This event is saved by threads which read it, as they need it for future
  use (to decode the ordinary events).

  @section Format_description_event_binary_format Binary Format

  The Post-Header has six components:

  <table>
  <caption>Post-Header for Format_description_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>created</td>
    <td>4 byte unsigned integer</td>
    <td>The creation timestamp, if non-zero,
        is the time in seconds when this event was created</td>
  </tr>
  <tr>
    <td>binlog_version</td>
    <td>2 byte unsigned integer</td>
    <td>This is 1 in MySQL 3.23 and 3 in MySQL 4.0 and 4.1
        (In MySQL 5.0 and up, FORMAT_DESCRIPTION_EVENT is
        used instead of START_EVENT_V3 and for them its 4).</td>
  </tr>
  <tr>
    <td>server_version</td>
    <td>char array of 50 bytes</td>
    <td>The MySQL server's version (example: 4.0.14-debug-log),
        padded with 0x00 bytes on the right</td>
  </tr>
  <tr>
    <td>common_header_len</td>
    <td>1 byte unsigned integer</td>
    <td>The length of the event header. This value includes the extra_headers
        field, so this header length - 19 yields the size
        of the extra_headers field.</td>
  </tr>
  <tr>
    <td>post_header_len</td>
    <td>array of type 1 byte unsigned integer</td>
    <td>The lengths for the fixed data part of each event</td>
  </tr>
  <tr>
    <td>server_version_split</td>
    <td>unsigned char array</td>
    <td>Stores the server version of the server
        and splits them in three parts</td>
  </tr>
  <tr>
    <td>number_of_event_types</td>
    <td>1 byte unsigned integer</td>
    <td>number of event types present in the server</td>
  </tr>
  </table>
*/
class Format_description_event : public Binary_log_event {
 public:
  /**
     If this event is at the start of the first binary log since server
     startup 'created' should be the timestamp when the event (and the
     binary log) was created.  In the other case (i.e. this event is at
     the start of a binary log created by FLUSH LOGS or automatic
     rotation), 'created' should be 0.  This "trick" is used by MySQL
     >=4.0.14 slaves to know whether they must drop stale temporary
     tables and whether they should abort unfinished transaction.

     Note that when 'created'!=0, it is always equal to the event's
     timestamp; indeed Format_description_event is written only in binlog.cc
     where the first constructor below is called, in which 'created' is set to
     'when'.  So in fact 'created' is a useless variable. When it is 0 we can
     read the actual value from timestamp ('when') and when it is non-zero we
     can read the same value from timestamp
     ('when'). Conclusion:
     - we use timestamp to print when the binlog was created.
     - we use 'created' only to know if this is a first binlog or not.
  */
  time_t created;
  uint16_t binlog_version;
  char server_version[ST_SERVER_VER_LEN];
  /*
    We set this to 1 if we don't want to have the created time in the log,
    which is the case when we rollover to a new log.
  */
  bool dont_set_created;

  /**
     The size of the fixed header which _all_ events have
     (for binlogs written by this version, this is equal to
     LOG_EVENT_HEADER_LEN), except FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT
     (those have a header of size LOG_EVENT_MINIMAL_HEADER_LEN).
  */
  uint8_t common_header_len;
  /*
    The list of post-headers' lengths followed
    by the checksum alg description byte
  */
  std::vector<uint8_t> post_header_len;
  unsigned char server_version_split[ST_SERVER_VER_SPLIT_LEN];

  /**
     Format_description_event 1st constructor.

     This constructor can be used to create the event to write to the binary log
     (when the server starts or when FLUSH LOGS)

     @param binlog_ver             the binlog version for which we want to build
     an event. It should only be 4, old versions are not compatible anymore
     since 8.0.2.
     @param server_ver             The MySQL server's version.
  */
  Format_description_event(uint8_t binlog_ver, const char *server_ver);
  /**
    The layout of Format_description_event data part is as follows:

    <pre>
    +=====================================+
    | event  | binlog_version   19 : 2    | = 4
    | data   +----------------------------+
    |        | server_version   21 : 50   |
    |        +----------------------------+
    |        | create_timestamp 71 : 4    |
    |        +----------------------------+
    |        | header_length    75 : 1    |
    |        +----------------------------+
    |        | post-header      76 : n    | = array of n bytes, one byte
    |        | lengths for all            |   per event type that the
    |        | event types                |   server knows about
    +=====================================+
    </pre>
    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).

    @note The fde passed to this constructor was created through another
          constructor of FDE class.
  */
  Format_description_event(const char *buf,
                           const Format_description_event *fde);

  Format_description_event(const Format_description_event &) = default;
  Format_description_event &operator=(const Format_description_event &) =
      default;
  uint8_t number_of_event_types;
  /**
    This method is used to find out the version of server that originated
    the current FD instance.

    @return the version of server.
  */
  unsigned long get_product_version() const;
  /**
    This method checks the MySQL version to determine whether checksums may be
    present in the events contained in the binary log.

    @retval true  if the event's version is earlier than one that introduced
                  the replication event checksum.
    @retval false otherwise.
  */
  bool is_version_before_checksum() const;
  /**
    This method populates the array server_version_split which is then used for
    lookups to find if the server which created this event has some known bug.
  */
  void calc_server_version_split();
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &info) override;
  void print_long_info(std::ostream &info) override;
#endif
  ~Format_description_event() override;

  bool header_is_valid() const {
    return ((common_header_len >= LOG_EVENT_MINIMAL_HEADER_LEN) &&
            (!post_header_len.empty()));
  }

  bool version_is_valid() const {
    /* It is invalid only when all version numbers are 0 */
    return server_version_split[0] != 0 || server_version_split[1] != 0 ||
           server_version_split[2] != 0;
  }
};

/**
  @class Stop_event

  A stop event is written to the log files under these circumstances:
  - A master writes the event to the binary log when it shuts down.
  - A slave writes the event to the relay log when it shuts down or
    when a RESET REPLICA statement is executed.

  @section Stop_event_binary_format Binary Format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/

class Stop_event : public Binary_log_event {
 public:
  /**
    It is the minimal constructor, and all it will do is set the type_code as
    STOP_EVENT in the header object in Binary_log_event.
  */
  Stop_event() : Binary_log_event(STOP_EVENT) {}

  /**
    A Stop_event is occurs under these circumstances:
    -  A master writes the event to the binary log when it shuts down
    -  A slave writes the event to the relay log when it shuts down or when a
       RESET REPLICA statement is executed
    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Stop_event(const char *buf, const Format_description_event *fde);

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &info) override;
#endif
};

/**
  @class Incident_event

   Class representing an incident, an occurrence out of the ordinary,
   that happened on the master.

   The event is used to inform the slave that something out of the
   ordinary happened on the master that might cause the database to be
   in an inconsistent state.

  @section Incident_event_binary_format Binary Format

   <table id="IncidentFormat">
   <caption>Incident event format</caption>
   <tr>
     <th>Symbol</th>
     <th>Format</th>
     <th>Description</th>
   </tr>
   <tr>
     <td>INCIDENT</td>
     <td align="right">2</td>
     <td>Incident number as an unsigned integer</td>
   </tr>
   <tr>
     <td>MSGLEN</td>
     <td align="right">1</td>
     <td>Message length as an unsigned integer</td>
   </tr>
   <tr>
     <td>MESSAGE</td>
     <td align="right">MSGLEN</td>
     <td>The message, if present. Not null terminated.</td>
   </tr>
   </table>

*/
class Incident_event : public Binary_log_event {
 public:
  /**
    Enumeration of the incidents that can occur for the server.
  */
  enum enum_incident {
    /** No incident */
    INCIDENT_NONE = 0,
    /** There are possibly lost events in the replication stream */
    INCIDENT_LOST_EVENTS = 1,
    /** Shall be last event of the enumeration */
    INCIDENT_COUNT
  };

  enum_incident get_incident_type() { return incident; }
  char *get_message() { return message; }

  /**
    This will create an Incident_event with an empty message and set the
    type_code as INCIDENT_EVENT in the header object in Binary_log_event.
  */
  explicit Incident_event(enum_incident incident_arg)
      : Binary_log_event(INCIDENT_EVENT),
        incident(incident_arg),
        message(nullptr),
        message_length(0) {}

  /**
    Constructor of Incident_event
    The buffer layout is as follows:
    <pre>
    +-----------------------------------------------------+
    | Incident_number | message_length | Incident_message |
    +-----------------------------------------------------+
    </pre>

    Incident number codes are listed in binlog_evnet.h.
    The only code currently used is INCIDENT_LOST_EVENTS, which indicates that
    there may be lost events (a "gap") in the replication stream that requires
    databases to be resynchronized.

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Incident_event(const char *buf, const Format_description_event *fde);
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &info) override;
  void print_long_info(std::ostream &info) override;
#endif
 protected:
  enum_incident incident;
  char *message;
  size_t message_length;
};

/**
  @class Xid_event

  An XID event is generated for a commit of a transaction that modifies one or
  more tables of an XA-capable storage engine.

  @section Xid_event_binary_format Binary Format

The Body has the following component:

  <table>
  <caption>Body for Xid_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>xid</td>
    <td>8 byte unsigned integer</td>
    <td>The XID transaction number.</td>
  </tr>
  </table>
  The Post-Header and Body for this event type are empty; it only has
  the common header.
*/
class Xid_event : public Binary_log_event {
 public:
  /**
    The minimal constructor of Xid_event, it initializes the instance variable
    xid and set the type_code as XID_EVENT in the header object in
    Binary_log_event
  */
  explicit Xid_event(uint64_t xid_arg)
      : Binary_log_event(XID_EVENT), xid(xid_arg) {}

  /**
    An XID event is generated for a commit of a transaction that modifies one or
    more tables of an XA-capable storage engine
    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Xid_event(const char *buf, const Format_description_event *fde);
  uint64_t xid;
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &info) override;
  void print_long_info(std::ostream &info) override;
#endif
};

/**
  @class XA_prepare_event

  An XA_prepare event is generated for a XA prepared transaction.
  Like Xid_event it contains XID of the *prepared* transaction.

  @section XA_prepare_event_binary_format Binary Format

The Body has the following component:

  <table>
  <caption>Body for XA_prepare_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>my_xid</td>
    <td>a struct similar to mysql/plugin.h containing three members.</td>
    <td>serialized XID representation of XA transaction.</td>
  </tr>

  <tr>
    <td>xid</td>
    <td>a pointer to XID object.</td>
    <td>a reference to an object for mysql logger.</td>
  </tr>

  <tr>
    <td>one_phase</td>
    <td>a bool</td>
    <td>the value specifies the current XA transaction commit method.</td>
  </tr>
  </table>
  The Post-Header and Body for this event type are empty; it only has
  the common header.
*/

class XA_prepare_event : public Binary_log_event {
  /*
    Struct def is copied from $MYSQL/include/mysql/plugin.h,
    consult there about fine details.
  */
  static const int MY_XIDDATASIZE = 128;

 public:
  struct MY_XID {
    long formatID;
    long gtrid_length;
    long bqual_length;
    char data[MY_XIDDATASIZE]; /* Not \0-terminated */
  };

 protected:
  /* size of serialization buffer is explained in $MYSQL/sql/xa.h. */
  static const uint16_t ser_buf_size =
      8 + 2 * MY_XIDDATASIZE + 4 * sizeof(long) + 1;
  MY_XID my_xid;
  void *xid; /* Master side only */
  bool one_phase;

 public:
  /**
    The minimal constructor of XA_prepare_event, it initializes the
    instance variable xid and set the type_code as XID_EVENT in the
    header object in Binary_log_event
  */
  XA_prepare_event(void *xid_arg, bool oph_arg)
      : Binary_log_event(XA_PREPARE_LOG_EVENT),
        xid(xid_arg),
        one_phase(oph_arg) {}

  /**
    An XID event is generated for a commit of a transaction that modifies one or
    more tables of an XA-capable storage engine
    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  XA_prepare_event(const char *buf, const Format_description_event *fde);
#ifndef HAVE_MYSYS
  /*
    todo: we need to find way how to exploit server's code of
    serialize_xid()
  */
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &) override {}
#endif
  /**
    Whether or not this `XA_prepare_event` represents an `XA COMMIT ONE
    PHASE`.

    @return true if it's an `XA COMMIT ONE PHASE`, false otherwise.
   */
  bool is_one_phase() const;
  /**
    Retrieves the content of `my_xid` member variable.

    @return The const-reference to the `my_xid` member variable.
   */
  MY_XID const &get_xid() const;
};

/**
  @class Ignorable_event

  Base class for ignorable log events. Events deriving from
  this class can be safely ignored by slaves that cannot
  recognize them. Newer slaves, will be able to read and
  handle them. This has been designed to be an open-ended
  architecture, so adding new derived events shall not harm
  the old slaves that support ignorable log event mechanism
  (they will just ignore unrecognized ignorable events).

  @note The only thing that makes an event ignorable is that it has
  the LOG_EVENT_IGNORABLE_F flag set.  It is not strictly necessary
  that ignorable event types derive from Ignorable_event; they may
  just as well derive from Binary_log_event and Log_event and pass
  LOG_EVENT_IGNORABLE_F as argument to the Log_event constructor.

  @section Ignoarble_event_binary_format Binary format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/
class Ignorable_event : public Binary_log_event {
 public:
  // buf is advanced in Binary_log_event constructor to point to beginning of
  // post-header

  /**
    The minimal constructor and all it will do is set the type_code as
    IGNORABLE_LOG_EVENT in the header object in Binary_log_event.
  */
  explicit Ignorable_event(Log_event_type type_arg = IGNORABLE_LOG_EVENT)
      : Binary_log_event(type_arg) {}
  /**
    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Ignorable_event(const char *buf, const Format_description_event *fde);
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &) override {}
#endif
};

/**
  @struct  gtid_info
  Structure to hold the members declared in the class Gtid_log_event those
  member are objects of classes defined in server(rpl_gtid.h). As we can not
  move all the classes defined there(in rpl_gtid.h) in libbinlogevents so this
  structure was created, to provide a way to map the decoded value in Gtid_event
  ctor and the class members defined in rpl_gtid.h, these classes are also the
  members of Gtid_log_event(subclass of this in server code)

  The structure contains the following components.
  <table>
  <caption>Structure gtid_info</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>rpl_gtid_sidno</td>
    <td>4 bytes integer</td>
    <td>SIDNO (source ID number, first component of GTID)</td>
  </tr>
  <tr>
    <td>rpl_gtid_gno</td>
    <td>8 bytes integer</td>
    <td>GNO (group number, second component of GTID)</td>
  </tr>
  </table>
*/
struct gtid_info {
  int32_t rpl_gtid_sidno;
  int64_t rpl_gtid_gno;
};

/// Event that encloses all the events of a transaction.
///
/// It is used for carrying compressed payloads, and contains
/// compression metadata.
class Transaction_payload_event : public Binary_log_event {
 public:
  using Buffer_sequence_view_t =
      mysql::binlog::event::compression::buffer::Buffer_sequence_view<>;

 private:
  Transaction_payload_event &operator=(const Transaction_payload_event &) =
      delete;
  Transaction_payload_event(const Transaction_payload_event &) = delete;

 protected:
  /// The compressed data, when entire payload is in one chunk.
  const char *m_payload{nullptr};

  /// The compressed data, when payload consists of a sequence of buffers
  Buffer_sequence_view_t *m_buffer_sequence_view;

  /// The size of the compressed data.
  uint64_t m_payload_size{0};

  /// The compression algorithm that was used.
  mysql::binlog::event::compression::type m_compression_type{
      mysql::binlog::event::compression::type::NONE};

  /// The uncompressed size of the data. This is the same as @c
  /// m_payload_size if the algorithms is NONE.
  uint64_t m_uncompressed_size{0};

 public:
  /// There are four fields: "compression type", "payload size",
  /// "uncompressed size", and "end mark".  Each of the three first
  /// fields is stored as a triple, where:
  /// - the first element is a type code,
  /// - the second element is a number containing the length of the
  ///   third element, and
  /// - the third element is the value.
  /// The last field, "end mark", is stored as only a type code.  All
  /// elements are stored in the "net_store_length" format.
  /// net_store_length stores 64 bit numbers in a variable length
  /// format, using 1 to 9 bytes depending on the magnitude of the
  /// value; 1 for values up to 250, longer for bigger values.
  ///
  /// So:
  /// - The first element in each triple is always length 1 since type
  ///   codes are small;
  /// - the second element in each triple is always length 1 since the
  ///   third field is at most 9 bytes;
  /// - the third field in each triple is:
  ///   - at most 1 for the "compression type" since type codes are small;
  ///   - at most 9 for the "payload size" and "uncompressed size".
  /// - the end mark is always 1 byte since it is a constant value
  ///   less than 250
  static constexpr size_t compression_type_max_length = 1 + 1 + 1;
  static constexpr size_t payload_size_max_length = 1 + 1 + 9;
  static constexpr size_t uncompressed_size_max_length = 1 + 1 + 9;
  static constexpr size_t end_mark_max_length = 1;

  /// The maximum size of the "payload data header".
  ///
  /// Any log event consists of the common-header (19 bytes, same
  /// format for all event types), followed by a post-header (size
  /// defined per event type; 0 for payload events), followed by data
  /// (variable length and defined by each event type).  For payload
  /// events, the data contains a payload data header (these 4
  /// fields), followed by the payload (compressed data).
  static constexpr size_t max_payload_data_header_length =
      compression_type_max_length + payload_size_max_length +
      uncompressed_size_max_length + end_mark_max_length;

  /// The maximum size of all headers, i.e., everything but the
  /// payload.
  ///
  /// This includes common-header, post-header, and payload
  /// data header.
  static constexpr size_t max_length_of_all_headers =
      LOG_EVENT_HEADER_LEN + Binary_log_event::TRANSACTION_PAYLOAD_HEADER_LEN +
      max_payload_data_header_length;
  /// The maximum length of the payload size, defined such that the total
  /// event size does not exceed max_log_event_size.
  static constexpr size_t max_payload_length =
      max_log_event_size - max_length_of_all_headers;

  /// Construct an object from the given fields.
  ///
  /// @param payload The (compressed) payload data.
  ///
  /// @param payload_size The size of @c payload in bytes.
  ///
  /// @param compression_type the compression type that was used to
  /// compress @c payload.
  ///
  /// @param uncompressed_size the size of the data when uncompressed.
  ///
  /// The function does not validate that the payload matches the
  /// metadata provided.
  Transaction_payload_event(const char *payload, uint64_t payload_size,
                            uint16_t compression_type,
                            uint64_t uncompressed_size);

  /// Decode the event from a buffer.
  ///
  /// @param buf The buffer to decode.
  ///
  /// @param fde The format description event used to decode the
  /// buffer.
  Transaction_payload_event(const char *buf,
                            const Format_description_event *fde);

  ~Transaction_payload_event() override;

  /// Set the compression type used for the enclosed payload.
  ///
  /// @note API clients must call either all or none of set_payload,
  /// set_payload_size, set_compression_type, and
  /// set_uncompressed_size.
  ///
  /// @param type the compression type.
  void set_compression_type(mysql::binlog::event::compression::type type) {
    m_compression_type = type;
  }

  /// @return the compression type.
  mysql::binlog::event::compression::type get_compression_type() const {
    return m_compression_type;
  }

  /// Set the (compressed) size of the payload in this event.
  ///
  /// @note API clients must call either all or none of set_payload,
  /// set_payload_size, set_compression_type, and
  /// set_uncompressed_size.
  ///
  /// @param size The compressed size of the payload.
  void set_payload_size(uint64_t size) { m_payload_size = size; }

  /// @return The payload size.
  uint64_t get_payload_size() const { return m_payload_size; }

  /// Set the uncompressed size of the payload.
  ///
  /// @note API clients must call either all or none of set_payload,
  /// set_payload_size, set_compression_type, and
  /// set_uncompressed_size.
  ///
  /// @param size The uncompressed size of the payload.
  void set_uncompressed_size(uint64_t size) { m_uncompressed_size = size; }

  /// Return the alleged uncompressed size according to the field
  /// stored in the event.
  ///
  /// This cannot be trusted; the actual size can only be computed by
  /// decompressing the event.
  uint64_t get_uncompressed_size() const { return m_uncompressed_size; }

  /// Set the (possibly compressed) payload for the event.
  ///
  /// The ownership and responsibility to destroy the data is
  /// transferred to the event.
  ///
  /// @note API clients must call either all or none of set_payload,
  /// set_payload_size, set_compression_type, and
  /// set_uncompressed_size.
  ///
  /// @param data The payload of the event.
  void set_payload(const char *data) { m_payload = data; }

  /// @return the payload of the event.
  const char *get_payload() const { return m_payload; }

  /// Set the (possibly compressed) payload for the event.
  ///
  /// The payload is given as a Buffer_sequence_view.  The ownership
  /// of the data remains with the caller; the caller must ensure that
  /// the iterators remain valid for as long as this event needs them.
  ///
  /// @note API clients must call either all or none of set_payload,
  /// set_payload_size, set_compression_type, and
  /// set_uncompressed_size.
  ///
  /// @param buffer_sequence_view Container holding the data.
  void set_payload(Buffer_sequence_view_t *buffer_sequence_view);

  /// @return a textual representation of this event.
  std::string to_string() const;

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &) override;
  void print_long_info(std::ostream &) override;
#endif
};

/**
  @class Gtid_event
  GTID stands for Global Transaction IDentifier
  It is composed of two parts:
    - TSID for Transaction Source Identifier, and
    - GNO for Group Number.
  The basic idea is to
     -  Associate an identifier, the Global Transaction IDentifier or GTID,
        to every transaction.
     -  When a transaction is copied to a slave, re-executed on the slave,
        and written to the slave's binary log, the GTID is preserved.
     -  When a  slave connects to a master, the slave uses GTIDs instead of
        (file, offset)

  @section Gtid_event_binary_format Binary Format

  The Body can have up to nine components:

  <table>
  <caption>Body for Gtid_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>GTID_FLAGS</td>
    <td>1 byte</td>
    <td>00000001 = Transaction may have changes logged with SBR.
        In 5.6, 5.7.0-5.7.18, and 8.0.0-8.0.1, this flag is always set.
        Starting in 5.7.19 and 8.0.2, this flag is cleared if the transaction
        only contains row events. It is set if any part of the transaction is
        written in statement format.</td>
  </tr>
  <tr>
    <td>SID</td>
    <td>16 byte sequence</td>
    <td>UUID representing the SID</td>
  </tr>
  <tr>
    <td>GNO</td>
    <td>8 byte integer</td>
    <td>Group number, second component of GTID.</td>
  </tr>
  <tr>
    <td>logical clock timestamp typecode</td>
    <td>1 byte integer</td>
    <td>The type of logical timestamp used in the logical clock fields.</td>
  </tr>
  <tr>
    <td>last_committed</td>
    <td>8 byte integer</td>
    <td>Store the transaction's commit parent sequence_number</td>
  </tr>
  <tr>
    <td>sequence_number</td>
    <td>8 byte integer</td>
    <td>The transaction's logical timestamp assigned at prepare phase</td>
  </tr>
  <tr>
    <td>immediate_commit_timestamp</td>
    <td>7 byte integer</td>
    <td>Timestamp of commit on the immediate master</td>
  </tr>
  <tr>
    <td>original_commit_timestamp</td>
    <td>7 byte integer</td>
    <td>Timestamp of commit on the originating master</td>
  </tr>
  <tr>
    <td>transaction_length</td>
    <td>1 to 9 byte integer // See net_length_size(ulonglong num)</td>
    <td>The packed transaction's length in bytes, including the Gtid</td>
  </tr>
  <tr>
    <td>immediate_server_version</td>
    <td>4 byte integer</td>
    <td>Server version of the immediate server</td>
  </tr>
  <tr>
    <td>original_server_version</td>
    <td>4 byte integer</td>
    <td>Version of the server where the transaction was originally executed</td>
  </tr>
  </table>

*/
class Gtid_event : public Binary_log_event,
                   public mysql::serialization::Serializable<Gtid_event> {
 public:
  /*
    The transaction's logical timestamps used for MTS: see
    Transaction_ctx::last_committed and
    Transaction_ctx::sequence_number for details.
    Note: Transaction_ctx is in the MySQL server code.
  */
  int64_t last_committed;
  int64_t sequence_number;
  /** GTID flags constants */
  unsigned const char FLAG_MAY_HAVE_SBR = 1;
  /** Transaction might have changes logged with SBR */
  bool may_have_sbr_stmts;
  /// GTID flags, used bits:
  /// - FLAG_MAY_HAVE_SBR (1st bit)
  unsigned char gtid_flags = 0;
  /// Timestamp when the transaction was committed on the originating source.
  uint64_t original_commit_timestamp;
  /// Timestamp when the transaction was committed on the nearest source.
  uint64_t immediate_commit_timestamp;
  /// Flag indicating whether this event contains commit timestamps
  bool has_commit_timestamps;
  /// The length of the transaction in bytes.
  uint64_t transaction_length;

 public:
  /**
    Ctor of Gtid_event

    The layout of the buffer is as follows
    <pre>
    +----------+---+---+-------+--------------+---------+----------+
    |gtid flags|SID|GNO|TS_TYPE|logical ts(:s)|commit ts|trx length|
    +----------+---+---+-------+------------------------+----------+
    </pre>
    TS_TYPE is from {G_COMMIT_TS2} singleton set of values
    Details on commit timestamps in Gtid_event(const char*...)

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */

  Gtid_event(const char *buf, const Format_description_event *fde);
  /**
    Constructor.
  */
  explicit Gtid_event(long long int last_committed_arg,
                      long long int sequence_number_arg,
                      bool may_have_sbr_stmts_arg,
                      unsigned long long int original_commit_timestamp_arg,
                      unsigned long long int immediate_commit_timestamp_arg,
                      uint32_t original_server_version_arg,
                      uint32_t immediate_server_version_arg)
      : Binary_log_event(GTID_LOG_EVENT),
        last_committed(last_committed_arg),
        sequence_number(sequence_number_arg),
        may_have_sbr_stmts(may_have_sbr_stmts_arg),
        original_commit_timestamp(original_commit_timestamp_arg),
        immediate_commit_timestamp(immediate_commit_timestamp_arg),
        transaction_length(0),
        original_server_version(original_server_version_arg),
        immediate_server_version(immediate_server_version_arg) {
    if (may_have_sbr_stmts_arg) {
      gtid_flags = FLAG_MAY_HAVE_SBR;
    }
  }
#ifndef HAVE_MYSYS
  // TODO(WL#7684): Implement the method print_event_info and print_long_info
  //               for all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &) override {}
#endif
  /*
    Commit group ticket consists of: 1st bit, used internally for
    synchronization purposes ("is in use"),  followed by 63 bits for
    the ticket value.
  */
  static constexpr int COMMIT_GROUP_TICKET_LENGTH = 8;
  /*
    Default value of commit_group_ticket, which means it is not
    being used.
  */
  static constexpr std::uint64_t kGroupTicketUnset = 0;

  using Field_missing_functor = mysql::serialization::Field_missing_functor;
  using Field_encode_predicate = mysql::serialization::Field_encode_predicate;
  using Uuid = mysql::gtid::Uuid;
  using Tag_plain = mysql::gtid::Tag_plain;

  /*
    Function defining how to deserialize GTID_TAGGED_LOG_EVENT
    For optional fields, we define what value to assign in case field is not
    present in the packet
  */
  decltype(auto) define_fields() {
    return std::make_tuple(
        mysql::serialization::define_field(gtid_flags),
        mysql::serialization::define_field_with_size<Uuid::BYTE_LENGTH>(
            tsid_parent_struct.get_uuid().bytes),
        mysql::serialization::define_field(gtid_info_struct.rpl_gtid_gno),
        mysql::serialization::define_field_with_size<
            mysql::gtid::tag_max_length>(
            tsid_parent_struct.get_tag_ref().get_data()),
        mysql::serialization::define_field(last_committed),
        mysql::serialization::define_field(sequence_number),
        mysql::serialization::define_field(immediate_commit_timestamp),
        mysql::serialization::define_field(
            original_commit_timestamp, Field_missing_functor([this]() -> auto {
              this->original_commit_timestamp =
                  this->immediate_commit_timestamp;
            })),
        mysql::serialization::define_field(transaction_length),
        mysql::serialization::define_field(immediate_server_version),
        mysql::serialization::define_field(
            original_server_version, Field_missing_functor([this]() -> auto {
              this->original_server_version = this->immediate_server_version;
            })),
        mysql::serialization::define_field(
            commit_group_ticket, Field_missing_functor([this]() -> auto {
              this->commit_group_ticket = Gtid_event::kGroupTicketUnset;
            })));
  }

  /*
    Function defining how to serialize GTID_TAGGED_LOG_EVENT
    For optional fields, we define function that will tell serializer whether
    or not to include fields in the packet. Although binlogevents do not define
    how to serialize an event, Gtid_event class defines functions that are
    used during event serialization (set transaction length related functions).
    We need to define how data is serialized in order to automatically calculate
    event size
  */
  decltype(auto) define_fields() const {
    return std::make_tuple(
        mysql::serialization::define_field(gtid_flags),
        mysql::serialization::define_field_with_size<Uuid::BYTE_LENGTH>(
            tsid_parent_struct.get_uuid().bytes),
        mysql::serialization::define_field(gtid_info_struct.rpl_gtid_gno),
        mysql::serialization::define_field_with_size<
            mysql::gtid::tag_max_length>(
            tsid_parent_struct.get_tag().get_data()),
        mysql::serialization::define_field(last_committed),
        mysql::serialization::define_field(sequence_number),
        mysql::serialization::define_field(immediate_commit_timestamp),
        mysql::serialization::define_field(
            original_commit_timestamp, Field_encode_predicate([this]() -> bool {
              return this->original_commit_timestamp !=
                     this->immediate_commit_timestamp;
            })),
        mysql::serialization::define_field(transaction_length),
        mysql::serialization::define_field(immediate_server_version),
        mysql::serialization::define_field(
            original_server_version, Field_encode_predicate([this]() -> bool {
              return this->original_server_version !=
                     this->immediate_server_version;
            })),
        mysql::serialization::define_field(
            commit_group_ticket, Field_encode_predicate([this]() -> bool {
              return this->commit_group_ticket != Gtid_event::kGroupTicketUnset;
            })));
  }

  /// @brief Function that reads GTID_TAGGED_LOG_EVENT event type from the
  /// given buffer
  /// @param buf Buffer to read from
  /// @param buf_size Number of bytes in the buffer
  void read_gtid_tagged_log_event(const char *buf, std::size_t buf_size);

  /// @brief Updates transaction length which was not yet considered
  void update_untagged_transaction_length();

  /// @brief Updated transaction length based on transaction length without
  /// event length
  /// @param[in] trx_len_without_event_len Transaction length without event body
  /// length
  void update_tagged_transaction_length(std::size_t trx_len_without_event_len);

 protected:
  static const int ENCODED_FLAG_LENGTH = 1;
  static const int ENCODED_SID_LENGTH = 16;  // Uuid::BYTE_LENGTH;
  static const int ENCODED_GNO_LENGTH = 8;
  /// Length of typecode for logical timestamps.
  static const int LOGICAL_TIMESTAMP_TYPECODE_LENGTH = 1;
  /// Length of two logical timestamps.
  static const int LOGICAL_TIMESTAMP_LENGTH = 16;
  // Type code used before the logical timestamps.
  static const int LOGICAL_TIMESTAMP_TYPECODE = 2;

  static const int IMMEDIATE_COMMIT_TIMESTAMP_LENGTH = 7;
  static const int ORIGINAL_COMMIT_TIMESTAMP_LENGTH = 7;
  // Length of two timestamps (from original/immediate masters)
  static const int FULL_COMMIT_TIMESTAMP_LENGTH =
      IMMEDIATE_COMMIT_TIMESTAMP_LENGTH + ORIGINAL_COMMIT_TIMESTAMP_LENGTH;
  // We use 7 bytes out of which 1 bit is used as a flag.
  static const int ENCODED_COMMIT_TIMESTAMP_LENGTH = 55;
  // Minimum and maximum lengths of transaction length field.
  static const int TRANSACTION_LENGTH_MIN_LENGTH = 1;
  static const int TRANSACTION_LENGTH_MAX_LENGTH = 9;
  /// Length of original_server_version
  static const int ORIGINAL_SERVER_VERSION_LENGTH = 4;
  /// Length of immediate_server_version
  static const int IMMEDIATE_SERVER_VERSION_LENGTH = 4;
  /// Length of original and immediate server version
  static const int FULL_SERVER_VERSION_LENGTH =
      ORIGINAL_SERVER_VERSION_LENGTH + IMMEDIATE_SERVER_VERSION_LENGTH;
  // We use 4 bytes out of which 1 bit is used as a flag.
  static const int ENCODED_SERVER_VERSION_LENGTH = 31;

  /* We have only original commit timestamp if both timestamps are equal. */
  int get_commit_timestamp_length() const {
    if (original_commit_timestamp != immediate_commit_timestamp)
      return FULL_COMMIT_TIMESTAMP_LENGTH;
    return ORIGINAL_COMMIT_TIMESTAMP_LENGTH;
  }

  /**
    We only store the immediate_server_version if both server versions are the
    same.
  */
  int get_server_version_length() const {
    if (original_server_version != immediate_server_version)
      return FULL_SERVER_VERSION_LENGTH;
    return IMMEDIATE_SERVER_VERSION_LENGTH;
  }

  gtid_info gtid_info_struct;
  mysql::gtid::Tsid tsid_parent_struct;

  /* Minimum GNO expected in a serialized GTID event */
  static const int64_t MIN_GNO = 1;
  /// One-past-the-max value of GNO
  static const std::int64_t GNO_END = INT64_MAX;

 public:
  virtual std::int64_t get_gno() const { return gtid_info_struct.rpl_gtid_gno; }
  mysql::gtid::Tsid get_tsid() const { return tsid_parent_struct; }
  /// Total length of post header
  static const int POST_HEADER_LENGTH =
      ENCODED_FLAG_LENGTH +               /* flags */
      ENCODED_SID_LENGTH +                /* SID length */
      ENCODED_GNO_LENGTH +                /* GNO length */
      LOGICAL_TIMESTAMP_TYPECODE_LENGTH + /* length of typecode */
      LOGICAL_TIMESTAMP_LENGTH;           /* length of two logical timestamps */

  using Write_archive_type = mysql::serialization::Write_archive_binary;
  using Read_archive_type = mysql::serialization::Read_archive_binary;
  using Encoder_type =
      mysql::serialization::Serializer_default<Write_archive_type>;
  using Decoder_type =
      mysql::serialization::Serializer_default<Read_archive_type>;

  Tag_plain generate_tag_specification() const {
    return Tag_plain(tsid_parent_struct.get_tag());
  }

  /// @brief Get maximum size of event
  /// @return Maximum size of the event in bytes
  static constexpr std::size_t get_max_event_length() {
    return LOG_EVENT_HEADER_LEN + get_max_payload_size();
  }

  /// @brief Get maximum size of event payload
  /// @return Maximum size of event payload (withouth log event header length)
  /// in bytes
  static constexpr std::size_t get_max_payload_size() {
    constexpr std::size_t max_tagged_length =
        Encoder_type::get_max_size<Gtid_event>();
    if constexpr (max_tagged_length > MAX_DATA_LENGTH + POST_HEADER_LENGTH) {
      return max_tagged_length;
    }
    return MAX_DATA_LENGTH + POST_HEADER_LENGTH;
  }

 private:
  /*
    We keep the commit timestamps in the body section because they can be of
    variable length.
    On the originating master, the event has only one timestamp as the two
    timestamps are equal. On every other server we have two timestamps.
  */
  static const int MAX_DATA_LENGTH =
      FULL_COMMIT_TIMESTAMP_LENGTH + TRANSACTION_LENGTH_MAX_LENGTH +
      FULL_SERVER_VERSION_LENGTH +
      COMMIT_GROUP_TICKET_LENGTH; /* 64-bit unsigned integer */

 public:
  /**
   Set the transaction length information.

    This function should be used when the full transaction length (including
    the Gtid event length) is known.

    @param transaction_length_arg The transaction length.
  */
  void set_trx_length(unsigned long long int transaction_length_arg) {
    transaction_length = transaction_length_arg;
  }

  unsigned long long get_trx_length() const { return transaction_length; }

  /** The version of the server where the transaction was originally executed */
  uint32_t original_server_version;
  /** The version of the immediate server */
  uint32_t immediate_server_version;

  /** Ticket number used to group sessions together during the BGC. */
  std::uint64_t commit_group_ticket{kGroupTicketUnset};

  /**
    Returns the length of the packed `commit_group_ticket` field. It may be
    8 bytes or 0 bytes, depending on whether or not the value is
    instantiated. This function may be used only for untagged GTID events

    @return The length of the packed `commit_group_ticket` field
  */
  int get_commit_group_ticket_length() const;

  /**
   Set the commit_group_ticket and update the transaction length if
   needed, that is, if the commit_group_ticket was not set already
   account it on the transaction size.

   @param value The commit_group_ticket value.
  */
  void set_commit_group_ticket_and_update_transaction_length(
      std::uint64_t value);

  /// @brief Checks whether this Gtid log event contains a tag
  /// @return True in case this event is tagged. False otherwise.
  bool is_tagged() const;
};

/**
  @class Previous_gtids_event

  @section Previous_gtids_event_binary_format Binary Format

  The Post-Header for this event type is empty.  The Body has two
  components:

  <table>
  <caption>Body for Previous_gtids_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>buf</td>
    <td>unsigned char array</td>
    <td>It contains the Gtids executed in the
        last binary log file.</td>
  </tr>

  <tr>
    <td>buf_size</td>
    <td>4 byte integer</td>
    <td>Size of the above buffer</td>
  </tr>
  </table>
*/
class Previous_gtids_event : public Binary_log_event {
 public:
  /**
    Decodes the gtid_executed in the last binlog file

    <pre>
    The buffer layout is as follows
    +--------------------------------------------+
    | Gtids executed in the last binary log file |
    +--------------------------------------------+
    </pre>
    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Previous_gtids_event(const char *buf, const Format_description_event *fde);
  /**
    This is the minimal constructor, and set the
    type_code as PREVIOUS_GTIDS_LOG_EVENT in the header object in
    Binary_log_event
  */
  Previous_gtids_event() : Binary_log_event(PREVIOUS_GTIDS_LOG_EVENT) {}
#ifndef HAVE_MYSYS
  // TODO(WL#7684): Implement the method print_event_info and print_long_info
  //               for all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &) override {}
#endif
 protected:
  size_t buf_size;
  const unsigned char *buf;
};

/**
  @class Transaction_context_event

  This class is used to combine the information of the ongoing transaction
  including the write set and other information of the thread executing the
  transaction.

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>thread_id</td>
    <td>4 byte integer</td>
    <td>The identifier for the thread executing the transaction.</td>
  </tr>

  <tr>
    <td>gtid_specified</td>
    <td>bool type variable</td>
    <td>Variable to identify whether the Gtid have been specified for the
  ongoing transaction or not.
    </td>
  </tr>

  <tr>
    <td>encoded_snapshot_version</td>
    <td>unsigned char array</td>
    <td>A gtid_set which is used to store the transaction set used for
        conflict detection.</td>
  </tr>

  <tr>
    <td>encoded_snapshot_version_length</td>
    <td>4 byte integer</td>
    <td>Length of the above char array.</td>
  </tr>

  <tr>
    <td>write_set</td>
    <td>variable length list to store the hash values. </td>
    <td>Used to store the hash values of the rows identifier for the rows
        which have changed in the ongoing transaction.
    </td>
  </tr>

  <tr>
    <td>read_set</td>
    <td>variable length list to store the read set values. Currently empty.
  </td> <td>Will be used to store the read set values of the current
  transaction.</td>
  </tr>

*/
class Transaction_context_event : public Binary_log_event {
 public:
  /**
    Decodes the transaction_context_log_event of the ongoing transaction.

    <pre>
    The buffer layout is as follows
    </pre>

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Transaction_context_event(const char *buf,
                            const Format_description_event *fde);

  Transaction_context_event(unsigned int thread_id_arg, bool is_gtid_specified)
      : Binary_log_event(TRANSACTION_CONTEXT_EVENT),
        thread_id(thread_id_arg),
        gtid_specified(is_gtid_specified) {}

  ~Transaction_context_event() override;

  static const char *read_data_set(const char *pos, uint32_t set_len,
                                   std::list<const char *> *set,
                                   uint32_t remaining_buffer);

  static void clear_set(std::list<const char *> *set);

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &) override {}
#endif

 protected:
  const char *server_uuid;
  uint32_t thread_id;
  bool gtid_specified;
  const unsigned char *encoded_snapshot_version;
  uint32_t encoded_snapshot_version_length;
  std::list<const char *> write_set;
  std::list<const char *> read_set;

  // The values mentioned on the next class constants is the offset where the
  // data that will be copied in the buffer.

  // 1 byte length.
  static const int ENCODED_SERVER_UUID_LEN_OFFSET = 0;
  // 4 bytes length.
  static const int ENCODED_THREAD_ID_OFFSET = 1;
  // 1 byte length.
  static const int ENCODED_GTID_SPECIFIED_OFFSET = 5;
  // 4 bytes length
  static const int ENCODED_SNAPSHOT_VERSION_LEN_OFFSET = 6;
  // 4 bytes length.
  static const int ENCODED_WRITE_SET_ITEMS_OFFSET = 10;
  // 4 bytes length.
  static const int ENCODED_READ_SET_ITEMS_OFFSET = 14;

  // The values mentioned on the next class's constants is the length of the
  // data that will be copied in the buffer.
  static const int ENCODED_READ_WRITE_SET_ITEM_LEN = 2;
  static const int ENCODED_SNAPSHOT_VERSION_LEN = 2;
};

/**
  @class View_change_event

  This class is used to add view change markers in the binary log when a
  member of the group enters or leaves the group.

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>view_id</td>
    <td>40 length character array</td>
    <td>This is used to store the view id value of the new view change when a
  node add or leaves the group.
    </td>
  </tr>

  <tr>
    <td>seq_number</td>
    <td>8 bytes integer</td>
    <td>Variable to identify the next sequence number to be allotted to the
  certified transaction.</td>
  </tr>

  <tr>
    <td>certification_info</td>
    <td>variable length map to store the certification data.</td>
    <td>Map to store the certification info ie. the hash of write_set and the
        snapshot sequence value.
    </td>
  </tr>

*/
class View_change_event : public Binary_log_event {
 public:
  /**
    Decodes the view_change_log_event generated in case a server enters or
    leaves the group.

    <pre>
    The buffer layout is as follows
    </pre>

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  View_change_event(const char *buf, const Format_description_event *fde);

  explicit View_change_event(const char *raw_view_id);

  ~View_change_event() override;

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &) override {}
  void print_long_info(std::ostream &) override {}
#endif

 protected:
  // The values mentioned on the next class constants is the offset where the
  // data that will be copied in the buffer.

  // 40 bytes length.
  static const int ENCODED_VIEW_ID_OFFSET = 0;
  // 8 bytes length.
  static const int ENCODED_SEQ_NUMBER_OFFSET = 40;
  // 4 bytes length.
  static const int ENCODED_CERT_INFO_SIZE_OFFSET = 48;

  /*
    The layout of the buffer is as follows
    +--------------------- -+-------------+----------+
    | View Id               | seq number  | map size |
    +-----------------------+-------------+----------+
   view id (40 bytes) + seq number (8 bytes) + map size (4 bytes)
   Sum of the length of the values at the above OFFSETS.
  */

  // The values mentioned on the next class constants is the length of the data
  // that will be copied in the buffer.

  // Field sizes on serialization
  static const int ENCODED_VIEW_ID_MAX_LEN = 40;
  static const int ENCODED_CERT_INFO_KEY_SIZE_LEN = 2;
  static const int ENCODED_CERT_INFO_VALUE_LEN = 4;

  char view_id[ENCODED_VIEW_ID_MAX_LEN];

  long long int seq_number;

  std::map<std::string, std::string> certification_info;
};

/**
  @class Heartbeat_event_v2

  Replication event to ensure to replica that source is alive.
  The event is originated by source's dump thread and sent straight to
  replica without being logged. Slave itself does not store it in relay log
  but rather uses a data for immediate checks and throws away the event.

  Two members of the class m_log_filename and m_log_position comprise
  @see the rpl_event_coordinates instance. The coordinates that a heartbeat
  instance carries correspond to the last event source has sent from
  its binlog.

  Also this event will be generated only for the source server with
  version > 8.0.26

  @section Heartbeat_event_v2_binary_format Binary Format

  The Body has one component:

  <table>
  <caption>Body for Heartbeat_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>m_log_filename</td>
    <td>String variable to store the binlog name</td>
    <td>Name of the current binlog being written to.</td>
  </tr>
  <tr>
    <td>m_log_pos</td>
    <td>8 byte unsigned integar</td>
    <td>Name of the current binlog being written to.</td>
  </tr>
  </table>
*/

class Heartbeat_event_v2 : public Binary_log_event {
 public:
  /**
    Sent by a source to a replica to let the replica know that the source is
    still alive. Events of this type do not appear in the binary or relay logs.
    They are generated on a source server by the thread that dumps events and
    sent straight to the replica without ever being written to the binary log.

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Heartbeat_event_v2(const char *buf, const Format_description_event *fde);

  /**
    Creates an empty heartbeat event.
   */
  Heartbeat_event_v2();

  virtual ~Heartbeat_event_v2() override = default;

  // Set the binlog filename
  void set_log_filename(const std::string name);
  // Set the position
  void set_log_position(uint64_t position);
  // Return the binlog filename
  const std::string get_log_filename() const;
  // Return the position
  uint64_t get_log_position() const;

  // Return the max length of an encoded packet.
  static uint64_t max_encoding_length();
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream &info) override;
  void print_long_info(std::ostream &info) override;
#endif
 protected:
  std::string m_log_filename{};
  uint64_t m_log_position{0};
};

/**
  @class Heartbeat_event

  Replication event to ensure to replica that source is alive.
  The event is originated by source's dump thread and sent straight to
  replica without being logged. Slave itself does not store it in relay log
  but rather uses a data for immediate checks and throws away the event.

  Two members of the class log_ident and Binary_log_event::log_pos comprise
  @see the rpl_event_coordinates instance. The coordinates that a heartbeat
  instance carries correspond to the last event source has sent from
  its binlog.

  @section Heartbeat_event_binary_format Binary Format

  The Body has one component:

  <table>
  <caption>Body for Heartbeat_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>log_ident</td>
    <td>variable length string without trailing zero, extending to the
    end of the event</td>
    <td>Name of the current binlog being written to.</td>
  </tr>
  </table>
*/
class Heartbeat_event : public Binary_log_event {
 public:
  /**
    Sent by a source to a replica to let the replica know that the source is
    still alive. Events of this type do not appear in the binary or relay logs.
    They are generated on a source server by the thread that dumps events and
    sent straight to the replica without ever being written to the binary log.

    @param buf  Contains the serialized event.
    @param fde  An FDE event (see Rotate_event constructor for more info).
  */
  Heartbeat_event(const char *buf, const Format_description_event *fde);

  // Return the file name
  const char *get_log_ident() { return log_ident; }
  // Return the length of file name
  unsigned int get_ident_len() { return ident_len; }

  ~Heartbeat_event() {
    if (log_ident) bapi_free(const_cast<char *>(log_ident));
  }

 protected:
  const char *log_ident;
  unsigned int ident_len; /** filename length */
};

}  // end namespace mysql::binlog::event

/// @}

#endif  // MYSQL_BINLOG_EVENT_CONTROL_EVENTS_H
