/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/rpl_master.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <memory>
#include <unordered_map>
#include <utility>

#include "binary_log_types.h"
#include "m_ctype.h"
#include "m_string.h"  // strmake
#include "map_helpers.h"
#include "my_byteorder.h"
#include "my_command.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/mysql_mutex_bits.h"
#include "mysql/components/services/psi_mutex_bits.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_base.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_global_access
#include "sql/binlog.h"            // mysql_bin_log
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/item.h"
#include "sql/item_func.h"           // user_var_entry
#include "sql/log.h"                 // log_*()
#include "sql/mysqld.h"              // server_id
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"
#include "sql/rpl_binlog_sender.h"      // Binlog_sender
#include "sql/rpl_filter.h"             // binlog_filter
#include "sql/rpl_group_replication.h"  // is_group_replication_running
#include "sql/rpl_gtid.h"
#include "sql/rpl_handler.h"  // RUN_HOOK
#include "sql/sql_class.h"    // THD
#include "sql/sql_list.h"
#include "sql/system_variables.h"
#include "sql_string.h"
#include "thr_mutex.h"
#include "typelib.h"

int max_binlog_dump_events = 0;  // unlimited
bool opt_sporadic_binlog_dump_fail = 0;

malloc_unordered_map<uint32, unique_ptr_my_free<SLAVE_INFO>> slave_list{
    key_memory_SLAVE_INFO};
extern TYPELIB binlog_checksum_typelib;

#define get_object(p, obj, msg)                  \
  {                                              \
    uint len;                                    \
    if (p >= p_end) {                            \
      my_error(ER_MALFORMED_PACKET, MYF(0));     \
      return 1;                                  \
    }                                            \
    len = (uint)*p++;                            \
    if (p + len > p_end || len >= sizeof(obj)) { \
      errmsg = msg;                              \
      goto err;                                  \
    }                                            \
    strmake(obj, (char *)p, len);                \
    p += len;                                    \
  }

static mysql_mutex_t LOCK_slave_list;
static bool slave_list_inited = false;
#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_slave_list;

