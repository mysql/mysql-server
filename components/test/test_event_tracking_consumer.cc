/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"

#include "mysql/components/services/component_status_var_service.h"
#include "mysql/components/services/mysql_current_thread_reader.h"
#include "mysql/components/services/mysql_rwlock.h"
#include "mysql/components/services/mysql_thd_store_service.h"
#include "mysql/components/services/psi_rwlock.h"
#include "mysql/components/services/udf_registration.h"

#include "mysql/components/util/event_tracking/event_tracking_authentication_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_command_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_connection_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_general_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_global_variable_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_lifecycle_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_message_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_parse_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_query_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_stored_program_consumer_helper.h"
#include "mysql/components/util/event_tracking/event_tracking_table_access_consumer_helper.h"

#include "scope_guard.h"

#define EVENT_NAME(x) #x

REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                mysql_status_var_service);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_store, mysql_thd_store_service);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader, thread_reader);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, mysql_udf_registration);
REQUIRES_SERVICE_PLACEHOLDER_AS(event_tracking_authentication_information,
                                mysql_authentication_information);
REQUIRES_SERVICE_PLACEHOLDER_AS(event_tracking_authentication_method,
                                mysql_authentication_method);
REQUIRES_SERVICE_PLACEHOLDER_AS(event_tracking_general_information,
                                mysql_general_information);
REQUIRES_MYSQL_RWLOCK_SERVICE_PLACEHOLDER;
REQUIRES_PSI_RWLOCK_SERVICE_PLACEHOLDER;

namespace Event_tracking_implementation {
// Forward declarations
bool configure_event_tracking_filter_init(UDF_INIT *initid, UDF_ARGS *args,
                                          char *message);
long long configure_event_tracking_filter(UDF_INIT *, UDF_ARGS *args,
                                          unsigned char *,
                                          unsigned char *error);
bool display_session_data_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void display_session_data_deinit(UDF_INIT *initid);
char *display_session_data(UDF_INIT *, UDF_ARGS *, char *,
                           unsigned long *length, unsigned char *is_null,
                           unsigned char *error);
bool reset_event_tracking_counter_init(UDF_INIT *initid, UDF_ARGS *args,
                                       char *message);
long long reset_event_tracking_counter(UDF_INIT *, UDF_ARGS *args,
                                       unsigned char *, unsigned char *error);
}  // namespace Event_tracking_implementation

namespace Event_tracking_consumer {

/** Slot assigned to the component to store data in THD */
mysql_thd_store_slot g_slot{nullptr};

/**
  An example of component specific data that
  can be stored in THD
*/
class Connection_data final {
 public:
  /**
    Create data object

    @param [in] connection_id Connection Identifier
  */
  explicit Connection_data(unsigned long connection_id)
      : connection_id_(connection_id),
        current_trace_("==============================================="),
        last_trace_(),
        indent_() {}

  /** Simple destructor */
  ~Connection_data() = default;

  /** Get session Identifier */
  unsigned long get_connection_id() const { return connection_id_; }

  /** Add to current trace */
  void append_to_current_trace(std::string &event, int indent) {
    if (indent == -1 && indent_.length() > 0)
      indent_ = indent_.substr(0, indent_.length() - 2);

    current_trace_ += "\n";
    current_trace_ += indent_;
    current_trace_ += event;

    if (indent == 1) indent_.append("--");
  }

  /** end_current_trace */
  void end_current_trace() {
    current_trace_ += "\n";
    current_trace_.append("===============================================");
    last_trace_ = current_trace_;
    current_trace_.assign("===============================================");
    indent_.clear();
  }

  /** Get last trace */
  std::string get_last_trace() { return last_trace_; }

 private:
  /** Connection Identifier */
  unsigned long connection_id_;
  /** Current execution trace */
  std::string current_trace_;
  /** Last execution trace */
  std::string last_trace_;
  /** Intendation */
  std::string indent_;
};

/**
  A map that stores references to all Connection_data objects
  created by the component.

  These references are freed during deinit to ensure that
  there is no memory leak.
*/
class Connection_data_map final {
 public:
  /** Construct the object */
  Connection_data_map() {
    static PSI_rwlock_key key_LOCK_session_data_objects_;
    static PSI_rwlock_info all_locks[] = {
        {&key_LOCK_session_data_objects_, "test_event_consumer", 0, 0,
         "A RW lock to guard session data objects."}};
    mysql_rwlock_register("data", all_locks, 1);
    mysql_rwlock_init(key_LOCK_session_data_objects_, &lock_);
  }

  /** Cleanup */
  ~Connection_data_map() {
    session_data_objects_.clear();
    mysql_rwlock_destroy(&lock_);
  }

