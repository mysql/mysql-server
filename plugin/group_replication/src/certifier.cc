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

#include <assert.h>
#include <signal.h>
#include <time.h>
#include <map>

#include <mysql/components/services/log_builtins.h>
#include "mutex_lock.h"
#include "my_dbug.h"
#include "my_systime.h"
#include "mysql/gtid/tsid.h"
#include "plugin/group_replication/include/certifier.h"
#include "plugin/group_replication/include/observer_trans.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message_compressed_parts.h"
#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"
#include "scope_guard.h"

using namespace gr;

const std::string Certifier::GTID_EXTRACTED_NAME = "gtid_extracted";
const std::string Certifier::CERTIFICATION_INFO_ERROR_NAME =
    "certification_info_error";

static void *launch_broadcast_thread(void *arg) {
  Certifier_broadcast_thread *handler = (Certifier_broadcast_thread *)arg;
  handler->dispatcher();
  return nullptr;
}

Certifier_broadcast_thread::Certifier_broadcast_thread()
    : aborted(false),
      broadcast_thd_state(),
      broadcast_counter(0),
      broadcast_gtid_executed_period(BROADCAST_GTID_EXECUTED_PERIOD) {
  DBUG_EXECUTE_IF("group_replication_certifier_broadcast_thread_big_period",
                  { broadcast_gtid_executed_period = 600; });

  mysql_mutex_init(key_GR_LOCK_cert_broadcast_run, &broadcast_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_cert_broadcast_run, &broadcast_run_cond);
  mysql_mutex_init(key_GR_LOCK_cert_broadcast_dispatcher_run,
                   &broadcast_dispatcher_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_cert_broadcast_dispatcher_run,
                  &broadcast_dispatcher_cond);
}

Certifier_broadcast_thread::~Certifier_broadcast_thread() {
  mysql_mutex_destroy(&broadcast_run_lock);
  mysql_cond_destroy(&broadcast_run_cond);
  mysql_mutex_destroy(&broadcast_dispatcher_lock);
  mysql_cond_destroy(&broadcast_dispatcher_cond);
}

int Certifier_broadcast_thread::initialize() {
  DBUG_TRACE;

  mysql_mutex_lock(&broadcast_run_lock);
  if (broadcast_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&broadcast_run_lock); /* purecov: inspected */
    return 0;                                /* purecov: inspected */
  }

  aborted = false;

  if ((mysql_thread_create(key_GR_THD_cert_broadcast, &broadcast_pthd,
                           get_connection_attrib(), launch_broadcast_thread,
                           (void *)this))) {
    mysql_mutex_unlock(&broadcast_run_lock); /* purecov: inspected */
    return 1;                                /* purecov: inspected */
  }
  broadcast_thd_state.set_created();

  while (broadcast_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for certifier broadcast thread to start"));
    mysql_cond_wait(&broadcast_run_cond, &broadcast_run_lock);
  }
  mysql_mutex_unlock(&broadcast_run_lock);

  return 0;
}

int Certifier_broadcast_thread::terminate() {
  DBUG_TRACE;

  mysql_mutex_lock(&broadcast_run_lock);
  if (broadcast_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&broadcast_run_lock);
    return 0;
  }

  aborted = true;
  while (broadcast_thd_state.is_thread_alive()) {
    DBUG_PRINT("loop", ("killing certifier broadcast thread"));
    mysql_mutex_lock(&broadcast_thd->LOCK_thd_data);

    // awake the cycle
    mysql_mutex_lock(&broadcast_dispatcher_lock);
    mysql_cond_broadcast(&broadcast_dispatcher_cond);
    mysql_mutex_unlock(&broadcast_dispatcher_lock);

    broadcast_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&broadcast_thd->LOCK_thd_data);
    mysql_cond_wait(&broadcast_run_cond, &broadcast_run_lock);
  }
  mysql_mutex_unlock(&broadcast_run_lock);

  return 0;
}

void Certifier_broadcast_thread::dispatcher() {
  DBUG_TRACE;

  // Thread context operations
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();
  global_thd_manager_add_thd(thd);
  broadcast_thd = thd;

  mysql_mutex_lock(&broadcast_run_lock);
  broadcast_thd_state.set_running();
  mysql_cond_broadcast(&broadcast_run_cond);
  mysql_mutex_unlock(&broadcast_run_lock);

  while (!aborted) {
    // Broadcast Transaction identifiers every 30 seconds
    if (broadcast_counter % 30 == 0) {
      applier_module->get_pipeline_stats_member_collector()
          ->set_send_transaction_identifiers();
      if (applier_module->is_applier_thread_waiting()) {
        applier_module->get_pipeline_stats_member_collector()
            ->clear_transactions_waiting_apply();
      }
    }

    applier_module->run_flow_control_step();

    if (broadcast_counter % broadcast_gtid_executed_period == 0) {
      broadcast_gtid_executed();
    }

    Certification_handler *cert = applier_module->get_certification_handler();
    Certifier_interface *cert_module = (cert ? cert->get_certifier() : nullptr);

    // garbage_collect() is capable to identify if all information required
    // for it to run is already delivered to this member.
    if (cert_module) {
      cert_module->garbage_collect();
    }

    mysql_mutex_lock(&broadcast_dispatcher_lock);
    if (aborted) {
      mysql_mutex_unlock(&broadcast_dispatcher_lock); /* purecov: inspected */
      break;                                          /* purecov: inspected */
    }
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&broadcast_dispatcher_cond, &broadcast_dispatcher_lock,
                         &abstime);
    mysql_mutex_unlock(&broadcast_dispatcher_lock);

    broadcast_counter++;
  }

  Gcs_interface_factory::cleanup_thread_communication_resources(
      Gcs_operations::get_gcs_engine());

  thd->release_resources();
  global_thd_manager_remove_thd(thd);
  delete thd;
  my_thread_end();

  mysql_mutex_lock(&broadcast_run_lock);
  broadcast_thd_state.set_terminated();
  mysql_cond_broadcast(&broadcast_run_cond);
  mysql_mutex_unlock(&broadcast_run_lock);

  my_thread_exit(nullptr);
}

int Certifier_broadcast_thread::broadcast_gtid_executed() {
  DBUG_TRACE;

  /*
    Member may be still joining group so we need to check if:
      1) communication interfaces are ready to be used;
      2) member is ONLINE, that is, distributed recovery is complete.
  */
  if (local_member_info == nullptr) return 0; /* purecov: inspected */
  Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if (member_status != Group_member_info::MEMBER_ONLINE &&
      member_status != Group_member_info::MEMBER_IN_RECOVERY)
    return 0;

  int error = 0;
  uchar *encoded_gtid_executed = nullptr;
  size_t length;
  get_server_encoded_gtid_executed(&encoded_gtid_executed, &length);

  Gtid_Executed_Message gtid_executed_message;
  std::vector<uchar> encoded_gtid_executed_message;
  gtid_executed_message.append_gtid_executed(encoded_gtid_executed, length);

  enum enum_gcs_error send_err =
      gcs_module->send_message(gtid_executed_message, true);
  if (send_err == GCS_MESSAGE_TOO_BIG) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_BROADCAST_COMMIT_MSSG_TOO_BIG); /* purecov: inspected */
    error = 1;                                     /* purecov: inspected */
  } else if (send_err == GCS_NOK) {
    LogPluginErr(
        INFORMATION_LEVEL,
        ER_GRP_RPL_BROADCAST_COMMIT_TRANS_MSSG_FAILED); /* purecov: inspected */
    error = 1;                                          /* purecov: inspected */
  }

#if !defined(NDEBUG)
  char *encoded_gtid_executed_string =
      encoded_gtid_set_to_string(encoded_gtid_executed, length);
  DBUG_PRINT("info", ("Certifier broadcast executed_set: %s",
                      encoded_gtid_executed_string));
  my_free(encoded_gtid_executed_string);
#endif

  my_free(encoded_gtid_executed);
  return error;
}

