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

#ifndef PIPELINE_INTERFACES_INCLUDED
#define PIPELINE_INTERFACES_INCLUDED

#include <list>

#include <mysql/group_replication_priv.h>
#include <mysql/plugin_group_replication.h>
#include "mysql/components/services/log_builtins.h"

#include "mysqld_error.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/plugin_server_include.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

// Define the data packet type
#define DATA_PACKET_TYPE 1

/**
  @class Packet

  A generic interface for different kinds of packets.
*/
class Packet {
 public:
  /**
   Create a new generic packet of a certain type.

   @param[in]  type             the packet type
  */
  Packet(int type) : packet_type(type) {}

  virtual ~Packet() {}

  /**
   @return the packet type
  */
  int get_packet_type() { return packet_type; }

 private:
  int packet_type;
};

/**
  @class Data_packet

  A wrapper for raw network packets.
*/
class Data_packet : public Packet {
 public:
  /**
    Create a new data packet wrapper.

    @param[in]  data             the packet data
    @param[in]  len              the packet length
    @param[in]  consistency_level  the transaction consistency level
    @param[in]  online_members     the ONLINE members when the transaction
                                   message was delivered
  */
  Data_packet(const uchar *data, ulong len,
              enum_group_replication_consistency_level consistency_level =
                  GROUP_REPLICATION_CONSISTENCY_EVENTUAL,
              std::list<Gcs_member_identifier> *online_members = NULL)
      : Packet(DATA_PACKET_TYPE),
        payload(NULL),
        len(len),
        m_consistency_level(consistency_level),
        m_online_members(online_members) {
    payload = (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, len, MYF(0));
    memcpy(payload, data, len);
  }

  ~Data_packet() {
    my_free(payload);
    delete m_online_members;
  }

  uchar *payload;
  ulong len;
  const enum_group_replication_consistency_level m_consistency_level;
  std::list<Gcs_member_identifier> *m_online_members;
};

// Define the data packet type
#define UNDEFINED_EVENT_MODIFIER 0

// Define the size of the pipeline event buffer
#define DEFAULT_EVENT_BUFFER_SIZE 16384

/**
  @class Pipeline_event

  A wrapper for log events/packets. This class allows for the marking of events
  and its transformation between the packet and log event formats as requested
  in the interface.

  @note Events can be marked as with event modifiers.
        This is a generic field allowing modifiers to vary with use context.
        If not specified, this field has a default value of 0.
*/
class Pipeline_event {
 public:
  /**
    Create a new pipeline wrapper based on a packet.

    @note If a modifier is not provided the event will be marked as `UNDEFINED`

    @param[in]  base_packet      the wrapper packet
    @param[in]  fde_event        the format description event for conversions
    @param[in]  modifier         the event modifier
    @param[in]  consistency_level  the transaction consistency level
    @param[in]  online_members     the ONLINE members when the transaction
                                   message was delivered
  */
  Pipeline_event(Data_packet *base_packet,
                 Format_description_log_event *fde_event,
                 int modifier = UNDEFINED_EVENT_MODIFIER,
                 enum_group_replication_consistency_level consistency_level =
                     GROUP_REPLICATION_CONSISTENCY_EVENTUAL,
                 std::list<Gcs_member_identifier> *online_members = NULL)
      : packet(base_packet),
        log_event(NULL),
        event_context(modifier),
        format_descriptor(fde_event),
        m_consistency_level(consistency_level),
        m_online_members(online_members),
        m_online_members_memory_ownership(true) {}

  /**
    Create a new pipeline wrapper based on a log event.

    @note If a modifier is not provided the event will be marked as `UNDEFINED`

    @param[in]  base_event       the wrapper log event
    @param[in]  fde_event        the format description event for conversions
    @param[in]  modifier         the event modifier
    @param[in]  consistency_level  the transaction consistency level
    @param[in]  online_members     the ONLINE members when the transaction
                                   message was delivered
  */
  Pipeline_event(Log_event *base_event, Format_description_log_event *fde_event,
                 int modifier = UNDEFINED_EVENT_MODIFIER,
                 enum_group_replication_consistency_level consistency_level =
                     GROUP_REPLICATION_CONSISTENCY_EVENTUAL,
                 std::list<Gcs_member_identifier> *online_members = NULL)
      : packet(NULL),
        log_event(base_event),
        event_context(modifier),
        format_descriptor(fde_event),
        m_consistency_level(consistency_level),
        m_online_members(online_members),
        m_online_members_memory_ownership(true) {}

  ~Pipeline_event() {
    if (packet != NULL) {
      delete packet;
    }
    if (log_event != NULL) {
      delete log_event;
    }
    if (m_online_members_memory_ownership) {
      delete m_online_members;
    }
  }

