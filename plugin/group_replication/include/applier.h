/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_APPLIER_INCLUDE
#define GCS_APPLIER_INCLUDE

#include <vector>

#include "plugin_utils.h"
#include "pipeline_factory.h"
#include "handlers/pipeline_handlers.h"
#include "handlers/applier_handler.h"
#include "handlers/certification_handler.h"

#include <mysql/group_replication_priv.h>


//Define the action packet type
#define ACTION_PACKET_TYPE  2

extern char applier_module_channel_name[];

/* Types of action packets used in the context of the applier module */
enum enum_packet_action
{
  TERMINATION_PACKET=0,  //Packet for a termination action
  SUSPENSION_PACKET,     //Packet to signal something to suspend
  VIEW_CHANGE_PACKET,    //Packet to signal a view change
  ACTION_NUMBER= 3       //The number of actions
};

/**
  @class Data packet
  A data packet to control the applier in a packet event way.
*/
class Action_packet: public Packet
{
public:

  /**
    Create a new data packet.

    @param  action           the packet action
  */
  Action_packet(enum_packet_action action)
    :Packet(ACTION_PACKET_TYPE), packet_action(action), payload(NULL), len(0)
  {
  }

  /**
    Create a new data packet with associated data.

    @param  action           the packet action
    @param  data             the packet data
    @param  len              the packet length
  */
  Action_packet(enum_packet_action action, uchar *data, uint len)
    :Packet(ACTION_PACKET_TYPE), packet_action(action), payload(NULL), len(len)
  {
    payload= (uchar*)my_malloc(
#ifdef HAVE_PSI_MEMORY_INTERFACE
                               PSI_NOT_INSTRUMENTED,
#endif
                               len, MYF(0));
    memcpy(payload, data, len);
  }

  ~Action_packet()
  {
    if (payload != NULL)
      my_free(payload);
  }

  enum_packet_action packet_action;
  uchar *payload;
  uint  len;
};

typedef enum enum_applier_state {
  APPLIER_STATE_ON= 1,
  APPLIER_STATE_OFF,
  APPLIER_ERROR
} Member_applier_state;

class Applier_module_interface
{

public:
  virtual ~Applier_module_interface() {}
  virtual Certification_handler* get_certification_handler()= 0;
  virtual bool is_own_event_channel(my_thread_id id)= 0;
  virtual int wait_for_applier_complete_suspension(bool *abort_flag)= 0;
  virtual void awake_applier_module()= 0;
  virtual void interrupt_applier_suspension_wait()= 0;
  virtual ulong get_message_queue_size()= 0;
  virtual void add_suspension_packet()= 0;
  virtual void add_view_change_packet(char* view_id)= 0;
  virtual int handle(uchar *data, uint len)= 0;
  virtual int handle_pipeline_action(Pipeline_action *action)= 0;
};

class Applier_module: public Applier_module_interface
{

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
  bool is_applier_thread_aborted()
  {
    return (applier_aborted || applier_thd->killed);
  }

  /**
    Is the applier running?

    @return applier running?
      @retval 0      no
      @retval !=0    yes
  */
  bool is_running()
  {
    return applier_running;
  }

