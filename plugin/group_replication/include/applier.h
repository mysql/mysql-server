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

#ifndef APPLIER_INCLUDE
#define APPLIER_INCLUDE

#include <mysql/group_replication_priv.h>
#include <mysql/plugin_group_replication.h>
#include <list>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/applier_channel_state_observer.h"
#include "plugin/group_replication/include/consistency_manager.h"
#include "plugin/group_replication/include/gcs_operations.h"
#include "plugin/group_replication/include/handlers/applier_handler.h"
#include "plugin/group_replication/include/handlers/certification_handler.h"
#include "plugin/group_replication/include/handlers/pipeline_handlers.h"
#include "plugin/group_replication/include/pipeline_factory.h"
#include "plugin/group_replication/include/pipeline_stats.h"
#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_utils.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "sql/sql_class.h"

// Define the applier packet types
#define ACTION_PACKET_TYPE 2
#define SINGLE_PRIMARY_PACKET_TYPE 4
#define SYNC_BEFORE_EXECUTION_PACKET_TYPE 5
#define TRANSACTION_PREPARED_PACKET_TYPE 6
#define LEAVING_MEMBERS_PACKET_TYPE 7
#define RECOVERY_METADATA_PROCESSING_PACKET_TYPE 8
#define ERROR_PACKET_TYPE 9  ///< Make the applier fail

// Define the applier return error codes
#define APPLIER_GTID_CHECK_TIMEOUT_ERROR -1
#define APPLIER_RELAY_LOG_NOT_INITED -2
#define APPLIER_THREAD_ABORTED -3

extern char applier_module_channel_name[];

/* Types of action packets used in the context of the applier module */
enum enum_packet_action {
  TERMINATION_PACKET = 0,  // Packet for a termination action
  SUSPENSION_PACKET,       // Packet to signal something to suspend
  CHECKPOINT_PACKET,       // Packet to wait for queue consumption
  ACTION_NUMBER = 3        // The number of actions
};

/**
  @class Action_packet
  A packet to control the applier in a event oriented way.
*/
class Action_packet : public Packet {
 public:
  /**
    Create a new action packet.
    @param  action           the packet action
  */
  Action_packet(enum_packet_action action)
      : Packet(ACTION_PACKET_TYPE), packet_action(action) {}

  ~Action_packet() override = default;

  enum_packet_action packet_action;
};

/**
  @class Recovery_metadata_processing_packets
  A packet to send Metadata related processing.
*/
class Recovery_metadata_processing_packets : public Packet {
 public:
  /**
    Create a new data packet with associated data.
  */
  Recovery_metadata_processing_packets()
      : Packet(RECOVERY_METADATA_PROCESSING_PACKET_TYPE) {}

  virtual ~Recovery_metadata_processing_packets() override = default;

  /*
    List of view-id of which metadata is received.
  */
  std::vector<std::string> m_view_id_to_be_deleted;

  /* List of member that left the group. */
  std::vector<Gcs_member_identifier> m_member_left_the_group;

  /* A flag that indicates all the recovery metadata should be cleared. */
  bool m_current_member_leaving_the_group{false};
};

/**
  @class Single_primary_action_packet
  A packet to send new primary election related info to the applier
*/
class Single_primary_action_packet : public Packet {
 public:
  enum enum_action { NEW_PRIMARY = 0, QUEUE_APPLIED = 1 };

  /**
    Create a new single primary action packet with associated data.

    @param  action_arg       the packet action
  */
  Single_primary_action_packet(enum enum_action action_arg)
      : Packet(SINGLE_PRIMARY_PACKET_TYPE), action(action_arg) {}

  ~Single_primary_action_packet() override = default;

  enum enum_action action;
};

/**
  @class Queue_checkpoint_packet
  A packet to wait for queue consumption
*/
class Queue_checkpoint_packet : public Action_packet {
 public:
  /**
    Create a new Queue_checkpoint_packet packet.
  */
  Queue_checkpoint_packet(
      std::shared_ptr<Continuation> checkpoint_condition_arg)
      : Action_packet(CHECKPOINT_PACKET),
        checkpoint_condition(checkpoint_condition_arg) {}

  void signal_checkpoint_reached() { checkpoint_condition->signal(); }

 private:
  /**If we discard a packet */
  std::shared_ptr<Continuation> checkpoint_condition;
};