  /**
    Return current format description event.

    @param[out]  out_fde    the outputted format description event

    @return Operation status
      @retval 0      OK
  */
  int get_FormatDescription(Format_description_log_event **out_fde) {
    *out_fde = format_descriptor;
    return 0;
  }

  /**
    Return a log event. If one does not exist, the contained packet will be
    converted into one.

    @param[out]  out_event    the outputted log event

    @return Operation status
      @retval 0      OK
      @retval !=0    error on conversion
  */
  int get_LogEvent(Log_event **out_event) {
    if (log_event == NULL)
      if (int error = convert_packet_to_log_event())
        return error; /* purecov: inspected */
    *out_event = log_event;
    return 0;
  }

  /**
    Sets the pipeline event's log event.

    @note This methods assume you have called reset_pipeline_event

    @param[in]  in_event    the given log event
  */
  void set_LogEvent(Log_event *in_event) { log_event = in_event; }

  /**
    Sets the pipeline event's packet.

    @note This methods assume you have called reset_pipeline_event

    @param[in]  in_packet    the given packet
  */
  void set_Packet(Data_packet *in_packet) { packet = in_packet; }

  /**
    Return a packet. If one does not exist, the contained log event will be
    converted into one.

    @param[out]  out_packet    the outputted packet

    @return the operation status
      @retval 0      OK
      @retval !=0    error on conversion
  */
  int get_Packet(Data_packet **out_packet) {
    if (packet == NULL)
      if (int error = convert_log_event_to_packet())
        return error; /* purecov: inspected */
    *out_packet = packet;
    return 0;
  }

  /**
    Returns the event type.
    Be it a Log_event or Packet, it's marked with a type we can extract.

    @return the pipeline event type
  */
  Log_event_type get_event_type() {
    if (packet != NULL)
      return (Log_event_type)packet->payload[EVENT_TYPE_OFFSET];
    else
      return log_event->get_type_code();
  }

  /**
    Sets the event context flag.

    @param[in]  modifier    the event modifier
  */
  void mark_event(int modifier) { event_context = modifier; }

  /**
    Returns the event context flag

    @return
  */
  int get_event_context() { return event_context; }

  /**
    Resets all variables in the event for reuse.
    Possible existing events/packets are deleted.
    The context flag is reset to UNDEFINED.
    Error messages are deleted.

    Format description events, are NOT deleted.
    This is due to the fact that they are given, and do not belong to the
    pipeline event.

    Transaction consistency level is not reset, despite the event
    is reset, consistency level belongs to the transaction.
  */
  void reset_pipeline_event() {
    if (packet != NULL) {
      delete packet; /* purecov: inspected */
      packet = NULL; /* purecov: inspected */
    }
    if (log_event != NULL) {
      delete log_event;
      log_event = NULL;
    }
    event_context = UNDEFINED_EVENT_MODIFIER;
  }

  /**
    Get transaction consistency level.
  */
  enum_group_replication_consistency_level get_consistency_level() {
    return m_consistency_level;
  }

  /**
    Get the list of ONLINE Group members when a
    Transaction_with_guarantee_message message was received, or NULL if
    if any group member version is from a version lower than
    #TRANSACTION_WITH_GUARANTEES_VERSION.
    For Transaction_message messages it always return NULL

    @return  list of all ONLINE members, if all members have version
             equal or greater than #TRANSACTION_WITH_GUARANTEES_VERSION
             for Transaction_with_guarantee_message messages
             otherwise  NULL

    @note the memory allocated for the list ownership belongs to the
          caller
  */
  std::list<Gcs_member_identifier> *get_online_members() {
    return m_online_members;
  }

  /**
    Release memory ownership of m_online_members.
  */
  void release_online_members_memory_ownership() {
    m_online_members_memory_ownership = false;
  }

 private:
  /**
    Converts the existing packet into a log event.

    @return the operation status
      @retval 0      OK
      @retval 1      Error on packet conversion
  */
  int convert_packet_to_log_event() {
    uint event_len = uint4korr(((uchar *)(packet->payload)) + EVENT_LEN_OFFSET);
    Binlog_read_error binlog_read_error = binlog_event_deserialize(
        packet->payload, event_len, format_descriptor, true, &log_event);

    if (unlikely(binlog_read_error.has_error())) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_CONVERT_PACKET_TO_EVENT,
                   binlog_read_error.get_str()); /* purecov: inspected */
    }

    delete packet;
    packet = NULL;

    return binlog_read_error.has_error();
  }

  /**
    Converts the existing log event into a packet.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on log event conversion
  */
  int convert_log_event_to_packet() {
    int error = 0;
    StringBuffer_ostream<DEFAULT_EVENT_BUFFER_SIZE> ostream;

    if ((error = log_event->write(&ostream))) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_CONVERT_EVENT_TO_PACKET,
                   "Out of memory"); /* purecov: inspected */
      return error;                  /* purecov: inspected */
    }

    packet = new Data_packet(reinterpret_cast<const uchar *>(ostream.c_ptr()),
                             ostream.length());

    delete log_event;
    log_event = NULL;

    return error;
  }

 private:
  Data_packet *packet;
  Log_event *log_event;
  int event_context;
  /* Format description event used on conversions */
  Format_description_log_event *format_descriptor;
  enum_group_replication_consistency_level m_consistency_level;
  std::list<Gcs_member_identifier> *m_online_members;
  bool m_online_members_memory_ownership;
};

