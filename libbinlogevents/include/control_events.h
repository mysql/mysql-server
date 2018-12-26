/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

  @file control_events.h

  @brief Contains the classes representing events operating in the replication
  stream properties. Each event is represented as a byte sequence with logical
  divisions as event header, event specific data and event footer. The header
  and footer are common to all the events and are represented as two different
  subclasses.
*/

#ifndef CONTROL_EVENT_INCLUDED
#define CONTROL_EVENT_INCLUDED

#include "binlog_event.h"
#include <list>
#include <map>
#include <vector>

namespace binary_log
{
/**
  @class Rotate_event

  When a binary log file exceeds a size limit, a ROTATE_EVENT is written
  at the end of the file that points to the next file in the squence.
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
class Rotate_event: public Binary_log_event
{
public:
  const char* new_log_ident;
  size_t ident_len;
  unsigned int flags;
  uint64_t pos;

  enum {
    /* Values taken by the flag member variable */
    DUP_NAME= 2, // if constructor should dup the string argument
    RELAY_LOG= 4 // rotate event for the relay log
  };

  enum {
    /* Rotate event post_header */
    R_POS_OFFSET= 0,
    R_IDENT_OFFSET= 8
  };

  /**
    This is the minimal constructor, it will set the type code as ROTATE_EVENT.
  */
  Rotate_event(const char* new_log_ident_arg, size_t ident_len_arg,
               unsigned int flags_arg, uint64_t pos_arg)
    : Binary_log_event(ROTATE_EVENT),
      new_log_ident(new_log_ident_arg),
      ident_len(ident_len_arg ? ident_len_arg : strlen(new_log_ident_arg)),
      flags(flags_arg), pos(pos_arg)
  {}

  /**
    <pre>
    The buffer layout is as follows:
    +-----------------------------------------------------------------------+
    | common_header | post_header | position of the first event | file name |
    +-----------------------------------------------------------------------+
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
  Rotate_event(const char* buf, unsigned int event_len,
               const Format_description_event *description_event);

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
#endif

  ~Rotate_event()
  {
    if (flags & DUP_NAME)
      bapi_free(const_cast<char*>(new_log_ident));
  }
};


/**
  @class Start_event_v3

  Start_event_v3 is the Start_event of binlog format 3 (MySQL 3.23 and
  4.x).

  @section Start_event_v3_binary_format Binary Format

  Format_description_event derives from Start_event_v3; it is
  the Start_event of binlog format 4 (MySQL 5.0), that is, the
  event that describes the other events' Common-Header/Post-Header
  lengths. This event is sent by MySQL 5.0 whenever it starts sending
  a new binlog if the requested position is >4 (otherwise if ==4 the
  event will be sent naturally).
  The Post-Header has four components:

  <table>
  <caption>Post-Header for Start_event_v3</caption>

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
    <td>dont_set_created</td>
    <td>type bool</td>
    <td>Set to 1 when you dont want to have created time in the log</td>
  </table>
*/

class Start_event_v3: public Binary_log_event
{
public:
/*
    If this event is at the start of the first binary log since server
    startup 'created' should be the timestamp when the event (and the
    binary log) was created.  In the other case (i.e. this event is at
    the start of a binary log created by FLUSH LOGS or automatic
    rotation), 'created' should be 0.  This "trick" is used by MySQL
    >=4.0.14 slaves to know whether they must drop stale temporary
    tables and whether they should abort unfinished transaction.

    Note that when 'created'!=0, it is always equal to the event's
    timestamp; indeed Start_event is written only in binlog.cc where
    the first constructor below is called, in which 'created' is set
    to 'when'.  So in fact 'created' is a useless variable. When it is
    0 we can read the actual value from timestamp ('when') and when it
    is non-zero we can read the same value from timestamp
    ('when'). Conclusion:
     - we use timestamp to print when the binlog was created.
     - we use 'created' only to know if this is a first binlog or not.
     In 3.23.57 we did not pay attention to this identity, so mysqlbinlog in
     3.23.57 does not print 'created the_date' if created was zero. This is now
     fixed.
  */
  time_t created;
  uint16_t binlog_version;
  char server_version[ST_SERVER_VER_LEN];
   /*
    We set this to 1 if we don't want to have the created time in the log,
    which is the case when we rollover to a new log.
  */
  bool dont_set_created;

protected:
  /**
    Empty ctor of Start_event_v3 called when we call the
    ctor of FDE which takes binlog_version and server_version as the parameter
  */
  explicit Start_event_v3(Log_event_type type_code= START_EVENT_V3);
public:
  /**
    This event occurs at the beginning of v1 or v3 binary log files.
    In MySQL 4.0 and 4.1, such events are written only to the first binary log
    file that mysqld creates after startup. Log files created subsequently
    (when someone issues a FLUSH LOGS statement or the current binary log file
    becomes too large) do not contain this event.

    <pre>
    The buffer layout for fixed part is as follows:
    +---------------------------------------------+
    | binlog_version | server_version | timestamp |
    +---------------------------------------------+
    </pre>

    @param buf                Contains the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */

  Start_event_v3(const char* buf, unsigned int event_len,
                 const Format_description_event* description_event);
#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info for
  //            all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
#endif
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
    <td>event_type_permutation</td>
    <td>const array of type 1 byte unsigned integer</td>
    <td>Provides mapping between the event types of
        some previous versions > 5.1 GA to current event_types</td>
  </tr>
    <tr>
    <td>number_of_event_types</td>
    <td>1 byte unsigned integer</td>
    <td>number of event types present in the server</td>
  </tr>
  </table>
*/
class Format_description_event: public virtual Start_event_v3
{
public:
  /**
   The size of the fixed header which _all_ events have
   (for binlogs written by this version, this is equal to
   LOG_EVENT_HEADER_LEN), except FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT
   (those have a header of size LOG_EVENT_MINIMAL_HEADER_LEN).
  */
  uint8_t common_header_len;
  /*
   The list of post-headers' lengths followed
   by the checksum alg decription byte
  */
  std::vector<uint8_t> post_header_len;
  unsigned char server_version_split[ST_SERVER_VER_SPLIT_LEN];
  /**
   In some previous version > 5.1 GA event types are assigned
   different event id numbers than in the present version, so we
   must map those id's to the our current event id's. This
   mapping is done using event_type_permutation
  */
  const uint8_t *event_type_permutation;

  /**
    Format_description_log_event 1st constructor.

    This constructor can be used to create the event to write to the binary log
    (when the server starts or when FLUSH LOGS), or to create artificial events
    to parse binlogs from MySQL 3.23 or 4.x.
    When in a client, only the 2nd use is possible.

    @param binlog_ver             the binlog version for which we want to build
                                  an event. Can be 1 (=MySQL 3.23), 3 (=4.0.x
                                  x>=2 and 4.1) or 4 (MySQL 5.0). Note that the
                                  old 4.0 (binlog version 2) is not supported;
                                  it should not be used for replication with
                                  5.0.
    @param server_ver             The MySQL server's version.
  */
  Format_description_event(uint8_t binlog_ver,
                           const char* server_ver);
  /**
    The layout of the event data part  in  Format_description_event
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
    @note The description_event passed to this constructor was created
          through another constructor of FDE class
  */
  Format_description_event(const char* buf, unsigned int event_len,
                           const Format_description_event *description_event);

  uint8_t number_of_event_types;
  unsigned long get_product_version() const;
  bool is_version_before_checksum() const;
  void calc_server_version_split();
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
#endif
  ~Format_description_event();
};

/**
  @class Stop_event

  A stop event is written to the log files under these circumstances:
  - A master writes the event to the binary log when it shuts down.
  - A slave writes the event to the relay log when it shuts down or
    when a RESET SLAVE statement is executed.

  @section Stop_event_binary_format Binary Format

  The Post-Header and Body for this event type are empty; it only has
  the Common-Header.
*/

class Stop_event: public Binary_log_event
{
public:
  /**
    It is the minimal constructor, and all it will do is set the type_code as
    STOP_EVENT in the header object in Binary_log_event.
  */
  Stop_event() : Binary_log_event(STOP_EVENT)
  {}
  //buf is advanced in Binary_log_event constructor to point to beginning of
  //post-header