  /**
    Create a Connection_data object, store it in global map
    and return the handle to caller.

    @param [in] connection_id  Used as key to store the
                               newly created object

    @return handle to Connection_data object in case of success,
            nullptr otherwise
  */
  Connection_data *create(unsigned long connection_id) {
    try {
      mysql_rwlock_wrlock(&lock_);
      auto release_guard =
          create_scope_guard([&] { mysql_rwlock_unlock(&lock_); });
      if (session_data_objects_.find(connection_id) !=
          session_data_objects_.end())
        return nullptr;
      session_data_objects_[connection_id] = std::unique_ptr<Connection_data>(
          new (std::nothrow) Connection_data(connection_id));
      return session_data_objects_[connection_id].get();
    } catch (...) {
      return nullptr;
    }
  }

  /**
    Remove object from map and free corresponding memory

    @param [in] connection_id  Key used to locate Connection_data object
  */
  void remove(unsigned long connection_id) {
    try {
      mysql_rwlock_wrlock(&lock_);
      session_data_objects_.erase(connection_id);
      mysql_rwlock_unlock(&lock_);
    } catch (...) {
    }
  }

 private:
  /** Structure to store all allocated Connecction_data objects */
  std::unordered_map<unsigned long, std::unique_ptr<Connection_data>>
      session_data_objects_;
  /** Lock to protect the map */
  mysql_rwlock_t lock_;
};

/** A global instance used for storing Connection data objects */
Connection_data_map *g_session_data_map{nullptr};

/** De-allocation callback for Connection_data */
int free_resource(void *resource) {
  if (resource) {
    auto *connection_data = reinterpret_cast<Connection_data *>(resource);
    if (g_session_data_map)
      g_session_data_map->remove(connection_data->get_connection_id());
  }
  return 0;
}

/** Represents types of services implemented by the component */
enum class Event_types {
  AUTHENTICATION = 0,
  COMMAND,
  CONNECTION,
  GENERAL,
  GLOBAL_VARIABLE,
  MESSAGE,
  PARSE,
  QUERY,
  SHUTDOWN,
  STARTUP,
  STORED_PROGRAM,
  TABLE_ACCESS,
  AUTHENTICATION_INFORMATION,
  GENERAL_INFORMATION,
  LAST
};

/** Status variables to keep track of various events */
class Event_tracking_counters {
 public:
  /** Constructor */
  Event_tracking_counters() {
    for (unsigned int i = 0; i < static_cast<unsigned int>(Event_types::LAST);
         ++i)
      event_counters_[i] = 0;
  }

  /** Destructor */
  ~Event_tracking_counters() = default;

  /**
    Helper function to fetch required counter value for
    a given status variable

    @param [out] var         Status variable type and value
    @param [out] buf         Buffer to store the status variable value
    @param [in]  event_type  Event type to identify the required counter

    @returns Status of operation
      @retval 0 Success
      @retval 1 Failure
  */
  int show_counter_value(SHOW_VAR *var, char *buf, Event_types event_type) {
    if (event_type == Event_types::LAST) return 1;

    var->type = SHOW_INT;
    var->value = buf;
    long *value = reinterpret_cast<long *>(buf);
    *value = static_cast<long>(
        event_counters_[static_cast<unsigned int>(event_type)].load());
    return 0;
  }

  /**
    Increment a counter

    @param [in] event_type  Event type to identify required counter
  */
  void increment_counter(Event_types event_type) {
    if (event_type != Event_types::LAST)
      ++event_counters_[static_cast<unsigned int>(event_type)];
  }

  /**
    Reset a counter to 0

    @param [in] event_type  Event type to identify required counter
  */
  void reset_event_tracking_counter(Event_types event_type) {
    if (event_type != Event_types::LAST)
      event_counters_[static_cast<unsigned int>(event_type)] = 0;
  }

  void reset_all() {
    for (unsigned int index = 0;
         index < static_cast<unsigned int>(Event_types::LAST); ++index)
      event_counters_[index] = 0;
  }

 private:
  /** Counters for various events */
  std::atomic<long>
      event_counters_[static_cast<unsigned int>(Event_types::LAST)];
};

/** Global object to maintain event counters */
Event_tracking_counters *g_event_tracking_counters{nullptr};

/**
  Helper function to fetch value of authentication
  event counter for status variable
*/
static int show_counter_authentication(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::AUTHENTICATION)
             : 1;
}

/**
  Helper function to fetch value of command
  event counter for status variable
*/
static int show_counter_command(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::COMMAND)
             : 1;
}

/**
  Helper function to fetch value of connection
  event counter for status variable
*/
static int show_counter_connection(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::CONNECTION)
             : 1;
}

/**
  Helper function to fetch value of general
  event counter for status variable
*/
static int show_counter_general(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::GENERAL)
             : 1;
}

/**
  Helper function to fetch value of global
  variable event counter for status variable
*/
static int show_counter_global_variable(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::GLOBAL_VARIABLE)
             : 1;
}

/**
  Helper function to fetch value of message
  event counter for status variable
*/
static int show_counter_message(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::MESSAGE)
             : 1;
}

/**
  Helper function to fetch value of parse
  event counter for status variable
*/
static int show_counter_parse(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(var, buf,
                                                             Event_types::PARSE)
             : 1;
}