/**
  @class Continuation

  Class used to wait on the execution of some action.
  The class can also be used to report whenever a transaction is discarded
  as a result of execution.
*/
class Continuation {
 public:
  Continuation() : ready(false), error_code(0), transaction_discarded(false) {
    mysql_mutex_init(key_GR_LOCK_pipeline_continuation, &lock,
                     MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_pipeline_continuation, &cond);
  }

  ~Continuation() {
    mysql_mutex_destroy(&lock);
    mysql_cond_destroy(&cond);
  }

  /**
    Wait until release.

    @note The continuation will not wait if an error as occurred in the past
          until reset_error_code() is invoked.

    @return the end status
      @retval 0      OK
      @retval !=0    Error returned on the execution
  */
  int wait() {
    mysql_mutex_lock(&lock);
    while (!ready && !error_code) {
      mysql_cond_wait(&cond, &lock); /* purecov: inspected */
    }
    ready = false;
    mysql_mutex_unlock(&lock);

    return error_code;
  }

  /**
    Signal the continuation that execution can continue.

    @param[in]  error             the error code if any
    @param[in]  tran_discarded    if the transaction to whom the event belongs
                                  was discarded
  */
  void signal(int error = 0, bool tran_discarded = false) {
    transaction_discarded = tran_discarded;
    error_code = error;

    mysql_mutex_lock(&lock);
    ready = true;
    mysql_mutex_unlock(&lock);
    mysql_cond_broadcast(&cond);
  }

  /**
    Reset the error code after a reported error.
  */
  void reset_error_code() { error_code = 0; }

  /**
    Sets the value of the flag for discarded transactions.

    @param[in]  discarded          is the transaction discarded.
  */
  void set_transation_discarded(bool discarded) {
    transaction_discarded = discarded;
  }

  /**
    Says if a transaction was discarded or not.

    @return the transaction discarded flag
      @retval 0       not discarded
      @retval !=0     discarded
  */
  bool is_transaction_discarded() { return transaction_discarded; }

 private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  bool ready;
  int error_code;
  bool transaction_discarded;
};

/**
  @class Pipeline_action

  A wrapper for pipeline actions.
  Pipeline actions, unlike normal events, do not transport data but execution
  instructions to be executed.

  @note On pipelines, actions unlike events, when submitted are always executed
        synchronously, meaning that when the call returns all handlers already
        processed it.
        Actions are good for executing start and stop actions for example, but
        also for configuring handlers.
*/
class Pipeline_action {
 public:
  Pipeline_action(int action_type) { type = action_type; }

  virtual ~Pipeline_action() {}

  /**
    Returns this action type.
    The type must be defined in all child classes.
    Different developing contexts can mean different sets of actions.

    @return the action type
  */
  int get_action_type() { return type; }

 private:
  int type;
};

/**
  @class Event_handler

  Interface for the application of events, them being packets or log events.
  Instances of this class can be composed among them to form execution
  pipelines.

  Handlers can also have roles that define their type of activity and can be
  used to identify them in a pipeline.
  Roles are defined by the user of this class according to his context.
*/
class Event_handler {
 public:
  Event_handler() : next_in_pipeline(NULL) {}

  virtual ~Event_handler() {}

  /**
    Initialization as defined in the handler implementation.

    @note It's up to the developer to decide its own initialization strategy,
    but the suggested approach is to initialize basic structures here and
    then depend on Action packets to configure and start existing handler
    routines.
  */
  virtual int initialize() = 0;

  /**
    Terminate the execution as defined in the handler implementation.
  */
  virtual int terminate() = 0;

  /**
    Handling of an event as defined in the handler implementation.

    As the handler can be included in a pipeline, somewhere in the
    method, the handler.next(event,continuation) method shall be
    invoked to allow the passing of the event to the next handler.

    Also, if an error occurs, the continuation object shall be used to
    propagate such error. This class can also be used to know/report
    when the transaction to whom the event belongs was discarded.

    @param[in]      event           the pipeline event to be handled
    @param[in,out]  continuation    termination notification object.
  */
  virtual int handle_event(Pipeline_event *event,
                           Continuation *continuation) = 0;

