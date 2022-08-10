/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#include "sql/rpl_source.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <memory>
#include <unordered_map>
#include <utility>

#include "m_ctype.h"
#include "m_string.h"  // strmake
#include "map_helpers.h"
#include "mutex_lock.h"  // Mutex_lock
#include "my_byteorder.h"
#include "my_command.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_mutex.h"
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
#include "sql/rpl_utility.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/system_variables.h"
#include "sql_string.h"
#include "thr_mutex.h"
#include "typelib.h"

int max_binlog_dump_events = 0;  // unlimited
bool opt_sporadic_binlog_dump_fail = false;

malloc_unordered_map<uint32, unique_ptr_my_free<REPLICA_INFO>> slave_list{
    key_memory_REPLICA_INFO};
extern TYPELIB binlog_checksum_typelib;

#define get_object(p, obj, msg)                  \
  {                                              \
    uint len;                                    \
    if (p >= p_end) {                            \
      my_error(ER_MALFORMED_PACKET, MYF(0));     \
      return 1;                                  \
    }                                            \
    len = net_field_length_ll(&p);               \
    if (p + len > p_end || len >= sizeof(obj)) { \
      errmsg = msg;                              \
      goto err;                                  \
    }                                            \
    strmake(obj, (char *)p, len);                \
    p += len;                                    \
  }

/**
  Register slave in 'slave_list' hash table.

  @return
    0	ok
  @return
    1	Error.   Error message sent to client
*/

int register_replica(THD *thd, uchar *packet, size_t packet_length) {
  int res;
  uchar *p = packet, *p_end = packet + packet_length;
  const char *errmsg = "Wrong parameters when registering replica";
  String replica_uuid;

  CONDITIONAL_SYNC_POINT("begin_register_replica");

  if (check_access(thd, REPL_SLAVE_ACL, any_db, nullptr, nullptr, false, false))
    return 1;

  unique_ptr_my_free<REPLICA_INFO> si((REPLICA_INFO *)my_malloc(
      key_memory_REPLICA_INFO, sizeof(REPLICA_INFO), MYF(MY_WME)));
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
  si->thd_id = thd->thread_id();
  si->valid_replica_uuid = false;
  if (get_replica_uuid(thd, &replica_uuid)) {
    si->valid_replica_uuid =
        !si->replica_uuid.parse(replica_uuid.c_ptr(), replica_uuid.length());
  }

  mysql_mutex_lock(&LOCK_replica_list);
  unregister_replica(thd, false, false /*need_lock_slave_list=false*/);
  res = !slave_list.emplace(si->server_id, std::move(si)).second;
  mysql_mutex_unlock(&LOCK_replica_list);
  return res;

err:
  my_message(ER_UNKNOWN_ERROR, errmsg, MYF(0)); /* purecov: inspected */
  return 1;
}

void unregister_replica(THD *thd, bool only_mine, bool need_lock_slave_list) {
  if (thd->server_id) {
    if (need_lock_slave_list)
      mysql_mutex_lock(&LOCK_replica_list);
    else
      mysql_mutex_assert_owner(&LOCK_replica_list);

    auto it = slave_list.find(thd->server_id);
    if (it != slave_list.end() &&
        (!only_mine || it->second->thd_id == thd->thread_id()))
      slave_list.erase(it);

    if (need_lock_slave_list) mysql_mutex_unlock(&LOCK_replica_list);
  }
}