/**
  @class Transaction_prepared_action_packet
  A packet to inform that a given member did prepare a given transaction.
*/
class Transaction_prepared_action_packet : public Packet {
 public:
  /**
    Create a new transaction prepared action.

    @param  tsid             the prepared transaction tsid
    @param  is_tsid_specified information on whether tsid is specified
    @param  gno              the prepared transaction gno
    @param  gcs_member_id    the member id that did prepare the
                             transaction
  */
  Transaction_prepared_action_packet(const gr::Gtid_tsid &tsid,
                                     bool is_tsid_specified, rpl_gno gno,
                                     const Gcs_member_identifier &gcs_member_id)
      : Packet(TRANSACTION_PREPARED_PACKET_TYPE),
        m_tsid_specified(is_tsid_specified),
        m_gno(gno),
        m_gcs_member_id(gcs_member_id.get_member_id()) {
    if (m_tsid_specified) {
      m_tsid = tsid;
    }
  }

  ~Transaction_prepared_action_packet() override = default;

  const bool m_tsid_specified;
  const rpl_gno m_gno;
  const Gcs_member_identifier m_gcs_member_id;

  /// @brief tsid accessor
  /// @return tsid const reference
  const gr::Gtid_tsid &get_tsid() const { return m_tsid; }

  /// @brief returns information on whether TSID is specified for this trx
  /// @return information on whether TSID is specified for this trx
  bool is_tsid_specified() const { return m_tsid_specified; }

 private:
  gr::Gtid_tsid m_tsid;
};

/**
  @class Sync_before_execution_action_packet
  A packet to request a synchronization point on the global message
  order on a given member before transaction execution.
*/
class Sync_before_execution_action_packet : public Packet {
 public:
  /**
    Create a new synchronization point request.

    @param  thread_id        the thread that did the request
    @param  gcs_member_id    the member id that did the request
  */
  Sync_before_execution_action_packet(
      my_thread_id thread_id, const Gcs_member_identifier &gcs_member_id)
      : Packet(SYNC_BEFORE_EXECUTION_PACKET_TYPE),
        m_thread_id(thread_id),
        m_gcs_member_id(gcs_member_id.get_member_id()) {}

  ~Sync_before_execution_action_packet() override = default;

  const my_thread_id m_thread_id;
  const Gcs_member_identifier m_gcs_member_id;
};

/**
  @class Leaving_members_action_packet
  A packet to inform pipeline listeners of leaving members,
  this packet will be handled on the global message order,
  that is, ordered with certification.
*/
class Leaving_members_action_packet : public Packet {
 public:
  /**
    Create a new leaving members packet.

    @param  leaving_members  the members that left the group
  */
  Leaving_members_action_packet(
      const std::vector<Gcs_member_identifier> &leaving_members)
      : Packet(LEAVING_MEMBERS_PACKET_TYPE),
        m_leaving_members(leaving_members) {}

  ~Leaving_members_action_packet() override = default;

  const std::vector<Gcs_member_identifier> m_leaving_members;
};

/// @class Error_action_packet
/// A packet to inform the applier it should fail.
/// It should include a message about the failure
class Error_action_packet : public Packet {
 public:
  /// Create a new error packet.
  /// @param  error_message  the error that will make the applier stop
  Error_action_packet(const char *error_message)
      : Packet(ERROR_PACKET_TYPE), m_error_message(error_message) {}

  ~Error_action_packet() override = default;

  /// @brief Returns the error message for the failure
  /// @return the error message
  const char *get_error_message() { return m_error_message; }

 private:
  /// The error message for the failure process
  const char *m_error_message;
};

typedef enum enum_applier_state {
  APPLIER_STATE_ON = 1,
  APPLIER_STATE_OFF,
  APPLIER_ERROR
} Member_applier_state;