Certifier::Certifier()
    : initialized(false),
      certification_info(
          Malloc_allocator<std::pair<const std::string, Gtid_set_ref *>>(
              key_certification_info)),
      positive_cert(0),
      negative_cert(0),
      parallel_applier_last_committed_global(1),
      parallel_applier_sequence_number(2),
      certifying_already_applied_transactions(false),
      conflict_detection_enable(!local_member_info->in_primary_mode()) {
  last_conflict_free_transaction.clear();

#if !defined(NDEBUG)
  certifier_garbage_collection_block = false;
  /*
    Debug flag to block the garbage collection and discard incoming stable
    set messages while garbage collection is on going.
  */
  DBUG_EXECUTE_IF("certifier_garbage_collection_block",
                  certifier_garbage_collection_block = true;);

  same_member_message_discarded = false;
  /*
    Debug flag to check for similar member sending multiple messages.
  */
  DBUG_EXECUTE_IF("certifier_inject_duplicate_certifier_data_message",
                  same_member_message_discarded = true;);
#endif

  certification_info_tsid_map = new Tsid_map(nullptr);
  incoming = new Synchronized_queue<Data_packet *>(key_certification_data_gc);

  stable_gtid_set_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_cert_stable_gtid_set
#endif
  );
  stable_tsid_map = new Tsid_map(stable_gtid_set_lock);
  stable_gtid_set = new Gtid_set(stable_tsid_map, stable_gtid_set_lock);
  broadcast_thread = new Certifier_broadcast_thread();

  group_gtid_tsid_map = new Tsid_map(nullptr);
  group_gtid_executed = new Gtid_set(group_gtid_tsid_map, nullptr);
  group_gtid_extracted = new Gtid_set(group_gtid_tsid_map, nullptr);

  mysql_mutex_init(key_GR_LOCK_certification_info, &LOCK_certification_info,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_cert_members, &LOCK_members, MY_MUTEX_INIT_FAST);
}

Certifier::~Certifier() {
  mysql_mutex_lock(&LOCK_certification_info);
  initialized = false;
  clear_certification_info();
  delete certification_info_tsid_map;

  delete stable_gtid_set;
  delete stable_tsid_map;
  delete stable_gtid_set_lock;
  delete group_gtid_executed;
  delete group_gtid_extracted;
  delete group_gtid_tsid_map;
  mysql_mutex_unlock(&LOCK_certification_info);
  delete broadcast_thread;

  mysql_mutex_lock(&LOCK_members);
  clear_members();
  clear_incoming();
  mysql_mutex_unlock(&LOCK_members);
  delete incoming;

  mysql_mutex_destroy(&LOCK_certification_info);
  mysql_mutex_destroy(&LOCK_members);
}

int Certifier::initialize_server_gtid_set(bool get_server_gtid_retrieved) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_certification_info);
  int error = 0;
  Get_system_variable *get_system_variable = nullptr;
  std::string gtid_executed;
  std::string applier_retrieved_gtids;

  gr::Gtid_tsid group_tsid;
  const char *group_name = get_group_name_var();
  gr::Gtid_tsid view_tsid;
  const char *view_uuid = get_view_change_uuid_var();
  if (group_tsid.from_cstring(group_name) == 0) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_GROUP_NAME_PARSE_ERROR); /* purecov: inspected */
    error = 1;                                       /* purecov: inspected */
    goto end;                                        /* purecov: inspected */
  }

  group_gtid_tsid_map_group_sidno = group_gtid_tsid_map->add_tsid(group_tsid);
  if (group_gtid_tsid_map_group_sidno < 0) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_ADD_GRPSID_TO_GRPGTIDSID_MAP_ERROR); /* purecov: inspected */
    error = 1;                                          /* purecov: inspected */
    goto end;                                           /* purecov: inspected */
  }

  if (group_gtid_executed->ensure_sidno(group_gtid_tsid_map_group_sidno) !=
      RETURN_STATUS_OK) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_UPDATE_GRPGTID_EXECUTED_ERROR); /* purecov: inspected */
    error = 1;                                     /* purecov: inspected */
    goto end;                                      /* purecov: inspected */
  }

  if (group_gtid_extracted->ensure_sidno(group_gtid_tsid_map_group_sidno) !=
      RETURN_STATUS_OK) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_DONOR_TRANS_INFO_ERROR); /* purecov: inspected */
    error = 1;                                       /* purecov: inspected */
    goto end;                                        /* purecov: inspected */
  }

  if (strcmp(view_uuid, "AUTOMATIC") == 0) {
    views_sidno_group_representation = group_gtid_tsid_map_group_sidno;
    views_sidno_server_representation = get_group_sidno();
  } else {
    if (view_tsid.from_cstring(view_uuid) == 0) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_VIEW_CHANGE_UUID_PARSE_ERROR);
      error = 1;
      goto end;
      /* purecov: end */
    }

    views_sidno_group_representation = group_gtid_tsid_map->add_tsid(view_tsid);
    if (views_sidno_group_representation < 0) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_ADD_VIEW_CHANGE_UUID_TO_GRP_SID_MAP_ERROR);
      error = 1;
      goto end;
      /* purecov: end */
    }
    views_sidno_server_representation = get_view_change_sidno();

    if (group_gtid_executed->ensure_sidno(views_sidno_group_representation) !=
        RETURN_STATUS_OK) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_UPDATE_GRPGTID_VIEW_CHANGE_UUID_EXECUTED_ERROR);
      error = 1;
      goto end;
      /* purecov: end */
    }

    if (group_gtid_extracted->ensure_sidno(views_sidno_group_representation) !=
        RETURN_STATUS_OK) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_DONOR_VIEW_CHANGE_UUID_TRANS_INFO_ERROR);
      error = 1;
      goto end;
      /* purecov: end */
    }
  }

  get_system_variable = new Get_system_variable();

  error = get_system_variable->get_global_gtid_executed(gtid_executed);
  DBUG_EXECUTE_IF("gr_server_gtid_executed_extraction_error", error = 1;);
  if (error) {
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_ERROR_FETCHING_GTID_EXECUTED_SET);
    goto end;
  }

  if (group_gtid_executed->add_gtid_text(gtid_executed.c_str()) !=
      RETURN_STATUS_OK) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_ADD_GTID_TO_GRPGTID_EXECUTED_ERROR); /* purecov: inspected */
    error = 1;                                          /* purecov: inspected */
    goto end;                                           /* purecov: inspected */
  }

  if (get_server_gtid_retrieved) {
    Replication_thread_api applier_channel("group_replication_applier");
    if (applier_channel.get_retrieved_gtid_set(applier_retrieved_gtids)) {
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_ERROR_FETCHING_GTID_SET); /* purecov: inspected */
      error = 1;                                        /* purecov: inspected */
      goto end;                                         /* purecov: inspected */
    }

    if (group_gtid_executed->add_gtid_text(applier_retrieved_gtids.c_str()) !=
        RETURN_STATUS_OK) {
      LogPluginErr(
          ERROR_LEVEL,
          ER_GRP_RPL_ADD_RETRIEVED_SET_TO_GRP_GTID_EXECUTED_ERROR); /* purecov:
                                                                       inspected
                                                                     */
      error = 1; /* purecov: inspected */
      goto end;  /* purecov: inspected */
    }
  }

  gtid_generator.recompute(*get_group_gtid_set());

end:
  delete get_system_variable;

  return error;
}

void Certifier::add_to_group_gtid_executed_internal(rpl_sidno sidno,
                                                    rpl_gno gno) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_certification_info);

  group_gtid_executed->_add_gtid(sidno, gno);
  /*
    We only need to track certified transactions on
    group_gtid_extracted while:
     1) certifier is handling already applied transactions
        on distributed recovery procedure;
     2) the transaction does have a group GTID.
     3) the transactions use the view UUID
  */
  if (certifying_already_applied_transactions &&
      (sidno == group_gtid_tsid_map_group_sidno ||
       sidno == views_sidno_group_representation))
    group_gtid_extracted->_add_gtid(sidno, gno);
}

void Certifier::clear_certification_info() {
  mysql_mutex_assert_owner(&LOCK_certification_info);
  for (Certification_info::iterator it = certification_info.begin();
       it != certification_info.end(); ++it) {
    // We can only delete the last reference.
    if (it->second->unlink() == 0) delete it->second;
  }

  certification_info.clear();
}

void Certifier::clear_incoming() {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_members);
  while (!this->incoming->empty()) {
    Data_packet *packet = nullptr;
    this->incoming->pop(&packet);
    delete packet;
  }
}

void Certifier::clear_members() {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_members);
  members.clear();
}

int Certifier::initialize(ulonglong gtid_assignment_block_size) {
  DBUG_TRACE;
  int error = 0;
  MUTEX_LOCK(guard, &LOCK_certification_info);

  if (is_initialized()) {
    return 1;
  }

  assert(gtid_assignment_block_size >= 1);

  gtid_generator.initialize(gtid_assignment_block_size);

  /*
    We need to initialize group_gtid_executed from both GTID_EXECUTED
    and applier retrieved GTID set to consider the already certified
    but not yet applied GTIDs, that may exist on applier relay log when
    this member is the one bootstrapping the group.
  */
  if (initialize_server_gtid_set(true)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CERTIFICATION_INITIALIZATION_FAILURE);
    return 1;
  }

  error = broadcast_thread->initialize();
  initialized = !error;
  return error;
}

