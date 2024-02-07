/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

#include "sql/rpl_info_factory.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "lex_string.h"
#include "m_string.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/mysqld.h"  // key_source_info_run_lock
#include "sql/rpl_filter.h"
#include "sql/rpl_info.h"
#include "sql/rpl_info_dummy.h"         // Rpl_info_dummy
#include "sql/rpl_info_table.h"         // Rpl_info_table
#include "sql/rpl_info_table_access.h"  // Rpl_info_table_access
#include "sql/rpl_mi.h"                 // Master_info
#include "sql/rpl_msr.h"                // channel_map
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"      // Relay_log_info
#include "sql/rpl_rli_pdb.h"  // Slave_worker
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"
#include "thr_lock.h"

/*
  Defines meta information on different repositories.
*/
Rpl_info_factory::struct_table_data Rpl_info_factory::rli_table_data;
Rpl_info_factory::struct_table_data Rpl_info_factory::mi_table_data;
Rpl_info_factory::struct_table_data Rpl_info_factory::worker_table_data;

/**
  Creates a Master info repository whose type is defined as a parameter.

  @param[in]  mi_option Type of the repository, e.g. TABLE.
  @param[in]  channel   the channel for which mi is to be created

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and makes the slave go out
  of sync.

  @retval Pointer to Master_info Success
  @retval NULL  Failure
*/
Master_info *Rpl_info_factory::create_mi(uint mi_option, const char *channel) {
  Master_info *mi = nullptr;
  Rpl_info_handler *handler = nullptr;
  uint instances = 1;
  const char *msg = "";

  bool is_error = false;

  Scope_guard cleanup_on_error([&] {
    if (is_error) {
      if (handler) delete handler;
      if (mi) {
        mi->set_rpl_info_handler(nullptr);
        mi->channel_wrlock();
        delete mi;
        mi = nullptr;
      }
      LogErr(ERROR_LEVEL, ER_RPL_ERROR_CREATING_CONNECTION_METADATA, msg);
    }
  });

  DBUG_TRACE;

  if (!(mi = new Master_info(
#ifdef HAVE_PSI_INTERFACE
            &key_source_info_run_lock, &key_source_info_data_lock,
            &key_source_info_sleep_lock, &key_source_info_thd_lock,
            &key_source_info_rotate_lock, &key_source_info_data_cond,
            &key_source_info_start_cond, &key_source_info_stop_cond,
            &key_source_info_sleep_cond, &key_source_info_rotate_cond,
#endif
            instances, channel))) {
    msg = "Failed to allocate memory for the connection metadata repository";
    is_error = true;
    return nullptr;
  }

  if (init_repository(mi_table_data, mi_option, &handler)) {
    msg = "Failed to initialize the connection metadata repository";
    is_error = true;
    return nullptr;
  }

  int error = handler->do_check_info(mi->get_internal_id());
  if (error == ERROR_CHECKING_REPOSITORY) {
    msg = "Error checking repositories";
    is_error = true;
    return nullptr;
  }

  if (mi->set_info_search_keys(handler)) {
    msg = "Failed to set keys for the connection metadata repository";
    is_error = true;
    return nullptr;
  }
  mi->set_rpl_info_handler(handler);
  return mi;
}

