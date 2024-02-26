/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef GROUP_REPLICATION_PRIV_INCLUDE
#define GROUP_REPLICATION_PRIV_INCLUDE

/**
  @file include/mysql/group_replication_priv.h
*/

#include "my_sys.h"
#include "my_thread.h"
#include "sql/binlog/group_commit/bgc_ticket_manager.h"
#include "sql/binlog_ostream.h"
#include "sql/binlog_reader.h"
#include "sql/debug_sync.h"
#include "sql/log_event.h"
#include "sql/replication.h"
#include "sql/rpl_channel_service_interface.h"
#include "sql/rpl_commit_stage_manager.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_write_set_handler.h"

/**
  Server side initializations.
*/
int group_replication_init();

/**
  Returns the server connection attribute

  @note This method implementation is on sql_class.cc

  @return the pthread for the connection attribute.
*/
my_thread_attr_t *get_connection_attrib();

/**
  Returns the server hostname, port and uuid.

  @param[out] hostname hostname
  @param[out] port port
  @param[out] uuid uuid
  @param[out] server_version server version
  @param[out] admin_port mysqld admin port
*/
void get_server_parameters(char **hostname, uint *port, char **uuid,
                           unsigned int *server_version, uint *admin_port);

/**
  Returns the server's client-server interface's ssl configuration values.

  @param[out] server_ssl_variables server's ssl_variables
*/
void get_server_main_ssl_parameters(
    st_server_ssl_variables *server_ssl_variables);

/**
  Returns the server's admin interface's ssl configuration values.

  @param[out] server_ssl_variables server's ssl_variables
*/
void get_server_admin_ssl_parameters(
    st_server_ssl_variables *server_ssl_variables);

/**
  Returns the server_id.

  @return server_id
*/
ulong get_server_id();

/**
  Returns the server auto_increment_increment

  @return auto_increment_increment
*/
ulong get_auto_increment_increment();

/**
  Returns the server auto_increment_offset

  @return auto_increment_offset
*/
ulong get_auto_increment_offset();

/**
  Set server auto_increment_increment

  @param[in] auto_increment_increment auto-increment increment
*/
void set_auto_increment_increment(ulong auto_increment_increment);

/**
  Set server auto_increment_offset

  @param[in] auto_increment_offset auto-increment offset
*/
void set_auto_increment_offset(ulong auto_increment_offset);

/**
  Returns a struct containing all server startup information needed to evaluate
  if one has conditions to proceed executing master-master replication.

  @param[out] requirements requirements
*/
void get_server_startup_prerequirements(Trans_context_info &requirements);

/**
  Returns the server GTID_EXECUTED encoded as a binary string.

  @note Memory allocated to encoded_gtid_executed must be release by caller.

  @param[out] encoded_gtid_executed binary string
  @param[out] length                binary string length
*/
bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      size_t *length);

#if !defined(NDEBUG)
/**
  Returns a text representation of a encoded GTID set.

  @note Memory allocated to returned pointer must be release by caller.

  @param[in] encoded_gtid_set      binary string
  @param[in] length                binary string length

  @return a pointer to text representation of the encoded set
*/
char *encoded_gtid_set_to_string(uchar *encoded_gtid_set, size_t length);
#endif

/**
  Return last gno for a given sidno, see
  Gtid_state::get_last_executed_gno() for details.
*/
rpl_gno get_last_executed_gno(rpl_sidno sidno);

/**
  Return sidno for a given sid, see Sid_map::add_sid() for details.
*/
rpl_sidno get_sidno_from_global_sid_map(rpl_sid sid);

/**
  Set slave thread default options.

  @param[in] thd  The thread
*/
void set_slave_thread_options(THD *thd);

/**
  Add thread to Global_THD_manager singleton.

  @param[in] thd  The thread
*/
void global_thd_manager_add_thd(THD *thd);

/**
  Remove thread from Global_THD_manager singleton.

  @param[in] thd  The thread
*/
void global_thd_manager_remove_thd(THD *thd);

/**
  Function that returns the write set extraction algorithm name.

  @param[in] algorithm  The algorithm value

  @return the algorithm name
*/
const char *get_write_set_algorithm_string(unsigned int algorithm);

/**
  Returns true if the given transaction is committed.

  @param[in] gtid  The transaction identifier

  @return true   the transaction is committed
          false  otherwise
*/
bool is_gtid_committed(const Gtid &gtid);

/**
  Returns the value of replica_max_allowed_packet.

  @return replica_max_allowed_packet
*/
unsigned long get_replica_max_allowed_packet();

/**
  Wait until the given Gtid_set is included in @@GLOBAL.GTID_EXECUTED.

  @param[in] gtid_set_text Gtid_set to wait for.
  @param[in] timeout       The maximum number of seconds that the
                           function should wait, or 0 to wait indefinitely.
  @param[in] update_thd_status
                           when true updates the stage info with
                           the new wait condition, when false keeps the
                           current stage info.

  @retval false the Gtid_set is included in @@GLOBAL.GTID_EXECUTED
  @retval true  otherwise
*/
bool wait_for_gtid_set_committed(const char *gtid_set_text, double timeout,
                                 bool update_thd_status);

/**
  @returns the maximum value of replica_max_allowed_packet.
 */
unsigned long get_max_replica_max_allowed_packet();

/**
  @returns if the server is restarting after a clone
*/
bool is_server_restarting_after_clone();

/**
  @returns if the server already dropped its data when cloning
*/
bool is_server_data_dropped();

#endif /* GROUP_REPLICATION_PRIV_INCLUDE */