int Certifier::terminate() {
  DBUG_TRACE;
  int error = 0;

  if (is_initialized()) error = broadcast_thread->terminate();

  return error;
}

void Certifier::increment_parallel_applier_sequence_number(
    bool update_parallel_applier_last_committed_global) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_certification_info);

  assert(parallel_applier_last_committed_global <
         parallel_applier_sequence_number);
  if (update_parallel_applier_last_committed_global)
    parallel_applier_last_committed_global = parallel_applier_sequence_number;

  parallel_applier_sequence_number++;
}

namespace {

/// @brief This function will add a given tsid into the gtid_set
/// In case adding tsid fails, plugin will report error_code
/// @param tsid Tsid to be added into gtid_set
/// @param gtid_set Gtid set into which tsid will be added
std::pair<rpl_sidno, mysql::utils::Return_status>
add_tsid_to_gtid_set_and_sid_map(gr::Gtid_tsid &tsid, Gtid_set &gtid_set) {
  // Add received transaction GTID tsid to TSID map in gtid_set
  auto certification_state = mysql::utils::Return_status::ok;
  auto sidno = gtid_set.get_tsid_map()->add_tsid(tsid);
  if (sidno < 1) {
    LogPluginErr(ERROR_LEVEL, ER_OUT_OF_RESOURCES);
    certification_state = mysql::utils::Return_status::error;
    sidno = 0;
  }
  if (gtid_set.ensure_sidno(sidno) != RETURN_STATUS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_OUT_OF_RESOURCES);
    certification_state = mysql::utils::Return_status::error;
    sidno = 0;
  }
  return std::make_pair(sidno, certification_state);
}

}  // namespace

std::tuple<rpl_sidno, rpl_sidno, rpl_sidno, mysql::utils::Return_status>
Certifier::extract_sidno(Gtid_log_event &gle, bool is_gtid_specified,
                         Gtid_set &snapshot_gtid_set,
                         Gtid_set &group_gtid_set) {
  std::tuple<rpl_sidno, rpl_sidno, rpl_sidno, mysql::utils::Return_status>
      result = std::make_tuple(0, 0, 0, mysql::utils::Return_status::error);

  // Get the tsid: either the specified one or the group's one.
  gr::Gtid_tsid tsid;
  rpl_sidno server_sidno;
  if (is_gtid_specified) {
    // SPECIFIED GTID
    tsid = gle.get_tsid();
    server_sidno = gle.get_sidno(true);
  } else {
    // AUTOMATIC tagged/untagged
    const char *group_name = get_group_name_var();
    server_sidno = get_group_sidno();
    std::ignore = tsid.from_cstring(group_name);
    if (gle.is_tagged()) {
      tsid.set_tag(gle.get_tsid().get_tag());
      server_sidno = get_sidno_from_global_tsid_map(tsid);
    }
  }

  if (server_sidno == -1) {
    LogPluginErr(ERROR_LEVEL, ER_OUT_OF_RESOURCES);
    return result;
  }

  // get snapshot sidno
  auto [snapshot_sidno, snapshot_add_code] =
      add_tsid_to_gtid_set_and_sid_map(tsid, snapshot_gtid_set);
  if (snapshot_add_code == mysql::utils::Return_status::error) {
    return result;
  }

  // get group sidno
  auto [group_sidno, group_add_code] =
      add_tsid_to_gtid_set_and_sid_map(tsid, group_gtid_set);
  if (group_add_code == mysql::utils::Return_status::error) {
    return result;
  }

  return std::make_tuple(group_sidno, snapshot_sidno, server_sidno,
                         mysql::utils::Return_status::ok);
}

Certified_gtid Certifier::end_certification_result(
    const rpl_sidno &gtid_global_sidno, const rpl_sidno &gtid_group_sidno,
    const rpl_gno &generated_gno, bool is_gtid_specified,
    bool local_transaction, const Certification_result &certification_result) {
  rpl_gno gno = generated_gno;
  if (certification_result == Certification_result::error) {
    gno = -1;
  } else if (certification_result == Certification_result::negative) {
    gno = 0;
  }
  DBUG_PRINT(
      "info",
      ("Group replication Certifier: certification result: %" PRId64, gno));
  Gtid server_gtid, group_gtid;
  server_gtid.clear();
  group_gtid.clear();
  server_gtid.sidno = gtid_global_sidno;
  group_gtid.sidno = gtid_group_sidno;
  server_gtid.gno = group_gtid.gno = gno;
  return Certified_gtid(server_gtid, group_gtid, is_gtid_specified,
                        local_transaction, certification_result);
}

Certification_result Certifier::add_writeset_to_certification_info(
    int64 &transaction_last_committed, Gtid_set *snapshot_version,
    std::list<const char *> *write_set, bool local_transaction) {
  // Only consider remote transactions for parallel applier indexes.
  int64 transaction_sequence_number =
      local_transaction ? -1 : parallel_applier_sequence_number;
  Gtid_set_ref *snapshot_version_value = new Gtid_set_ref(
      certification_info_tsid_map, transaction_sequence_number);
  if (snapshot_version_value->add_gtid_set(snapshot_version) !=
      RETURN_STATUS_OK) {
    delete snapshot_version_value;
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UPDATE_TRANS_SNAPSHOT_REF_VER_ERROR);
    return Certification_result::error;
  }

  for (std::list<const char *>::iterator it = write_set->begin();
       it != write_set->end(); ++it) {
    int64 item_previous_sequence_number = -1;

    add_item(*it, snapshot_version_value, &item_previous_sequence_number);

    /*
      Exclude previous sequence number that are smaller than global
      last committed and that are the current sequence number.
      transaction_last_committed is initialized with
      parallel_applier_last_committed_global on the beginning of
      "certify" method.
    */
    if (item_previous_sequence_number > transaction_last_committed &&
        item_previous_sequence_number != parallel_applier_sequence_number)
      transaction_last_committed = item_previous_sequence_number;
  }
  return Certification_result::positive;
}

namespace {

/*
  Only throw the error if the gtid is both on group_gtid_executed
  and executed_gtids due to the following scenario(bug#34157846):

  It is possible that gtid can be present in group_gtid_executed but
  not in executed_gtids(i.e the gtid is not logged in the binary
  log).
   1)replica-worker - starts transaction execution,
                      slave_worker_exec_event()->..calls
                      group_replication_trans_before_commit.
   2)gr-applier     - certifies the transaction and add gtid to
                      group_gtid_executed.
   3)replica-worker - proceeds to commit but commit order deadlock
                      occurred and rollbacked the transaction.
   4)replica-worker - retries the transaction,
                      i) calls group_replication_trans_before_commit.
                      ii) gr-applier tries to certify again the retried
                          transaction.
                      iii) retry certification would fail, if there is no
                           check on gtid present in both executed_gtids
                           and group_gtid_executed, since gtid is already
                           added to group_gtid_executed as part of initial
                           try(step 2).
*/
[[NODISCARD]] Certification_result check_gtid_collision(
    rpl_sidno gtid_group_sidno, rpl_sidno gtid_global_sidno, rpl_gno gno,
    Gtid_set &group_gtid_executed, const std::string &sid_str) {
  if (group_gtid_executed.contains_gtid(gtid_group_sidno, gno)) {
    // sidno is relative to global_tsid_map.
    Gtid gtid = {gtid_global_sidno, gno};
    if (is_gtid_committed(gtid)) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GTID_ALREADY_USED, sid_str.c_str(),
                   gno);
      return Certification_result::negative;
    }
  }
  return Certification_result::positive;
}

}  // namespace