/**
  Creates a Relay log info repository whose type is defined as a parameter.

  @param[in]  rli_option        Type of the Relay log info repository
  @param[in]  is_slave_recovery If the slave should try to start a recovery
                                process to get consistent relay log files
  @param[in]  channel   the channel for which mi is to be created

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and make the slave go out
  of sync.

  @retval Pointer to Relay_log_info Success
  @retval NULL  Failure
*/
Relay_log_info *Rpl_info_factory::create_rli(uint rli_option,
                                             bool is_slave_recovery,
                                             const char *channel) {
  DBUG_TRACE;
  Relay_log_info *rli = nullptr;
  Rpl_info_handler *handler = nullptr;
  uint instances = 1;
  uint worker_repository = INVALID_INFO_REPOSITORY;
  const char *msg = "";
  Rpl_filter *rpl_filter = nullptr;
  bool is_error = false;

  Scope_guard cleanup_on_error([&] {
    if (is_error) {
      if (handler) delete handler;
      if (rli) {
        rli->set_rpl_info_handler(nullptr);
        delete rli;
        rli = nullptr;
      }
      LogErr(ERROR_LEVEL, ER_RPL_ERROR_CREATING_APPLIER_METADATA, msg);
    }
  });

  /*
    Returns how many occurrences of worker's repositories exist. For example,
    if the repository is a table, this retrieves the number of rows in it.
    Besides, it also returns the type of the repository where entries were
    found.
  */
  if (rli_option != INFO_REPOSITORY_DUMMY &&
      scan_and_check_repositories(worker_repository, worker_table_data)) {
    msg = "Error occurred while scanning and checking repositories";
    is_error = true;
    return nullptr;
  }
  if (!(rli = new Relay_log_info(
            is_slave_recovery,
#ifdef HAVE_PSI_INTERFACE
            &key_relay_log_info_run_lock, &key_relay_log_info_data_lock,
            &key_relay_log_info_sleep_lock, &key_relay_log_info_thd_lock,
            &key_relay_log_info_data_cond, &key_relay_log_info_start_cond,
            &key_relay_log_info_stop_cond, &key_relay_log_info_sleep_cond,
#endif
            instances, channel, (rli_option != INFO_REPOSITORY_TABLE)))) {
    msg = "Failed to allocate memory for the applier metadata repository";
    is_error = true;
    return nullptr;
  }

  if (init_repository(rli_table_data, rli_option, &handler)) {
    msg = "Failed to initialize the applier metadata repository";
    is_error = true;
    return nullptr;
  }

  int error = handler->do_check_info(rli->get_internal_id());
  if (error == ERROR_CHECKING_REPOSITORY) {
    msg = "Error checking repositories";
    is_error = true;
    return nullptr;
  }

  if (rli->set_info_search_keys(handler)) {
    msg = "Failed to set keys for the applier metadata repository";
    is_error = true;
    return nullptr;
  }
  rli->set_rpl_info_handler(handler);

  /*
    By this time, rli must be loaded with it's primary key,
    which is channel_name
  */

  /* Set filters here to guarantee that any rli object has a valid filter */
  rpl_filter = rpl_channel_filters.get_channel_filter(channel);
  if (rpl_filter == nullptr) {
    msg = "Creating filter failed";
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_FILTER_CREATE_FAILED, channel);
    is_error = true;
    return nullptr;
  }
  rli->set_filter(rpl_filter);
  rpl_filter->set_attached();
  return rli;
}

/**
   Delete all info from Worker info tables to render them useless in
   future MTS recovery, and indicate that in Coordinator info table.

   @retval false on success
   @retval true when a failure in deletion or writing to Coordinator table
   fails.
*/
bool Rpl_info_factory::reset_workers(Relay_log_info *rli) {
  bool error = true;

  DBUG_TRACE;

  /*
    Skip the optimization check if the last value of the number of workers
    might not have been persisted
  */
  if (rli->recovery_parallel_workers == 0 && !rli->mi->is_gtid_only_mode())
    return false;

  if (Rpl_info_table::do_reset_info(Slave_worker::get_number_worker_fields(),
                                    MYSQL_SCHEMA_NAME.str, WORKER_INFO_NAME.str,
                                    rli->channel,
                                    &worker_table_data.nullable_fields))
    goto err;

  error = false;

  DBUG_EXECUTE_IF("mta_debug_reset_workers_fails", error = true;);

err:
  if (error)
    LogErr(ERROR_LEVEL,
           ER_RPL_FAILED_TO_DELETE_FROM_REPLICA_WORKERS_INFO_REPOSITORY);
  rli->recovery_parallel_workers = 0;
  rli->clear_mts_recovery_groups();
  if (rli->flush_info(Relay_log_info::RLI_FLUSH_IGNORE_SYNC_OPT)) {
    error = true;
    LogErr(ERROR_LEVEL,
           ER_RPL_FAILED_TO_RESET_STATE_IN_REPLICA_INFO_REPOSITORY);
  }
  return error;
}