/**
  Helper function to fetch value of query
  event counter for status variable
*/
static int show_counter_query(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(var, buf,
                                                             Event_types::QUERY)
             : 1;
}

/**
  Helper function to fetch value of shutdown
  event counter for status variable
*/
static int show_counter_shutdown(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::SHUTDOWN)
             : 1;
}

/**
  Helper function to fetch value of startup
  event counter for status variable
*/
static int show_counter_startup(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::STARTUP)
             : 1;
}

/**
  Helper function to fetch value of stored program
  executionnevent counter for status variable
*/
static int show_counter_stored_program(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::STORED_PROGRAM)
             : 1;
}

/**
  Helper function to fetch value of table
  access event counter for status variable
*/
static int show_counter_table_access(MYSQL_THD, SHOW_VAR *var, char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::TABLE_ACCESS)
             : 1;
}

/**
  Helper function to fetch value of authentication
  information retrieval counter for status variable
*/
static int show_counter_authentication_information(MYSQL_THD, SHOW_VAR *var,
                                                   char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::AUTHENTICATION_INFORMATION)
             : 1;
}

/**
  Helper function to fetch value of general
  information retrieval counter for status variable
*/
static int show_counter_general_information(MYSQL_THD, SHOW_VAR *var,
                                            char *buf) {
  return g_event_tracking_counters
             ? g_event_tracking_counters->show_counter_value(
                   var, buf, Event_types::GENERAL_INFORMATION)
             : 1;
}

/** Status variable exposing various event counters */
static SHOW_VAR status_vars[] = {
    {"test_event_tracking_consumer.counter_authentication",
     (char *)show_counter_authentication, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_command",
     (char *)show_counter_command, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_connection",
     (char *)show_counter_connection, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_general",
     (char *)show_counter_general, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_global_variable",
     (char *)show_counter_global_variable, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_message",
     (char *)show_counter_message, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_parse", (char *)show_counter_parse,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_query", (char *)show_counter_query,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_shutdown",
     (char *)show_counter_shutdown, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_startup",
     (char *)show_counter_startup, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_stored_program",
     (char *)show_counter_stored_program, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_table_access",
     (char *)show_counter_table_access, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_authentication_information",
     (char *)show_counter_authentication_information, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"test_event_tracking_consumer.counter_general_information",
     (char *)show_counter_general_information, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    // null terminator required
    {nullptr, nullptr, SHOW_UNDEF, SHOW_SCOPE_UNDEF}};

/** Helper method to unregister functions */
void unregister_functions() {
  int was_present = 0;
  (void)mysql_udf_registration->udf_unregister(
      "configure_event_tracking_filter", &was_present);
  (void)mysql_udf_registration->udf_unregister("display_session_data",
                                               &was_present);
  (void)mysql_udf_registration->udf_unregister("reset_event_tracking_counter",
                                               &was_present);
}

/** Helper method to register functions */
bool register_functions() {
  if (mysql_udf_registration->udf_register(
          "configure_event_tracking_filter", INT_RESULT,
          (Udf_func_any)
              Event_tracking_implementation::configure_event_tracking_filter,
          Event_tracking_implementation::configure_event_tracking_filter_init,
          nullptr) ||
      mysql_udf_registration->udf_register(
          "display_session_data", STRING_RESULT,
          (Udf_func_any)Event_tracking_implementation::display_session_data,
          Event_tracking_implementation::display_session_data_init,
          Event_tracking_implementation::display_session_data_deinit) ||
      mysql_udf_registration->udf_register(
          "reset_event_tracking_counter", INT_RESULT,
          (Udf_func_any)
              Event_tracking_implementation::reset_event_tracking_counter,
          Event_tracking_implementation::reset_event_tracking_counter_init,
          nullptr)) {
    unregister_functions();
    return true;
  }
  return false;
}

/**
  Initialization function for component - Used when loading the component
*/
static mysql_service_status_t init() {
  bool slot_registered = true;
  bool variables_registered = true;
  bool functions_registered = true;

  auto cleanup = create_scope_guard([&] {
    if (!slot_registered)
      (void)mysql_thd_store_service->unregister_slot(g_slot);
    if (!variables_registered)
      (void)mysql_status_var_service->unregister_variable(status_vars);
    if (!functions_registered) unregister_functions();
    if (g_session_data_map) delete g_session_data_map;
    g_session_data_map = nullptr;
  });

  /* Register slot for the component */
  if ((slot_registered = mysql_thd_store_service->register_slot(
           "component_test_event_tracking_consumer",
           Event_tracking_consumer::free_resource, &g_slot)))
    return 1;

  /* Register status variables */
  if ((variables_registered =
           mysql_status_var_service->register_variable(status_vars))) {
    return 1;
  }

  /* Register functions */
  if ((functions_registered = register_functions())) {
    return 1;
  }

  g_session_data_map = new (std::nothrow) Connection_data_map();
  if (!g_session_data_map) return 1;

  g_event_tracking_counters = new (std::nothrow) Event_tracking_counters();
  if (!g_event_tracking_counters) return 1;

  cleanup.release();
  return 0;
}

/**
  De-initialization function for component - Used when unloading the component
*/
static mysql_service_status_t deinit() {
  /*
    The session might have data stored in the slot assigned to the component.
    Reset it to nullptr here. Otherwise the session will generate a warning
    session diconnect. Reseting session data to nullptr is sufficient because
    deallocation of g_session_data_map takes care of releasing memory.
  */
  MYSQL_THD o_thd{nullptr};
  if (!thread_reader->get(&o_thd)) {
    mysql_thd_store_service->set(o_thd, g_slot, nullptr);
  }

  if (g_event_tracking_counters) delete g_event_tracking_counters;
  if (g_session_data_map) delete g_session_data_map;

  /* Unregister functions */
  unregister_functions();

  /* Unregister status variables */
  if (mysql_status_var_service->unregister_variable(status_vars)) return 1;

  /* Unregister session store slot */
  if (mysql_thd_store_service->unregister_slot(g_slot)) return 1;

  return 0;
}

}  // namespace Event_tracking_consumer