class Applier_module_interface {
 public:
  virtual ~Applier_module_interface() = default;
  virtual Certification_handler *get_certification_handler() = 0;
  virtual int wait_for_applier_complete_suspension(
      bool *abort_flag, bool wait_for_execution = true) = 0;
  virtual void awake_applier_module() = 0;
  virtual void interrupt_applier_suspension_wait() = 0;
  virtual int wait_for_applier_event_execution(
      double timeout, bool check_and_purge_partial_transactions) = 0;
  virtual bool wait_for_current_events_execution(
      std::shared_ptr<Continuation> checkpoint_condition, bool *abort_flag,
      bool update_THD_status = true) = 0;
  virtual bool get_retrieved_gtid_set(std::string &retrieved_set) = 0;
  virtual int wait_for_applier_event_execution(
      std::string &retrieved_set, double timeout,
      bool update_THD_status = true) = 0;
  virtual size_t get_message_queue_size() = 0;
  virtual Member_applier_state get_applier_status() = 0;
  virtual void add_suspension_packet() = 0;
  virtual void add_packet(Packet *packet) = 0;
  virtual void add_view_change_packet(View_change_packet *packet) = 0;
  virtual void add_metadata_processing_packet(
      Recovery_metadata_processing_packets *packet) = 0;
  virtual void add_single_primary_action_packet(
      Single_primary_action_packet *packet) = 0;
  virtual void add_transaction_prepared_action_packet(
      Transaction_prepared_action_packet *packet) = 0;
  virtual void add_sync_before_execution_action_packet(
      Sync_before_execution_action_packet *packet) = 0;
  virtual void add_leaving_members_action_packet(
      Leaving_members_action_packet *packet) = 0;
  virtual int handle(const uchar *data, ulong len,
                     enum_group_replication_consistency_level consistency_level,
                     std::list<Gcs_member_identifier> *online_members,
                     PSI_memory_key key) = 0;
  virtual int handle_pipeline_action(Pipeline_action *action) = 0;
  virtual Flow_control_module *get_flow_control_module() = 0;
  virtual void run_flow_control_step() = 0;
  virtual int purge_applier_queue_and_restart_applier_module() = 0;
  virtual bool queue_and_wait_on_queue_checkpoint(
      std::shared_ptr<Continuation> checkpoint_condition) = 0;
  virtual Pipeline_stats_member_collector *
  get_pipeline_stats_member_collector() = 0;
};

class Applier_module : public Applier_module_interface {
 public:
  Applier_module();
  ~Applier_module() override;

  /**
    Initializes and launches the applier thread

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_applier_thread();

  /**
   * Return the local applier stats.
   */
  Pipeline_member_stats *get_local_pipeline_stats();

  /**
    Terminates the applier thread.

    @return the operation status
      @retval 0      OK
      @retval !=0    A timeout occurred
  */
  int terminate_applier_thread();

  /**
    Is the applier marked for shutdown?

    @return is the applier on shutdown
      @retval 0      no
      @retval !=0    yes
  */
  bool is_applier_thread_aborted() {
    return (applier_aborted || applier_thd == nullptr || applier_thd->killed);
  }

  /**
    Is the applier running?

    @return applier running?
      @retval 0      no
      @retval !=0    yes
  */
  bool is_running() { return (applier_thd_state.is_running()); }

  /**
    Configure the applier pipeline according to the given configuration

    @param[in] pipeline_type              the chosen pipeline
    @param[in] reset_logs                 if a reset happened in the server
    @param[in] stop_timeout               the timeout when waiting on shutdown
    @param[in] group_sidno                the group configured sidno
    @param[in] gtid_assignment_block_size the group gtid assignment block size

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int setup_applier_module(Handler_pipeline_type pipeline_type, bool reset_logs,
                           ulong stop_timeout, rpl_sidno group_sidno,
                           ulonglong gtid_assignment_block_size);

  /**
    Configure the applier pipeline handlers

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int setup_pipeline_handlers();

  /**
    Purges the relay logs and restarts the applier thread.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int purge_applier_queue_and_restart_applier_module() override;

  /**
    Runs the applier thread process, reading events and processing them.

    @note When killed, the thread will finish handling the current packet, and
    then die, ignoring all possible existing events in the incoming queue.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int applier_thread_handle();

  /**
    Queues the packet coming from the reader for future application.

    @param[in]  data      the packet data
    @param[in]  len       the packet length
    @param[in]  consistency_level  the transaction consistency level
    @param[in]  online_members     the ONLINE members when the transaction
                                   message was delivered
    @param[in]  key       the memory instrument key

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  int handle(const uchar *data, ulong len,
             enum_group_replication_consistency_level consistency_level,
             std::list<Gcs_member_identifier> *online_members,
             PSI_memory_key key) override {
    this->incoming->push(
        new Data_packet(data, len, key, consistency_level, online_members));
    return 0;
  }

  /**
    Gives the pipeline an action for execution.

    @param[in]  action      the action to be executed

    @return the operation status
      @retval 0      OK
      @retval !=0    Error executing the action
  */
  int handle_pipeline_action(Pipeline_action *action) override {
    return this->pipeline->handle_action(action);
  }