void Certifier::update_transaction_dependency_timestamps(
    Gtid_log_event &gle, bool has_write_set, bool has_write_set_large_size,
    int64 transaction_last_committed) {
  bool update_parallel_applier_last_committed_global = false;

  /*
    'CREATE TABLE ... AS SELECT' is considered a DML, though in reality it
    is DDL + DML, which write-sets do not capture all dependencies.
    It is flagged through gle->last_committed and gle->sequence_number so
    that it is only executed on parallel applier after all precedent
    transactions like any other DDL.
  */
  if (0 == gle.last_committed && 0 == gle.sequence_number) {
    update_parallel_applier_last_committed_global = true;
  }

  if (!has_write_set || has_write_set_large_size ||
      update_parallel_applier_last_committed_global) {
    /*
      DDL does not have write-set, so we need to ensure that it
      is applied without any other transaction in parallel.
    */
    transaction_last_committed = parallel_applier_sequence_number - 1;
  }

  gle.last_committed = transaction_last_committed;
  gle.sequence_number = parallel_applier_sequence_number;
  assert(gle.last_committed >= 0);
  assert(gle.sequence_number > 0);
  assert(gle.last_committed < gle.sequence_number);

  increment_parallel_applier_sequence_number(
      !has_write_set || has_write_set_large_size ||
      update_parallel_applier_last_committed_global);

  /*
    Every Group Replication is started and the first remote transaction
    is queued on replication_group_applier channel, we need to reset
    applier internal previous sequence_number. Otherwise, if during the
    start there was backlog to apply on replication_group_applier channel,
    the previous sequence_number will be greater than the new one, which
    is considered a error case.
    Previously this reset was done by the View_change_log_event transaction,
    but now that transaction may not be logged.
  */
  if (is_first_remote_transaction_certified) {
    is_first_remote_transaction_certified = false;
    gle.last_committed = 0;
    gle.sequence_number = 0;
  }
}

void debug_print_group_gtid_sets(const Gtid_set &group_gtid_executed,
                                 const Gtid_set &group_gtid_extracted,
                                 bool set_value) {
#ifndef NDEBUG
  char *group_gtid_executed_string = nullptr;
  char *group_gtid_extracted_string = nullptr;
  group_gtid_executed.to_string(&group_gtid_executed_string, true);
  group_gtid_extracted.to_string(&group_gtid_extracted_string, true);
  DBUG_PRINT(
      "info",
      ("Set certifying_already_applied_transactions to %d. "
       "group_gtid_executed: \"%s\"; group_gtid_extracted_string: \"%s\"",
       set_value, group_gtid_executed_string, group_gtid_extracted_string));
  my_free(group_gtid_executed_string);
  my_free(group_gtid_extracted_string);
#endif
}

Certified_gtid Certifier::certify(Gtid_set *snapshot_version,
                                  std::list<const char *> *write_set,
                                  bool is_gtid_specified,
                                  const char *member_uuid, Gtid_log_event *gle,
                                  bool local_transaction) {
  DBUG_TRACE;

  rpl_sidno gtid_group_sidno = 0, gtid_snapshot_sidno = 0,
            gtid_global_sidno = 0;

  rpl_gno gtid_gno = 0;

  const bool has_write_set = !write_set->empty();
  bool write_set_large_size = false;

  auto end_certification = [
    &is_gtid_specified, &gtid_global_sidno, &gtid_group_sidno, &gtid_gno,
    local_transaction,
    this
  ](Certification_result result) -> auto {
    update_certified_transaction_count(result == Certification_result::positive,
                                       local_transaction);
    return end_certification_result(gtid_global_sidno, gtid_group_sidno,
                                    gtid_gno, is_gtid_specified,
                                    local_transaction, result);
  };

  if (!is_initialized()) {
    return end_certification(Certification_result::error);
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);
  int64 transaction_last_committed = parallel_applier_last_committed_global;

  DBUG_EXECUTE_IF("certifier_force_1_negative_certification", {
    DBUG_SET("-d,certifier_force_1_negative_certification");
    return end_certification(Certification_result::negative);
  });

  if (conflict_detection_enable) {
    for (std::list<const char *>::iterator it = write_set->begin();
         it != write_set->end(); ++it) {
      Gtid_set *certified_write_set_snapshot_version =
          get_certified_write_set_snapshot_version(*it);

      /*
        If the previous certified transaction snapshot version is not
        a subset of the incoming transaction snapshot version, the current
        transaction was executed on top of outdated data, so it will be
        negatively certified. Otherwise, this transaction is marked
        certified and goes into applier.
      */
      if (certified_write_set_snapshot_version != nullptr &&
          !certified_write_set_snapshot_version->is_subset(snapshot_version))
        return end_certification(Certification_result::negative);
    }
  }

  if (certifying_already_applied_transactions &&
      !group_gtid_extracted->is_subset_not_equals(group_gtid_executed)) {
    certifying_already_applied_transactions = false;

    debug_print_group_gtid_sets(*group_gtid_executed, *group_gtid_extracted,
                                false);
  }

  mysql::utils::Return_status certification_state;
  std::tie(gtid_group_sidno, gtid_snapshot_sidno, gtid_global_sidno,
           certification_state) =
      extract_sidno(*gle, is_gtid_specified, *snapshot_version,
                    *group_gtid_executed);

  if (certification_state == mysql::utils::Return_status::error) {
    return end_certification(Certification_result::error);
  }

  /*
    If the current transaction doesn't have a specified GTID, one
    for group UUID will be generated.
    This situation happens when transactions are executed with
    GTID_NEXT equal to AUTOMATIC_GTID (the default case).
  */
  if (!is_gtid_specified) {
    mysql::utils::Return_status gno_generation_result;
    std::tie(gtid_gno, gno_generation_result) =
        gtid_generator.get_next_available_gtid(member_uuid, gtid_group_sidno,
                                               *get_group_gtid_set());
    if (gno_generation_result != mysql::utils::Return_status::ok) {
      return end_certification(Certification_result::error);
    }
    DBUG_PRINT("info", ("Group replication Certifier: generated transaction "
                        "identifier: %" PRId64,
                        gtid_gno));
  } else {
    gtid_gno = gle->get_gno();
    auto tsid_str = gle->get_tsid().to_string();
    auto gtid_collision_check_code =
        check_gtid_collision(gtid_group_sidno, gtid_global_sidno, gtid_gno,
                             *group_gtid_executed, tsid_str);
    if (gtid_collision_check_code == Certification_result::negative) {
      return end_certification(Certification_result::negative);
    }
    DBUG_PRINT(
        "info",
        ("Group replication Certifier: there was no transaction identifier "
         "generated since transaction already had a GTID specified"));
  }

  // Add received transaction GTID to transaction snapshot version.
  snapshot_version->_add_gtid(gtid_snapshot_sidno, gtid_gno);

  // Store last conflict free transaction identification.
  // sidno must be relative to group_gtid_sid_map.
  last_conflict_free_transaction.set(gtid_group_sidno, gtid_gno);

  /*
    When the group is in single-primary mode and
    group_replication_preemptive_garbage_collection is enabled, if the number
    of write-sets on a transaction is equal or greater than
    group_replication_preemptive_garbage_collection_rows_threshold, the
    write-sets are not added to certification info and the last_committed
    timestamps is incremented.
  */
  if (get_single_primary_mode_var() &&
      get_preemptive_garbage_collection_var() &&
      write_set->size() >=
          get_preemptive_garbage_collection_rows_threshold_var()) {
    write_set_large_size = true;
  }

  /*
    Add the transaction's write set to certification info.
  */
  if (has_write_set && !write_set_large_size) {
    auto add_writeset_code = add_writeset_to_certification_info(
        transaction_last_committed, snapshot_version, write_set,
        local_transaction);
    if (add_writeset_code != Certification_result::positive) {
      return end_certification(Certification_result::error);
    }
  }

  // Update parallel applier indexes for local transactions
  if (!local_transaction) {
    update_transaction_dependency_timestamps(
        *gle, has_write_set, write_set_large_size, transaction_last_committed);
  }

  return end_certification(Certification_result::positive);
}

int Certifier::add_gtid_to_group_gtid_executed(const Gtid &gtid) {
  DBUG_TRACE;

  if (!is_initialized()) {
    return 1;
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);
  add_to_group_gtid_executed_internal(gtid.sidno, gtid.gno);
  return 0;
}

const Gtid_set *Certifier::get_group_gtid_set() const {
  return certifying_already_applied_transactions ? group_gtid_extracted
                                                 : group_gtid_executed;
}

Gtid_set *Certifier::get_group_gtid_set() {
  return certifying_already_applied_transactions ? group_gtid_extracted
                                                 : group_gtid_executed;
}

void Certifier::gtid_intervals_computation() {
  DBUG_TRACE;

  if (!is_initialized()) {
    return;
  }

  mysql_mutex_lock(&LOCK_certification_info);
  if (gtid_generator.get_gtid_assignment_block_size() > 1) {
    gtid_generator.recompute(*get_group_gtid_set());
  }
  mysql_mutex_unlock(&LOCK_certification_info);
}