namespace Event_tracking_implementation {

const size_t MAX_STRING_SIZE = 1024;

using Event_tracking_consumer::Connection_data;
using Event_tracking_consumer::Event_types;
using Event_tracking_consumer::g_event_tracking_counters;
using Event_tracking_consumer::g_session_data_map;
using Event_tracking_consumer::g_slot;

/**
  Names of the services implemented by the component
  These are used by following functions:
  - Function to set sub-event filters
  - Function to reset event specific counters
*/
const char *service_names[] = {"event_tracking_authentication",
                               "event_tracking_command",
                               "event_tracking_connection",
                               "event_tracking_general",
                               "event_tracking_global_variable",
                               "event_tracking_message",
                               "event_tracking_parse",
                               "event_tracking_query",
                               "event_tracking_lifecycle",
                               "event_tracking_lifecycle",
                               "event_tracking_stored_program",
                               "event_tracking_table_access",
                               "",
                               "",
                               nullptr};

/**  Init method for configure_event_tracking_filter function */
bool configure_event_tracking_filter_init(UDF_INIT *initid, UDF_ARGS *args,
                                          char *message) {
  initid->ptr = nullptr;

  if (args->arg_count != 2) {
    sprintf(message,
            "Mismatch in number of arguments to the function. Expected 2 "
            "arguments");
    return true;
  }

  if (args->arg_type[0] != STRING_RESULT) {
    sprintf(message,
            "Mismatch in type of argument. Expected string argument for event "
            "name");
    return true;
  }

  if (args->arg_type[1] != INT_RESULT) {
    sprintf(message,
            "Mismatch in type of argument. Expected integer argument for "
            "filtered subevent.");
    return true;
  }

  return false;
}

/**  Function to set sub-event filters for a given event */
long long configure_event_tracking_filter(UDF_INIT *, UDF_ARGS *args,
                                          unsigned char *,
                                          unsigned char *error) {
  auto cleanup = create_scope_guard([&] { *error = 1; });

  if (!args->args[0] || !args->args[1]) return 0;

  std::string event_name{args->args[0], args->lengths[0]};
  unsigned int index = 0;

  bool found = false;

  for (index = 0; index < static_cast<unsigned int>(Event_types::LAST);
       ++index) {
    if (event_name == service_names[index]) {
      found = true;
      break;
    }
  }

  if (!found) return 0;

  unsigned long long new_filter =
      *(reinterpret_cast<unsigned long long *>(args->args[1]));

  switch (static_cast<Event_types>(index)) {
    case Event_types::AUTHENTICATION:
      Event_tracking_authentication_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_AUTHENTICATION_ALL);
      break;
    case Event_types::COMMAND:
      Event_tracking_command_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_COMMAND_ALL);
      break;
    case Event_types::CONNECTION:
      Event_tracking_connection_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_CONNECTION_ALL);
      break;
    case Event_types::GENERAL:
      Event_tracking_general_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_GENERAL_ALL);
      break;
    case Event_types::GLOBAL_VARIABLE:
      Event_tracking_global_variable_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_GLOBAL_VARIABLE_ALL);
      break;
    case Event_types::MESSAGE:
      Event_tracking_message_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_MESSAGE_ALL);
      break;
    case Event_types::PARSE:
      Event_tracking_parse_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_PARSE_ALL);
      break;
    case Event_types::QUERY:
      Event_tracking_query_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_QUERY_ALL);
      break;
    case Event_types::SHUTDOWN:
      Event_tracking_lifecycle_implementation::shutdown_filtered_sub_events =
          new_filter & (EVENT_TRACKING_SHUTDOWN_ALL);
      break;
    case Event_types::STARTUP:
      Event_tracking_lifecycle_implementation::startup_filtered_sub_events =
          new_filter & (EVENT_TRACKING_STARTUP_ALL);
      break;
    case Event_types::STORED_PROGRAM:
      Event_tracking_stored_program_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_STORED_PROGRAM_ALL);
      break;
    case Event_types::TABLE_ACCESS:
      Event_tracking_table_access_implementation::filtered_sub_events =
          new_filter & (EVENT_TRACKING_TABLE_ACCESS_ALL);
      break;
    default:
      return 0;
  };

  cleanup.release();
  return 1;
}