/**
  Creates a Slave worker repository whose type is defined as a parameter.

  @param[in]  rli_option Type of the repository, e.g. TABLE.
  @param[in]  worker_id  ID of the worker to be created.
  @param[in]  rli        Pointer to Relay_log_info.
  @param[in]  is_gaps_collecting_phase See Slave_worker::rli_init_info

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and make the slave go out
  of sync.

  @retval Pointer to Slave_worker Success
  @retval NULL  Failure
*/
Slave_worker *Rpl_info_factory::create_worker(uint rli_option, uint worker_id,
                                              Relay_log_info *rli,
                                              bool is_gaps_collecting_phase) {
  Rpl_info_handler *handler = nullptr;
  Slave_worker *worker = nullptr;
  const char *msg = "";
  bool is_error = false;

  Scope_guard cleanup_on_error([&] {
    if (is_error) {
      if (handler) delete handler;
      if (worker) {
        worker->set_rpl_info_handler(nullptr);
        delete worker;
        worker = nullptr;
      }
      LogErr(ERROR_LEVEL, ER_RPL_ERROR_CREATING_APPLIER_METADATA, msg);
    }
  });

  DBUG_TRACE;

  if (!(worker = new Slave_worker(
            rli,
#ifdef HAVE_PSI_INTERFACE
            &key_relay_log_info_run_lock, &key_relay_log_info_data_lock,
            &key_relay_log_info_sleep_lock, &key_relay_log_info_thd_lock,
            &key_relay_log_info_data_cond, &key_relay_log_info_start_cond,
            &key_relay_log_info_stop_cond, &key_relay_log_info_sleep_cond,
#endif
            worker_id, rli->get_channel()))) {
    msg = "Failed to allocate memory for the worker metadata repository";
    is_error = true;
    return nullptr;
  }

  if (init_repository(worker_table_data, rli_option, &handler)) {
    msg = "Failed to initialize the worker metadata repository";
    is_error = true;
    return nullptr;
  }

  if (worker->set_info_search_keys(handler)) {
    msg = "Failed to set keys for the worker metadata repository";
    is_error = true;
    return nullptr;
  }

  worker->set_rpl_info_handler(handler);

  if (DBUG_EVALUATE_IF("mta_worker_thread_init_fails", 1, 0) ||
      worker->rli_init_info(is_gaps_collecting_phase)) {
    DBUG_EXECUTE_IF("enable_mta_worker_failure_init", {
      DBUG_SET("-d,mta_worker_thread_init_fails");
      DBUG_SET("-d,enable_mta_worker_failure_init");
    });
    DBUG_EXECUTE_IF("enable_mta_wokrer_failure_in_recovery_finalize", {
      DBUG_SET("-d,mta_worker_thread_init_fails");
      DBUG_SET("-d,enable_mta_wokrer_failure_in_recovery_finalize");
    });
    msg = "Failed to create the worker metadata repository structure";
    is_error = true;
    return nullptr;
  }

  if (rli->info_thd && rli->info_thd->is_error()) {
    msg = "Failed to initialize worker data";
    is_error = true;
    return nullptr;
  }
  return worker;
}