/**
  Execute a SHOW REPLICAS / SHOW SLAVE HOSTS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval false success
  @retval true failure
*/
bool show_replicas(THD *thd) {
  mem_root_deque<Item *> field_list(thd->mem_root);
  Protocol *protocol = thd->get_protocol();
  DBUG_TRACE;

  field_list.push_back(new Item_return_int("Server_Id", 10, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Host", HOSTNAME_LENGTH));
  if (opt_show_replica_auth_info) {
    field_list.push_back(new Item_empty_string("User", USERNAME_CHAR_LENGTH));
    field_list.push_back(new Item_empty_string("Password", 20));
  }
  field_list.push_back(new Item_return_int("Port", 7, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Source_Id", 10, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Replica_UUID", UUID_LENGTH));

  // TODO: once the old syntax is removed, remove this as well.
  if (thd->lex->is_replication_deprecated_syntax_used())
    rename_fields_use_old_replica_source_terms(thd, field_list);

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  mysql_mutex_lock(&LOCK_replica_list);

  for (const auto &key_and_value : slave_list) {
    REPLICA_INFO *si = key_and_value.second.get();
    protocol->start_row();
    protocol->store((uint32)si->server_id);
    protocol->store(si->host, &my_charset_bin);
    if (opt_show_replica_auth_info) {
      protocol->store(si->user, &my_charset_bin);
      protocol->store(si->password, &my_charset_bin);
    }
    protocol->store((uint32)si->port);
    protocol->store((uint32)si->master_id);

    if (si->valid_replica_uuid) {
      char text_buf[binary_log::Uuid::TEXT_LENGTH + 1];
      si->replica_uuid.to_string(text_buf);
      protocol->store(text_buf, &my_charset_bin);
    } else {
      protocol->store("", &my_charset_bin);
    }

    if (protocol->end_row()) {
      mysql_mutex_unlock(&LOCK_replica_list);
      return true;
    }
  }
  mysql_mutex_unlock(&LOCK_replica_list);
  my_eof(thd);
  return false;
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
      The string is evaluated to apply workarounds on the slave. </td></tr>
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
  <caption>Payload</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>binlog-version</td>
      <td>Version of the binlog format.
        See @ref sect_protocol_replication_binlog_version</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_fix "string[50]"</td>
    <td>mysql-server version</td>
    <td>version of the MySQL Server that created the binlog.
      The string is evaluated to apply workarounds on the slave. </td></tr>
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

  A @ref sect_protocol_replication_event_stop has not payload or post-header

  @subsection sect_protocol_replication_event_rotate ROTATE_EVENT

  The rotate event is added to the binlog as last event to tell the reader what
  binlog to request next.

  <table>
  <caption>Post-header</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td colspan="3">if binlog-version > 1 {</td></tr>
  <tr><td>@ref a_protocol_type_int8 "int&lt;8&gt;"</td>
      <td>position</td>
      <td></td></tr>
  <tr><td colspan="3">}</td></tr>
  </table>

  <table>
  <caption>Payload</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "string&lt;EOF&gt;"</td>
      <td>binlog</td>
      <td>name of the next binlog</td></tr>
  </table>

  @subsection sect_protocol_replication_event_slave SLAVE_EVENT

  @note Ignored !

  @subsection sect_protocol_replication_event_incident INCIDENT_EVENT

  <table>
  <caption>Payload</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>type</td>
      <td>
         <table>
         <tr><th>Hex</th><th>Name</th></tr>
         <tr><td>0x0000</td><td>INCIDENT_NONE</td></tr>
         <tr><td>0x0001</td><td>INCIDENT_LOST_EVENTS</td></tr>
      </td></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>message_length</td>
      <td>Length of `message`</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_var "binary&lt;var&gt;"</td>
      <td>message</td>
      <td>Incident message with length `message_length`</td></tr>
  </table>

  @subsection sect_protocol_replication_event_heartbeat HEARTBEAT_EVENT

  An artificial event generated by the master. It isn't written to the relay
  logs.

  It is added by the master after the replication connection was idle for
  `x` seconds to update the slave's `Seconds_behind_master timestamp in the
  SHOW SLAVE STATUS output.

  It has no payload nor post-header.

  @section sect_protocol_replication_binlog_event_sbr Statement Based Replication Events

  Statement Based Replication or SBR sends the SQL queries a client sent to
  the master AS IS to the slave. It needs extra events to mimic the client
  connection's state on the slave side.

  @subsection sect_protocol_replication_event_query QUERY_EVENT

  The query event is used to send text queries through the binlod

  <table>
  <caption>Post-header</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>slave_proxy_id</td>
      <td></td></tr>
  <tr><td>@ref a_protocol_type_int4 "int&lt;4&gt;"</td>
      <td>execution time</td>
      <td></td></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>schema length</td>
      <td></td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>error code</td>
      <td></td></tr>
  <tr><td colspan="3">if binlog-version >= 4 {</td></tr>
  <tr><td>@ref a_protocol_type_int2 "int&lt;2&gt;"</td>
      <td>status_vars length</td>
      <td>Number of bytes in the following sequence of `status_vars`</td></tr>
  <tr><td colspan="3">}</td></tr>
  </table>

  <table>
  <caption>Payload</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_var "binary&lt;var&gt;"</td>
  <td>status-vars</td>
  <td>A sequence of status key/value pairs. The key is 1-byte, while the
  value is dependent on the key
  <table>
  <tr><th>Hex</th><th>Flag</th><th>Value Length</th></tr>
  <tr><td>0x00</td><td>@ref sect_protocol_replication_event_query_00 "Q_FLAGS2_CODE"</td>
    <td>4</td></tr>
  <tr><td>0x01</td><td>@ref sect_protocol_replication_event_query_01 "Q_SQL_MODE_CODE"</td>
    <td>8</td></tr>
  <tr><td>0x02</td><td>@ref sect_protocol_replication_event_query_02 "Q_AUTO_INCREMENT"</td>
    <td>1 + n + 1</td></tr>
  <tr><td>0x03</td><td>@ref sect_protocol_replication_event_query_03 "Q_CATALOG"</td>
    <td>2 + 2</td></tr>
  <tr><td>0x04</td><td>@ref sect_protocol_replication_event_query_04 "Q_CHARSET_CODE"</td>
    <td>2 + 2 + 2</td></tr>
  <tr><td>0x05</td><td>@ref sect_protocol_replication_event_query_05 "Q_TIME_ZONE_CODE"</td>
    <td>1 + 1</td></tr>
  <tr><td>0x06</td><td>@ref sect_protocol_replication_event_query_06 "Q_CATALOG_NZ_CODE"</td>
    <td>1 + n</td></tr>
  <tr><td>0x07</td><td>@ref sect_protocol_replication_event_query_07 "Q_LC_TIME_NAMES_CODE"</td>
    <td>2</td></tr>
  <tr><td>0x08</td><td>@ref sect_protocol_replication_event_query_08 "Q_CHARSET_DATABASE_CODE"</td>
    <td>2</td></tr>
  <tr><td>0x09</td><td>@ref sect_protocol_replication_event_query_09 "Q_TABLE_MAP_FOR_UPDATE_CODE"</td>
    <td>8</td></tr>
  <tr><td>0x0a</td><td>@ref sect_protocol_replication_event_query_0a "Q_MASTER_DATA_WRITTEN_CODE"</td>
    <td>4</td></tr>
  <tr><td>0x0b</td><td>@ref sect_protocol_replication_event_query_0b "Q_INVOKERS"</td>
    <td>1 + n + 1 + n</td></tr>
  <tr><td>0x0c</td><td>@ref sect_protocol_replication_event_query_0c "Q_UPDATED_DB_NAMES"</td>
    <td>1 + n*nul-term-string</td></tr>
  <tr><td>0x0d</td><td>@ref sect_protocol_replication_event_query_0d "Q_MICROSECONDS"</td>
    <td>3</td></tr>
  </table>

  The value of the different status vars are:

  @anchor sect_protocol_replication_event_query_00 <b>Q_FLAGS2_CODE</b>

  Bitmask of flags that are usually set with the SET command:

  * SQL_AUTO_IS_NULL
  * FOREIGN_KEY_CHECKS
  * UNIQUE_CHECKS
  * AUTOCOMMIT

  <table>
    <tr><th>Hex</th><th>Flag</th></tr>
    <tr><td>0x00004000</td><td>::OPTION_AUTO_IS_NULL</td></tr>
    <tr><td>0x00080000</td><td>::OPTION_NOT_AUTOCOMMIT</td></tr>
    <tr><td>0x04000000</td><td>::OPTION_NO_FOREIGN_KEY_CHECKS</td></tr>
    <tr><td>0x08000000</td><td>::OPTION_RELAXED_UNIQUE_CHECKS</td></tr>
  </table>

  @anchor sect_protocol_replication_event_query_01 <b>Q_SQL_MODE_CODE</b>

  Bitmask of flags that are usually set with SET sql_mode:

  <table>
    <tr><th>Hex</th><th>Flag</th></tr>
    <tr><td>0x00000001</td><td>::MODE_REAL_AS_FLOAT</td></tr>
    <tr><td>0x00000002</td><td>::MODE_PIPES_AS_CONCAT</td></tr>
    <tr><td>0x00000004</td><td>::MODE_ANSI_QUOTES</td></tr>
    <tr><td>0x00000008</td><td>::MODE_IGNORE_SPACE</td></tr>
    <tr><td>0x00000010</td><td>::MODE_NOT_USED</td></tr>
    <tr><td>0x00000020</td><td>::MODE_ONLY_FULL_GROUP_BY</td></tr>
    <tr><td>0x00000040</td><td>::MODE_NO_UNSIGNED_SUBTRACTION</td></tr>
    <tr><td>0x00000080</td><td>::MODE_NO_DIR_IN_CREATE</td></tr>
    <tr><td>0x00000100</td><td>MODE_POSTGRESQL</td></tr>
    <tr><td>0x00000200</td><td>MODE_ORACLE</td></tr>
    <tr><td>0x00000400</td><td>MODE_MSSQL</td></tr>
    <tr><td>0x00000800</td><td>MODE_DB2</td></tr>
    <tr><td>0x00001000</td><td>MODE_MAXDB</td></tr>
    <tr><td>0x00002000</td><td>MODE_NO_KEY_OPTIONS</td></tr>
    <tr><td>0x00004000</td><td>MODE_NO_TABLE_OPTIONS</td></tr>
    <tr><td>0x00008000</td><td>MODE_NO_FIELD_OPTIONS</td></tr>
    <tr><td>0x00010000</td><td>MODE_MYSQL323</td></tr>
    <tr><td>0x00020000</td><td>MODE_MYSQL40</td></tr>
    <tr><td>0x00040000</td><td>::MODE_ANSI</td></tr>
    <tr><td>0x00080000</td><td>::MODE_NO_AUTO_VALUE_ON_ZERO</td></tr>
    <tr><td>0x00100000</td><td>::MODE_NO_BACKSLASH_ESCAPES</td></tr>
    <tr><td>0x00200000</td><td>::MODE_STRICT_TRANS_TABLES</td></tr>
    <tr><td>0x00400000</td><td>::MODE_STRICT_ALL_TABLES</td></tr>
    <tr><td>0x00800000</td><td>::MODE_NO_ZERO_IN_DATE</td></tr>
    <tr><td>0x01000000</td><td>::MODE_NO_ZERO_DATE</td></tr>
    <tr><td>0x02000000</td><td>::MODE_INVALID_DATES</td></tr>
    <tr><td>0x04000000</td><td>::MODE_ERROR_FOR_DIVISION_BY_ZERO</td></tr>
    <tr><td>0x08000000</td><td>::MODE_TRADITIONAL</td></tr>
    <tr><td>0x10000000</td><td>MODE_NO_AUTO_CREATE_USER</td></tr>
    <tr><td>0x20000000</td><td>::MODE_HIGH_NOT_PRECEDENCE</td></tr>
    <tr><td>0x40000000</td><td>::MODE_NO_ENGINE_SUBSTITUTION</td></tr>
    <tr><td>0x80000000</td><td>::MODE_PAD_CHAR_TO_FULL_LENGTH</td></tr>
  </table>

  @anchor sect_protocol_replication_event_query_02 <b>Q_AUTO_INCREMENT</b>

  2 byte autoincrement-increment and 2 byte autoincrement-offset

  @note Only written if the -increment is > 1


  @anchor sect_protocol_replication_event_query_03 <b>Q_CATALOG</b>

  1 byte length + &lt;length&gt; chars of the cataiog + &lsquo;0&lsquo;-char

  @note Only written if length > 0


  @anchor sect_protocol_replication_event_query_04 <b>Q_CHARSET_CODE</b>

  2 bytes character_set_client + 2 bytes collation_connection + 2 bytes collation_server

  See @ref page_protocol_basic_character_set


  @anchor sect_protocol_replication_event_query_05 <b>Q_TIME_ZONE_CODE</b>

  1 byte length + &lt;length&gt; chars of the timezone.

  Timezone the master is in.

  @note only written when length > 0


  @anchor sect_protocol_replication_event_query_06 <b>Q_CATALOG_NZ_CODE</b>

  1 byte length + &lt;length&gt; chars of the catalog.

  @note only written when length > 0


  @anchor sect_protocol_replication_event_query_07 <b>Q_LC_TIME_NAMES_CODE</b>

  LC_TIME of the server. Defines how to parse week-, month and day-names in timestamps

  @note only written when length > 0


  @anchor sect_protocol_replication_event_query_08 <b>Q_CHARSET_DATABASE_CODE</b>

  character set and collation of the schema


  @anchor sect_protocol_replication_event_query_09 <b>Q_TABLE_MAP_FOR_UPDATE_CODE</b>

  a 64bit field ... should only be used in @ref sect_protocol_replication_binlog_event_rbr
  and multi-table updates


  @anchor sect_protocol_replication_event_query_0a <b>Q_MASTER_DATA_WRITTEN_CODE</b>

  4 byte ...


  @anchor sect_protocol_replication_event_query_0b <b>Q_INVOKERS</b>

  1 byte length + &lt;length&gt; bytes username and 1 byte length + &lt;length&gt; bytes hostname


  @anchor sect_protocol_replication_event_query_0c <b>Q_UPDATED_DB_NAMES</b>

  1 byte count + &lt;count&gt; \0 terminated string


  @anchor sect_protocol_replication_event_query_0d <b>Q_MICROSECONDS</b>

  3 byte microseconds

  </td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_var "binary&lt;var&gt;"</td>
      <td>schema</td>
      <td></td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "binary&lt;eof&gt;"</td>
      <td>query</td>
      <td>text of the query</td></tr>
  </table>

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
  DBUG_TRACE;
  ulong pos;
  ushort flags = 0;
  const uchar *packet_position = (uchar *)packet;
  size_t packet_bytes_todo = packet_length;

  assert(!thd->status_var_aggregated);
  thd->status_var.com_other++;
  thd->enable_slow_log = opt_log_slow_admin_statements;
  if (check_global_access(thd, REPL_SLAVE_ACL)) return false;

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
  mysql_binlog_send(thd, thd->mem_strdup(packet + 10), (my_off_t)pos, nullptr,
                    flags);

  unregister_replica(thd, true, true /*need_lock_slave_list=true*/);
  /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
  return true;

error_malformed_packet:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  return true;
}

bool com_binlog_dump_gtid(THD *thd, char *packet, size_t packet_length) {
  DBUG_TRACE;
  /*
    Before going GA, we need to make this protocol extensible without
    breaking compatitibilty. /Alfranio.
  */
  ushort flags = 0;
  uint32 data_size = 0;
  uint64 pos = 0;
  char name[FN_REFLEN + 1];
  uint32 name_size = 0;
  char *gtid_string = nullptr;
  const uchar *packet_position = (uchar *)packet;
  size_t packet_bytes_todo = packet_length;
  Sid_map sid_map(
      nullptr /*no sid_lock because this is a completely local object*/);
  Gtid_set slave_gtid_executed(&sid_map);

  assert(!thd->status_var_aggregated);
  thd->status_var.com_other++;
  thd->enable_slow_log = opt_log_slow_admin_statements;
  if (check_global_access(thd, REPL_SLAVE_ACL)) return false;

  READ_INT(flags, 2);
  READ_INT(thd->server_id, 4);
  READ_INT(name_size, 4);
  READ_STRING(name, name_size, sizeof(name));
  READ_INT(pos, 8);
  DBUG_PRINT("info", ("pos=%" PRIu64 " flags=%d server_id=%d", pos, flags,
                      thd->server_id));
  READ_INT(data_size, 4);
  CHECK_PACKET_SIZE(data_size);
  if (slave_gtid_executed.add_gtid_encoding(packet_position, data_size) !=
      RETURN_STATUS_OK)
    return true;
  slave_gtid_executed.to_string(&gtid_string);
  DBUG_PRINT("info",
             ("Slave %d requested to read %s at position %" PRIu64 " gtid set "
              "'%s'.",
              thd->server_id, name, pos, gtid_string));

  kill_zombie_dump_threads(thd);
  query_logger.general_log_print(thd, thd->get_command(),
                                 "Log: '%s' Pos: %" PRIu64 " GTIDs: '%s'", name,
                                 pos, gtid_string);
  my_free(gtid_string);
  mysql_binlog_send(thd, name, (my_off_t)pos, &slave_gtid_executed, flags);

  unregister_replica(thd, true, true /*need_lock_slave_list=true*/);
  /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
  return true;

error_malformed_packet:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  return true;
}

void mysql_binlog_send(THD *thd, char *log_ident, my_off_t pos,
                       Gtid_set *slave_gtid_executed, uint32 flags) {
  Binlog_sender sender(thd, log_ident, pos, slave_gtid_executed, flags);

  sender.run();
}

const user_var_entry *get_user_var_from_alternatives(const THD *thd,
                                                     const std::string alt1,
                                                     const std::string alt2) {
  mysql_mutex_assert_owner(&thd->LOCK_thd_data);
  const auto &uv = thd->user_vars;
  auto it = uv.find(alt1);
  if (it == uv.end()) it = uv.find(alt2);
  if (it == uv.end()) return nullptr;
  return it->second.get();
}

/**
  An auxiliary function extracts slave UUID.

  @param[in]    thd  THD to access a user variable
  @param[out]   value String to return UUID value.

  @return       if success value is returned else NULL is returned.
*/
String *get_replica_uuid(THD *thd, String *value) {
  if (value == nullptr) return nullptr;

  /* Protects thd->user_vars. */
  MUTEX_LOCK(lock_guard, &thd->LOCK_thd_data);

  // Get user_var.
  const auto &uv =
      get_user_var_from_alternatives(thd, "replica_uuid", "slave_uuid");

  // Get value of user_var.
  if (uv && uv->length() > 0) {
    value->copy(uv->ptr(), uv->length(), nullptr);
    return value;
  }
  return nullptr;
}

/**
  Callback function used by kill_zombie_dump_threads() function to
  to find zombie dump thread from the thd list.

  @note It acquires LOCK_thd_data mutex when it finds matching thd.
  It is the responsibility of the caller to release this mutex.
*/
class Find_zombie_dump_thread : public Find_THD_Impl {
 public:
  Find_zombie_dump_thread(String value) : m_replica_uuid(value) {}
  bool operator()(THD *thd) override {
    THD *cur_thd = current_thd;
    if (thd != cur_thd && (thd->get_command() == COM_BINLOG_DUMP ||
                           thd->get_command() == COM_BINLOG_DUMP_GTID)) {
      String tmp_uuid;
      bool is_zombie_thread = false;
      get_replica_uuid(thd, &tmp_uuid);
      if (m_replica_uuid.length()) {
        is_zombie_thread =
            (tmp_uuid.length() &&
             !strncmp(m_replica_uuid.c_ptr(), tmp_uuid.c_ptr(), UUID_LENGTH));
      } else {
        /*
          Check if it is a 5.5 slave's dump thread i.e., server_id should be
          same && dump thread should not contain 'UUID'.
        */
        is_zombie_thread =
            ((thd->server_id == cur_thd->server_id) && !tmp_uuid.length());
      }
      if (is_zombie_thread) return true;
    }
    return false;
  }

 private:
  String m_replica_uuid;
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
  String replica_uuid;
  get_replica_uuid(thd, &replica_uuid);
  if (replica_uuid.length() == 0 && thd->server_id == 0) return;

  Find_zombie_dump_thread find_zombie_dump_thread(replica_uuid);
  THD_ptr tmp_ptr =
      Global_THD_manager::get_instance()->find_thd(&find_zombie_dump_thread);
  if (tmp_ptr) {
    /*
      Here we do not call kill_one_thread() as
      it will be slow because it will iterate through the list
      again. We just to do kill the thread ourselves.
    */
    if (log_error_verbosity > 2) {
      if (replica_uuid.length()) {
        LogErr(INFORMATION_LEVEL, ER_RPL_ZOMBIE_ENCOUNTERED, "UUID",
               replica_uuid.c_ptr(), "UUID", tmp_ptr->thread_id());
      } else {
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), "%u", thd->server_id);
        LogErr(INFORMATION_LEVEL, ER_RPL_ZOMBIE_ENCOUNTERED, "server_id",
               numbuf, "server_id", tmp_ptr->thread_id());
      }
    }
    tmp_ptr->duplicate_slave_id = true;
    tmp_ptr->awake(THD::KILL_QUERY);
  }
}