/** Init function for display_session_data */
bool display_session_data_init(UDF_INIT *initid, UDF_ARGS *args,
                               char *message) {
  initid->ptr = nullptr;
  if (args->arg_count != 0) {
    sprintf(message, "No argument is expected for the function.");
    return true;
  }

  char *result = new (std::nothrow) char[MAX_STRING_SIZE]{'\0'};
  if (result == nullptr) {
    sprintf(message, "Failed to allocated required memory for result");
    return true;
  }
  initid->max_length = MAX_STRING_SIZE;
  initid->maybe_null = true;
  initid->ptr = result;
  return false;
}

/** Deinit function for display_session_data */
void display_session_data_deinit(UDF_INIT *initid) {
  if (initid->ptr) delete[] initid->ptr;
  initid->ptr = nullptr;
}

/** Fetch component specific data from THD and show connection_id from it */
char *display_session_data(UDF_INIT *initid, UDF_ARGS *, char *,
                           unsigned long *length, unsigned char *is_null,
                           unsigned char *error) {
  auto cleanup = create_scope_guard([&] {
    *is_null = 1;
    *error = 1;
  });

  MYSQL_THD o_thd{nullptr};
  if (thread_reader->get(&o_thd)) return nullptr;

  Connection_data *session_data = reinterpret_cast<Connection_data *>(
      mysql_thd_store_service->get(o_thd, g_slot));
  if (!session_data) return nullptr;

  std::string last_trace = session_data->get_last_trace();

  if (last_trace.empty() ||
      last_trace.length() > static_cast<size_t>(initid->max_length - 1))
    return nullptr;

  strncpy(initid->ptr, last_trace.c_str(), last_trace.length());
  *length = last_trace.length();

  cleanup.release();
  return initid->ptr;
}

/** Init function for reset_event_tracking_counter */
bool reset_event_tracking_counter_init(UDF_INIT *initid, UDF_ARGS *args,
                                       char *message) {
  initid->ptr = nullptr;
  if (args->arg_count != 1) {
    sprintf(message,
            "Mismatch in number of arguments to the function. Expected 1 "
            "arguments");
    return true;
  }

  if (args->arg_type[0] != STRING_RESULT) {
    sprintf(message,
            "Mismatch in type of argument. Expected string argument for event "
            "name");
    return true;
  }
  return false;
}

/** Reset event tracking counter for a given event type */
long long reset_event_tracking_counter(UDF_INIT *, UDF_ARGS *args,
                                       unsigned char *, unsigned char *error) {
  auto cleanup = create_scope_guard([&] { *error = 1; });

  if (!args->args[0]) return 0;

  std::string event_name{args->args[0], args->lengths[0]};

  if (event_name == "all") {
    g_event_tracking_counters->reset_all();
  } else {
    unsigned int index = 0;

    bool found = false;

    for (index = 0; index < static_cast<unsigned int>(Event_types::LAST);
         ++index) {
      if (event_name == service_names[index]) {
        found = true;
        break;
      }
    }

    if (!found || index >= static_cast<unsigned int>(Event_types::LAST))
      return 0;

    g_event_tracking_counters->reset_event_tracking_counter(
        static_cast<Event_types>(index));
  }
  cleanup.release();
  return 1;
}

static bool update_current_trace(std::string &event_name,
                                 unsigned long connection_id, int indent = 0) {
  MYSQL_THD o_thd{nullptr};
  if (thread_reader->get(&o_thd)) return true;

  Connection_data *session_data = reinterpret_cast<Connection_data *>(
      mysql_thd_store_service->get(o_thd, g_slot));
  if (!session_data) {
    session_data = g_session_data_map->create(connection_id);
    if (!session_data) return true;

    if (mysql_thd_store_service->set(o_thd, g_slot,
                                     reinterpret_cast<void *>(session_data)))
      g_session_data_map->remove(connection_id);
  }

  session_data->append_to_current_trace(event_name, indent);
  return false;
}

static bool end_current_trace() {
  MYSQL_THD o_thd{nullptr};
  if (thread_reader->get(&o_thd)) return true;

  Connection_data *session_data = reinterpret_cast<Connection_data *>(
      mysql_thd_store_service->get(o_thd, g_slot));
  if (!session_data) return true;

  session_data->end_current_trace();
  return false;
}

/* START: Service Implementation */

mysql_event_tracking_authentication_subclass_t
    Event_tracking_authentication_implementation::filtered_sub_events = 0;