  /**
    A Stop_event is occurs under these circumstances:
    -  A master writes the event to the binary log when it shuts down
    -  A slave writes the event to the relay log when it shuts down or when a
       RESET SLAVE statement is executed
    @param buf                Contains the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Stop_event(const char* buf,
             const Format_description_event *description_event)
    :Binary_log_event(&buf, description_event->binlog_version,
                      description_event->server_version)
  {}

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info);
#endif
};


/**
  @class Incident_event

   Class representing an incident, an occurance out of the ordinary,
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
class Incident_event: public Binary_log_event
{
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

  enum_incident get_incident_type()
  {
    return incident;
  }
  char* get_message()
  {
    return message;
  }


  /**
    This will create an Incident_event with an empty message and set the
    type_code as INCIDENT_EVENT in the header object in Binary_log_event.
  */
  explicit Incident_event(enum_incident incident_arg)
    : Binary_log_event(INCIDENT_EVENT),
      incident(incident_arg),
      message(NULL),
      message_length(0)
  {}

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
  Incident_event(const char *buf, unsigned int event_len,
                 const Format_description_event *description_event);
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
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
class Xid_event: public Binary_log_event
{
public:
  /**
    The minimal constructor of Xid_event, it initializes the instance variable
    xid and set the type_code as XID_EVENT in the header object in
    Binary_log_event
  */
  explicit Xid_event(uint64_t xid_arg)
    : Binary_log_event(XID_EVENT),
      xid(xid_arg)
  {
  }

  /**
    An XID event is generated for a commit of a transaction that modifies one or
    more tables of an XA-capable storage engine
    @param buf                Contains the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Xid_event(const char *buf, const Format_description_event *fde);
  uint64_t xid;
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info);
  void print_long_info(std::ostream& info);
#endif
};

/**
  @class XA_prepare_event

  An XA_prepare event is generated for a XA prepared transaction.
  Like Xid_event it contans XID of the *prepared* transaction.

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

class XA_prepare_event: public Binary_log_event
{
  /*
    Struct def is copied from $MYSQL/include/mysql/plugin.h,
    consult there about fine details.
  */
  static const int MY_XIDDATASIZE= 128;

  struct st_mysql_xid {
    long formatID;
    long gtrid_length;
    long bqual_length;
    char data[MY_XIDDATASIZE];  /* Not \0-terminated */
  };
  typedef struct st_mysql_xid MY_XID;

protected:
  /* size of serialization buffer is explained in $MYSQL/sql/xa.h. */
  static const uint16_t ser_buf_size=
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
    : Binary_log_event(XA_PREPARE_LOG_EVENT), xid(xid_arg), one_phase(oph_arg)
  {
  }

  /**
    An XID event is generated for a commit of a transaction that modifies one or
    more tables of an XA-capable storage engine
    @param buf    Contains the serialized event.
    @param fde    An FDE event, used to get the following information
                     -binlog_version
                     -server_version
                     -post_header_len
                     -common_header_len
                     The content of this object
                     depends on the binlog-version currently in use.
  */
  XA_prepare_event(const char *buf, const Format_description_event *fde);
#ifndef HAVE_MYSYS
  /*
    todo: we need to find way how to exploit server's code of
    serialize_xid()
  */
  void print_event_info(std::ostream& info) {};
  void print_long_info(std::ostream& info)  {};
#endif
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
class Ignorable_event: public Binary_log_event
{
public:
  //buf is advanced in Binary_log_event constructor to point to beginning of
  //post-header

  /**
    The minimal constructor and all it will do is set the type_code as
    IGNORABLE_LOG_EVENT in the header object in Binary_log_event.
  */
  explicit Ignorable_event(Log_event_type type_arg= IGNORABLE_LOG_EVENT)
    : Binary_log_event(type_arg)
  {}
  /*
   @param buf                Contains the serialized event.
   @param description_event  An FDE event, used to get the
                             following information
                             -binlog_version
                             -server_version
                             -post_header_len
                             -common_header_len
                             The content of this object
                             depends on the binlog-version currently in use.
  */
  Ignorable_event(const char *buf, const Format_description_event *descr_event);
#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
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
struct gtid_info
{
  int32_t  rpl_gtid_sidno;
  int64_t  rpl_gtid_gno;
};

/**
  @struct  Uuid

  This is a POD.  It has to be a POD because it is a member of
  Sid_map::Node which is stored in HASH in mysql-server code.
  The structure contains the following components.
  <table>
  <caption>Structure gtid_info</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>byte</th>
    <td>unsigned char array</th>
    <td>This stores the Uuid of the server on which transaction
        is originated</th>
  </tr>
*/

struct Uuid
{