  /**
    Configure the applier pipeline according to the given configuration

    @param[in] pipeline_type        the chosen pipeline
    @param[in] reset_logs           if a reset happened in the server
    @param[in] stop_timeout         the timeout when waiting on shutdown
    @param[in] cluster_sidno        the cluster configured sidno

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int setup_applier_module(Handler_pipeline_type pipeline_type,
                           bool reset_logs,
                           ulong stop_timeout,
                           rpl_sidno cluster_sidno);

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
  int handle(uchar *data, uint len)
  {
    return this->incoming->push(new Data_packet(data, len));
  }

  /**
    Gives the pipeline an action for execution.

    @param[in]  action      the action to be executed

    @return the operation status
      @retval 0      OK
      @retval !=0    Error executing the action
  */
  int handle_pipeline_action(Pipeline_action *action)
  {
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
  int inject_event_into_pipeline(Pipeline_event* pevent, Continuation* cont);

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
  void set_stop_wait_timeout (ulong timeout){
    stop_wait_timeout= timeout;

    //Configure any thread based applier
    Handler_applier_configuration_action *conf_action=
      new Handler_applier_configuration_action(timeout);
    pipeline->handle_action(conf_action);

    delete conf_action;
  }

  // Packet based interface methods

  /**
    Queues a packet that will eventually make the applier module suspend.
    This will happen only after all the previous packets are processed.

    @note This will happen only after all the previous packets are processed.
  */
  virtual void add_suspension_packet()
  {
    this->incoming->push(new Action_packet(SUSPENSION_PACKET));
  }

  /**
    Queues a packet that will make the applier module terminate it's handling
    process. Due to the blocking nature of the queue, this method is useful to
    unblock the handling process on shutdown.

    @note This will happen only after all the previous packets are processed.
  */
  void add_termination_packet()
  {
    this->incoming->push(new Action_packet(TERMINATION_PACKET));
  }

  /**
    Queues a view change packet into the applier.
    This packets contain the new view id and they mark the exact frontier
    between transactions from the old and new views.

    @note This will happen only after all the previous packets are processed.
  */
  virtual void add_view_change_packet(char* view_id)
  {
    Packet* packet= new Action_packet(VIEW_CHANGE_PACKET,
                                      (uchar*)view_id,
                                      strlen(view_id) + 1);
    incoming->push(packet);
  }

  /**
   Awakes the applier module
  */
  virtual void awake_applier_module()
  {
    DBUG_EXECUTE_IF("gcs_applier_do_not_start_sql_thread",
                    {
                      //Send a stop signal to all handlers.
                      Pipeline_action *stop_action = new Handler_stop_action();
                      applier_error= pipeline->handle_action(stop_action);
                      delete stop_action;
                    };);

    mysql_mutex_lock(&suspend_lock);
    suspended= false;
    mysql_mutex_unlock(&suspend_lock);
    mysql_cond_broadcast(&suspend_cond);
  }

  /**
   Waits for the applier to suspend and apply all the transactions previous to
   the suspend request.

   @param abort_flag   a pointer to a flag that the caller can use to cancel
                       the request.

   @return the operation status
     @retval 0      OK
     @retval !=0    Error when accessing the applier module status
  */
  virtual int wait_for_applier_complete_suspension(bool *abort_flag);

  /**
   Interrupts the current applier waiting process either for it's suspension
   or it's wait for the consumption of pre suspension events
  */
  virtual void interrupt_applier_suspension_wait();

  /**
    Waits for the execution of all events by part of the current SQL applier.
    Due to the possible asynchronous nature of module's applier handler, this
    method inquires the current handler to check if all transactions queued up
    to this point are already executed.

    If no handler exists, then it is assumed that transactions were processed.

    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied

    @return the operation status
      @retval 0      All transactions were executed
      @retval -1     A timeout occurred
      @retval -2     An error occurred
  */
  int wait_for_applier_event_execution(ulonglong timeout);

  /**
    Returns the handler instance in the applier module responsible for
    certification.

    @note If new certification handlers appear, an interface must be created.

    @return a pointer to the applier's certification handler.
      @retval !=NULL The certification handler
      @retval NULL   No certification handler present
  */
  virtual Certification_handler* get_certification_handler();

  /**
     Checks if the given id matches any of  the event applying threads in the applier module handlers
     @param id  the thread id

     @return if it belongs to a thread
       @retval true   the id matches a SQL or worker thread
       @retval false  the id doesn't match any thread
   */
  virtual bool is_own_event_channel(my_thread_id id);

  /**
    Returns the applier module's queue size.

    @return the size of the queue
  */
  virtual ulong get_message_queue_size()
  {
    return incoming->size();
  }

  Member_applier_state get_applier_status()
  {
    if(applier_running)
      return APPLIER_STATE_ON;
    else if(suspended)
      return APPLIER_STATE_OFF;
    else
      return APPLIER_ERROR;
  }

private:

  /**
    Suspends the applier module, being transactions still queued in the incoming
    queue.

    @note if the proper condition is set, possible listeners can be awaken by
    this method.
  */
  void suspend_applier_module()
  {
    mysql_mutex_lock(&suspend_lock);

    suspended = true;

    THD_STAGE_INFO(applier_thd, stage_suspending);

    //Alert any interested party about the applier suspension
    mysql_cond_broadcast(&suspension_waiting_condition);

    while (suspended)
    {
      mysql_cond_wait(&suspend_cond, &suspend_lock);
    }

    THD_STAGE_INFO(applier_thd, stage_executing);

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

  //applier thread variables
  my_thread_handle applier_pthd;
#ifdef HAVE_PSI_INTERFACE
  PSI_thread_key key_thread_receiver;
#endif
  THD *applier_thd;

  //run conditions and locks
  mysql_mutex_t run_lock;
  mysql_cond_t  run_cond;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key run_key_mutex;
  PSI_cond_key  run_key_cond;
#endif
  /* Applier running flag */
  bool applier_running;
  /* Applier abort flag */
  bool applier_aborted;
  /* Applier error during execution */
  int applier_error;

  //condition and lock used to suspend/awake the applier module
  /* The lock for suspending/wait for the awake of  the applier module */
  mysql_mutex_t suspend_lock;
  /* The condition for suspending/wait for the awake of  the applier module */
  mysql_cond_t suspend_cond;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key suspend_key_mutex;
  PSI_cond_key suspend_key_cond;
#endif
  /* Suspend flag that informs if the applier is suspended */
  bool suspended;
  /* Suspend wait flag used when waiting for the applier to suspend */
  bool waiting_for_applier_suspension;

  /* The condition for signaling the applier suspension*/
  mysql_cond_t suspension_waiting_condition;
#ifdef HAVE_PSI_INTERFACE
  PSI_cond_key suspend_wait_key_cond;
#endif

  /* The incoming event queue */
  Synchronized_queue<Packet *> *incoming;

  /* The applier pipeline for event execution */
  Event_handler *pipeline;

  /**
    The Format description event used for Pipeline Events
    One event is enough for now as we assume that the cluster is homogeneous.
    If heterogeneous sources are used, then different format description events
    can be used to describe each source.
  */
  Format_description_log_event fde_evt;

  /* Applier timeout on shutdown */
  ulong stop_wait_timeout;
};

#endif /* GCS_APPLIER_INCLUDE */