bool Event_tracking_authentication_implementation::callback(
    const mysql_event_tracking_authentication_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::AUTHENTICATION);

  auto retrieve_and_compare =
      [&](bool expect_auth_methods, bool expect_user, bool expect_host,
          bool expect_role,
          Event_tracking_consumer::Event_types event) -> void {
    unsigned int auth_methods;
    mysql_cstring_with_length cstring_value;
    bool bool_value = false;
    event_tracking_authentication_information_handle handle = nullptr;
    event_tracking_authentication_method_handle method_handle = nullptr;
    bool result = false;

    if (mysql_authentication_information->init(&handle)) return;

    auto cleanup_guard = create_scope_guard([&] {
      (void)mysql_authentication_information->deinit(handle);
      handle = nullptr;
    });

    result = mysql_authentication_information->get(
        handle, "authentcation_method_count", &auth_methods);
    if (expect_auth_methods != !result) return;

    if (expect_auth_methods) {
      result = mysql_authentication_information->get(
          handle, "authentication_method_info", &method_handle);
      if (result) return;
      for (unsigned i = 0; i < auth_methods; ++i) {
        result = mysql_authentication_method->get(method_handle, i, "name",
                                                  &cstring_value);
        if (result) return;
      }
    }

    result = mysql_authentication_information->get(handle, "new_user",
                                                   &cstring_value);
    if (expect_user != !result) return;

    result = mysql_authentication_information->get(handle, "new_host",
                                                   &cstring_value);
    if (expect_host != !result) return;

    result =
        mysql_authentication_information->get(handle, "is_role", &bool_value);
    if (expect_role != !result) return;

    g_event_tracking_counters->increment_counter(event);

    return;
  };

  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_AUTHENTICATION_FLUSH: {
      retrieve_and_compare(false, false, false, true,
                           Event_types::AUTHENTICATION_INFORMATION);
      event_name.assign(EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_FLUSH));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE: {
      retrieve_and_compare(true, false, false, true,
                           Event_types::AUTHENTICATION_INFORMATION);
      event_name.assign(
          EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE: {
      retrieve_and_compare(true, false, false, true,
                           Event_types::AUTHENTICATION_INFORMATION);
      event_name.assign(
          EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME: {
      retrieve_and_compare(true, true, true, true,
                           Event_types::AUTHENTICATION_INFORMATION);
      event_name.assign(
          EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME));
      break;
    }
    case EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP: {
      retrieve_and_compare(true, false, false, true,
                           Event_types::AUTHENTICATION_INFORMATION);
      event_name.assign(EVENT_NAME(EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP));
      break;
    }
    default:
      return true;
  };

  return update_current_trace(event_name, data->connection_id);
}

mysql_event_tracking_command_subclass_t
    Event_tracking_command_implementation::filtered_sub_events = 0;
bool Event_tracking_command_implementation::callback(
    const mysql_event_tracking_command_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::COMMAND);
  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_COMMAND_START: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_COMMAND_START));
      event_name += "(Command: ";
      event_name.append(data->command.str, data->command.length);
      event_name += ")";
      return update_current_trace(event_name, data->connection_id, 1);
    }
    case EVENT_TRACKING_COMMAND_END: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_COMMAND_END));
      event_name += "(Command: ";
      event_name.append(data->command.str, data->command.length);
      event_name += ")";
      return (update_current_trace(event_name, data->connection_id, -1) ||
              end_current_trace());
    }
    default:
      return true;
  };
}

mysql_event_tracking_connection_subclass_t
    Event_tracking_connection_implementation::filtered_sub_events = 0;
bool Event_tracking_connection_implementation::callback(
    const mysql_event_tracking_connection_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::CONNECTION);
  if (data->event_subclass & (EVENT_TRACKING_CONNECTION_CONNECT |
                              EVENT_TRACKING_CONNECTION_CHANGE_USER |
                              EVENT_TRACKING_CONNECTION_DISCONNECT)) {
    MYSQL_THD o_thd{nullptr};

    if (!thread_reader->get(&o_thd)) {
      switch (data->event_subclass) {
        case EVENT_TRACKING_CONNECTION_CONNECT: {
          Connection_data *session_data =
              g_session_data_map->create(data->connection_id);
          if (session_data) {
            if (mysql_thd_store_service->set(
                    o_thd, g_slot, reinterpret_cast<void *>(session_data)))
              g_session_data_map->remove(data->connection_id);
          }
          break;
        }
        case EVENT_TRACKING_CONNECTION_DISCONNECT: {
          Connection_data *session_data = reinterpret_cast<Connection_data *>(
              mysql_thd_store_service->get(o_thd, g_slot));
          if (session_data) {
            g_session_data_map->remove(data->connection_id);
            (void)mysql_thd_store_service->set(o_thd, g_slot, nullptr);
          }
          break;
        }
        case EVENT_TRACKING_CONNECTION_CHANGE_USER: {
          Connection_data *session_data = reinterpret_cast<Connection_data *>(
              mysql_thd_store_service->get(o_thd, g_slot));
          if (session_data) {
            session_data = reinterpret_cast<Connection_data *>(
                mysql_thd_store_service->get(o_thd, g_slot));
            if (session_data) {
              (void)mysql_thd_store_service->set(o_thd, g_slot, nullptr);
              g_session_data_map->remove(data->connection_id);
            }
            session_data = g_session_data_map->create(data->connection_id);
            if (session_data) {
              if (mysql_thd_store_service->set(
                      o_thd, g_slot, reinterpret_cast<void *>(session_data)))
                delete session_data;
            }
          }
          break;
        }
        default:
          break;
      };
    }
  }
  return false;
}