/**
  Execute a RESET MASTER statement.

  @param thd Pointer to THD object of the client thread executing the
  statement.
  @param unlock_global_read_lock Unlock the global read lock acquired
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

  /*
    No RESET MASTER commands are allowed while Group Replication is running
    unless executed during a clone operation as part of the process.
  */
  if (is_group_replication_running() && !is_group_replication_cloning()) {
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
    Unlock the global read lock (which was acquired by this
    session as part of RESET MASTER) before running the hook
    which informs plugins.
  */
  if (unlock_global_read_lock) {
    assert(thd->global_read_lock.is_acquired());
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
  char *gtid_set_buffer = nullptr;
  int gtid_set_size = 0;

  DBUG_TRACE;

  global_sid_lock->wrlock();
  const Gtid_set *gtid_set = gtid_state->get_executed_gtids();
  if ((gtid_set_size = gtid_set->to_string(&gtid_set_buffer)) < 0) {
    global_sid_lock->unlock();
    my_eof(thd);
    my_free(gtid_set_buffer);
    return true;
  }
  global_sid_lock->unlock();

  mem_root_deque<Item *> field_list(thd->mem_root);
  field_list.push_back(new Item_empty_string("File", FN_REFLEN));
  field_list.push_back(
      new Item_return_int("Position", 20, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Binlog_Do_DB", 255));
  field_list.push_back(new Item_empty_string("Binlog_Ignore_DB", 255));
  field_list.push_back(
      new Item_empty_string("Executed_Gtid_Set", gtid_set_size));

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    my_free(gtid_set_buffer);
    return true;
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
      return true;
    }
  }
  my_eof(thd);
  my_free(gtid_set_buffer);
  return false;
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
  size_t length;
  size_t cur_dir_len;
  Protocol *protocol = thd->get_protocol();
  DBUG_TRACE;

  if (!mysql_bin_log.is_open()) {
    my_error(ER_NO_BINARY_LOGGING, MYF(0));
    return true;
  }

  mem_root_deque<Item *> field_list(thd->mem_root);
  field_list.push_back(new Item_empty_string("Log_name", 255));
  field_list.push_back(
      new Item_return_int("File_size", 20, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Encrypted", 3));
  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  mysql_mutex_lock(mysql_bin_log.get_log_lock());
  DEBUG_SYNC(thd, "show_binlogs_after_lock_log_before_lock_index");
  mysql_bin_log.lock_index();
  index_file = mysql_bin_log.get_index_file();

  mysql_bin_log.raw_get_current_log(&cur);           // dont take mutex
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());  // lockdep, OK

  cur_dir_len = dirname_length(cur.log_file_name);

  reinit_io_cache(index_file, READ_CACHE, (my_off_t)0, false, false);

  /* The file ends with EOF or empty line */
  while ((length = my_b_gets(index_file, fname, sizeof(fname))) > 1) {
    size_t dir_len;
    int encrypted_header_size = 0;
    ulonglong file_length = 0;  // Length if open fails
    fname[--length] = '\0';     // remove the newline

    protocol->start_row();
    dir_len = dirname_length(fname);
    length -= dir_len;
    protocol->store_string(fname + dir_len, length, &my_charset_bin);

    if (!(strncmp(fname + dir_len, cur.log_file_name + cur_dir_len, length))) {
      /* Encryption header size shall be accounted in the file_length */
      encrypted_header_size = cur.encrypted_header_size;
      file_length = cur.pos; /* The active log, use the active position */
      file_length = file_length + encrypted_header_size;
    } else {
      /* this is an old log, open it and find the size */
      if ((file = mysql_file_open(key_file_binlog, fname, O_RDONLY, MYF(0))) >=
          0) {
        unsigned char magic[Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE];
        if (mysql_file_read(file, magic, BINLOG_MAGIC_SIZE, MYF(0)) == 4 &&
            memcmp(magic, Rpl_encryption_header::ENCRYPTION_MAGIC,
                   Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE) == 0) {
          /* Encryption header size is already accounted in the file_length */
          encrypted_header_size = 1;
        }
        file_length = (ulonglong)mysql_file_seek(file, 0L, MY_SEEK_END, MYF(0));
        mysql_file_close(file, MYF(0));
      }
    }
    protocol->store(file_length);
    protocol->store(encrypted_header_size ? "Yes" : "No", &my_charset_bin);
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
  return false;

err:
  mysql_bin_log.unlock_index();
  return true;
}