/**
  Initializes startup information on different repositories.
*/
void Rpl_info_factory::init_repository_metadata() {
  rli_table_data.n_fields = Relay_log_info::get_number_info_rli_fields();
  rli_table_data.schema = MYSQL_SCHEMA_NAME.str;
  rli_table_data.name = RLI_INFO_NAME.str;
  rli_table_data.n_pk_fields = 0;
  rli_table_data.pk_field_indexes = nullptr;
  Relay_log_info::set_nullable_fields(&rli_table_data.nullable_fields);

  mi_table_data.n_fields = Master_info::get_number_info_mi_fields();
  mi_table_data.schema = MYSQL_SCHEMA_NAME.str;
  mi_table_data.name = MI_INFO_NAME.str;
  mi_table_data.n_pk_fields = 1;
  mi_table_data.pk_field_indexes = Master_info::get_table_pk_field_indexes();
  Master_info::set_nullable_fields(&mi_table_data.nullable_fields);

  worker_table_data.n_fields = Slave_worker::get_number_worker_fields();
  worker_table_data.schema = MYSQL_SCHEMA_NAME.str;
  worker_table_data.name = WORKER_INFO_NAME.str;
  worker_table_data.n_pk_fields = 2;
  worker_table_data.pk_field_indexes =
      Slave_worker::get_table_pk_field_indexes();
  Slave_worker::set_nullable_fields(&worker_table_data.nullable_fields);
}

/**
  This method is used to initialize the repositories through a low-level
  interface, which means that if they do not exist nothing will be created.

  @param[in]  info         Either master info or relay log info.
  @param[out] handler      Repository handler

  @retval false No error
  @retval true  Failure
*/
bool Rpl_info_factory::init_repository(Rpl_info *info,
                                       Rpl_info_handler **handler) {
  return ((*handler)->do_init_info(info->get_internal_id()));
}

/**
  Creates repositories that will be associated to either the Master_info
  or Relay_log_info.

  @param[in] table_data    Defines information to create a table repository.
  @param[in] rep_option    Identifies the type of the repository that will
                           be used, i.e., destination repository.
  @param[out] handler      Destination repository

  @retval false No error
  @retval true  Failure
*/
bool Rpl_info_factory::init_repository(const struct_table_data &table_data,
                                       uint rep_option,
                                       Rpl_info_handler **handler) {
  DBUG_TRACE;

  assert(handler != nullptr);
  switch (rep_option) {
    case INFO_REPOSITORY_TABLE:
      if (!(*handler = new Rpl_info_table(
                table_data.n_fields, table_data.schema, table_data.name,
                table_data.n_pk_fields, table_data.pk_field_indexes,
                &table_data.nullable_fields)))
        return true;
      break;

    case INFO_REPOSITORY_DUMMY:
      if (!(*handler =
                new Rpl_info_dummy(Master_info::get_number_info_mi_fields())))
        return true;
      break;

    default:
      assert(0);
  }
  return false;
}

bool Rpl_info_factory::scan_and_count_repositories(
    ulonglong &found_instances, uint &found_rep_option,
    const struct_table_data &table_data) {
  ulonglong table_instances = 0;

  DBUG_TRACE;

  if (Rpl_info_table::do_count_info(
          table_data.n_fields, table_data.schema, table_data.name,
          &table_data.nullable_fields, &table_instances)) {
    return true;
  }

  if (table_instances != 0) {
    found_instances = table_instances;
    found_rep_option = INFO_REPOSITORY_TABLE;
  } else {
    found_instances = 0;
    found_rep_option = INVALID_INFO_REPOSITORY;
  }

  return false;
}

bool Rpl_info_factory::scan_and_check_repositories(
    uint &found_rep_option, const struct_table_data &table_data) {
  DBUG_TRACE;

  auto [error, table_in_use] = Rpl_info_table::table_in_use(
      table_data.n_fields, table_data.schema, table_data.name,
      &table_data.nullable_fields);

  if (error) return true;

  if (table_in_use) {
    found_rep_option = INFO_REPOSITORY_TABLE;
  } else {
    found_rep_option = INVALID_INFO_REPOSITORY;
  }

  return false;
}