mysql_event_tracking_general_subclass_t
    Event_tracking_general_implementation::filtered_sub_events = 0;
bool Event_tracking_general_implementation::callback(
    const mysql_event_tracking_general_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::GENERAL);
  std::string event_name;

  auto retrieve_and_compare = [&](bool expect_rows, bool expect_time,
                                  bool expect_external_user,
                                  Event_types event) {
    event_tracking_general_information_handle handle = nullptr;
    uint64_t num_value;
    mysql_cstring_with_length cstring_value;
    bool result = false;

    if (mysql_general_information->init(&handle)) return;

    auto cleanup_guard = create_scope_guard([&] {
      (void)mysql_general_information->deinit(handle);
      handle = nullptr;
    });

    result =
        mysql_general_information->get(handle, "external_user", &cstring_value);
    if (expect_external_user != !result) return;

    result = mysql_general_information->get(handle, "time", &num_value);
    if (expect_time != !result) return;

    result = mysql_general_information->get(handle, "rows", &num_value);
    if (expect_rows != !result) return;

    g_event_tracking_counters->increment_counter(event);

    return;
  };

  switch (data->event_subclass) {
    case EVENT_TRACKING_GENERAL_LOG: {
      retrieve_and_compare(true, true, true, Event_types::GENERAL_INFORMATION);
      event_name.append(EVENT_NAME(EVENT_TRACKING_GENERAL_LOG));
      break;
    }
    case EVENT_TRACKING_GENERAL_ERROR: {
      retrieve_and_compare(true, true, true, Event_types::GENERAL_INFORMATION);
      event_name.append(EVENT_NAME(EVENT_TRACKING_GENERAL_ERROR));
      break;
    }
    case EVENT_TRACKING_GENERAL_RESULT: {
      retrieve_and_compare(true, true, true, Event_types::GENERAL_INFORMATION);
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GENERAL_RESULT));
      break;
    }
    case EVENT_TRACKING_GENERAL_STATUS: {
      retrieve_and_compare(true, true, true, Event_types::GENERAL_INFORMATION);
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GENERAL_STATUS));
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id);
}

mysql_event_tracking_global_variable_subclass_t
    Event_tracking_global_variable_implementation::filtered_sub_events = 0;
bool Event_tracking_global_variable_implementation::callback(
    const mysql_event_tracking_global_variable_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::GLOBAL_VARIABLE);
  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_GLOBAL_VARIABLE_GET: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GLOBAL_VARIABLE_GET));
      break;
    }
    case EVENT_TRACKING_GLOBAL_VARIABLE_SET: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_GLOBAL_VARIABLE_SET));
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id);
}

mysql_event_tracking_startup_subclass_t
    Event_tracking_lifecycle_implementation::startup_filtered_sub_events = 0;
bool Event_tracking_lifecycle_implementation::callback(
    const mysql_event_tracking_startup_data *data [[maybe_unused]]) {
  g_event_tracking_counters->increment_counter(Event_types::STARTUP);
  return false;
}

mysql_event_tracking_shutdown_subclass_t
    Event_tracking_lifecycle_implementation::shutdown_filtered_sub_events = 0;
bool Event_tracking_lifecycle_implementation::callback(
    const mysql_event_tracking_shutdown_data *data [[maybe_unused]]) {
  g_event_tracking_counters->increment_counter(Event_types::SHUTDOWN);
  return false;
}

mysql_event_tracking_message_subclass_t
    Event_tracking_message_implementation::filtered_sub_events = 0;
bool Event_tracking_message_implementation::callback(
    const mysql_event_tracking_message_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::MESSAGE);
  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_MESSAGE_INTERNAL: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_MESSAGE_INTERNAL));
      break;
    }
    case EVENT_TRACKING_MESSAGE_USER: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_MESSAGE_USER));
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id);
}

mysql_event_tracking_parse_subclass_t
    Event_tracking_parse_implementation::filtered_sub_events = 0;
bool Event_tracking_parse_implementation::callback(
    mysql_event_tracking_parse_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::PARSE);
  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_PARSE_PREPARSE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_PARSE_PREPARSE));
      event_name += "(Query: ";
      event_name.append(data->query.str, data->query.length);
      event_name += ")";
      break;
    }
    case EVENT_TRACKING_PARSE_POSTPARSE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_PARSE_POSTPARSE));
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id);
}

