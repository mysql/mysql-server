/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
#ifndef GCS_LOG_SYSTEM_INCLUDED
#define GCS_LOG_SYSTEM_INCLUDED

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_psi.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_thread.h"

#ifndef XCOM_STANDALONE
#include "my_sys.h"
#endif /* XCOM_STANDALONE */

/**
  Maximum size of a message stored in a single entry in the circular buffer.
*/
#define GCS_MAX_LOG_BUFFER 512

/**
  Default number of circular buffer entries.
*/
#define DEFAULT_ASYNC_BUFFERS 4096

/*
  Definitions to compose a message to be written into a sink.
*/
#define GCS_PREFIX "[GCS] "
#define GCS_PREFIX_SIZE 6
#define GCS_DEBUG_PREFIX "[MYSQL_GCS_DEBUG] "
#define GCS_DEBUG_PREFIX_SIZE 18
#ifdef _WIN32
#define GCS_NEWLINE "\r\n"
#define GCS_NEWLINE_SIZE 2
#else
#define GCS_NEWLINE "\n"
#define GCS_NEWLINE_SIZE 1
#endif

/**
  Entry or element in the circular buffer maintained by the Gcs_async_buffer
  responsible for storing a message that will eventually be asynchronously
  written to a sink.
*/
class Gcs_log_event {
 public:
  explicit Gcs_log_event() = default;

  /**
    Set whether the message is ready to be consumed or not.
  */

  inline void set_event(bool ready) { m_ready_flag.store(ready); }

  /**
    Write the current message into a sink.

    @param sink Where the message should be written to.

    @retval Currently, it always false.
  */

  inline bool flush_event(Sink_interface &sink) {
    /*
      The producer is filling in the message and eventually it will be available
      so the consumer should wait for a while until the flag is true. Note that
      the number of entries in the buffer is increased before filling the
      buffer. This happens because the copy itself is done outside the critical
      section.

      After consuming the entry, the flag is set back to false again.
    */
    while (!m_ready_flag.load()) {
      My_xp_thread_util::yield();
    }
    sink.log_event(m_message_buffer, get_buffer_size());
    m_ready_flag.store(false);

    return false;
  }

  /**
      Get a reference to a buffer entry that holds a message that will be
     eventually written to a sink.
  */

  inline char *get_buffer() { return m_message_buffer; }

  /**
    Get the content size provided it was already filled in. Otherwise,
    an unknown value is returned.
  */

  inline size_t get_buffer_size() const { return m_message_size; }

  /**
    Get the maximum buffer size.
  */

  inline size_t get_max_buffer_size() const { return GCS_MAX_LOG_BUFFER - 3; }

  /**
    Set the message's size.
  */

  inline void set_buffer_size(size_t message_size) {
    m_message_size = message_size;
  }

 private:
  /**
    Buffer to hold a message that will eventually be written to a sink.
  */
  char m_message_buffer[GCS_MAX_LOG_BUFFER]{};

  /*
   Size of the message stored in the buffer entry.
  */
  size_t m_message_size{0};

  /**
   Flag used to indicate whether the message can be consumed or not.
  */
  std::atomic<bool> m_ready_flag{false};

  /*
    Disabling copy constructor and assignment operator.
  */
  Gcs_log_event(const Gcs_log_event &other);
  Gcs_log_event &operator=(const Gcs_log_event &e);
};

/**
  Circular buffer that can be used to asynchronously feed a sink. In this,
  messages are temporarily stored in-memory and asynchronously written to the
  sink. Using this in-memory intermediate buffer is possible to minimize
  performance drawbacks associated with the direct access to the sink which is
  usually the terminal, a file or a remote process.

  By default, the circular buffer has DEFAULT_ASYNC_BUFFERS entries and this
  value can be changed by providing different contructor's parameters. Note
  that, however, this is not currently exposed to the end-user. If there is no
  free slot available, the caller thread will be temporarily blocked until it
  can copy its message into a free slot. Only one thread will read the entries
  in the circular buffer and write them to a sink.

  Concurrent access to the buffer is controlled by using a mutex and atomic
  variables. If you are tempted to change this, please, measure the performance
  first before changing anything. We have done so and the bulk of the time is
  spent in formatting the messages and for that reason a simple circular buffer
  implementation is enough.

  Another alternative would be to format the message within the consumer but
  this would require to always pass information by value. In order to give
  users flexibility, we have decided not to do this. Besides, XCOM almost
  always formats its messages within the context of the caller thread. For
  those reasons, we kept the current behavior but we might revisit this in
  the future.
*/
class Gcs_async_buffer {
 private:
  /**
    Slots where messages will be copied to before a consumer thread writes
    them to a sink.
  */
  std::vector<Gcs_log_event> m_buffer;