bool Rpl_info_factory::configure_channel_replication_filters(
    Relay_log_info *rli, const char *channel_name) {
  DBUG_TRACE;

  /*
    GROUP REPLICATION channels should not be configurable using
    --replicate* nor CHANGE REPLICATION FILTER, and should not
    inherit from global filters.
  */
  if (channel_map.is_group_replication_channel_name(channel_name)) return false;

  if (Master_info::is_configured(rli->mi)) {
    /*
      A slave replication channel would copy global replication filters
      to its per-channel replication filters if there are no per-channel
      replication filters and there are global filters on the filter type
      when it is being configured.
    */
    if (rli->rpl_filter->copy_global_replication_filters()) {
      LogErr(ERROR_LEVEL, ER_RPL_REPLICA_GLOBAL_FILTERS_COPY_FAILED,
             channel_name);
      return true;
    }
  } else {
    /*
      When starting server, users may set rpl filter options on an
      uninitialzied channel. The filter options will be reset with an
      warning.
    */
    if (!rli->rpl_filter->is_empty()) {
      LogErr(WARNING_LEVEL, ER_RPL_REPLICA_RESET_FILTER_OPTIONS, channel_name);
      rli->rpl_filter->reset();
    }
  }
  return false;
}

/**
  This function should be called from init_replica() only.

  During the server start, read all the replica repositories
  on disk and create corresponding Relay_log_info
  slave info objects. Each thus created object is
  added to pchannel_map.

  In a new server, an empty-named channel for a replica-source connection
  is created by default.
  If there are multiple 'named' channels, but and if a default_channel
  is not created, it is created.

 @note:  In general, the algorithm in creation of slave info object is:
          l1: new slave_info;
          l2: Initialize the repository handlers
          l3: if (default_channel)
                 check and convert repositories
              else
                   // TABLE type repository
                  set the value of PK in the TABLE handler.

  @param[in]       mi_option         the user provided repository type for MI
  @param[in]       rli_option        the user provided repository type for RLI
  @param[in]       thread_mask       thread mask
  @param[in]       pchannel_map          the pointer to the multi source map
                                     (see, rpl_msr.h)

  @retval          false             success
  @retval          true              fail
*/

bool Rpl_info_factory::create_slave_info_objects(
    uint mi_option, uint rli_option, int thread_mask,
    Multisource_info *pchannel_map) {
  DBUG_TRACE;

  Master_info *mi = nullptr;
  bool error = false, channel_error;
  bool default_channel_existed_previously = false;

  std::vector<std::string> channel_list;

  /* Number of instances of Master_info repository */
  ulonglong mi_instances = 0;

  /* At this point, the repository in invalid or unknown */
  uint mi_repository = INVALID_INFO_REPOSITORY;

  /*
    Number of instances of applier metadata repository.
    (Number of Slave worker objects that will be created by the Coordinator
    (when replica_parallel_workers>0) at a later stage and not here).
  */
  ulonglong rli_instances = 0;

  /* At this point, the repository is invalid or unknown */
  uint rli_repository = INVALID_INFO_REPOSITORY;

  /*
    Initialize the repository metadata. This metadata is the
    name of table to look in case of TABLE type repository.
  */
  Rpl_info_factory::init_repository_metadata();

  /* Count the number of Master_info and Relay_log_info repositories */
  if (scan_and_count_repositories(mi_instances, mi_repository, mi_table_data) ||
      scan_and_count_repositories(rli_instances, rli_repository,
                                  rli_table_data)) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_GENERIC_MESSAGE,
           "Failed to scan and count repository instances");
    error = true;
    goto end;
  }

  /* Make a list of all channels if the slave was connected to previously*/
  if (load_channel_names_from_repository(channel_list, mi_instances,
                                         mi_repository,
                                         pchannel_map->get_default_channel(),
                                         &default_channel_existed_previously)) {
    LogErr(ERROR_LEVEL, ER_RPL_REPLICA_COULD_NOT_CREATE_CHANNEL_LIST);
    error = true;
    goto end;
  }

  /* Adding the default channel if needed. */
  if (!default_channel_existed_previously) {
    std::string str(pchannel_map->get_default_channel());
    channel_list.push_back(str);
  }

  /*
    Create and initialize the channels.

    Even if there is an error during one channel creation, we continue to
    iterate until we have created the other channels.

    For compatibility reasons, we have to separate the print out
    of the error messages.
  */
  for (std::vector<std::string>::iterator it = channel_list.begin();
       it != channel_list.end(); ++it) {
    const char *cname = (*it).c_str();
    bool is_default_channel =
        !strcmp(cname, pchannel_map->get_default_channel());
    channel_error = !(mi = create_mi_and_rli_objects(mi_option, rli_option,
                                                     cname, pchannel_map));
    /*
      Read the channel configuration from the repository if the channel name
      was read from the repository.
    */
    if (!channel_error &&
        (!is_default_channel || default_channel_existed_previously)) {
      bool ignore_if_no_info = (channel_list.size() == 1) ? true : false;
      channel_error = load_mi_and_rli_from_repositories(
          mi, ignore_if_no_info, thread_mask, false, true);
    }

    if (!channel_error) {
      error = configure_channel_replication_filters(mi->rli, cname);
      invalidate_repository_position(mi);
      // With GTID ONLY the worker info is not needed
      if (mi->is_gtid_only_mode()) Rpl_info_factory::reset_workers(mi->rli);
    } else {
      LogErr(ERROR_LEVEL,
             ER_RPL_REPLICA_FAILED_TO_INIT_A_CONNECTION_METADATA_STRUCTURE,
             cname);
    }
    error = error || channel_error;
  }