   /// Set to all zeros.
  void clear() { memset(bytes, 0, BYTE_LENGTH); }
   /// Copies the given 16-byte data to this UUID.
  void copy_from(const unsigned char *data)
  {
    memcpy(bytes, data, BYTE_LENGTH);
  }
  /// Copies the given UUID object to this UUID.
  void copy_from(const Uuid &data) { copy_from((unsigned char *)data.bytes); }
  /// Copies the given UUID object to this UUID.
  void copy_to(unsigned char *data) const { memcpy(data, bytes,
                                           BYTE_LENGTH); }
  /// Returns true if this UUID is equal the given UUID.
  bool equals(const Uuid &other) const
  { return memcmp(bytes, other.bytes, BYTE_LENGTH) == 0; }
  /**
    Return true if parse() would return succeed, but don't actually
    store the result anywhere.
  */
  static bool is_valid(const char *string);

  /**
    Stores the UUID represented by a string on the form
    XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX in this object.

     @return  0   success.
             >0   failure
  */
  int parse(const char *string);
  /** The number of bytes in the data of a Uuid. */
  static const size_t BYTE_LENGTH= 16;
  /** The data for this Uuid. */
  unsigned char bytes[BYTE_LENGTH];
  /**
    Generates a 36+1 character long representation of this UUID object
    in the given string buffer.

    @retval 36 - the length of the resulting string.
  */
  size_t to_string(char *buf) const;
  /// Convert the given binary buffer to a UUID
  static size_t to_string(const unsigned char* bytes_arg, char *buf);
  void print() const
  {
    char buf[TEXT_LENGTH + 1];
    to_string(buf);
    printf("%s\n", buf);
  }
  /// The number of bytes in the textual representation of a Uuid.
  static const size_t TEXT_LENGTH= 36;
  /// The number of bits in the data of a Uuid.
  static const size_t BIT_LENGTH= 128;
  static const int NUMBER_OF_SECTIONS= 5;
  static const int bytes_per_section[NUMBER_OF_SECTIONS];
  static const int hex_to_byte[256];
};


/**
  @class Gtid_event
  GTID stands for Global Transaction IDentifier
  It is composed of two parts:
    - SID for Source Identifier, and
    - GNO for Group Number.
  The basic idea is to
     -  Associate an identifier, the Global Transaction IDentifier or GTID,
        to every transaction.
     -  When a transaction is copied to a slave, re-executed on the slave,
        and written to the slave's binary log, the GTID is preserved.
     -  When a  slave connects to a master, the slave uses GTIDs instead of
        (file, offset)

  @section Gtid_event_binary_format Binary Format

  The Body has seven components:

  <table>
  <caption>Body for Gtid_event</caption>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

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
    <td>ENCODED_SID_LENGTH</td>
    <td>4 bytes static const integer</td>
    <td>Length of SID in event encoding</td>
  </tr>
  <tr>
    <td>ENCODED_GNO_LENGTH</td>
    <td>4 bytes static const integer</td>
    <td>Length of GNO in event encoding.</td>
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
  </table>

*/
class Gtid_event: public Binary_log_event
{
public:
  /*
    The transaction's logical timestamps used for MTS: see
    Transaction_ctx::last_committed and
    Transaction_ctx::sequence_number for details.
    Note: Transaction_ctx is in the MySQL server code.
  */
  long long int last_committed;
  long long int sequence_number;
  /** GTID flags constants */
  unsigned static const char FLAG_MAY_HAVE_SBR= 1;
  /** Transaction might have changes logged with SBR */
  bool may_have_sbr_stmts;
  /**
    Ctor of Gtid_event

    The layout of the buffer is as follows
    +----------+-----------+-- --------+-------+--------------+
    |gtid flags|ENCODED SID|ENCODED GNO|TS_TYPE|logical ts(:s)|
    +----------+-----------+-----------+-------+--------------+
    TS_TYPE is from {G_COMMIT_TS2} singleton set of values

    @param buffer             Contains the serialized event.
    @param event_len          Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */

  Gtid_event(const char *buffer, uint32_t event_len,
             const Format_description_event *descr_event);
  /**
    Constructor.
  */
  explicit Gtid_event(long long int last_committed_arg,
                      long long int sequence_number_arg,
                      bool may_have_sbr_stmts_arg)
    : Binary_log_event(GTID_LOG_EVENT),
      last_committed(last_committed_arg),
      sequence_number(sequence_number_arg),
      may_have_sbr_stmts(may_have_sbr_stmts_arg)
  {}
#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info
  //               for all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
#endif
protected:
  static const int ENCODED_FLAG_LENGTH= 1;
  static const int ENCODED_SID_LENGTH= 16;// Uuid::BYTE_LENGTH;
  static const int ENCODED_GNO_LENGTH= 8;
  /// Length of typecode for logical timestamps.
  static const int LOGICAL_TIMESTAMP_TYPECODE_LENGTH= 1;
  /// Length of two logical timestamps.
  static const int LOGICAL_TIMESTAMP_LENGTH= 16;
  // Type code used before the logical timestamps.
  static const int LOGICAL_TIMESTAMP_TYPECODE= 2;
  gtid_info gtid_info_struct;
  Uuid Uuid_parent_struct;
public:
  /// Total length of post header
  static const int POST_HEADER_LENGTH=
    ENCODED_FLAG_LENGTH               +  /* flags */
    ENCODED_SID_LENGTH                +  /* SID length */
    ENCODED_GNO_LENGTH                +  /* GNO length */
    LOGICAL_TIMESTAMP_TYPECODE_LENGTH + /* length of typecode */
    LOGICAL_TIMESTAMP_LENGTH;           /* length of two logical timestamps */

  static const int MAX_EVENT_LENGTH=
    LOG_EVENT_HEADER_LEN + POST_HEADER_LENGTH;
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
class Previous_gtids_event : public Binary_log_event
{
public:

  /**
    Decodes the gtid_executed in the last binlog file

    <pre>
    The buffer layout is as follows
    +--------------------------------------------+
    | Gtids executed in the last binary log file |
    +--------------------------------------------+
    </pre>
    @param buffer             Contains the serialized event.
    @param event_len          Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Previous_gtids_event(const char *buf, unsigned int event_len,
                       const Format_description_event *descr_event);
  /**
    This is the minimal constructor, and set the
    type_code as PREVIOUS_GTIDS_LOG_EVENT in the header object in
    Binary_log_event
  */
  Previous_gtids_event()
    : Binary_log_event(PREVIOUS_GTIDS_LOG_EVENT)
  {}
#ifndef HAVE_MYSYS
  //TODO(WL#7684): Implement the method print_event_info and print_long_info
  //               for all the events supported  in  MySQL Binlog
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
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
    <td>Variable to identify whether the Gtid have been specified for the ongoing
        transaction or not.
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
    <td>variable length list to store the read set values. Currently empty. </td>
    <td>Will be used to store the read set values of the current transaction.</td>
  </tr>

*/
class Transaction_context_event : public Binary_log_event
{
public:
  /**
    Decodes the transaction_context_log_event of the ongoing transaction.

    <pre>
    The buffer layout is as follows
    </pre>

    @param buf                Contains the serialized event.
    @param event_len          Length of the serialized event.
    @param descr_event        An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Transaction_context_event(const char *buf, unsigned int event_len,
                            const Format_description_event *descr_event);

  Transaction_context_event(unsigned int thread_id_arg,
                            bool is_gtid_specified_arg)
    : Binary_log_event(TRANSACTION_CONTEXT_EVENT),
      thread_id(thread_id_arg), gtid_specified(is_gtid_specified_arg)
  {}

  virtual ~Transaction_context_event();

  static const char *read_data_set(const char *pos, uint32_t set_len,
                                   std::list<const char*> *set,
                                   uint32_t remaining_buffer);

  static void clear_set(std::list<const char*> *set);

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
#endif

protected:
  const char *server_uuid;
  uint32_t thread_id;
  bool gtid_specified;
  const unsigned char *encoded_snapshot_version;
  uint32_t encoded_snapshot_version_length;
  std::list<const char*> write_set;
  std::list<const char*> read_set;

  // The values mentioned on the next class constants is the offset where the
  // data that will be copied in the buffer.

  // 1 byte length.
  static const int ENCODED_SERVER_UUID_LEN_OFFSET= 0;
  // 4 bytes length.
  static const int ENCODED_THREAD_ID_OFFSET= 1;
  // 1 byte length.
  static const int ENCODED_GTID_SPECIFIED_OFFSET= 5;
  // 4 bytes length
  static const int ENCODED_SNAPSHOT_VERSION_LEN_OFFSET= 6;
  // 4 bytes length.
  static const int ENCODED_WRITE_SET_ITEMS_OFFSET= 10;
  // 4 bytes length.
  static const int ENCODED_READ_SET_ITEMS_OFFSET=  14;

  // The values mentioned on the next class's constants is the length of the
  // data that will be copied in the buffer.
  static const int ENCODED_READ_WRITE_SET_ITEM_LEN= 2;
  static const int ENCODED_SNAPSHOT_VERSION_LEN= 2;
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
    <td>This is used to store the view id value of the new view change when a node add or
        leaves the group.
    </td>
  </tr>

  <tr>
    <td>seq_number</td>
    <td>8 bytes integer</td>
    <td>Variable to identify the next sequence number to be alloted to the certified transaction.</td>
  </tr>

  <tr>
    <td>certification_info</td>
    <td>variable length map to store the certification data.</td>
    <td>Map to store the certification info ie. the hash of write_set and the
        snapshot sequence value.
    </td>
  </tr>

*/
class View_change_event : public Binary_log_event
{
public:
  /**
    Decodes the view_change_log_event generated incase a server enters or
    leaves the group.

    <pre>
    The buffer layout is as follows
    </pre>

    @param buf                Contains the serialized event.
    @param event_len          Length of the serialized event.
    @param descr_event        An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  View_change_event(const char *buf, unsigned int event_len,
                    const Format_description_event *descr_event);

  explicit View_change_event(char* raw_view_id);

  virtual ~View_change_event();

  static char *read_data_map(char *pos, uint32_t map_len,
                             std::map<std::string, std::string> *map,
                             uint32_t consumable);

#ifndef HAVE_MYSYS
  void print_event_info(std::ostream& info) { }
  void print_long_info(std::ostream& info) { }
#endif

protected:
  // The values mentioned on the next class constants is the offset where the
  // data that will be copied in the buffer.

  // 40 bytes length.
  static const int ENCODED_VIEW_ID_OFFSET= 0;
  // 8 bytes length.
  static const int ENCODED_SEQ_NUMBER_OFFSET= 40;
  // 4 bytes length.
  static const int ENCODED_CERT_INFO_SIZE_OFFSET= 48;


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

  //Field sizes on serialization
  static const int ENCODED_VIEW_ID_MAX_LEN= 40;
  static const int ENCODED_CERT_INFO_KEY_SIZE_LEN= 2;
  static const int ENCODED_CERT_INFO_VALUE_LEN= 4;

  char view_id[ENCODED_VIEW_ID_MAX_LEN];

  long long int seq_number;

  std::map<std::string, std::string> certification_info;
};


/**
  @class Heartbeat_event

  Replication event to ensure to slave that master is alive.
  The event is originated by master's dump thread and sent straight to
  slave without being logged. Slave itself does not store it in relay log
  but rather uses a data for immediate checks and throws away the event.

  Two members of the class log_ident and Binary_log_event::log_pos comprise
  @see the rpl_event_coordinates instance. The coordinates that a heartbeat
  instance carries correspond to the last event master has sent from
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
class Heartbeat_event: public Binary_log_event
{
public:

  /**
    Sent by a master to a slave to let the slave know that the master is
    still alive. Events of this type do not appear in the binary or relay logs.
    They are generated on a master server by the thread that dumps events and
    sent straight to the slave without ever being written to the binary log.

    @param buf                Contains the serialized event.
    @param event_len          Length of the serialized event.
    @param description_event  An FDE event, used to get the
                              following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
  Heartbeat_event(const char* buf, unsigned int event_len,
                  const Format_description_event *description_event);

  const char* get_log_ident() { return log_ident; }
  unsigned int get_ident_len() { return ident_len; }

protected:
  const char* log_ident;
  unsigned int ident_len;                      /** filename length */
};

} // end namespace binary_log
/**
  @} (end of group Replication)
*/
#endif	/* CONTROL_EVENTS_INCLUDED */