  /**
    Number of available slots in the buffer.
  */
  int m_buffer_size;

  /**
    Next entry in the buffer where producers will write their messages to.
  */
  int64_t m_write_index;

  /**
    Next entry in the buffer that will be read by the consumer.
  */
  int64_t m_read_index;

  /**
    Number of entries written by producers and not yet consumed.
  */
  int64_t m_number_entries;

  /**
    Whether the asynchronous circular buffer has been stopped or not.
  */
  bool m_terminated;

  /**
    Whether the asynchronous circular buffer has been started or not.
  */
  bool m_initialized;

  /**
    Sink where the consumer will write messages to.
  */
  Sink_interface *m_sink;

  /**
    Consumer thread that is responsible for reading the entries in
    the circular buffer.
  */
  My_xp_thread *m_consumer;

  /**
    Conditional variable that is used by the producer to notify the consumer
    that it should wake up.
  */
  My_xp_cond *m_wait_for_events_cond;

  /**
    Conditional variable that is used by the consumer to notify the producer
    that there are free slots.
  */
  My_xp_cond *m_free_buffer_cond;

  /**
    Mutex variable that is used to synchronize access to the circular buffer
    in particular the m_number_entries, m_write_index and m_terminated shared
    variables.
  */
  My_xp_mutex *m_free_buffer_mutex;

 public:
  explicit Gcs_async_buffer(Sink_interface *sink,
                            const int buffer_size = DEFAULT_ASYNC_BUFFERS);
  ~Gcs_async_buffer();

  /**
    Asynchronous circular buffer initialization method.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  enum_gcs_error initialize();

  /**
    Asynchronous circular buffer finalization method.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  enum_gcs_error finalize();

  /**
    The purpose of this method is to return information on the associated
    sink such as its location. In this particular case, it will return the
    string "asynchronous" along with the information returned by then sink
    in use.

    Calling this method would return "asynchronous::output" if the sink
    in use was the standard output.
  */

  const std::string get_information() const;

  /**
    Consumer thread invokes this method to process log events until it is
    terminated.
  */

  void consume_events();

  /**
    Producer threads invoke this method to log events (i.e. messages).

    This method is only provided for the sake of completeness and is
    currently not used because the message would have to be copied into the
    circular buffer and usually it is necessary to compose the message first
    thus incurring an extra copy.

    Currently, a producer calls directly the get_entry() and notify_entry()
    methods directly.
  */

  void produce_events(const char *message, size_t message_size);

  /**
    Producer threads invoke this method to log events (i.e. messages).

    This method is only provided for the sake of completeness and is
    currently not used because the message would have to be copied into the
    circular buffer and usually it is necessary to compose the message first
    thus incurring an extra copy.

    Currently, a producer calls directly the get_entry() and notify_entry()
    methods directly
  */

  void produce_events(const std::string &message);

  /**
    Get a reference to an in-memory buffer where a message content will be
    written to.
  */

  Gcs_log_event &get_entry();

  /**
    Notify that an in-memory buffer was filled in and is ready to be
    consumed.
  */
  void notify_entry(Gcs_log_event &buffer_entry);

  /*
    Get a reference to the associated sink.
  */

  Sink_interface *get_sink() const;

 private:
  /**
    Get the correct index to an entry according to the buffer size.
  */

  uint64_t get_index(int64_t index) const { return index % m_buffer_size; }

  /**
     Get an index entry to an in-memory buffer where a message content will
     be written to.
  */

  int64_t get_write_index();

  /**
    Make the consumer sleep while there is no entry to be consumed.
  */

  void sleep_consumer() const {
    m_wait_for_events_cond->wait(m_free_buffer_mutex->get_native_mutex());
  }

  /**
    Wake up the consumer thread so that it can write whatever was added to
    the asynchronous buffer to a sink.
  */

  void wake_up_consumer() const { m_wait_for_events_cond->signal(); }

  /*
    Disabling copy constructor and assignment operator.
  */
  Gcs_async_buffer(Gcs_async_buffer &l);
  Gcs_async_buffer &operator=(const Gcs_async_buffer &l);
};