bool Certifier::add_item(const char *item, Gtid_set_ref *snapshot_version,
                         int64 *item_previous_sequence_number) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_certification_info);
  bool error = true;
  std::string key(item);
  Certification_info::iterator it = certification_info.find(key);
  snapshot_version->link();

  if (it == certification_info.end()) {
    std::pair<Certification_info::iterator, bool> ret =
        certification_info.insert(
            std::pair<std::string, Gtid_set_ref *>(key, snapshot_version));
    error = !ret.second;
  } else {
    *item_previous_sequence_number =
        it->second->get_parallel_applier_sequence_number();

    if (it->second->unlink() == 0) delete it->second;

    it->second = snapshot_version;
    error = false;
  }

  DBUG_EXECUTE_IF("group_replication_certifier_after_add_item", {
    const char act[] =
        "now signal "
        "signal.group_replication_certifier_after_add_item_reached "
        "wait_for "
        "signal.group_replication_certifier_after_add_item_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  return error;
}

Gtid_set *Certifier::get_certified_write_set_snapshot_version(
    const char *item) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&LOCK_certification_info);

  if (!is_initialized()) return nullptr; /* purecov: inspected */

  Certification_info::iterator it;
  std::string item_str(item);

  it = certification_info.find(item_str);

  if (it == certification_info.end())
    return nullptr;
  else
    return it->second;
}

int Certifier::get_group_stable_transactions_set_string(char **buffer,
                                                        size_t *length) {
  DBUG_TRACE;
  int error = 1;

  if (!is_initialized()) {
    return 1;
  }

  /*
    Stable transactions set may not be accurate during recovery,
    thence we do not externalize it on
    performance_schema.replication_group_member_stats table.
  */
  if (local_member_info->get_recovery_status() ==
      Group_member_info::MEMBER_IN_RECOVERY) {
    return 0;
  }

  char *m_buffer = nullptr;
  int m_length = stable_gtid_set->to_string(&m_buffer, true);
  if (m_length >= 0) {
    *buffer = m_buffer;
    *length = static_cast<size_t>(m_length);
    error = 0;
  } else
    my_free(m_buffer); /* purecov: inspected */

  return error;
}

void Certifier::garbage_collect(Gtid_set *executed_gtid_set,
                                bool on_member_join) {
  DBUG_TRACE;

  bool update_metrics = false;

  if (!is_initialized()) return; /* purecov: inspected */

  /* Start garbage collection duration. */
  const auto garbage_collection_begin = Metrics_handler::get_current_time();

  if (!on_member_join) {
    assert(nullptr == executed_gtid_set);

    if (get_single_primary_mode_var() &&
        get_preemptive_garbage_collection_var() &&
        get_certification_info_size() >=
            get_preemptive_garbage_collection_rows_threshold_var()) {
      garbage_collect_internal(nullptr, true);
      update_metrics = true;
    }

    if (intersect_members_gtid_executed_and_garbage_collect()) {
      update_metrics = true;
    }

  } else {
    /* executed_gtid_set only is empty when gtid_executed don't have
     * any change, for example, when a group do boostrap without any
     * GTID.
     * To avoid don't have a increment on garbage collector counter on
     * a view change we also do it when executed_gtid_set is empty.
     */
    update_metrics = true;
    if (!executed_gtid_set->is_empty()) {
      garbage_collect_internal(executed_gtid_set);
    }
  }

  if (update_metrics) {
    /* Update garbage collection metrics. */
    const auto garbage_collection_end = Metrics_handler::get_current_time();
    metrics_handler->add_garbage_collection_run(garbage_collection_begin,
                                                garbage_collection_end);
  }
}

void Certifier::garbage_collect_internal(Gtid_set *executed_gtid_set,
                                         bool preemptive) {
  DBUG_TRACE;

  if (!is_initialized()) {
    return;
  }

  /*
    This debug option works on every call to garbage_collect
    by disabling the garbage collection.
    Calls to garbage collect happen:
     1) when a member joins.
     2) periodically, using the the intersection of members gtid executed.
        Period that can be controlled by the debug flag.
        `group_replication_certifier_broadcast_thread_big_period`.
     3) preemptively, please see option
        `group_replication_preemptive_garbage_collection`.
  */
  DBUG_EXECUTE_IF("group_replication_do_not_clear_certification_database",
                  { return; };);

  /*
   If `executed_gtid_set` is already contained on `stable_gtid_set`,
   no new transactions were committed on all members after the last
   garbage collection run, thence there is nothing to garbage collect
   with `executed_gtid_set`.
  */
  if (!preemptive &&
      update_stable_set(*executed_gtid_set) != Certifier::STABLE_SET_UPDATED) {
    return;
  }

  /*
    Data structures to hold a copy of certified gtids so that we can
    use them without require to hold `LOCK_certification_info`.
  */
  bool update_stable_set_after_preemptive_garbage_collection = false;
  Tsid_map certified_gtids_copy_sid_map(nullptr);
  Gtid_set certified_gtids_copy_set(&certified_gtids_copy_sid_map, nullptr);

  {
    MUTEX_LOCK(lock, &LOCK_certification_info);

    if (preemptive) {
      assert(nullptr == executed_gtid_set);

      if (!get_single_primary_mode_var() ||
          !get_preemptive_garbage_collection_var()) {
        return;
      }
      /*
       On preemptive garbage collect runs we use group_gtid_executed,
       we are on single primary so if transactions are certified by
       the group we can add to stable gtid set and clear all certification
       info.
      */
      clear_certification_info();
      update_stable_set_after_preemptive_garbage_collection = true;
      certified_gtids_copy_set.add_gtid_set(group_gtid_executed);
    }

    else {
      /*
        When a transaction "t" is applied to all group members and for all
        ongoing, i.e., not yet committed or aborted transactions,
        "t" was already committed when they executed (thus "t"
        precedes them), then "t" is stable and can be removed from
        the certification info.
        */
      Certification_info::iterator it = certification_info.begin();
      stable_gtid_set_lock->wrlock();

      while (it != certification_info.end()) {
        if (it->second->is_subset_not_equals(stable_gtid_set)) {
          if (it->second->unlink() == 0) delete it->second;
          certification_info.erase(it++);
        } else
          ++it;
      }
      stable_gtid_set_lock->unlock();
    }

    /*
      We need to update parallel applier indexes since we do not know
      what write sets were purged, which may cause transactions
      last committed to be incorrectly computed.
      */
    increment_parallel_applier_sequence_number(true);

#if !defined(NDEBUG)
    /*
      This part blocks the garbage collection process for 300 sec in order to
      simulate the case that while garbage collection is going on, we should
      skip the stable set messages round in order to prevent simultaneous
      access to stable_gtid_set.
      */
    if (certifier_garbage_collection_block) {
      certifier_garbage_collection_block = false;
      // my_sleep expects a given number of microseconds.
      my_sleep(broadcast_thread->BROADCAST_GTID_EXECUTED_PERIOD * 1500000);
    }
#endif
  }

  /*
    Update stable set using a copy of certified gtids so that we dot not
    require to hold `LOCK_certification_info`.
  */
  if (preemptive && update_stable_set_after_preemptive_garbage_collection) {
    update_stable_set(certified_gtids_copy_set);
  }

  /*
    Applier channel received set does only contain the GTIDs of the
    remote (committed by other members) transactions. On the long
    term, the gaps may create performance issues on the received
    set update. To avoid that, periodically, we update the received
    set with the full set of transactions committed on the group,
    closing the gaps.
  */
  if (channel_add_executed_gtids_to_received_gtids(
          applier_module_channel_name)) {
    LogPluginErr(
        WARNING_LEVEL,
        ER_GRP_RPL_RECEIVED_SET_MISSING_GTIDS); /* purecov: inspected */
  }
}

Certifier::enum_update_status Certifier::update_stable_set(
    const Gtid_set &set) {
  DBUG_TRACE;
  Checkable_rwlock::Guard g(*stable_gtid_set_lock,
                            Checkable_rwlock::WRITE_LOCK);

  if (set.is_subset(stable_gtid_set)) {
    return STABLE_SET_ALREADY_CONTAINED;
  }

  if (stable_gtid_set->add_gtid_set(&set) != RETURN_STATUS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SET_STABLE_TRANS_ERROR);
    return STABLE_SET_ERROR;
  }
  return STABLE_SET_UPDATED;
}