end:
  return error;
}

/**
   Create Master_info and Relay_log_info objects for a new channel.
   Also, set cross dependencies between these objects used all over
   the code.

   Both master_info and relay_log_info repositories should be of the type
   TABLE. We do a check for this here as well.

   @param[in]    mi_option        master info repository
   @param[in]    rli_option       relay log info repository
   @param[in]    channel          the channel for which these objects
                                  should be created.
   @param[in]    pchannel_map     a pointer to channel_map

   @return      Pointer         pointer to the created Master_info
   @return      NULL            when creation fails

*/

Master_info *Rpl_info_factory::create_mi_and_rli_objects(
    uint mi_option, uint rli_option, const char *channel,
    Multisource_info *pchannel_map) {
  DBUG_TRACE;

  Master_info *mi = nullptr;
  Relay_log_info *rli = nullptr;

  if (!(mi = Rpl_info_factory::create_mi(mi_option, channel))) return nullptr;

  if (!(rli = Rpl_info_factory::create_rli(rli_option, relay_log_recovery,
                                           channel))) {
    mi->channel_wrlock();
    delete mi;
    mi = nullptr;
    return nullptr;
  }

  /* Set the cross dependencies used all over the code */
  mi->set_relay_log_info(rli);
  rli->set_master_info(mi);

  /* Add to multisource map*/
  if (pchannel_map->add_mi(channel, mi)) {
    mi->channel_wrlock();
    delete mi;
    delete rli;
    return nullptr;
  }

  return mi;
}

/**
   Make a list of any channels that may have existed on the previous slave run.

   @param[out]  channel_list    the names of all channels that exist
                                on this slave.

   @param[in]   mi_instances    number of master_info repositories

   @param[in]   mi_repository   Found master_info repository

   @param[in]   default_channel pointer to default channel.

   @param[out]  default_channel_existed_previously
                                Value filled with true if default channel
                                existed previously. False if it is not.

   @retval      true            fail
   @retval      false           success

*/

bool Rpl_info_factory::load_channel_names_from_repository(
    std::vector<std::string> &channel_list, uint mi_instances [[maybe_unused]],
    uint mi_repository, const char *default_channel,
    bool *default_channel_existed_previously) {
  DBUG_TRACE;

  *default_channel_existed_previously = false;
  switch (mi_repository) {
    case INFO_REPOSITORY_TABLE:
      if (load_channel_names_from_table(channel_list, default_channel,
                                        default_channel_existed_previously))
        return true;
      break;
    case INVALID_INFO_REPOSITORY:
      /* file and table instanaces are zero, nothing to be done*/
      break;
    default:
      assert(0);
  }

  return false;
}