/* purecov: begin deadcode */
/**
  Standard output sink.
*/
class Gcs_output_sink : public Sink_interface {
 public:
  explicit Gcs_output_sink();
  ~Gcs_output_sink() override = default;

  /**
    Output sink initialization method.

    @retval GCS_OK in case everything goes well. Any other value of
              gcs_error in case of error
  */

  enum_gcs_error initialize() override;

  /**
    Output sink finalization method.

    @retval GCS_OK in case everything goes well. Any other value of
              gcs_error in case of error.
  */

  enum_gcs_error finalize() override;

  /**
    Print the received message to the standard output stream.

    @param message rendered stream of the logging message
  */

  void log_event(const std::string &message) override;

  /**
    Print the received message to the standard output stream.

    @param message rendered stream of the logging message
    @param message_size logging message size
  */

  void log_event(const char *message, size_t message_size) override;

  /**
     Return information on the sink such as its location.
  */

  const std::string get_information() const override;

 private:
  /*
    Whether the sink was initialized or not.
  */
  bool m_initialized;

  /*
    Disabling copy constructor and assignment operator.
  */
  Gcs_output_sink(Gcs_output_sink &s);
  Gcs_output_sink &operator=(const Gcs_output_sink &s);
};

/**
 Default logger which is internally used by GCS and XCOM if nothing else
 is injected by Group Replication.
*/
class Gcs_default_logger : public Logger_interface {
 public:
  explicit Gcs_default_logger(Gcs_async_buffer *sink);
  ~Gcs_default_logger() override = default;

  /**
    Default logger initialization method.

    @retval GCS_OK
  */

  enum_gcs_error initialize() override;

  /**
    Default logger finalization method.

    @retval GCS_OK
  */

  enum_gcs_error finalize() override;

  /**
    Asynchronously forwards the received message to a sink.

    This method prepares the message and writes it to an in-memory buffer.
    If there is no free entry in the in-memory buffer, the call blocks until
    an entry becomes available.

    Note that the write to the sink is done asynchronously.

    This method shouldn't be invoked directly in the code, as it is wrapped
    by the MYSQL_GCS_LOG_[LEVEL] macros which deal with the rendering of the
    logging message into a final string that is then handed alongside with
    the level to this method.

    @param level logging level of the message
    @param message rendered string of the logging message
  */

  void log_event(const gcs_log_level_t level,
                 const std::string &message) override;

 private:
  /**
    Reference to an asynchronous buffer that encapsulates a sink.
  */
  Gcs_async_buffer *m_sink;

  /*
    Disabling copy constructor and assignment operator.
  */
  Gcs_default_logger(Gcs_default_logger &l);
  Gcs_default_logger &operator=(const Gcs_default_logger &l);
};
/* purecov: end */

/**
  Default debugger which is used only by GCS and XCOM.
*/
class Gcs_default_debugger {
 public:
  explicit Gcs_default_debugger(Gcs_async_buffer *sink);
  virtual ~Gcs_default_debugger() = default;

  /**
    Default debugger initialization method.

    @retval GCS_OK
  */

  enum_gcs_error initialize();

  /**
    Default debugger finalization method.

    @retval GCS_OK
  */

  enum_gcs_error finalize();

  /**
     Asynchronously forwards the received message to a sink.

     This method prepares the message and writes it to an in-memory buffer.
     If there is no free entry in the in-memory buffer, the call blocks until
     an entry becomes available.

     Note that the write to the sink is done asynchronously.

     This method shouldn't be invoked directly in the code, as it is wrapped
     by the MYSQL_GCS_LOG_[LEVEL] macros which deal with the rendering of the
     logging message into a final string that is then handed alongside with
     the level to this method.

     @param [in] format Message format using a c-style string
     @param [in] args Arguments to fill in the format string
   */

  inline void log_event(const char *format, va_list args)
      MY_ATTRIBUTE((format(printf, 2, 0))) {
    Gcs_log_event &event = m_sink->get_entry();
    char *buffer = event.get_buffer();
    size_t size = append_prefix(buffer);
    size += vsnprintf(buffer + size, event.get_max_buffer_size() - size, format,
                      args);
    if (unlikely(size > event.get_max_buffer_size())) {
      fprintf(stderr, "The following message was truncated: %s\n", buffer);
      size = event.get_max_buffer_size();
    }
    size += append_sufix(buffer, size);
    event.set_buffer_size(size);
    m_sink->notify_entry(event);
  }