static PSI_mutex_info all_slave_list_mutexes[] = {
    {&key_LOCK_slave_list, "LOCK_slave_list", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static void init_all_slave_list_mutexes(void) {
  int count;

  count = static_cast<int>(array_elements(all_slave_list_mutexes));
  mysql_mutex_register("sql", all_slave_list_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

void init_slave_list() {
#ifdef HAVE_PSI_INTERFACE
  init_all_slave_list_mutexes();
#endif

  mysql_mutex_init(key_LOCK_slave_list, &LOCK_slave_list, MY_MUTEX_INIT_FAST);
  slave_list_inited = true;
}

void end_slave_list() {
  if (slave_list_inited) {
    mysql_mutex_destroy(&LOCK_slave_list);
    slave_list_inited = false;
  }
}

/**
  Register slave in 'slave_list' hash table.

  @return
    0	ok
  @return
    1	Error.   Error message sent to client
*/

int register_slave(THD *thd, uchar *packet, size_t packet_length) {
  int res;
  uchar *p = packet, *p_end = packet + packet_length;
  const char *errmsg = "Wrong parameters to function register_slave";

  if (check_access(thd, REPL_SLAVE_ACL, any_db, NULL, NULL, 0, 0)) return 1;

  unique_ptr_my_free<SLAVE_INFO> si((SLAVE_INFO *)my_malloc(
      key_memory_SLAVE_INFO, sizeof(SLAVE_INFO), MYF(MY_WME)));
  if (si == nullptr) return 1;

  /* 4 bytes for the server id */
  if (p + 4 > p_end) {
    my_error(ER_MALFORMED_PACKET, MYF(0));
    return 1;
  }

  thd->server_id = si->server_id = uint4korr(p);
  p += 4;
  get_object(p, si->host, "Failed to register slave: too long 'report-host'");
  get_object(p, si->user, "Failed to register slave: too long 'report-user'");
  get_object(p, si->password,
             "Failed to register slave; too long 'report-password'");
  if (p + 10 > p_end) goto err;
  si->port = uint2korr(p);
  p += 2;
  /*
     We need to by pass the bytes used in the fake rpl_recovery_rank
     variable. It was removed in patch for BUG#13963. But this would
     make a server with that patch unable to connect to an old master.
     See: BUG#49259
  */
  p += 4;
  if (!(si->master_id = uint4korr(p))) si->master_id = server_id;
  si->thd = thd;

  mysql_mutex_lock(&LOCK_slave_list);
  unregister_slave(thd, false, false /*need_lock_slave_list=false*/);
  res = !slave_list.emplace(si->server_id, std::move(si)).second;
  mysql_mutex_unlock(&LOCK_slave_list);
  return res;

err:
  my_message(ER_UNKNOWN_ERROR, errmsg, MYF(0)); /* purecov: inspected */
  return 1;
}

void unregister_slave(THD *thd, bool only_mine, bool need_lock_slave_list) {
  if (thd->server_id) {
    if (need_lock_slave_list)
      mysql_mutex_lock(&LOCK_slave_list);
    else
      mysql_mutex_assert_owner(&LOCK_slave_list);

    auto it = slave_list.find(thd->server_id);
    if (it != slave_list.end() && (!only_mine || it->second->thd == thd))
      slave_list.erase(it);

    if (need_lock_slave_list) mysql_mutex_unlock(&LOCK_slave_list);
  }
}

/**
  Execute a SHOW SLAVE HOSTS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval false success
  @retval true failure
*/
bool show_slave_hosts(THD *thd) {
  List<Item> field_list;
  Protocol *protocol = thd->get_protocol();
  DBUG_ENTER("show_slave_hosts");

  field_list.push_back(new Item_return_int("Server_id", 10, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Host", 20));
  if (opt_show_slave_auth_info) {
    field_list.push_back(new Item_empty_string("User", 20));
    field_list.push_back(new Item_empty_string("Password", 20));
  }
  field_list.push_back(new Item_return_int("Port", 7, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Master_id", 10, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Slave_UUID", UUID_LENGTH));

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(true);

  mysql_mutex_lock(&LOCK_slave_list);

  for (const auto &key_and_value : slave_list) {
    SLAVE_INFO *si = key_and_value.second.get();
    protocol->start_row();
    protocol->store((uint32)si->server_id);
    protocol->store(si->host, &my_charset_bin);
    if (opt_show_slave_auth_info) {
      protocol->store(si->user, &my_charset_bin);
      protocol->store(si->password, &my_charset_bin);
    }
    protocol->store((uint32)si->port);
    protocol->store((uint32)si->master_id);

    /* get slave's UUID */
    String slave_uuid;
    if (get_slave_uuid(si->thd, &slave_uuid))
      protocol->store(slave_uuid.c_ptr_safe(), &my_charset_bin);
    if (protocol->end_row()) {
      mysql_mutex_unlock(&LOCK_slave_list);
      DBUG_RETURN(true);
    }
  }
  mysql_mutex_unlock(&LOCK_slave_list);
  my_eof(thd);
  DBUG_RETURN(false);
}

/* clang-format off */
/**
  @page page_protocol_replication Replication Protocol

  Replication uses binlogs to ship changes done on the master to the slave
  and can be written to @ref sect_protocol_replication_binlog_file and sent
  over the network as @ref sect_protocol_replication_binlog_stream.

  @section sect_protocol_replication_binlog_file Binlog File

  Binlog files start with a @ref sect_protocol_replication_binlog_file_header
  followed by a series of @subpage page_protocol_replication_binlog_event

  @subsection sect_protocol_replication_binlog_file_header Binlog File Header

  A binlog file start with a `Binlog File Header [ 0xFE 'bin']`
  ~~~~~
  $ hexdump -C /tmp/binlog-test.log
  00000000  fe 62 69 6e 19 6f c9 4c  0f 01 00 00 00 66 00 00  |.bin.o.L.....f..|
  00000010  00 6a 00 00 00 00 00 04  00 6d 79 73 71 6c 2d 70  |.j.......mysql-p|
  00000020  72 6f 78 79 2d 30 2e 37  2e 30 00 00 00 00 00 00  |roxy-0.7.0......|
  ...
  ~~~~~

  @section sect_protocol_replication_binlog_stream Binlog Network Stream

  Network streams are requested with @subpage page_protocol_com_binlog_dump and
  prepend each @ref page_protocol_replication_binlog_event with `00` OK-byte.

  @section sect_protocol_replication_binlog_version Binlog Version

  Depending on the MySQL version that created the binlog the format is slightly
  different. Four versions are currently known:

  Binlog version | MySQL Version
  ---------------|-----------------------
  1              | MySQL 3.23 - < 4.0.0
  2              | MySQL 4.0.0 - 4.0.1
  3              | MySQL 4.0.2 - < 5.0.0
  4              | MySQL 5.0.0+

  @subsection sect_protocol_replication_binlog_version_v1 Version 1

  Supported @ref sect_protocol_replication_binlog_event_sbr

  @subsection sect_protocol_replication_binlog_version_v2 Version 2

  Can be ignored as it was only used in early alpha versinos of MySQL 4.1 and
  won't be documented here.

  @subsection sect_protocol_replication_binlog_version_v3 Version 3

  Added the relay logs and changed the meaning of the log position

  @subsection sect_protocol_replication_binlog_version_v4 Version 4

  Added the @ref sect_protocol_replication_event_format_desc and made the
  protocol extensible.

  In MySQL 5.1.x the @ref sect_protocol_replication_binlog_event_rbr were
  added.
*/

/**
  @page page_protocol_replication_binlog_event Binlog Event

  The events contain the actual data that should be shipped from the master to
  the slave. Depending on the use, different events are sent.

  @section sect_protocol_replication_binlog_event_mgmt Binlog Management

  The first event is either a @ref sect_protocol_replication_event_start_v3 or
  a @ref sect_protocol_replication_event_format_desc while the last event is
  either a @ref sect_protocol_replication_event_stop or
  @ref sect_protocol_replication_event_rotate.

  @subsection sect_protocol_replication_event_start_v3 START_EVENT_V3

  <table>
  <caption>Binlog::START_EVENT_V3:</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>binlog-version</td>
      <td>Version of the binlog format.
        See @ref sect_protocol_replication_binlog_version</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_fix "string[50]"</td>
    <td>mysql-server version</td>
    <td>version of the MySQL Server that created the binlog.
      The string is evaluted to apply work-arounds in the slave. </td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>create-timestamp</td>
      <td>seconds since Unix epoch when the binlog was created</td></tr>
  </table>

  @subsection sect_protocol_replication_event_format_desc FORMAT_DESCRIPTION_EVENT

  A format description event is the first event of a binlog for
  binlog @ref sect_protocol_replication_binlog_version_v4.
  It described how the other events are laid out.

  @note Added in MySQL 5.0.0 as a replacement for
  @ref sect_protocol_replication_event_start_v3.

  <table>
  <caption>Binlog::FORMAT_DESCRIPTION_EVENT:</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>binlog-version</td>
      <td>Version of the binlog format.
        See @ref sect_protocol_replication_binlog_version</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_fix "string[50]"</td>
    <td>mysql-server version</td>
    <td>version of the MySQL Server that created the binlog.
      The string is evaluted to apply work-arounds in the slave. </td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>create-timestamp</td>
      <td>seconds since Unix epoch when the binlog was created</td></tr>
  </table>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>event-header-length</td>
      <td>Length of the @ref sect_protocol_replication_binlog_event_header
        of next events. Should always be 19.</td></tr>
  </table>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "string&lt;EOF&gt;"</td>
      <td>event type header lengths</td>
      <td>a array indexed by `binlog-event-type - 1` to extract the length
        of the event specific header</td></tr>
  </table>

  @par Example
  ~~~~~~~~~~~~
  $ hexdump -v -s 4 -C relay-bin.000001
  00000004  82 2d c2 4b 0f 02 00 00  00 67 00 00 00 6b 00 00  |.-.K.....g...k..|
  00000014  00 00 00 04 00 35 2e 35  2e 32 2d 6d 32 00 00 00  |.....5.5.2-m2...|
  00000024  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
  00000034  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
  00000044  00 00 00 00 00 00 00 82  2d c2 4b 13 38 0d 00 08  |........-.K.8...|
  00000054  00 12 00 04 04 04 04 12  00 00 54 00 04 1a 08 00  |..........T.....|
  00000064  00 00 08 08 08 02 00                              |........        |
  ~~~~~~~~~~~~

  For mysql-5.5.2-m2 the event specific header lengths are:


  <table>
  <tr><th rowspan="2">Event Name</th><th colspan="3">Header Length</th></tr>
  <tr><th>v4</th><th>v3</th><th>v1</th></tr>
  <tr><td>@ref sect_protocol_replication_binlog_event_header</td>
    <td colspan="2">19</td><td>13</td></tr>
  <tr><td>@ref sect_protocol_replication_event_start_v3</td>
    <td colspan="3">56</td></tr>
  <tr><td>@ref sect_protocol_replication_event_query</td>
    <td>13</td><td colspan="2">11</td></tr>
  <tr><td>@ref sect_protocol_replication_event_stop</td>
    <td colspan="3">0</td></tr>
  <tr><td>@ref sect_protocol_replication_event_rotate</td>
    <td colspan="2">8</td><td>0</td></tr>
  <tr><td>@ref sect_protocol_replication_event_intvar</td>
    <td colspan="3">0</td></tr>
  <tr><td>@ref sect_protocol_replication_event_load</td>
    <td colspan="3">18</td></tr>
  <tr><td>@ref sect_protocol_replication_event_slave</td>
    <td colspan="3">0</td></tr>
  <tr><td>@ref sect_protocol_replication_event_create_file</td>
    <td colspan="3">4</td></tr>
  <tr><td>@ref sect_protocol_replication_event_append_block</td>
    <td colspan="3">4</td></tr>
  <tr><td>@ref sect_protocol_replication_event_exec_load</td>
    <td colspan="3">4</td></tr>
  <tr><td>@ref sect_protocol_replication_event_delete_file</td>
    <td colspan="3">4</td></tr>
  <tr><td>@ref sect_protocol_replication_event_new_load</td>
    <td colspan="3">18</td></tr>
  <tr><td>@ref sect_protocol_replication_event_rand</td>
    <td colspan="3">0</td></tr>
  <tr><td>@ref sect_protocol_replication_event_uservar</td>
    <td colspan="3">0</td></tr>
  <tr><td>@ref sect_protocol_replication_event_format_desc</td>
    <td>84</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_xid</td>
    <td>0</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_load_query_begin</td>
    <td>4</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_load_query_execute</td>
    <td>26</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_table_map</td>
    <td>8</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_delete_rows_v0</td>
    <td>0</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_update_rows_v0</td>
    <td>0</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_write_rows_v0</td>
    <td>0</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_delete_rows_v1</td>
    <td>8/6</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_update_rows_v1</td>
    <td>8/6</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_write_rows_v1</td>
    <td>8/6</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_incident</td>
    <td>2</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_heartbeat</td>
    <td>0</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_delete_rows_v2</td>
    <td>10</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_update_rows_v2</td>
    <td>10</td><td colspan="2">---</td></tr>
  <tr><td>@ref sect_protocol_replication_event_write_rows_v2</td>
    <td>10</td><td colspan="2">---</td></tr>
  </table>

  The `event-size` of `0x67` (`103`) minus the `event-header` length of
  `0x13` (`19`) should match the event type header length of the
  @ref sect_protocol_replication_event_format_desc `0x54` (`84`).

  The number of events understood by the master may differ from what the slave
  supports. It is calculated by:

  ~~~~~~~
  event_size - event_header_length - 2 - 50 - 4 - 1
  ~~~~~~~

  For mysql-5.5.2-m2 it is `0x1b` (`27`).

  @subsection sect_protocol_replication_event_stop STOP_EVENT
  @subsection sect_protocol_replication_event_rotate ROTATE_EVENT
  @subsection sect_protocol_replication_event_slave SLAVE_EVENT
  @subsection sect_protocol_replication_event_incident INCIDENT_EVENT
  @subsection sect_protocol_replication_event_heartbeat HEARTBEAT_EVENT

  @section sect_protocol_replication_binlog_event_sbr Statement Based Replication Events

  Statement Based Replication or SBR sends the SQL queries a client sent to
  the master AS IS to the slave. It needs extra events to mimic the client
  connection's state on the slave side.

  @subsection sect_protocol_replication_event_query QUERY_EVENT
  @subsection sect_protocol_replication_event_intvar INTVAR_EVENT
  @subsection sect_protocol_replication_event_rand RAND_EVENT
  @subsection sect_protocol_replication_event_uservar USER_VAR_EVENT
  @subsection sect_protocol_replication_event_xid XID_EVENT

  @section sect_protocol_replication_binlog_event_rbr Row Based Replication Events

  In Row Based replication the changed rows are sent to the slave which removes
  side-effects and makes it more reliable. Now all statements can be sent with
  RBR though. Most of the time you will see RBR and SBR side by side.

  @subsection sect_protocol_replication_event_table_map TABLE_MAP_EVENT
  @subsection sect_protocol_replication_event_delete_rows_v0 DELETE_ROWS_EVENTv0
  @subsection sect_protocol_replication_event_update_rows_v0 UPDATE_ROWS_EVENTv0
  @subsection sect_protocol_replication_event_write_rows_v0 WRITE_ROWS_EVENTv0
  @subsection sect_protocol_replication_event_delete_rows_v1 DELETE_ROWS_EVENTv1
  @subsection sect_protocol_replication_event_update_rows_v1 UPDATE_ROWS_EVENTv1
  @subsection sect_protocol_replication_event_write_rows_v1 WRITE_ROWS_EVENTv1
  @subsection sect_protocol_replication_event_delete_rows_v2 DELETE_ROWS_EVENTv2
  @subsection sect_protocol_replication_event_update_rows_v2 UPDATE_ROWS_EVENTv2
  @subsection sect_protocol_replication_event_write_rows_v2 WRITE_ROWS_EVENTv2

  @section sect_protocol_replication_binlog_event_load_file LOAD INFILE replication

  `LOAD DATA|XML INFILE` is a special SQL statement as it has to ship the files
  over to the slave too to execute the statement.

  @subsection sect_protocol_replication_event_load LOAD_EVENT
  @subsection sect_protocol_replication_event_create_file CREATE_FILE_EVENT
  @subsection sect_protocol_replication_event_append_block APPEND_BLOCK_EVENT
  @subsection sect_protocol_replication_event_exec_load EXEC_LOAD_EVENT
  @subsection sect_protocol_replication_event_delete_file DELETE_FILE_EVENT
  @subsection sect_protocol_replication_event_new_load NEW_LOAD_EVENT
  @subsection sect_protocol_replication_event_load_query_begin BEGIN_LOAD_QUERY_EVENT
  @subsection sect_protocol_replication_event_load_query_execute EXECUTE_LOAD_QUERY_EVENT

  A binlog event starts with @ref sect_protocol_replication_binlog_event_header
  and is followed by a event specific part.

  @section sect_protocol_replication_binlog_event_header Binlog Event Header

  The binlog event header starts each event and is either 13 or 19 bytes long, depending
  on the @ref sect_protocol_replication_binlog_version

  <table>
  <caption>Binlog::EventHeader:</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>timestamp</td>
      <td>seconds since unix epoch</td></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>event_type</td>
      <td>See binary_log::Log_event_type</td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>server-id</td>
      <td>server-id of the originating mysql-server. Used to filter out events
        in circular replication</td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>event-size</td>
      <td>size of the event (header, post-header, body)</td></tr>
  <tr><td colspan="3">if binlog-version > 1 {</td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>log-pos</td>
      <td>position of the next event</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>flags</td>
      <td>See @ref group_cs_binglog_event_header_flags</td></tr>
  </table>
*/


/**
  @page page_protocol_com_binlog_dump COM_BINLOG_DUMP

  @brief Request a @ref sect_protocol_replication_binlog_stream from the server

  @return @ref sect_protocol_replication_binlog_stream on success or
    @ref page_protocol_basic_err_packet on error

  <table>
  <caption>Payload</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>status</td>
      <td>[0x12] COM_BINLOG_DUMP</td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>binlog-pos</td>
      <td>position in the binlog-file to start the stream with</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>flags</td>
      <td>can right now has one possible value:
          ::BINLOG_DUMP_NON_BLOCK</td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>server-id</td>
      <td>Server id of this slave</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "string&lt;EOF&gt;"</td>
      <td>binlog-filename</td>
      <td>filename of the binlog on the master</td></tr>
  </table>

  @sa com_binlog_dump
*/
/* clang-format on */

/**
  If there are less than BYTES bytes left to read in the packet,
  report error.
*/
#define CHECK_PACKET_SIZE(BYTES)                                \
  do {                                                          \
    if (packet_bytes_todo < BYTES) goto error_malformed_packet; \
  } while (0)

/**
  Auxiliary macro used to define READ_INT and READ_STRING.

  Check that there are at least BYTES more bytes to read, then read
  the bytes using the given DECODER, then advance the reading
  position.
*/
#define READ(DECODE, BYTES)     \
  do {                          \
    CHECK_PACKET_SIZE(BYTES);   \
    DECODE;                     \
    packet_position += BYTES;   \
    packet_bytes_todo -= BYTES; \
  } while (0)

/**
  Check that there are at least BYTES more bytes to read, then read
  the bytes and decode them into the given integer VAR, then advance
  the reading position.
*/
#define READ_INT(VAR, BYTES) \
  READ(VAR = uint##BYTES##korr(packet_position), BYTES)

/**
  Check that there are at least BYTES more bytes to read and that
  BYTES+1 is not greater than BUFFER_SIZE, then read the bytes into
  the given variable VAR, then advance the reading position.
*/
#define READ_STRING(VAR, BYTES, BUFFER_SIZE)               \
  do {                                                     \
    if (BUFFER_SIZE <= BYTES) goto error_malformed_packet; \
    READ(memcpy(VAR, packet_position, BYTES), BYTES);      \
    VAR[BYTES] = '\0';                                     \
  } while (0)

bool com_binlog_dump(THD *thd, char *packet, size_t packet_length) {
  DBUG_ENTER("com_binlog_dump");
  ulong pos;
  ushort flags = 0;
  const uchar *packet_position = (uchar *)packet;
  size_t packet_bytes_todo = packet_length;

  thd->status_var.com_other++;
  thd->enable_slow_log = opt_log_slow_admin_statements;
  if (check_global_access(thd, REPL_SLAVE_ACL)) DBUG_RETURN(false);

  /*
    4 bytes is too little, but changing the protocol would break
    compatibility.  This has been fixed in the new protocol. @see
    com_binlog_dump_gtid().
  */
  READ_INT(pos, 4);
  READ_INT(flags, 2);
  READ_INT(thd->server_id, 4);

  DBUG_PRINT("info",
             ("pos=%lu flags=%d server_id=%d", pos, flags, thd->server_id));

  kill_zombie_dump_threads(thd);

  query_logger.general_log_print(thd, thd->get_command(), "Log: '%s'  Pos: %ld",
                                 packet + 10, (long)pos);
  mysql_binlog_send(thd, thd->mem_strdup(packet + 10), (my_off_t)pos, NULL,
                    flags);

  unregister_slave(thd, true, true /*need_lock_slave_list=true*/);
  /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
  DBUG_RETURN(true);

error_malformed_packet:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  DBUG_RETURN(true);
}

bool com_binlog_dump_gtid(THD *thd, char *packet, size_t packet_length) {
  DBUG_ENTER("com_binlog_dump_gtid");
  /*
    Before going GA, we need to make this protocol extensible without
    breaking compatitibilty. /Alfranio.
  */
  ushort flags = 0;
  uint32 data_size = 0;
  uint64 pos = 0;
  char name[FN_REFLEN + 1];
  uint32 name_size = 0;
  char *gtid_string = NULL;
  const uchar *packet_position = (uchar *)packet;
  size_t packet_bytes_todo = packet_length;
  Sid_map sid_map(
      NULL /*no sid_lock because this is a completely local object*/);
  Gtid_set slave_gtid_executed(&sid_map);

  thd->status_var.com_other++;
  thd->enable_slow_log = opt_log_slow_admin_statements;
  if (check_global_access(thd, REPL_SLAVE_ACL)) DBUG_RETURN(false);

  READ_INT(flags, 2);
  READ_INT(thd->server_id, 4);
  READ_INT(name_size, 4);
  READ_STRING(name, name_size, sizeof(name));
  READ_INT(pos, 8);
  DBUG_PRINT("info",
             ("pos=%llu flags=%d server_id=%d", pos, flags, thd->server_id));
  READ_INT(data_size, 4);
  CHECK_PACKET_SIZE(data_size);
  if (slave_gtid_executed.add_gtid_encoding(packet_position, data_size) !=
      RETURN_STATUS_OK)
    DBUG_RETURN(true);
  slave_gtid_executed.to_string(&gtid_string);
  DBUG_PRINT("info", ("Slave %d requested to read %s at position %llu gtid set "
                      "'%s'.",
                      thd->server_id, name, pos, gtid_string));

  kill_zombie_dump_threads(thd);
  query_logger.general_log_print(thd, thd->get_command(),
                                 "Log: '%s' Pos: %llu GTIDs: '%s'", name, pos,
                                 gtid_string);
  my_free(gtid_string);
  mysql_binlog_send(thd, name, (my_off_t)pos, &slave_gtid_executed, flags);

  unregister_slave(thd, true, true /*need_lock_slave_list=true*/);
  /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
  DBUG_RETURN(true);

error_malformed_packet:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  DBUG_RETURN(true);
}

void mysql_binlog_send(THD *thd, char *log_ident, my_off_t pos,
                       Gtid_set *slave_gtid_executed, uint32 flags) {
  Binlog_sender sender(thd, log_ident, pos, slave_gtid_executed, flags);

  sender.run();
}

/**
  An auxiliary function extracts slave UUID.

  @param[in]    thd  THD to access a user variable
  @param[out]   value String to return UUID value.

  @return       if success value is returned else NULL is returned.
*/
String *get_slave_uuid(THD *thd, String *value) {
  if (value == NULL) return NULL;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  const auto it = thd->user_vars.find("slave_uuid");
  if (it != thd->user_vars.end() && it->second->length() > 0) {
    value->copy(it->second->ptr(), it->second->length(), NULL);
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return value;
  }

  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return NULL;
}

/**
  Callback function used by kill_zombie_dump_threads() function to
  to find zombie dump thread from the thd list.

  @note It acquires LOCK_thd_data mutex when it finds matching thd.
  It is the responsibility of the caller to release this mutex.
*/
class Find_zombie_dump_thread : public Find_THD_Impl {
 public:
  Find_zombie_dump_thread(String value) : m_slave_uuid(value) {}
  virtual bool operator()(THD *thd) {
    THD *cur_thd = current_thd;
    if (thd != cur_thd && (thd->get_command() == COM_BINLOG_DUMP ||
                           thd->get_command() == COM_BINLOG_DUMP_GTID)) {
      String tmp_uuid;
      bool is_zombie_thread = false;
      get_slave_uuid(thd, &tmp_uuid);
      if (m_slave_uuid.length()) {
        is_zombie_thread =
            (tmp_uuid.length() &&
             !strncmp(m_slave_uuid.c_ptr(), tmp_uuid.c_ptr(), UUID_LENGTH));
      } else {
        /*
          Check if it is a 5.5 slave's dump thread i.e., server_id should be
          same && dump thread should not contain 'UUID'.
        */
        is_zombie_thread =
            ((thd->server_id == cur_thd->server_id) && !tmp_uuid.length());
      }
      if (is_zombie_thread) {
        mysql_mutex_lock(&thd->LOCK_thd_data);
        return true;
      }
    }
    return false;
  }

 private:
  String m_slave_uuid;
};

/*

  Kill all Binlog_dump threads which previously talked to the same slave
  ("same" means with the same UUID(for slave versions >= 5.6) or same server id
  (for slave versions < 5.6). Indeed, if the slave stops, if the
  Binlog_dump thread is waiting (mysql_cond_wait) for binlog update, then it
  will keep existing until a query is written to the binlog. If the master is
  idle, then this could last long, and if the slave reconnects, we could have 2
  Binlog_dump threads in SHOW PROCESSLIST, until a query is written to the
  binlog. To avoid this, when the slave reconnects and sends COM_BINLOG_DUMP,
  the master kills any existing thread with the slave's UUID/server id (if this
  id is not zero; it will be true for real slaves, but false for mysqlbinlog
  when it sends COM_BINLOG_DUMP to get a remote binlog dump).

  SYNOPSIS
    kill_zombie_dump_threads()
    @param thd newly connected dump thread object

*/

void kill_zombie_dump_threads(THD *thd) {
  String slave_uuid;
  get_slave_uuid(thd, &slave_uuid);
  if (slave_uuid.length() == 0 && thd->server_id == 0) return;

  Find_zombie_dump_thread find_zombie_dump_thread(slave_uuid);
  THD *tmp =
      Global_THD_manager::get_instance()->find_thd(&find_zombie_dump_thread);
  if (tmp) {
    /*
      Here we do not call kill_one_thread() as
      it will be slow because it will iterate through the list
      again. We just to do kill the thread ourselves.
    */
    if (log_error_verbosity > 2) {
      if (slave_uuid.length()) {
        LogErr(INFORMATION_LEVEL, ER_RPL_ZOMBIE_ENCOUNTERED, "UUID",
               slave_uuid.c_ptr(), "UUID", tmp->thread_id());
      } else {
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), "%u", thd->server_id);
        LogErr(INFORMATION_LEVEL, ER_RPL_ZOMBIE_ENCOUNTERED, "server_id",
               numbuf, "server_id", tmp->thread_id());
      }
    }
    tmp->duplicate_slave_id = true;
    tmp->awake(THD::KILL_QUERY);
    mysql_mutex_unlock(&tmp->LOCK_thd_data);
  }
}

/**
  Execute a RESET MASTER statement.

  @param thd Pointer to THD object of the client thread executing the
  statement.
  @param unlock_global_read_lock Unlock the global read lock aquired
  by RESET MASTER.
  @retval false success
  @retval true error
*/
bool reset_master(THD *thd, bool unlock_global_read_lock) {
  bool ret = false;

  /*
    RESET MASTER command should ignore 'read-only' and 'super_read_only'
    options so that it can update 'mysql.gtid_executed' replication repository
    table.

    Please note that skip_readonly_check flag should be set even when binary log
    is not enabled, as RESET MASTER command will clear 'gtid_executed' table.
  */
  thd->set_skip_readonly_check();
  if (is_group_replication_running()) {
    my_error(ER_CANT_RESET_MASTER, MYF(0), "Group Replication is running");
    ret = true;
    goto end;
  }

  if (mysql_bin_log.is_open()) {
    /*
      mysql_bin_log.reset_logs will delete the binary logs *and* clear
      gtid_state.  It is important to do both these operations from
      within reset_logs, since the operations can then use the same
      lock.  I.e., if we would remove the call to gtid_state->clear
      from reset_logs and call gtid_state->clear explicitly from this
      function instead, it would be possible for a concurrent thread
      to commit between the point where the binary log was removed and
      the point where the gtid_executed table is cleared. This would
      lead to an inconsistent state.
    */
    ret = mysql_bin_log.reset_logs(thd);
  } else {
    global_sid_lock->wrlock();
    ret = (gtid_state->clear(thd) != 0);
    global_sid_lock->unlock();
  }

end:
  /*
    Unlock the global read lock (which was aquired by this
    session as part of RESET MASTER) before running the hook
    which informs plugins.
  */
  if (unlock_global_read_lock) {
    DBUG_ASSERT(thd->global_read_lock.is_acquired());
    thd->global_read_lock.unlock_global_read_lock(thd);
  }

  /*
    Only run after_reset_master hook, when all reset operations preceding this
    have succeeded.
  */
  if (!ret)
    (void)RUN_HOOK(binlog_transmit, after_reset_master, (thd, 0 /* flags */));
  return ret;
}

/**
  Execute a SHOW MASTER STATUS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval false success
  @retval true failure
*/
bool show_master_status(THD *thd) {
  Protocol *protocol = thd->get_protocol();
  char *gtid_set_buffer = NULL;
  int gtid_set_size = 0;
  List<Item> field_list;

  DBUG_ENTER("show_binlog_info");

  global_sid_lock->wrlock();
  const Gtid_set *gtid_set = gtid_state->get_executed_gtids();
  if ((gtid_set_size = gtid_set->to_string(&gtid_set_buffer)) < 0) {
    global_sid_lock->unlock();
    my_eof(thd);
    my_free(gtid_set_buffer);
    DBUG_RETURN(true);
  }
  global_sid_lock->unlock();

  field_list.push_back(new Item_empty_string("File", FN_REFLEN));
  field_list.push_back(
      new Item_return_int("Position", 20, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Binlog_Do_DB", 255));
  field_list.push_back(new Item_empty_string("Binlog_Ignore_DB", 255));
  field_list.push_back(
      new Item_empty_string("Executed_Gtid_Set", gtid_set_size));

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    my_free(gtid_set_buffer);
    DBUG_RETURN(true);
  }
  protocol->start_row();

  if (mysql_bin_log.is_open()) {
    LOG_INFO li;
    mysql_bin_log.get_current_log(&li);
    size_t dir_len = dirname_length(li.log_file_name);
    protocol->store(li.log_file_name + dir_len, &my_charset_bin);
    protocol->store((ulonglong)li.pos);
    store(protocol, binlog_filter->get_do_db());
    store(protocol, binlog_filter->get_ignore_db());
    protocol->store(gtid_set_buffer, &my_charset_bin);
    if (protocol->end_row()) {
      my_free(gtid_set_buffer);
      DBUG_RETURN(true);
    }
  }
  my_eof(thd);
  my_free(gtid_set_buffer);
  DBUG_RETURN(false);
}

/**
  Execute a SHOW BINARY LOGS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval false success
  @retval true failure
*/
bool show_binlogs(THD *thd) {
  IO_CACHE *index_file;
  LOG_INFO cur;
  File file;
  char fname[FN_REFLEN];
  List<Item> field_list;
  size_t length;
  size_t cur_dir_len;
  Protocol *protocol = thd->get_protocol();
  DBUG_ENTER("show_binlogs");

  if (!mysql_bin_log.is_open()) {
    my_error(ER_NO_BINARY_LOGGING, MYF(0));
    DBUG_RETURN(true);
  }

  field_list.push_back(new Item_empty_string("Log_name", 255));
  field_list.push_back(
      new Item_return_int("File_size", 20, MYSQL_TYPE_LONGLONG));
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(true);

  mysql_mutex_lock(mysql_bin_log.get_log_lock());
  DEBUG_SYNC(thd, "show_binlogs_after_lock_log_before_lock_index");
  mysql_bin_log.lock_index();
  index_file = mysql_bin_log.get_index_file();

  mysql_bin_log.raw_get_current_log(&cur);           // dont take mutex
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());  // lockdep, OK

  cur_dir_len = dirname_length(cur.log_file_name);

  reinit_io_cache(index_file, READ_CACHE, (my_off_t)0, 0, 0);

  /* The file ends with EOF or empty line */
  while ((length = my_b_gets(index_file, fname, sizeof(fname))) > 1) {
    size_t dir_len;
    ulonglong file_length = 0;  // Length if open fails
    fname[--length] = '\0';     // remove the newline

    protocol->start_row();
    dir_len = dirname_length(fname);
    length -= dir_len;
    protocol->store(fname + dir_len, length, &my_charset_bin);

    if (!(strncmp(fname + dir_len, cur.log_file_name + cur_dir_len, length)))
      file_length = cur.pos; /* The active log, use the active position */
    else {
      /* this is an old log, open it and find the size */
      if ((file = mysql_file_open(key_file_binlog, fname, O_RDONLY, MYF(0))) >=
          0) {
        file_length = (ulonglong)mysql_file_seek(file, 0L, MY_SEEK_END, MYF(0));
        mysql_file_close(file, MYF(0));
      }
    }
    protocol->store(file_length);
    if (protocol->end_row()) {
      DBUG_PRINT(
          "info",
          ("stopping dump thread because protocol->write failed at line %d",
           __LINE__));
      goto err;
    }
  }
  if (index_file->error == -1) goto err;
  mysql_bin_log.unlock_index();
  my_eof(thd);
  DBUG_RETURN(false);

err:
  mysql_bin_log.unlock_index();
  DBUG_RETURN(true);
}