/**
  In a multisourced slave, during init_replica(), the repositories
  are read to initialize the slave info objects. To initialize
  the slave info objects, we need the number of channels the slave
  was connected to previously. The following function, finds the
  number of channels in the master info repository.
  Later, this chanenl list is used to create a pair of {mi, rli}
  objects required for IO and SQL threads respectively.

  @param [out]  channel_list    A reference to the channel list.
                                This will be filled after reading the
                                master info table, row by row.

  @param[in]   default_channel  pointer to default channel.

  @param[out]  default_channel_existed_previously
                                Value filled with true if default channel
                                existed previously. False if it is not.

  @todo: Move it to Rpl_info_table and make it generic to extract
         all the PK list from the tables (but it not yet necessary)
*/
bool Rpl_info_factory::load_channel_names_from_table(
    std::vector<std::string> &channel_list, const char *default_channel,
    bool *default_channel_existed_previously) {
  DBUG_TRACE;

  int error = 1;
  TABLE *table = nullptr;
  ulong saved_mode;
  Open_tables_backup backup;
  Rpl_info_table *info = nullptr;
  THD *thd = nullptr;
  char buff[MAX_FIELD_WIDTH];
  *default_channel_existed_previously = false;
  String str(buff, sizeof(buff),
             system_charset_info);  // to extract channel names

  uint channel_field = Master_info::get_channel_field_num() - 1;

  if (!(info = new Rpl_info_table(mi_table_data.n_fields, mi_table_data.schema,
                                  mi_table_data.name, mi_table_data.n_pk_fields,
                                  mi_table_data.pk_field_indexes,
                                  &mi_table_data.nullable_fields)))
    return true;

  thd = info->access->create_thd();
  saved_mode = thd->variables.sql_mode;

  /*
     Opens and locks the rpl_info table before accessing it.
  */
  if (info->access->open_table(thd, to_lex_cstring(info->str_schema),
                               to_lex_cstring(info->str_table),
                               info->get_number_info(), TL_READ, &table,
                               &backup)) {
    /*
      We cannot simply print out a warning message at this
      point because this may represent a bootstrap.
    */
    error = 0;
    goto err;
  }

  /* Do ha_handler random init for full scanning */
  if ((error = table->file->ha_rnd_init(true))) return true;

  /* Ensure that the table pk (Channel_name) is at the correct position */
  if (info->verify_table_primary_key_fields(table)) {
    LogErr(ERROR_LEVEL,
           ER_RPL_REPLICA_FAILED_TO_CREATE_CHANNEL_FROM_CONNECTION_METADATA);
    error = -1;
    goto err;
  }

  /*
    Load all the values in record[0] for each row
    and then extract channel name from it
  */

  do {
    error = table->file->ha_rnd_next(table->record[0]);
    switch (error) {
      case 0:
        /* extract the channel name from table->field and append to the list */
        table->field[channel_field]->val_str(&str);
        channel_list.push_back(std::string(str.c_ptr_safe()));
        if (!strcmp(str.c_ptr_safe(), default_channel))
          *default_channel_existed_previously = true;
        break;

      case HA_ERR_END_OF_FILE:
        break;

      default:
        DBUG_PRINT("info", ("Failed to get next record"
                            " (ha_rnd_next returns %d)",
                            error));
    }
  } while (!error);

  /*close the table */
err:

  table->file->ha_rnd_end();
  info->access->close_table(thd, table, &backup, error);
  thd->variables.sql_mode = saved_mode;
  info->access->drop_thd(thd);
  delete info;
  return error != HA_ERR_END_OF_FILE && error != 0;
}

void Rpl_info_factory::invalidate_repository_position(Master_info *mi) {
  if (mi->is_gtid_only_mode()) {
    mi->set_receiver_position_info_invalid(true);
    mi->rli->set_applier_source_position_info_invalid(true);
  }
}