  /**
    Default debugger simply forwards the received message to a sink.

    @param message rendered string of the logging message
  */

  void log_event(const std::string &message);

  /**
    Get a reference to the in-memory buffer where the message content will
    be copied to if there is any.
  */

  inline Gcs_log_event &get_entry() { return m_sink->get_entry(); }

  /**
    Notify that the in-memory buffer were filled in and is ready to be
    consumed.
  */

  inline void notify_entry(Gcs_log_event &entry) {
    m_sink->notify_entry(entry);
  }

  /**
    Asynchronously forwards the received message to a sink.

    This method prepares the message and writes it to an in-memory buffer.
    If there is no free entry in the in-memory buffer, the call blocks until
    an entry becomes available.

    Note that the write to the sink is done asynchronously.

    This method shouldn't be invoked directly in the code, as it is wrapped
    by the MYSQL_GCS_LOG_[LEVEL] macros which deal with the rendering of the
    logging message into a final string that is then handed alongside with
    the level to this method.

    @param [in] options Debug options that are associated with the message
    @param [in] message Message to be written to the in-memory buffer
  */

  inline void log_event(int64_t options, const char *message) {
    log_event(options, "%s", message);
  }

  /**
     Asynchronously forwards the received message to a sink.

     This method prepares the message and writes it to an in-memory buffer.
     If there is no free entry in the in-memory buffer, the call blocks until
     an entry becomes available.

     Note that the write to the sink is done asynchronously.

     This method shouldn't be invoked directly in the code, as it is wrapped
     by the MYSQL_GCS_LOG_[LEVEL] macros which deal with the rendering of the
     logging message into a final string that is then handed alongside with
     the level to this method.

     @param [in] options Debug options that are associated with the message
     @param [in] args Arguments This includes the c-style string and arguments
     to fill it in
    */
  template <typename... Args>
  inline void log_event(const int64_t options, Args... args) {
    if (Gcs_debug_options::test_debug_options(options)) {
      Gcs_log_event &event = get_entry();
      char *buffer = event.get_buffer();
      size_t size = append_prefix(buffer);
      size +=
          snprintf(buffer + size, event.get_max_buffer_size() - size, args...);
      if (unlikely(size > event.get_max_buffer_size())) {
        fprintf(stderr, "The following message was truncated: %s\n", buffer);
        size = event.get_max_buffer_size();
      }
      size += append_sufix(buffer, size);
      event.set_buffer_size(size);
      notify_entry(event);
    }
  }

 private:
  /**
    Reference to an asynchronous buffer that encapsulates a sink.
  */
  Gcs_async_buffer *m_sink;

  /**
    Add extra information as a message prefix.

    We assume that there is room to accommodate it. Before changing this
    method, make sure the maximum buffer size will always have room to
    accommodate any new information.

    @return Return the size of appended information
  */
  inline size_t append_prefix(char *buffer) {
    strcpy(buffer, GCS_DEBUG_PREFIX);
    strcpy(buffer + GCS_DEBUG_PREFIX_SIZE, GCS_PREFIX);

    return GCS_DEBUG_PREFIX_SIZE + GCS_PREFIX_SIZE;
  }

  /**
    Append information into a message such as end of line.

    We assume that there is room to accommodate it. Before changing this
    method, make sure the maximum buffer size will always have room to
    accommodate any new information.

    @return Return the size of appended information
  */
  inline size_t append_sufix(char *buffer, size_t size) {
    strcpy(buffer + size, GCS_NEWLINE);
    buffer[size + GCS_NEWLINE_SIZE] = '\0';
    return GCS_NEWLINE_SIZE;
  }

  /*
    Disabling copy constructor and assignment operator.
  */
  Gcs_default_debugger(Gcs_default_debugger &d);
  Gcs_default_debugger &operator=(const Gcs_default_debugger &d);
};

class Gcs_debug_manager : public Gcs_debug_options {
 private:
  /*
    It stores a reference to a debugger interface object.
  */

  static Gcs_default_debugger *m_debugger;