  /**
     Injects an event into the pipeline and waits for its handling.

     @param[in] pevent   the event to be injected
     @param[in] cont     the object used to wait

     @return the operation status
       @retval 0      OK
       @retval !=0    Error on queue
   */
  int inject_event_into_pipeline(Pipeline_event *pevent, Continuation *cont);

  /**
    Terminates the pipeline, shutting down the handlers and deleting them.

    @note the pipeline will always be deleted even if an error occurs.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on pipeline termination
  */
  int terminate_applier_pipeline();

  /**
    Sets the applier shutdown timeout.

    @param[in]  timeout      the timeout
  */
  void set_stop_wait_timeout(ulong timeout) {
    stop_wait_timeout = timeout;

    // Configure any thread based applier
    Handler_applier_configuration_action *conf_action =
        new Handler_applier_configuration_action(timeout);
    pipeline->handle_action(conf_action);

    delete conf_action;
  }

  /**
   This method informs the applier module that an applying thread stopped
  */
  void inform_of_applier_stop(char *channel_name, bool aborted);

  /**
   Check whether to ignore applier errors during stop or not.
   Errors put the members into ERROR state.
   If errors are ignored member will stay in ONLINE state.
   During clone, applier errors are ignored, since data will come from clone.

    @param[in]  ignore_errors  if true ignore applier errors during stop
  */
  void ignore_errors_during_stop(bool ignore_errors) {
    m_ignore_applier_errors_during_stop = ignore_errors;
  }

  // Packet based interface methods

  /**
    Queues a packet that will eventually make the applier module suspend.
    This will happen only after all the previous packets are processed.

    @note This will happen only after all the previous packets are processed.
  */
  void add_suspension_packet() override {
    this->incoming->push(new Action_packet(SUSPENSION_PACKET));
  }

  /**
    Queues a packet that will make the applier module terminate it's handling
    process. Due to the blocking nature of the queue, this method is useful to
    unblock the handling process on shutdown.

    @note This will happen only after all the previous packets are processed.
  */
  void add_termination_packet() {
    this->incoming->push(new Action_packet(TERMINATION_PACKET));
  }

  /// @brief Generic add packet method
  /// @param packet the packet to be queued in the applier
  void add_packet(Packet *packet) override { incoming->push(packet); }

  /**
    Queues a view change packet into the applier.
    This packets contain the new view id and they mark the exact frontier
    between transactions from the old and new views.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The view change packet to be queued
  */
  void add_view_change_packet(View_change_packet *packet) override {
    incoming->push(packet);
  }

  void add_metadata_processing_packet(
      Recovery_metadata_processing_packets *packet) override {
    incoming->push(packet);
  }

  /**
    Queues a single primary action packet into the applier.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The packet to be queued
  */
  void add_single_primary_action_packet(
      Single_primary_action_packet *packet) override {
    incoming->push(packet);
  }

  /**
    Queues a transaction prepared action packet into the applier.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The packet to be queued
  */
  void add_transaction_prepared_action_packet(
      Transaction_prepared_action_packet *packet) override {
    incoming->push(packet);
  }

  /**
    Queues a synchronization before execution action packet into the applier.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The packet to be queued
  */
  void add_sync_before_execution_action_packet(
      Sync_before_execution_action_packet *packet) override {
    incoming->push(packet);
  }

  /**
    Queues a leaving members action packet into the applier.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The packet to be queued
  */
  void add_leaving_members_action_packet(
      Leaving_members_action_packet *packet) override {
    incoming->push(packet);
  }

  /**
    Queues a single a packet that will enable certification on this member
   */
  virtual void queue_certification_enabling_packet();

  /**
   Awakes the applier module
  */
  void awake_applier_module() override {
    mysql_mutex_lock(&suspend_lock);
    suspended = false;
    mysql_cond_broadcast(&suspend_cond);
    mysql_mutex_unlock(&suspend_lock);
  }

  /**
   Waits for the applier to suspend and apply all the transactions previous to
   the suspend request.

   @param abort_flag          a pointer to a flag that the caller can use to
                              cancel the request.
   @param wait_for_execution  specify if the suspension waits for transactions
                              execution

   @return the operation status
     @retval 0      OK
     @retval !=0    Error when accessing the applier module status
  */
  int wait_for_applier_complete_suspension(
      bool *abort_flag, bool wait_for_execution = true) override;

  /**
   Interrupts the current applier waiting process either for it's suspension
   or it's wait for the consumption of pre suspension events
  */
  void interrupt_applier_suspension_wait() override;