int Certifier::handle_certifier_data(
    const uchar *data, ulong len, const Gcs_member_identifier &gcs_member_id) {
  DBUG_TRACE;
  bool member_message_received = false;

  if (!is_initialized()) return 1; /* purecov: inspected */

  /*
    On members recovering through clone the GTID_EXECUTED is only
    updated after the server restart that finishes the procedure.
    During that procedure they will periodically send the GTID_EXECUTED
    that the server had once joined the group. This will restrain the
    common set of transactions applied on all members, which in consequence
    will render the certification garbage collection void.
    As such, we only consider ONLINE members for the common set of
    transactions applied on all members.
    When recovering members change to ONLINE state, their certification
    info will be updated with the one of the donor at the join, being
    garbage collect on the future calls of this method.
  */
  if (group_member_mgr->get_group_member_status_by_member_id(gcs_member_id) !=
      Group_member_info::MEMBER_ONLINE) {
    return 0;
  }

  mysql_mutex_lock(&LOCK_members);
  std::string member_id = gcs_member_id.get_member_id();
#if !defined(NDEBUG)
  if (same_member_message_discarded) {
    /*
      Injecting the member_id in the member's vector to simulate the case of
      same member sending multiple messages.
    */
    this->members.push_back(member_id);
  }
#endif

  const size_t number_of_members_online =
      group_member_mgr->get_number_of_members_online();
  if (this->members.size() != number_of_members_online) {
    /*
      We check for the member_id of the current message if it is present in
      the member vector or not. If it is present, we will need to discard the
      message. If not we will add the message in the incoming message
      synchronized queue for stable set handling.
    */
    std::vector<std::string>::iterator it;
    it = std::find(members.begin(), members.end(), member_id);
    if (it != members.end())
      member_message_received = true;
    else
      this->members.push_back(member_id);

    /*
      Since member is not present we can queue this message.
    */
    if (!member_message_received) {
      this->incoming->push(
          new Data_packet(data, len, key_certification_data_gc));
    }
    // else: ignore the message, no point in alerting the user about this.
  }

#if !defined(NDEBUG)
  if (same_member_message_discarded) {
    /*
      Clearing the flag here as the members vector is not cleaned above.
    */
    same_member_message_discarded = false;
    clear_members();
  }
#endif

  mysql_mutex_unlock(&LOCK_members);
  return 0;
}

bool Certifier::intersect_members_gtid_executed_and_garbage_collect() {
  DBUG_TRACE;

  if (!is_initialized() || nullptr == group_member_mgr) {
    return false;
  }

  /*
    If the incoming message queue size is equal to the number of the ONLINE
    members in the group, we are sure that each ONLINE member has sent
    their gtid_executed. So we can go ahead with the stable set handling.
  */
  mysql_mutex_lock(&LOCK_members);
  const size_t incoming_size = this->incoming->size();
  const size_t number_of_members_online =
      group_member_mgr->get_number_of_members_online();
  if (incoming_size < 1 || number_of_members_online < 1 ||
      incoming_size != number_of_members_online) {
    mysql_mutex_unlock(&LOCK_members);
    return false;
  }

  Data_packet *packet = nullptr;
  int error = 0;

  Tsid_map tsid_map(nullptr);
  Gtid_set executed_set(&tsid_map, nullptr);

  /*
    Compute intersection between all received sets.
  */
  while (!error && !this->incoming->empty()) {
    this->incoming->pop(&packet);

    if (packet == nullptr) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_NULL_PACKET); /* purecov: inspected */
      error = 1;                            /* purecov: inspected */
      break;                                /* purecov: inspected */
    }

    uchar *payload = packet->payload;
    Gtid_set member_set(&tsid_map, nullptr);
    Gtid_set intersection_result(&tsid_map, nullptr);

    if (member_set.add_gtid_encoding(payload, packet->len) !=
        RETURN_STATUS_OK) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_CANT_READ_GTID); /* purecov: inspected */
      error = 1;                               /* purecov: inspected */
    } else {
      /*
        First member set? If so we only need to add it to executed set.
      */
      if (executed_set.is_empty()) {
        if (executed_set.add_gtid_set(&member_set)) {
          LogPluginErr(
              ERROR_LEVEL,
              ER_GRP_RPL_PROCESS_GTID_SET_ERROR); /* purecov: inspected */
          error = 1;                              /* purecov: inspected */
        }
      } else {
        /*
          We have three sets:
            member_set:          the one sent from a given member;
            executed_set:        the one that contains the intersection of
                                 the computed sets until now;
            intersection_result: the intersection between set and
                                 intersection_result.
          So we compute the intersection between set and executed_set, and
          set that value to executed_set to be used on the next intersection.
        */
        if (member_set.intersection(&executed_set, &intersection_result) !=
            RETURN_STATUS_OK) {
          LogPluginErr(
              ERROR_LEVEL,
              ER_GRP_RPL_PROCESS_INTERSECTION_GTID_SET_ERROR); /* purecov:
                                                                  inspected */
          error = 1; /* purecov: inspected */
        } else {
          executed_set.clear();
          if (executed_set.add_gtid_set(&intersection_result) !=
              RETURN_STATUS_OK) {
            LogPluginErr(
                ERROR_LEVEL,
                ER_GRP_RPL_PROCESS_GTID_SET_ERROR); /* purecov: inspected */
            error = 1;                              /* purecov: inspected */
          }
        }
      }
    }

    delete packet;
  }

#if !defined(NDEBUG)
  char *executed_set_string;
  executed_set.to_string(&executed_set_string);
  DBUG_PRINT("info",
             ("Certifier intersect_members_gtid_executed_and_garbage_collect: "
              "executed_set: %s",
              executed_set_string));
  my_free(executed_set_string);
#endif

  /*
    Clearing the members to proceed with the next round of garbage
    collection.
  */
  clear_members();
  mysql_mutex_unlock(&LOCK_members);

  if (!error) {
    garbage_collect_internal(&executed_set);
    return true;
  }

  return false;
}

void Certifier::handle_view_change() {
  DBUG_TRACE;

  if (!is_initialized()) {
    return;
  }

  mysql_mutex_lock(&LOCK_members);
  clear_incoming();
  clear_members();
  mysql_mutex_unlock(&LOCK_members);
}

void Certifier::get_certification_info(
    std::map<std::string, std::string> *cert_info) {
  DBUG_TRACE;

  if (!is_initialized()) {
    return;
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);

  for (Certification_info::iterator it = certification_info.begin();
       it != certification_info.end(); ++it) {
    std::string key = it->first;
    assert(key.compare(GTID_EXTRACTED_NAME) != 0);

    size_t len = it->second->get_encoded_length();
    uchar *buf = (uchar *)my_malloc(key_certification_data, len, MYF(0));
    it->second->encode(buf);
    std::string value(reinterpret_cast<const char *>(buf), len);
    my_free(buf);

    (*cert_info).insert(std::pair<std::string, std::string>(key, value));
  }

  // Add the group_gtid_executed to certification info sent to joiners.
  size_t len = group_gtid_executed->get_encoded_length();
  uchar *buf = (uchar *)my_malloc(key_certification_data, len, MYF(0));
  group_gtid_executed->encode(buf);
  std::string value(reinterpret_cast<const char *>(buf), len);
  my_free(buf);
  (*cert_info)
      .insert(std::pair<std::string, std::string>(GTID_EXTRACTED_NAME, value));
}

