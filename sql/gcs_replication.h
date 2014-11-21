/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_REPLICATION_INCLUDED
#define GCS_REPLICATION_INCLUDED

#include <mysql/plugin.h>
#include <mysql/plugin_gcs_rpl.h>
#include "sql_plugin.h"
#include "rpl_gtid.h"
#include "log.h"
#include "log_event.h"
#include "replication.h"
#include <map>

class Gcs_replication_handler
{
public:
  Gcs_replication_handler();
  ~Gcs_replication_handler();
  int gcs_handler_init();
  int gcs_rpl_start();
  int gcs_rpl_stop();
  bool is_gcs_rpl_running();
  /* functions accessing GCS cluster status and stats */
  bool get_gcs_stats_info(RPL_GCS_STATS_INFO *info);
  bool get_gcs_nodes_info(uint index, RPL_GCS_NODES_INFO *info);
  bool get_gcs_nodes_stat_info(RPL_GCS_NODE_STATS_INFO *info);
  uint get_gcs_nodes_number();

private:
  LEX_CSTRING plugin_name;
  plugin_ref plugin;
  st_mysql_gcs_rpl* plugin_handle;
  int gcs_init();
};

int init_gcs_rpl();
int start_gcs_rpl();
int stop_gcs_rpl();
bool is_running_gcs_rpl();
int cleanup_gcs_rpl();
bool is_gcs_plugin_loaded();

bool get_gcs_stats(RPL_GCS_STATS_INFO *info);
bool get_gcs_nodes_stats(uint index, RPL_GCS_NODES_INFO *info);
bool get_gcs_nodes_dbsm_stats(RPL_GCS_NODE_STATS_INFO *info);
uint get_gcs_nodes_stats_number();
/* Server access methods and variables */

extern ulong opt_rli_repository_id;
extern char *opt_relay_logname;
extern char *opt_relaylog_index_name;
extern char *relay_log_info_file;

/**
  Returns if the server engine initialization as ended or not.

  @return is the server ready
    @retval false     not ready
    @retval true      ready
*/
bool is_server_engine_ready();

/**
  Returns the server connection attribute

  @Note This method implementation is on sql_class.cc

  @return the pthread for the connection attribute.
*/
pthread_attr_t *get_connection_attrib();

/**
  Returns the defined variable for the MTS group checkpoint

  @return the user/default option for MTS checkpoint groups.
*/
uint get_opt_mts_checkpoint_group();

/**
  Returns the defined variable for the MTS slave parallel slaves

  @return the user/default option for MTS slave parallel slaves.
*/
ulong get_opt_mts_slave_parallel_workers();

/**
  Returns the defined variable for the relay log repository type.

  @return the user/default option for relay log repository type.
*/
ulong get_opt_rli_repository_id();

/**
  Sets the relay log name variable.

  @param[in]  name    the base name for the file

  @return the previous variable name
*/
char *set_relay_log_name(char* name);

/**
  Sets the relay log name variable for the index file.

  @param[in]  name    the base name for the file

  @return the previous variable name
*/
char *set_relay_log_index_name(char* name);

/**
  Sets the relay log name variable for the info file.

  @param[in]  name    the base name for the file

  @return the previous variable name
*/
char *set_relay_log_info_name(char* name);

//Recovery related methods

/**
  This method sets the condition and associated mutex to allow to be awaken when
  recovery info is set on this server interface.

  @param recovery_cond   The condition to awake the recovery process
  @param recovery_lock   The condition associated lock.
 */
void set_recovery_wait_structures(mysql_cond_t *recovery_cond,
                                  mysql_mutex_t *recovery_lock);

/**
  Method to set retrieved certification info from a non GCS channel extracted
  from a given View_change event

  @note a copy of the certification database is made here.

  @param view_change_event   the given view_change_event.
*/
void set_retrieved_cert_info(View_change_log_event* view_change_event);


/**
  Returns the present certification database assuming one was set before.

  @return a pointer to the retrieved certification database
    @retval NULL     database not set
    @retval !NULL    a valid database
*/
std::map<std::string, rpl_gno>* get_retrieved_cert_db();

/**
  Returns the present certification sequence number

  @return a seq number
*/
rpl_gno get_retrieved_seq_number();

/**
  Resets the seq number to a value representing a invalid value.
 */
void reset_retrieved_seq_number();

/**
  Returns the server hostname, port and uuid.

  @param[out] hostname
  @param[out] port
  @param[out] uuid
*/
void get_server_host_port_uuid(char **hostname, uint *port, char** uuid);

/**
  Returns a struct containing all server startup information needed to evaluate
  if one has conditions to proceed executing master-master replication.

  @param[out] requirements
 */
void
get_server_startup_prerequirements(Trans_context_info& requirements);

/**
  Returns the server GTID_EXECUTED encoded as a binary string.

  @note Memory allocated to encoded_gtid_executed must be release by caller.

  @param[out] encoded_gtid_executed binary string
  @param[out] length                binary string length
*/
bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      uint *length);
#if !defined(DBUG_OFF)
/**
  Returns a text representation of a encoded GTID set.

  @note Memory allocated to returned pointer must be release by caller.

  @param[in] encoded_gtid_set      binary string
  @param[in] length                binary string length

  @return a pointer to text representation of the encoded set
*/
char* encoded_gtid_set_to_string(uchar *encoded_gtid_set, uint length);
#endif

#endif /* GCS_REPLICATION_INCLUDED */
