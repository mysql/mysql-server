/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef APPLIER_INCLUDE
#define APPLIER_INCLUDE

#include <mysql/group_replication_priv.h>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/applier_channel_state_observer.h"
#include "plugin/group_replication/include/handlers/applier_handler.h"
#include "plugin/group_replication/include/handlers/certification_handler.h"
#include "plugin/group_replication/include/handlers/pipeline_handlers.h"
#include "plugin/group_replication/include/pipeline_factory.h"
#include "plugin/group_replication/include/pipeline_stats.h"
#include "plugin/group_replication/include/plugin_utils.h"

// Define the applier packet types
#define ACTION_PACKET_TYPE 2
#define VIEW_CHANGE_PACKET_TYPE 3
#define SINGLE_PRIMARY_PACKET_TYPE 4

// Define the applier return error codes
#define APPLIER_GTID_CHECK_TIMEOUT_ERROR -1
#define APPLIER_RELAY_LOG_NOT_INITED -2
#define APPLIER_THREAD_ABORTED -3

extern char applier_module_channel_name[];

/* Types of action packets used in the context of the applier module */
enum enum_packet_action {
  TERMINATION_PACKET = 0,  // Packet for a termination action
  SUSPENSION_PACKET,       // Packet to signal something to suspend
  ACTION_NUMBER = 2        // The number of actions
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

  ~Action_packet() {}

  enum_packet_action packet_action;
};

/**
  @class View_change_packet
  A packet to send view change related info to the applier
*/
class View_change_packet : public Packet {
 public:
  /**
    Create a new data packet with associated data.

    @param  view_id_arg    The view id associated to this view
  */
  View_change_packet(std::string &view_id_arg)
      : Packet(VIEW_CHANGE_PACKET_TYPE), view_id(view_id_arg) {}

  ~View_change_packet() {}

  std::string view_id;
  std::vector<std::string> group_executed_set;
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

  virtual ~Single_primary_action_packet() {}

  enum enum_action action;
};

typedef enum enum_applier_state {
  APPLIER_STATE_ON = 1,
  APPLIER_STATE_OFF,
  APPLIER_ERROR
} Member_applier_state;

class Applier_module_interface {
 public:
  virtual ~Applier_module_interface() {}
  virtual Certification_handler *get_certification_handler() = 0;
  virtual int wait_for_applier_complete_suspension(
      bool *abort_flag, bool wait_for_execution = true) = 0;
  virtual void awake_applier_module() = 0;
  virtual void interrupt_applier_suspension_wait() = 0;
  virtual int wait_for_applier_event_execution(
      double timeout, bool check_and_purge_partial_transactions) = 0;
  virtual size_t get_message_queue_size() = 0;
  virtual Member_applier_state get_applier_status() = 0;
  virtual void add_suspension_packet() = 0;
  virtual void add_view_change_packet(View_change_packet *packet) = 0;
  virtual void add_single_primary_action_packet(
      Single_primary_action_packet *packet) = 0;
  virtual int handle(const uchar *data, ulong len) = 0;
  virtual int handle_pipeline_action(Pipeline_action *action) = 0;
  virtual Flow_control_module *get_flow_control_module() = 0;
  virtual void run_flow_control_step() = 0;
  virtual int purge_applier_queue_and_restart_applier_module() = 0;
  virtual void kill_pending_transactions(bool set_read_mode,
                                         bool threaded_sql_session) = 0;
  virtual uint64
  get_pipeline_stats_member_collector_transactions_applied_during_recovery() = 0;
};

class Applier_module : public Applier_module_interface {
 public:
  Applier_module();
  ~Applier_module();