bool Certifier::set_certification_info_recovery_metadata(
    Recovery_metadata_message *recovery_metadata_message) {
  /*
    1. Get Compressed Certification info packet count from the received
       recovery metadata.
  */
  std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
            unsigned int>
      payload_certification_info_packet_count_error =
          recovery_metadata_message
              ->get_decoded_compressed_certification_info_packet_count();

  /*
    1.1. If certification info packet count is 0 which means certification info
         payload is empty return false as recovery still need to process.
  */
  if (payload_certification_info_packet_count_error.first ==
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          ERR_CERT_INFO_EMPTY) {
    return false;
  }

  // 1.2. If error while decoding certification info packet count, return error.
  if (payload_certification_info_packet_count_error.first !=
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          RECOVERY_METADATA_MESSAGE_OK) {
    return true;
  }

  // 1.3. Get certification info packet count value.
  unsigned int compressed_certification_info_packet_count{
      payload_certification_info_packet_count_error.second};

  DBUG_EXECUTE_IF("group_replication_certification_info_packet_count_check",
                  assert(compressed_certification_info_packet_count > 1););

  // 2. Get Compression type from the received recovery metadata.
  std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
            GR_compress::enum_compression_type>
      payload_compression_type_error =
          recovery_metadata_message->get_decoded_compression_type();

  if (payload_compression_type_error.first !=
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          RECOVERY_METADATA_MESSAGE_OK) {
    return true;
  }

  // 2.1 Get Compression type value.
  GR_compress::enum_compression_type compression_type{
      payload_compression_type_error.second};

  /*
    3. Get compressed certification info iterator to iterate through
       multiple packets of compressed certification info.
  */
  Recovery_metadata_message_compressed_parts compressed_parts(
      recovery_metadata_message, compressed_certification_info_packet_count);

  if (!is_initialized()) {
    return true;
  }

  mysql_mutex_lock(&LOCK_certification_info);
  clear_certification_info();

  // 3.1. Iterate through compressed certification info packets.
  uint compressed_certification_info_packet_count_aux{0};
  for (auto single_compressed_part : compressed_parts) {
    /*
      3.2. Decompress, unserialize using protobuf and then add it's content
           to local certification info.
    */
    if (set_certification_info_part(compression_type,
                                    std::get<0>(single_compressed_part),
                                    std::get<1>(single_compressed_part),
                                    std::get<2>(single_compressed_part))) {
      mysql_mutex_unlock(&LOCK_certification_info);
      return true;
    }
    ++compressed_certification_info_packet_count_aux;
  }

  /*
    3.3. Check if number of received compressed certification info packets match
         with packets sent.
  */
  if (compressed_certification_info_packet_count !=
      compressed_certification_info_packet_count_aux) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GROUP_REPLICATION_METADATA_CERT_INFO_PACKET_COUNT_ERROR);
    mysql_mutex_unlock(&LOCK_certification_info);
    return true;
  }

  /*
    4. Sets the received gtid_executed from metadata sender.
       Extract the donor group_gtid_executed so that it can be used to
       while member is applying transactions that were already applied
       by distributed recovery procedure.
  */
  std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
            std::reference_wrapper<std::string>>
      payload_after_gtids_error =
          recovery_metadata_message->get_decoded_group_gtid_executed();

  // 4.1. Set group_gtid_extracted if not error.
  if (payload_after_gtids_error.first ==
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          RECOVERY_METADATA_MESSAGE_OK) {
    std::string gtid_extracted_set{payload_after_gtids_error.second.get()};
    if (group_gtid_extracted->add_gtid_text(gtid_extracted_set.c_str()) !=
        RETURN_STATUS_OK) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_READ_GTID_EXECUTED);
      mysql_mutex_unlock(&LOCK_certification_info);
      return true;
    }
  } else {
    // Error decoding group_gtid_executed.
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_READ_GTID_EXECUTED);
    mysql_mutex_unlock(&LOCK_certification_info);
    return true;
  }

  mysql_mutex_unlock(&LOCK_certification_info);
  return false;
}

bool Certifier::set_certification_info_part(
    GR_compress::enum_compression_type compression_type,
    const unsigned char *buffer, unsigned long long buffer_length,
    unsigned long long uncompressed_buffer_length) {
  DBUG_TRACE;
  unsigned char *uncompressed_buffer{nullptr};
  std::size_t uncompressed_buffer_size{0};

  mysql_mutex_assert_owner(&LOCK_certification_info);

  if (buffer != nullptr && buffer_length > 0 &&
      uncompressed_buffer_length > 0) {
    // 1. Initialize compression library.
    GR_decompress *decompress = new GR_decompress(compression_type);

    // 2. Decompress data.
    GR_decompress::enum_decompression_error decompression_error =
        decompress->decompress(buffer, buffer_length,
                               uncompressed_buffer_length);

    // 3. Verify decompression is successful.
    if (decompression_error !=
        GR_decompress::enum_decompression_error::DECOMPRESSION_OK) {
      LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_DECOMPRESS_PROCESS);
      delete decompress;
      return true;
    }

    // 4. Get data after decompression.
    std::tie(uncompressed_buffer, uncompressed_buffer_size) =
        decompress->get_buffer();
    if (uncompressed_buffer == nullptr || uncompressed_buffer_size == 0) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_CERT_INFO_PACKET_EMPTY);
      delete decompress;
      return true;
    }

    // 5. Unserialize uncompressed data using Protobuf.
    ProtoCertificationInformationMap cert_info;
    if (!cert_info.ParseFromArray(uncompressed_buffer,
                                  uncompressed_buffer_size)) {
      LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_PROTOBUF_PARSING);
      delete decompress;
      return true;
    }

    // 6. Now release compression library object so output buffer memory can be
    //    released.
    delete decompress;

    // 7. Insert data to certification info.
    for (auto it = cert_info.data().begin(); it != cert_info.data().end();
         ++it) {
      std::string key = it->first;

      Gtid_set_ref *value = new Gtid_set_ref(certification_info_tsid_map, -1);
      if (value->add_gtid_encoding(
              reinterpret_cast<const uchar *>(it->second.c_str()),
              it->second.length()) != RETURN_STATUS_OK) {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANT_READ_WRITE_SET_ITEM,
                     key.c_str());
        return true;
      }
      value->link();
      certification_info.insert(
          std::pair<std::string, Gtid_set_ref *>(key, value));
    }

    return false;
  }

  // 8. Error if input compressed certification_info packet is empty.
  LogPluginErr(ERROR_LEVEL,
               ER_GROUP_REPLICATION_METADATA_CERT_INFO_PACKET_EMPTY);
  return true;
}

bool Certifier::initialize_server_gtid_set_after_distributed_recovery() {
  DBUG_TRACE;

  if (!is_initialized()) {
    return true;
  }

  mysql_mutex_lock(&LOCK_certification_info);
  if (initialize_server_gtid_set(false)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INIT_CERTIFICATION_INFO_FAILURE);
    mysql_mutex_unlock(&LOCK_certification_info);
    return true;
  }

  mysql_mutex_unlock(&LOCK_certification_info);
  return false;
}

bool Certifier::compress_packet(
    ProtoCertificationInformationMap &proto_cert_info,
    unsigned char **uncompresssed_buffer,
    std::vector<GR_compress *> &compressor_list,
    GR_compress::enum_compression_type compression_type) {
  size_t proto_cert_info_size = proto_cert_info.ByteSizeLong();
  *uncompresssed_buffer =
      (uchar *)my_realloc(key_compression_data, *uncompresssed_buffer,
                          proto_cert_info_size, MYF(0));

  if (*uncompresssed_buffer == nullptr) {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                 "Serializing Protobuf Map");
    return true;
  }

  // 1. Serialize Protobuf Map
  if (!proto_cert_info.SerializeToArray(*uncompresssed_buffer,
                                        proto_cert_info_size)) {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_PROTOBUF_SERIALIZING_ERROR,
                 "Certification_info");
    return true;
  }

  proto_cert_info.clear_data();

  // 2. Initialize compression library.
  GR_compress *compress = new GR_compress(compression_type);

  // 3. Compress data.
  GR_compress::enum_compression_error error =
      compress->compress(*uncompresssed_buffer, proto_cert_info_size);

  // 4. Verify compression is successful.
  if (error != GR_compress::enum_compression_error::COMPRESSION_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_COMPRESS_PROCESS);
    delete compress;
    return true;
  }

  // 5. Add compressed data to vector.
  compressor_list.push_back(compress);
  return false;
}

bool Certifier::get_certification_info_recovery_metadata(
    Recovery_metadata_message *recovery_metadata_message) {
  DBUG_TRACE;
  bool error{false};
  size_t max_length{0};
  size_t max_compressed_packet_size_val{MAX_COMPRESSED_PACKET_SIZE};
  std::string key{};
  uchar *buf{nullptr};
  uchar *uncompresssed_buffer{nullptr};
  std::string value{};
  size_t len{0};
  ProtoCertificationInformationMap proto_cert_info;

  if (!is_initialized()) {
    return true;
  }

  mysql_mutex_lock(&LOCK_certification_info);

  // I. Generate Compressed certification_info packets.
  for (Certification_info::iterator it = certification_info.begin();
       it != certification_info.end(); ++it) {
    // 1. Read data from certification_info map.
    key.assign(it->first);

    len = it->second->get_encoded_length();
    buf = (uchar *)my_realloc(key_certification_data, buf, len, MYF(0));
    if (buf == nullptr) {
      LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                   "reading data from certification_info");
      error = true;
      goto err;
    }
    it->second->encode(buf);
    value.assign(reinterpret_cast<const char *>(buf), len);

    // 2. Add to Protobuf map.
    (*proto_cert_info.mutable_data())[key] = value;

    // 3. If read size is greater than MAX_COMPRESSED_PACKET_SIZE,
    //    call compress_packet() which will
    //    - serialize Protobuf Map,
    //    - compress serialized string,
    //    - The compressed data is pushed to a std::vector, so that multiple
    //      packets of compressed data is prepared.
    max_length += (key.length() + len);
    DBUG_EXECUTE_IF("group_replication_max_compressed_packet_size_10000",
                    { max_compressed_packet_size_val = 10000; });
    if (max_length > max_compressed_packet_size_val) {
      if (compress_packet(
              proto_cert_info, &uncompresssed_buffer,
              recovery_metadata_message->get_encode_compressor_list(),
              recovery_metadata_message->get_encode_compression_type())) {
        error = true;
        goto err;
      }
      max_length = 0;
    }
  }

  if (max_length > 0) {
    if (compress_packet(
            proto_cert_info, &uncompresssed_buffer,
            recovery_metadata_message->get_encode_compressor_list(),
            recovery_metadata_message->get_encode_compression_type())) {
      error = true;
      goto err;
    }
  }

  // II. Get executed gtid set.
  //     Add the group_gtid_executed to Recovery Metadata which will be sent
  //     to joiners.
  len = group_gtid_executed->get_encoded_length();
  buf = (uchar *)my_realloc(key_certification_data, buf, len, MYF(0));
  if (buf == nullptr) {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                 "getting executed gtid set for Recovery Metadata");
    error = true;
    goto err;
  }
  group_gtid_executed->encode(buf);
  recovery_metadata_message->get_encode_group_gtid_executed().assign(
      reinterpret_cast<const char *>(buf), len);

