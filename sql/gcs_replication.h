/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
  bool get_gcs_nodes_info(RPL_GCS_NODES_INFO *info);

private:
  LEX_STRING plugin_name;
  plugin_ref plugin;
  st_mysql_gcs_rpl* plugin_handle;
  int gcs_init();
};

/*
  This method adds a members to the thread_to_seq_num_map which is the
  thread identifier and cluster transaction identifier, i.e. pair of
  sequence number and cluster_sidno. In case the transaction is not
  certified we store std::pair<0, 0>.

  @param[in] thread_id      The key of the map.
  @param[in] seq_num        The sequence number of the certified transaction.
  @param[in] cluster_sidno  The cluster sidno.

  @retval  0  if the addition of the sequence number was successful
          !=0 if there was a failure during the addition to the map.

*/
int add_transaction_certification_result(my_thread_id thread_id,
                                         rpl_gno seq_num,
                                         rpl_sidno cluster_sidno);

/*
  This method fetches the sequence number from the sequence number
  map.

  @param[in] thread_id   The key of the map

  @retval returns the sequence number of the transaction being executed
          by the thread. Incase the thread_id is not found we return
          std::pair (-1, -1);
*/
std::pair <rpl_sidno, rpl_gno> get_transaction_certification_result(my_thread_id thread_id);

/*
  This method deletes a sequence number and thread id pair from the
  sequence number map once the after the transaction is committed/rollback.

  @param[in] thread_id   The key of the map
*/
void delete_transaction_certification_result(my_thread_id thread_id);

int init_gcs_rpl();
int start_gcs_rpl();
int stop_gcs_rpl();
bool is_running_gcs_rpl();
int cleanup_gcs_rpl();
bool is_gcs_plugin_loaded();

bool get_gcs_stats(RPL_GCS_STATS_INFO *info);
bool get_gcs_nodes_stats(RPL_GCS_NODES_INFO *info);
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

/**
  Returns the server hostname, port and uuid.

  @param[out] hostname
  @param[out] port
  @param[out] uuid
*/
void get_server_host_port_uuid(char **hostname, uint *port, char** uuid);

#endif /* GCS_REPLICATION_INCLUDED */