 public:
  /**
    Set the debugger object and initialize it by invoking its initialization
    method.

    This allows any resources needed by the debugging system to be initialized,
    and ensures its usage throughout the lifecycle of the current application.

    @param[in] debugger debugging system
    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  static enum_gcs_error initialize(Gcs_default_debugger *debugger) {
    m_debugger = debugger;
    return m_debugger->initialize();
  }

  /**
    Get a reference to the debugger object if there is any.

    @return The current debugging system.
  */

  static Gcs_default_debugger *get_debugger() { return m_debugger; }

  /**
    Free any resource used in the debugging system.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  static enum_gcs_error finalize() {
    enum_gcs_error ret = GCS_NOK;

    if (m_debugger != nullptr) {
      ret = m_debugger->finalize();
      m_debugger = nullptr;
    }

    return ret;
  }
};

/* purecov: end */

#ifndef XCOM_STANDALONE
/* File debugger */
class Gcs_file_sink : public Sink_interface {
 public:
  Gcs_file_sink(const std::string &file_name, const std::string &dir_name);
  ~Gcs_file_sink() override = default;

  /**
    File sink initialization method.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  enum_gcs_error initialize() override;

  /**
    File sink finalization method.

    @retval GCS_OK in case everything goes well. Any other value of
            gcs_error in case of error.
  */

  enum_gcs_error finalize() override;

  /**
    Print the received message to a log file.
    @param message rendered stream of the logging message
  */

  void log_event(const std::string &message) override;

  /**
    Print the received message to a log file.

    @param message rendered stream of the logging message
    @param message_size logging message size
  */

  void log_event(const char *message, size_t message_size) override;

  /**
    The purpose of this method is to return information on the sink such
    as its location.
  */

  const std::string get_information() const override;

  /**
    Return the full path of the file that shall be created. If the path is
    too long GCS_NOK is returned.

    @param[out] file_name_buffer Buffer that will contain the resulting path
    @retval GCS_OK in case everything goes well. Any other value of
              gcs_error in case of error
  */

  enum_gcs_error get_file_name(char *file_name_buffer) const;

 private:
  /*
    File descriptor.
  */
  File m_fd;

  /*
    Reference to a file name.
  */
  std::string m_file_name;

  /*
    Reference to a directory that will be used as relative path.
  */
  std::string m_dir_name;

  /*
    Whether the sink was initialized or not.
  */
  bool m_initialized;

  /*
    Disabling copy constructor and assignment operator.
  */
  Gcs_file_sink(Gcs_file_sink &d);
  Gcs_file_sink &operator=(const Gcs_file_sink &d);
};
#endif /* XCOM_STANDALONE */

#define MYSQL_GCS_LOG(l, x)                                   \
  do {                                                        \
    if (Gcs_log_manager::get_logger() != NULL) {              \
      std::stringstream log;                                  \
      log << GCS_PREFIX << x;                                 \
      Gcs_log_manager::get_logger()->log_event(l, log.str()); \
    }                                                         \
  } while (0);

#define MYSQL_GCS_LOG_INFO(x) MYSQL_GCS_LOG(GCS_INFO, x)
#define MYSQL_GCS_LOG_WARN(x) MYSQL_GCS_LOG(GCS_WARN, x)
#define MYSQL_GCS_LOG_ERROR(x) MYSQL_GCS_LOG(GCS_ERROR, x)
#define MYSQL_GCS_LOG_FATAL(x) MYSQL_GCS_LOG(GCS_FATAL, x)

#define MYSQL_GCS_DEBUG_EXECUTE(x) \
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(GCS_DEBUG_BASIC | GCS_DEBUG_TRACE, x)

#define MYSQL_GCS_TRACE_EXECUTE(x) \
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(GCS_DEBUG_TRACE, x)

#define MYSQL_GCS_LOG_DEBUG(...)                                     \
  MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_BASIC | GCS_DEBUG_TRACE, \
                                  __VA_ARGS__)

#define MYSQL_GCS_LOG_TRACE(...) \
  MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_TRACE, __VA_ARGS__)

#define MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(option, x)   \
  do {                                                   \
    if (Gcs_debug_manager::test_debug_options(option)) { \
      x;                                                 \
    }                                                    \
  } while (0);

#define MYSQL_GCS_LOG_DEBUG_WITH_OPTION(options, ...)                   \
  do {                                                                  \
    Gcs_default_debugger *debugger = Gcs_debug_manager::get_debugger(); \
    debugger->log_event(options, __VA_ARGS__);                          \
  } while (0);
#endif /* GCS_LOG_SYSTEM_INCLUDED */