err:
  my_free(buf);
  my_free(uncompresssed_buffer);
  mysql_mutex_unlock(&LOCK_certification_info);
  return error;
}

std::pair<Gtid, mysql::utils::Return_status>
Certifier::generate_view_change_group_gtid() {
  DBUG_TRACE;

  if (!is_initialized()) {
    Gtid resulting_gtid{-1, -1};
    return std::make_pair(resulting_gtid, mysql::utils::Return_status::error);
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);
  auto [generated_gno, generation_code] =
      gtid_generator.get_next_available_gtid(
          nullptr, views_sidno_group_representation, *get_group_gtid_set());

  DBUG_EXECUTE_IF("certifier_assert_next_seqno_equal_5",
                  assert(generated_gno == 5););
  DBUG_EXECUTE_IF("certifier_assert_next_seqno_equal_7",
                  assert(generated_gno == 7););

  if (generation_code == mysql::utils::Return_status::ok)
    add_to_group_gtid_executed_internal(views_sidno_group_representation,
                                        generated_gno);

  Gtid resulting_gtid{views_sidno_server_representation, generated_gno};
  return std::make_pair(resulting_gtid, generation_code);
}

int Certifier::set_certification_info(
    std::map<std::string, std::string> *cert_info) {
  DBUG_TRACE;
  assert(cert_info != nullptr);

  if (!is_initialized()) {
    return 1;
  }

  if (cert_info->size() == 1) {
    std::map<std::string, std::string>::iterator it =
        cert_info->find(CERTIFICATION_INFO_ERROR_NAME);
    if (it != cert_info->end()) {
      // The certification database could not be transmitted
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_CERT_DB_INSTALL,
                   it->second.c_str());
      return 1;
    }
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);

  clear_certification_info();
  for (std::map<std::string, std::string>::iterator it = cert_info->begin();
       it != cert_info->end(); ++it) {
    std::string key = it->first;

    /*
      Extract the donor group_gtid_executed so that it can be used to
      while member is applying transactions that were already applied
      by distributed recovery procedure.
    */
    if (it->first.compare(GTID_EXTRACTED_NAME) == 0) {
      if (group_gtid_extracted->add_gtid_encoding(
              reinterpret_cast<const uchar *>(it->second.c_str()),
              it->second.length()) != RETURN_STATUS_OK) {
        LogPluginErr(
            ERROR_LEVEL,
            ER_GRP_RPL_CANT_READ_GRP_GTID_EXTRACTED); /* purecov: inspected */
        return 1;                                     /* purecov: inspected */
      }
      continue;
    }

    Gtid_set_ref *value = new Gtid_set_ref(certification_info_tsid_map, -1);
    if (value->add_gtid_encoding(
            reinterpret_cast<const uchar *>(it->second.c_str()),
            it->second.length()) != RETURN_STATUS_OK) {
      delete value; /* purecov: inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANT_READ_WRITE_SET_ITEM,
                   key.c_str()); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }
    value->link();
    certification_info.insert(
        std::pair<std::string, Gtid_set_ref *>(key, value));
  }

  if (initialize_server_gtid_set()) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_INIT_CERTIFICATION_INFO_FAILURE); /* purecov: inspected */
    return 1;                                        /* purecov: inspected */
  }

  if (group_gtid_extracted->is_subset_not_equals(group_gtid_executed)) {
    certifying_already_applied_transactions = true;
    gtid_generator.recompute(*get_group_gtid_set());

    debug_print_group_gtid_sets(*group_gtid_executed, *group_gtid_extracted,
                                true);
  }

  return 0;
}

void Certifier::update_certified_transaction_count(bool result,
                                                   bool local_transaction) {
  mysql_mutex_assert_owner(&LOCK_certification_info);

  if (result)
    positive_cert++;
  else
    negative_cert++;

  const Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  assert(member_status == Group_member_info::MEMBER_ONLINE ||
         member_status == Group_member_info::MEMBER_IN_RECOVERY);

  applier_module->get_pipeline_stats_member_collector()
      ->increment_transactions_certified();

  /*
    If transaction is local and rolledback
    increment local negative certifier count
  */
  if (local_transaction && !result) {
    applier_module->get_pipeline_stats_member_collector()
        ->increment_transactions_local_rollback();
  }

  if (member_status == Group_member_info::MEMBER_IN_RECOVERY) {
    applier_module->get_pipeline_stats_member_collector()
        ->increment_transactions_certified_during_recovery();

    if (!result) {
      applier_module->get_pipeline_stats_member_collector()
          ->increment_transactions_certified_negatively_during_recovery();
    }
  }
}

ulonglong Certifier::get_positive_certified() { return positive_cert; }

ulonglong Certifier::get_negative_certified() { return negative_cert; }

ulonglong Certifier::get_certification_info_size() {
  return certification_info.size();
}

void Certifier::get_last_conflict_free_transaction(std::string *value) {
  int length = 0;
  char buffer[Gtid::MAX_TEXT_LENGTH + 1];

  if (!is_initialized()) {
    return;
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);
  if (last_conflict_free_transaction.is_empty()) return;

  length =
      last_conflict_free_transaction.to_string(group_gtid_tsid_map, buffer);
  if (length > 0) value->assign(buffer);
}

void Certifier::enable_conflict_detection() {
  DBUG_TRACE;

  if (!is_initialized()) {
    return;
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);
  conflict_detection_enable = true;
  local_member_info->enable_conflict_detection();
}

void Certifier::disable_conflict_detection() {
  DBUG_TRACE;
  assert(local_member_info->in_primary_mode());

  if (!is_initialized()) {
    return;
  }

  {
    MUTEX_LOCK(guard, &LOCK_certification_info);
    conflict_detection_enable = false;
    local_member_info->disable_conflict_detection();
  }
  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_CONFLICT_DETECTION_DISABLED);
}

bool Certifier::is_conflict_detection_enable() {
  DBUG_TRACE;

  if (!is_initialized()) {
    return false;
  }

  MUTEX_LOCK(guard, &LOCK_certification_info);
  bool result = conflict_detection_enable;
  return result;
}

/*
  Gtid_Executed_Message implementation
 */

Gtid_Executed_Message::Gtid_Executed_Message()
    : Plugin_gcs_message(CT_CERTIFICATION_MESSAGE) {}

Gtid_Executed_Message::~Gtid_Executed_Message() = default;

void Gtid_Executed_Message::append_gtid_executed(uchar *gtid_data, size_t len) {
  data.insert(data.end(), gtid_data, gtid_data + len);
}

void Gtid_Executed_Message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  encode_payload_item_type_and_length(buffer, PIT_GTID_EXECUTED, data.size());
  buffer->insert(buffer->end(), data.begin(), data.end());

  encode_payload_item_int8(buffer, PIT_SENT_TIMESTAMP,
                           Metrics_handler::get_current_time());
}

void Gtid_Executed_Message::decode_payload(const unsigned char *buffer,
                                           const unsigned char *) {
  DBUG_TRACE;
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  decode_payload_item_type_and_length(&slider, &payload_item_type,
                                      &payload_item_length);
  data.clear();
  data.insert(data.end(), slider, slider + payload_item_length);
}

uint64_t Gtid_Executed_Message::get_sent_timestamp(const unsigned char *buffer,
                                                   size_t length) {
  DBUG_TRACE;
  return Plugin_gcs_message::get_sent_timestamp(buffer, length,
                                                PIT_SENT_TIMESTAMP);
}