mysql_event_tracking_query_subclass_t
    Event_tracking_query_implementation::filtered_sub_events = 0;
bool Event_tracking_query_implementation::callback(
    const mysql_event_tracking_query_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::QUERY);
  std::string event_name;
  auto add_query_details = [&] {
    if (data->query.length > 0) {
      event_name += "(Query: ";
      event_name += {data->query.str, data->query.length};
      event_name += ")";
    }
  };
  int indent{0};
  switch (data->event_subclass) {
    case EVENT_TRACKING_QUERY_START: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_START));
      add_query_details();
      indent = 1;
      break;
    }
    case EVENT_TRACKING_QUERY_NESTED_START: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_NESTED_START));
      add_query_details();
      indent = 1;
      break;
    }
    case EVENT_TRACKING_QUERY_STATUS_END: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_STATUS_END));
      add_query_details();
      indent = -1;
      break;
    }
    case EVENT_TRACKING_QUERY_NESTED_STATUS_END: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_QUERY_NESTED_STATUS_END));
      add_query_details();
      indent = -1;
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id, indent);
}

mysql_event_tracking_stored_program_subclass_t
    Event_tracking_stored_program_implementation::filtered_sub_events = 0;
bool Event_tracking_stored_program_implementation::callback(
    const mysql_event_tracking_stored_program_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::STORED_PROGRAM);
  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_STORED_PROGRAM_EXECUTE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_STORED_PROGRAM_EXECUTE));
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id);
}

mysql_event_tracking_table_access_subclass_t
    Event_tracking_table_access_implementation::filtered_sub_events = 0;
bool Event_tracking_table_access_implementation::callback(
    const mysql_event_tracking_table_access_data *data) {
  g_event_tracking_counters->increment_counter(Event_types::TABLE_ACCESS);
  std::string event_name;
  switch (data->event_subclass) {
    case EVENT_TRACKING_TABLE_ACCESS_READ: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_READ));
      break;
    }
    case EVENT_TRACKING_TABLE_ACCESS_INSERT: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_INSERT));
      break;
    }
    case EVENT_TRACKING_TABLE_ACCESS_UPDATE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_UPDATE));
      break;
    }
    case EVENT_TRACKING_TABLE_ACCESS_DELETE: {
      event_name.assign(EVENT_NAME(EVENT_TRACKING_TABLE_ACCESS_DELETE));
      break;
    }
    default:
      return true;
  };
  return update_current_trace(event_name, data->connection_id);
}

/* END: Service Implementation */

}  // namespace Event_tracking_implementation

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
IMPLEMENTS_SERVICE_EVENT_TRACKING_AUTHENTICATION(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_COMMAND(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_CONNECTION(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_GENERAL(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_GLOBAL_VARIABLE(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_LIFECYCLE(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_MESSAGE(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_PARSE(component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_QUERY(component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_STORED_PROGRAM(
    component_test_event_tracking_consumer);
IMPLEMENTS_SERVICE_EVENT_TRACKING_TABLE_ACCESS(
    component_test_event_tracking_consumer);

/** This component provides following services */
BEGIN_COMPONENT_PROVIDES(component_test_event_tracking_consumer)
PROVIDES_SERVICE_EVENT_TRACKING_AUTHENTICATION(
    component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_COMMAND(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_CONNECTION(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_GENERAL(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_GLOBAL_VARIABLE(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_LIFECYCLE(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_MESSAGE(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_PARSE(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_QUERY(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_STORED_PROGRAM(
        component_test_event_tracking_consumer),
    PROVIDES_SERVICE_EVENT_TRACKING_TABLE_ACCESS(
        component_test_event_tracking_consumer),
    END_COMPONENT_PROVIDES();

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(component_test_event_tracking_consumer)
REQUIRES_SERVICE_AS(status_variable_registration, mysql_status_var_service),
    REQUIRES_SERVICE_AS(mysql_thd_store, mysql_thd_store_service),
    REQUIRES_SERVICE_AS(mysql_current_thread_reader, thread_reader),
    REQUIRES_SERVICE_AS(udf_registration, mysql_udf_registration),
    REQUIRES_SERVICE_AS(event_tracking_authentication_information,
                        mysql_authentication_information),
    REQUIRES_SERVICE_AS(event_tracking_authentication_method,
                        mysql_authentication_method),
    REQUIRES_SERVICE_AS(event_tracking_general_information,
                        mysql_general_information),
    REQUIRES_MYSQL_RWLOCK_SERVICE, REQUIRES_PSI_RWLOCK_SERVICE,
    END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(component_test_event_tracking_consumer)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("component_test_event_tracking_consumer", "1"),
    END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(component_test_event_tracking_consumer,
                  "component_test_event_tracking_consumer")
Event_tracking_consumer::init,
    Event_tracking_consumer::deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(
    component_test_event_tracking_consumer) END_DECLARE_LIBRARY_COMPONENTS