  /**
    Initializes and launches the applier thread

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize_applier_thread();

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
    return (applier_aborted || applier_thd->killed);
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
    @param[in] shared_stop_lock           the lock used to block transactions

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int setup_applier_module(Handler_pipeline_type pipeline_type, bool reset_logs,
                           ulong stop_timeout, rpl_sidno group_sidno,
                           ulonglong gtid_assignment_block_size,
                           Shared_writelock *shared_stop_lock);

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
  virtual int purge_applier_queue_and_restart_applier_module();

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

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  int handle(const uchar *data, ulong len) {
    this->incoming->push(new Data_packet(data, len));
    return 0;
  }

  /**
    Gives the pipeline an action for execution.

    @param[in]  action      the action to be executed

    @return the operation status
      @retval 0      OK
      @retval !=0    Error executing the action
  */
  int handle_pipeline_action(Pipeline_action *action) {
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

  // Packet based interface methods

  /**
    Queues a packet that will eventually make the applier module suspend.
    This will happen only after all the previous packets are processed.

    @note This will happen only after all the previous packets are processed.
  */
  virtual void add_suspension_packet() {
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

  /**
    Queues a view change packet into the applier.
    This packets contain the new view id and they mark the exact frontier
    between transactions from the old and new views.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The view change packet to be queued
  */
  virtual void add_view_change_packet(View_change_packet *packet) {
    incoming->push(packet);
  }

  /**
    Queues a single primary action packet into the applier.

    @note This will happen only after all the previous packets are processed.

    @param[in]  packet              The packet to be queued
  */
  void add_single_primary_action_packet(Single_primary_action_packet *packet) {
    incoming->push(packet);
  }

  /**
   Awakes the applier module
  */
  virtual void awake_applier_module() {
    mysql_mutex_lock(&suspend_lock);
    suspended = false;
    mysql_mutex_unlock(&suspend_lock);
    mysql_cond_broadcast(&suspend_cond);
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
  virtual int wait_for_applier_complete_suspension(
      bool *abort_flag, bool wait_for_execution = true);

  /**
   Interrupts the current applier waiting process either for it's suspension
   or it's wait for the consumption of pre suspension events
  */
  virtual void interrupt_applier_suspension_wait();

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
  virtual int wait_for_applier_event_execution(
      double timeout, bool check_and_purge_partial_transactions);

  /**
    Returns the handler instance in the applier module responsible for
    certification.

    @note If new certification handlers appear, an interface must be created.

    @return a pointer to the applier's certification handler.
      @retval !=NULL The certification handler
      @retval NULL   No certification handler present
  */
  virtual Certification_handler *get_certification_handler();

  /**
    Returns the applier module's queue size.

    @return the size of the queue
  */
  virtual size_t get_message_queue_size() { return incoming->size(); }

  virtual Member_applier_state get_applier_status() {
    if (applier_thd_state.is_running())
      return APPLIER_STATE_ON;
    else if (suspended)         /* purecov: inspected */
      return APPLIER_STATE_OFF; /* purecov: inspected */
    else
      return APPLIER_ERROR; /* purecov: inspected */
  }

  Pipeline_stats_member_collector *get_pipeline_stats_member_collector() {
    return &pipeline_stats_member_collector;
  }

  Flow_control_module *get_flow_control_module() {
    return &flow_control_module;
  }

  virtual void run_flow_control_step() {
    flow_control_module.flow_control_step(&pipeline_stats_member_collector);
  }

  /**
    Kill pending transactions and enable super_read_only mode, if chosen.

    @param set_read_mode         if true, enable super_read_only mode
    @param threaded_sql_session  if true, creates a thread to open the
                                 SQL session
  */
  void kill_pending_transactions(bool set_read_mode, bool threaded_sql_session);

  /**
    Return the number of transactions applied during recovery from the pipeline
    stats member.

    @return number of transactions applied during recovery
  */
  uint64
  get_pipeline_stats_member_collector_transactions_applied_during_recovery() {
    return pipeline_stats_member_collector
        .get_transactions_applied_during_recovery();
  }

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
    @param cache    the applier IO cache to convert Log Events and Packets
    @param cont     the applier Continuation Object

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when injecting event
  */
  int apply_view_change_packet(View_change_packet *view_change_packet,
                               Format_description_log_event *fde_evt,
                               IO_CACHE *cache, Continuation *cont);

  /**
    Apply a Data packet received by the applier.
    It executes some certification operations and queues a View Change Event

    @param data_packet  the received data packet packet
    @param fde_evt  the Format description event associated to the event
    @param cache    the applier IO cache to convert Log Events and Packets
    @param cont     the applier Continuation Object

    @return the operation status
      @retval 0      OK
      @retval !=0    Error when injecting event
  */
  int apply_data_packet(Data_packet *data_packet,
                        Format_description_log_event *fde_evt, IO_CACHE *cache,
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
    Suspends the applier module, being transactions still queued in the incoming
    queue.

    @note if the proper condition is set, possible listeners can be awaken by
    this method.
  */
  void suspend_applier_module() {
    mysql_mutex_lock(&suspend_lock);

    suspended = true;

#ifndef _WIN32
    THD_STAGE_INFO(applier_thd, stage_suspending);
#endif

    // Alert any interested party about the applier suspension
    mysql_cond_broadcast(&suspension_waiting_condition);

    while (suspended) {
      mysql_cond_wait(&suspend_cond, &suspend_lock);
    }

#ifndef _WIN32
    THD_STAGE_INFO(applier_thd, stage_executing);
#endif

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
    Prints an error to the log and tries to leave the group
  */
  void leave_group_on_failure();

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

  /**
    This method checks if the primary did already apply all transactions
    from the previous primary.
    If it did, a message is sent to all group members notifying that.

    @return the operation status
        @retval 0   all went fine
        @retval !=0 error
  */
  int check_single_primary_queue_status();

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

  // condition and lock used to suspend/awake the applier module
  /* The lock for suspending/wait for the awake of  the applier module */
  mysql_mutex_t suspend_lock;
  /* The condition for suspending/wait for the awake of  the applier module */
  mysql_cond_t suspend_cond;
  /* Suspend flag that informs if the applier is suspended */
  bool suspended;
  /* Suspend wait flag used when waiting for the applier to suspend */
  bool waiting_for_applier_suspension;

  /* The condition for signaling the applier suspension*/
  mysql_cond_t suspension_waiting_condition;

  /* The stop lock used when killing transaction/stopping server*/
  Shared_writelock *shared_stop_write_lock;

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
};

#endif /* APPLIER_INCLUDE */