  /**
    Checks if the applier, and its workers when parallel applier is
    enabled, has already consumed all relay log, that is, applier is
    waiting for transactions to be queued.

    @return the applier status
      @retval true      the applier is waiting
      @retval false     otherwise
  */
  bool is_applier_thread_waiting();

  /**
    Waits for the execution of all events by part of the current SQL applier.
    Due to the possible asynchronous nature of module's applier handler, this
    method inquires the current handler to check if all transactions queued up
    to this point are already executed.

    If no handler exists, then it is assumed that transactions were processed.

    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied
    @param check_and_purge_partial_transactions
                    on successful executions, check and purge partial
                    transactions in the relay log

    @return the operation status
      @retval 0      All transactions were executed
      @retval -1     A timeout occurred
      @retval -2     An error occurred
  */
  int wait_for_applier_event_execution(
      double timeout, bool check_and_purge_partial_transactions) override;

  /**
    Waits for the execution of all current events by part of the current SQL
    applier.

    The current gtid retrieved set is extracted and a loop is executed until
    these transactions are executed.

    If the applier SQL thread stops, the method will return an error.

    If no handler exists, then it is assumed that transactions were processed.

    @param checkpoint_condition  the class used to wait for the queue to be
                                 consumed. Can be used to cancel the wait.
    @param abort_flag            a pointer to a flag that the caller can use to
                                 cancel the request.
    @param update_THD_status     Shall the method update the THD stage


    @return the operation status
      @retval false    All transactions were executed
      @retval true     An error occurred
  */
  bool wait_for_current_events_execution(
      std::shared_ptr<Continuation> checkpoint_condition, bool *abort_flag,
      bool update_THD_status = true) override;

  /**
    Returns the retrieved gtid set for the applier channel

    @param[out] retrieved_set the set in string format.

    @retval true there was an error.
    @retval false the operation has succeeded.
  */
  bool get_retrieved_gtid_set(std::string &retrieved_set) override;

  /**
    Waits for the execution of all events in the given set by the current SQL
    applier. If no handler exists, then it is assumed that transactions were
    processed.

    @param retrieved_set the set in string format of transaction to wait for
    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied
    @param update_THD_status     Shall the method update the THD stage

    @return the operation status
      @retval 0      All transactions were executed
      @retval -1     A timeout occurred
      @retval -2     An error occurred
  */
  int wait_for_applier_event_execution(std::string &retrieved_set,
                                       double timeout,
                                       bool update_THD_status = true) override;

  /**
    Returns the handler instance in the applier module responsible for
    certification.

    @note If new certification handlers appear, an interface must be created.

    @return a pointer to the applier's certification handler.
      @retval !=NULL The certification handler
      @retval NULL   No certification handler present
  */
  Certification_handler *get_certification_handler() override;

  /**
    Returns the applier module's queue size.

    @return the size of the queue
  */
  size_t get_message_queue_size() override { return incoming->size(); }

  Member_applier_state get_applier_status() override {
    if (applier_thd_state.is_running())
      return APPLIER_STATE_ON;
    else if (suspended)         /* purecov: inspected */
      return APPLIER_STATE_OFF; /* purecov: inspected */
    else
      return APPLIER_ERROR; /* purecov: inspected */
  }

  Pipeline_stats_member_collector *get_pipeline_stats_member_collector()
      override {
    return &pipeline_stats_member_collector;
  }

  Flow_control_module *get_flow_control_module() override {
    return &flow_control_module;
  }

  void run_flow_control_step() override {
    flow_control_module.flow_control_step(&pipeline_stats_member_collector);
  }

  bool queue_and_wait_on_queue_checkpoint(
      std::shared_ptr<Continuation> checkpoint_condition) override;

 private:
  // Applier packet handlers

  /**
    Apply an action packet received by the applier.
    It can be a order to suspend or terminate.

    @param action_packet  the received action packet

    @return if the applier should terminate (with no associated error).
  */
  bool apply_action_packet(Action_packet *action_packet);

  /**
    Apply a View Change packet received by the applier.
    It executes some certification operations and queues a View Change Event

    @param view_change_packet  the received view change packet
    @param fde_evt  the Format description event associated to the event
    @param cont     the applier Continuation Object

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when injecting event
  */
  int apply_view_change_packet(View_change_packet *view_change_packet,
                               Format_description_log_event *fde_evt,
                               Continuation *cont);

