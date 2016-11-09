/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#ifndef GCS_LOG_SYSTEM_INCLUDED
#define	GCS_LOG_SYSTEM_INCLUDED

/* purecov: begin deadcode */
#include <stddef.h>
#include <cstdarg>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <errno.h>
#include <string.h>

#include "gcs_logging.h"

#include "xplatform/my_xp_thread.h"


#define BUF_SIZE 256
#define BUF_MASK (BUF_SIZE-1)

/* Complex logger */
class Gcs_log_events_recipient_interface
{
public:
  virtual ~Gcs_log_events_recipient_interface() {}

  virtual bool process(gcs_log_level_t l, std::string m)= 0;
};

class Gcs_log_events_default_recipient
  : public Gcs_log_events_recipient_interface
{
private:
  static Gcs_log_events_recipient_interface* m_default_recipient;

public:
  Gcs_log_events_default_recipient() {}
  ~Gcs_log_events_default_recipient() {}

  static Gcs_log_events_recipient_interface* get_default_recipient();

  bool process(gcs_log_level_t level, std::string msg);
};

class Gcs_log_event
{
  private:
    gcs_log_level_t m_level;
    std::string m_msg;
    bool m_logged;
    Gcs_log_events_recipient_interface *m_recipient;
    My_xp_mutex *m_mutex;

  public:
    Gcs_log_event();
    Gcs_log_event(Gcs_log_events_recipient_interface *r);
    ~Gcs_log_event();
    Gcs_log_event(const Gcs_log_event &other);
    Gcs_log_event &operator=(const Gcs_log_event &e);
    bool get_logged();
    void set_values(gcs_log_level_t l, std::string m, bool d);
    bool process();
};


class Gcs_ext_logger_impl : public Ext_logger_interface
{
  private:
    std::vector<Gcs_log_event> m_buffer;
    int m_write_index;
    int m_max_read_index;
    int m_read_index;
    bool m_initialized;
    bool m_terminated;

    My_xp_thread *m_consumer;
    My_xp_cond *m_wait_for_events;
    My_xp_mutex *m_wait_for_events_mutex;
    My_xp_mutex *m_write_index_mutex;
    My_xp_mutex *m_max_read_index_mutex;


  public:
    Gcs_ext_logger_impl();
    Gcs_ext_logger_impl(Gcs_log_events_recipient_interface *e);
    ~Gcs_ext_logger_impl() {};
    enum_gcs_error initialize();
    enum_gcs_error finalize();

    /**
      Invoked by the publisher to push the logging message and corresponding
      level into a free buffer's event slot.

      @param level logging level of the message
      @param message rendered string of the logging message
    */

    void log_event(gcs_log_level_t level, const char *message);


    /**
      Consumer thread invokes this method to process log events until it is
      terminated.
    */

    void consume_events();


    bool is_terminated();

  /*
    Disabling copy constructor and assignment operator.
  */
  private:
    Gcs_ext_logger_impl(Gcs_ext_logger_impl &l);
    Gcs_ext_logger_impl& operator=(const Gcs_ext_logger_impl& l);

    bool my_cas(int *ptr, int old_value, int new_value);
    bool my_read_cas(int old_value, int new_value);
};


/* Simple logger */
class Gcs_simple_ext_logger_impl : public Ext_logger_interface
{
  public:
    Gcs_simple_ext_logger_impl() {}
    ~Gcs_simple_ext_logger_impl() {}


    /**
      Simple logger initialization method.

      @retval GCS_OK
    */

    enum_gcs_error initialize();


    /**
      Simple logger finalization method.

      @retval GCS_OK
    */

    enum_gcs_error finalize();


    /**
      Simple logger simply prints the received message to the adequate stream,
      according to level.

      @param level logging level of the message
      @param message rendered string of the logging message
    */

    void log_event(gcs_log_level_t level, const char *message);

  /*
    Disabling copy constructor and assignment operator.
  */
  private:
    Gcs_simple_ext_logger_impl(Gcs_simple_ext_logger_impl &l);
    Gcs_simple_ext_logger_impl& operator=(const Gcs_simple_ext_logger_impl& l);
};


/**
  Consumer thread function which invokes the consume_events method of the
  Gcs_ext_logger_impl instance conveyed as a parameter.

  @param ptr Pointer to the Gcs_ext_logger_impl instance
*/

void *consumer_function(void *ptr);
/* purecov: end */
#endif	/* GCS_LOG_SYSTEM_INCLUDED */