  /**
    Handling of an action as defined in the handler implementation.

    As the handler can be included in a pipeline, somewhere in the
    method, the handler.next(action) method shall be invoked to allow
    the passing of the action to the next handler.

    @note Actions should not be treated asynchronously and as so, Continuations
    are not used here. Errors are returned directly or passed by in the action
    if it includes support for such

    @param[in]      action         the pipeline event to be handled
  */
  virtual int handle_action(Pipeline_action *action) = 0;

  // pipeline appending methods

  /**
    Plug an handler to be the next in line for execution.

    @param[in]      next_handler       the next handler in line
  */
  void plug_next_handler(Event_handler *next_handler) {
    next_in_pipeline = next_handler;
  }

  /**
    Append an handler to be the last in line for execution.

    @param[in]      last_handler    the last handler in line
  */
  void append(Event_handler *last_handler) {
    Event_handler *pipeline_iter = this;
    while (pipeline_iter->next_in_pipeline) {
      pipeline_iter = pipeline_iter->next_in_pipeline;
    }
    pipeline_iter->plug_next_handler(last_handler);
  }

  /**
    Append an handler to a given pipeline.

    @note if the pipeline is null, the given handler will take its place

    @param[in,out]  pipeline       the pipeline to append the handler
    @param[in]      event_handler  the event handler to append
  */
  static void append_handler(Event_handler **pipeline,
                             Event_handler *event_handler) {
    if (!(*pipeline))
      *pipeline = event_handler;
    else
      (*pipeline)->append(event_handler);
  }

  // pipeline information methods

  /**
    Returns an handler that plays the given role

    @note if the pipeline is null, or the handler is not found, the retrieved
    handler will be null.

    @param[in]      pipeline       the handler pipeline
    @param[in]      role           the role to retrieve
    @param[out]     event_handler  the retrieved event handler
  */
  static void get_handler_by_role(Event_handler *pipeline, int role,
                                  Event_handler **event_handler) {
    *event_handler = NULL;

    if (pipeline == NULL) return; /* purecov: inspected */

    Event_handler *pipeline_iter = pipeline;
    while (pipeline_iter) {
      if (pipeline_iter->get_role() == role) {
        *event_handler = pipeline_iter;
        return;
      }
      pipeline_iter = pipeline_iter->next_in_pipeline;
    }
  }

  /**
    This method identifies the handler as being unique.

    An handler that is defined as unique is an handler that cannot be used
    more than once in a pipeline. Such tasks as certification and event
    application can only be done once. Unique handlers are also the only that,
    by being one of a kind, can be extracted during the pipeline life allowing
    dynamic changes to them.

    @return if the handler is the a unique handler
      @retval true      is a unique handler
      @retval false     is a repeatable handler
  */
  virtual bool is_unique() = 0;

  /**
    This method returns the handler role.
    Handlers can have different roles according to the tasks they
    represent. Is based on this role that certain components can
    extract and interact with pipeline handlers. This means that if a
    role is given to a singleton handler, no one else can have that
    role.

    @return the handler role
  */
  virtual int get_role() = 0;

  // pipeline destruction methods

  /**
    Shutdown and delete all handlers in the pipeline.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int terminate_pipeline() {
    int error = 0;
    while (next_in_pipeline != NULL) {
      Event_handler *pipeline_iter = this;
      Event_handler *temp_handler = NULL;
      while (pipeline_iter->next_in_pipeline != NULL) {
        temp_handler = pipeline_iter;
        pipeline_iter = pipeline_iter->next_in_pipeline;
      }
      if (pipeline_iter->terminate())
        error = 1;  // report an error, but try to finish the job /* purecov:
                    // inspected */
      delete temp_handler->next_in_pipeline;
      temp_handler->next_in_pipeline = NULL;
    }
    this->terminate();
    return error;
  }

 protected:
  /**
    Pass the event to the next handler in line. If none exists, this method
    will signal the continuation method and exit.

    @param[in]      event           the pipeline event to be handled
    @param[in,out]  continuation    termination notification object.
  */
  int next(Pipeline_event *event, Continuation *continuation) {
    if (next_in_pipeline)
      next_in_pipeline->handle_event(event, continuation);
    else
      continuation->signal();
    return 0;
  }

  /**
    Pass the action to the next handler in line.
    If none exists, this method will return

    @param[in]  action     the pipeline action to be handled
  */
  int next(Pipeline_action *action) {
    int error = 0;

    if (next_in_pipeline) error = next_in_pipeline->handle_action(action);

    return error;
  }

 private:
  // The next handler in the pipeline
  Event_handler *next_in_pipeline;
};

#endif