  /**
    Apply a Recovery metadata processing information received from the GCS.

    @param metadata_processing_packet Information of member left the group

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when injecting event
  */
  int apply_metadata_processing_packet(
      Recovery_metadata_processing_packets *metadata_processing_packet);

  /**
    Apply a Data packet received by the applier.
    It executes some certification operations and queues a View Change Event

    @param data_packet  the received data packet packet
    @param fde_evt  the Format description event associated to the event
    @param cont     the applier Continuation Object

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when injecting event
  */
  int apply_data_packet(Data_packet *data_packet,
                        Format_description_log_event *fde_evt,
                        Continuation *cont);

  /**
    Apply an single primary action packet received by the applier.

    @param packet  the received action packet

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when applying packet
  */
  int apply_single_primary_action_packet(Single_primary_action_packet *packet);

  /**
    Apply a transaction prepared action packet received by the applier.

    @param packet  the received action packet

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when applying packet
  */
  int apply_transaction_prepared_action_packet(
      Transaction_prepared_action_packet *packet);

  /**
    Apply a synchronization before execution action packet received
    by the applier.

    @param packet  the received action packet

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when applying packet
  */
  int apply_sync_before_execution_action_packet(
      Sync_before_execution_action_packet *packet);

  /**
    Apply a leaving members action packet received by the applier.

    @param packet  the received action packet

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when applying packet
  */
  int apply_leaving_members_action_packet(
      Leaving_members_action_packet *packet);

  /**
    Suspends the applier module, being transactions still queued in the incoming
    queue.

    @note if the proper condition is set, possible listeners can be awaken by
    this method.
  */
  void suspend_applier_module() {
    mysql_mutex_lock(&suspend_lock);

    suspended = true;
    stage_handler.set_stage(info_GR_STAGE_module_suspending.m_key, __FILE__,
                            __LINE__, 0, 0);

    // Alert any interested party about the applier suspension
    mysql_cond_broadcast(&suspension_waiting_condition);

    while (suspended) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&suspend_cond, &suspend_lock, &abstime);
    }
    stage_handler.set_stage(info_GR_STAGE_module_executing.m_key, __FILE__,
                            __LINE__, 0, 0);

    mysql_mutex_unlock(&suspend_lock);
  }

  /**
    Cleans the thread context for the applier thread
    This includes such tasks as removing the thread from the global thread list
  */
  void clean_applier_thread_context();

  /**
    Set the thread context for the applier thread.
    This allows the thread to behave like an slave thread and perform
    such tasks as queuing to a relay log.
  */
  void set_applier_thread_context();

  /**
    This method calculates the intersection of the given sets passed as a list
    of strings.

    @param[in]  gtid_sets   the vector containing the GTID sets to intersect
    @param[out] output_set  the final GTID calculated from the intersection

    @return the operation status
        @retval 0   all went fine
        @retval !=0 error
  */
  int intersect_group_executed_sets(std::vector<std::string> &gtid_sets,
                                    Gtid_set *output_set);

  // applier thread variables
  my_thread_handle applier_pthd;
  THD *applier_thd;

  // configuration options
  bool reset_applier_logs;
  rpl_sidno group_replication_sidno;
  ulonglong gtid_assignment_block_size;

  // run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t run_cond;
  /* Applier thread state */
  thread_state applier_thd_state;
  /* Applier abort flag */
  bool applier_aborted;
  /* Applier error during execution */
  int applier_error;
  /* Applier killed status */
  bool applier_killed_status;
  /* Ignore applier errors during stop. */
  bool m_ignore_applier_errors_during_stop{false};

  // condition and lock used to suspend/awake the applier module
  /* The lock for suspending/wait for the awake of  the applier module */
  mysql_mutex_t suspend_lock;
  /* The condition for suspending/wait for the awake of  the applier module */
  mysql_cond_t suspend_cond;
  /* Suspend flag that informs if the applier is suspended */
  bool suspended;

  /* The condition for signaling the applier suspension*/
  mysql_cond_t suspension_waiting_condition;

  /* The incoming event queue */
  Synchronized_queue<Packet *> *incoming;

  /* The applier pipeline for event execution */
  Event_handler *pipeline;

  /* Applier timeout on shutdown */
  ulong stop_wait_timeout;

  /* Applier channel observer to detect failures */
  Applier_channel_state_observer *applier_channel_observer;

  Pipeline_stats_member_collector pipeline_stats_member_collector;
  Flow_control_module flow_control_module;
  Plugin_stage_monitor_handler stage_handler;
};

#endif /* APPLIER_INCLUDE */
