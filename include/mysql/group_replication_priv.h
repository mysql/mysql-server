/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef GROUP_REPLICATION_PRIV_INCLUDE
#define	GROUP_REPLICATION_PRIV_INCLUDE

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#ifndef HAVE_REPLICATION
#define HAVE_REPLICATION
#endif

#include <my_global.h>
#include <my_thread.h>
#include <my_sys.h>

#include <debug_sync.h>
#include <log_event.h>
#include <replication.h>
#include <rpl_channel_service_interface.h>
#include <rpl_gtid.h>
#include <rpl_write_set_handler.h>


/**
  Server side initializations.
*/
int group_replication_init();


/**
  Returns the server connection attribute

  @Note This method implementation is on sql_class.cc

  @return the pthread for the connection attribute.
*/
my_thread_attr_t *get_connection_attrib();

/**
  Returns the server hostname, port and uuid.

  @param[out] hostname
  @param[out] port
  @param[out] uuid
  @param[out] server_version
  @param[out] server_ssl_variables

*/
void get_server_parameters(char **hostname, uint *port, char **uuid,
                           unsigned int *server_version,
                           st_server_ssl_variables* server_ssl_variables);

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

  @param[in] auto_increment_increment
*/
void set_auto_increment_increment(ulong auto_increment_increment);


/**
  Set server auto_increment_offset

  @param[in] auto_increment_offset
*/
void set_auto_increment_offset(ulong auto_increment_offset);


/**
  Returns a struct containing all server startup information needed to evaluate
  if one has conditions to proceed executing master-master replication.

  @param[out] requirements

  @param[in] has_lock Caller should set this to true if the calling
  thread holds gtid_mode_lock; otherwise set it to false.
*/
void get_server_startup_prerequirements(Trans_context_info& requirements,
                                        bool has_lock);


/**
  Returns the server GTID_EXECUTED encoded as a binary string.

  @note Memory allocated to encoded_gtid_executed must be release by caller.

  @param[out] encoded_gtid_executed binary string
  @param[out] length                binary string length
*/
bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      size_t *length);

#if !defined(DBUG_OFF)
/**
  Returns a text representation of a encoded GTID set.

  @note Memory allocated to returned pointer must be release by caller.

  @param[in] encoded_gtid_set      binary string
  @param[in] length                binary string length

  @return a pointer to text representation of the encoded set
*/
char* encoded_gtid_set_to_string(uchar *encoded_gtid_set, size_t length);
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
void set_slave_thread_options(THD* thd);


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
const char* get_write_set_algorithm_string(unsigned int algorithm);

#endif	/* GROUP_REPLICATION_PRIV_INCLUDE */

