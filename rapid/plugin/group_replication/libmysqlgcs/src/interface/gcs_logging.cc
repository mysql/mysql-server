/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <vector>

#include "gcs_log_system.h"

Ext_logger_interface *Gcs_logger::log= NULL;


// Logging infrastructure interface
Ext_logger_interface *Gcs_logger::get_logger()
{
  return log;
}

enum_gcs_error Gcs_logger::initialize(Ext_logger_interface* logger)
{
  enum_gcs_error ret= GCS_NOK;

  log= logger;
  ret= log->initialize();

  return ret;
}


enum_gcs_error Gcs_logger::finalize()
{
  enum_gcs_error ret= GCS_NOK;
  if(log != NULL)
  {
    ret= log->finalize();
    log= NULL;
  }

  return ret;
}

/* purecov: begin deadcode */
// GCS Logging systems implementation

Gcs_log_events_recipient_interface *Gcs_log_events_default_recipient::m_default_recipient= NULL;

Gcs_log_events_recipient_interface* Gcs_log_events_default_recipient::get_default_recipient()
{
  if(m_default_recipient == NULL)
    m_default_recipient= new Gcs_log_events_default_recipient();

  return m_default_recipient;
}


bool Gcs_log_events_default_recipient::process(gcs_log_level_t level, std::string msg)
{
  // Print message adding timestamp and level before message
  if(level < GCS_INFO)
    std::cerr << My_xp_util::getsystime() << " "
      << gcs_log_levels[level] << msg << std::endl;
  else
    std::cout << My_xp_util::getsystime() << " "
      << gcs_log_levels[level] << msg << std::endl;

  return true;
}


// Gcs Logging Event
Gcs_log_event::Gcs_log_event()
: m_level(GCS_TRACE), m_msg(""), m_logged(true),
  m_recipient(Gcs_log_events_default_recipient::get_default_recipient()),
  m_mutex(new My_xp_mutex_impl())
{
  m_mutex->init(NULL);
}

Gcs_log_event::Gcs_log_event(Gcs_log_events_recipient_interface *r)
: m_level(GCS_TRACE), m_msg(""), m_logged(true), m_recipient(r),
  m_mutex(new My_xp_mutex_impl())
{
  m_mutex->init(NULL);
}


Gcs_log_event::~Gcs_log_event()
{
  m_mutex->destroy();
  delete m_mutex;
}


Gcs_log_event::Gcs_log_event(const Gcs_log_event &other)
  : m_level(other.m_level), m_msg(other.m_msg.c_str()),
  m_logged(other.m_logged), m_recipient(other.m_recipient),
  m_mutex(new My_xp_mutex_impl())
{
  m_mutex->init(NULL);
}


bool Gcs_log_event::get_logged()
{
  bool current;
  m_mutex->lock();
  current= m_logged;
  m_mutex->unlock();

  return current;
}

void Gcs_log_event::set_values(gcs_log_level_t l, std::string m, bool d)
{
  m_mutex->lock();
  m_level= l;
  m_msg= m;
  m_logged= d;
  m_mutex->unlock();
}

bool Gcs_log_event::process()
{
  m_mutex->lock();
  if(!m_logged)
  {
    // Mark event as logged if everything went fine
    m_logged= m_recipient->process(m_level, m_msg);
  }
  m_mutex->unlock();

  return m_logged;
}


// Gcs Logging system implementation
Gcs_ext_logger_impl::Gcs_ext_logger_impl()
  :m_buffer(std::vector<Gcs_log_event>(BUF_SIZE, Gcs_log_event())),
  m_write_index(0), m_max_read_index(0), m_read_index(0), m_initialized(false),
  m_terminated(false), m_consumer(new My_xp_thread_impl()),
  m_wait_for_events(new My_xp_cond_impl()),
  m_wait_for_events_mutex(new My_xp_mutex_impl()),
  m_write_index_mutex(new My_xp_mutex_impl()),
  m_max_read_index_mutex(new My_xp_mutex_impl())
{}

Gcs_ext_logger_impl::Gcs_ext_logger_impl(Gcs_log_events_recipient_interface *e)
  :m_buffer(std::vector<Gcs_log_event>(BUF_SIZE, Gcs_log_event(e))),
  m_write_index(0), m_max_read_index(0), m_read_index(0),
  m_initialized(false), m_terminated(false),
  m_consumer(new My_xp_thread_impl()),
  m_wait_for_events(new My_xp_cond_impl()),
  m_wait_for_events_mutex(new My_xp_mutex_impl()),
  m_write_index_mutex(new My_xp_mutex_impl()),
  m_max_read_index_mutex(new My_xp_mutex_impl())
{}


bool Gcs_ext_logger_impl::is_terminated()
{
  return m_terminated;
}

/**
  Logger initialization method.
*/

enum_gcs_error Gcs_ext_logger_impl::initialize()
{
  m_wait_for_events->init();
  m_wait_for_events_mutex->init(NULL);
  m_write_index_mutex->init(NULL);
  m_max_read_index_mutex->init(NULL);

  int res= m_consumer->create(NULL, consumer_function, (void *) this);
  if(res)
  {
    std::cerr << "Unable to create Gcs_ext_logger_impl consumer thread, " << res << std::endl;
    return GCS_NOK;
  }

  m_initialized= true;

  return GCS_OK;
}


/**
  Logger finalization method.
*/

enum_gcs_error Gcs_ext_logger_impl::finalize()
{
  if(!m_initialized)
    return GCS_NOK;

  if(m_terminated)
    return GCS_NOK;

  // Stop logging task and wake it up
  m_terminated= true;

  // Wake up consumer before leaving
  m_wait_for_events_mutex->lock();
  m_wait_for_events->signal();
  m_wait_for_events_mutex->unlock();

  // Wait for consumer to finish processing events
  m_consumer->join(NULL);

  m_wait_for_events->destroy();
  m_wait_for_events_mutex->destroy();
  m_write_index_mutex->destroy();
  m_max_read_index_mutex->destroy();

  delete Gcs_log_events_default_recipient::get_default_recipient();
  delete m_consumer;
  delete m_wait_for_events;
  delete m_wait_for_events_mutex;
  delete m_write_index_mutex;
  delete m_max_read_index_mutex;

  return GCS_OK;
}


void Gcs_ext_logger_impl::log_event(gcs_log_level_t level, const char *message)
{
  // Get and increment write index
  m_write_index_mutex->lock();
  int current_write_index= m_write_index++;
  int index= current_write_index & BUF_MASK;
  m_write_index_mutex->unlock();

  while(!m_buffer[index].get_logged())
  {
    m_wait_for_events_mutex->lock();
    m_wait_for_events->signal();
    m_wait_for_events_mutex->unlock();
  }

  m_buffer[index].set_values(level, message, false);

  // Increment max_read_index
  while(!my_read_cas(current_write_index, (current_write_index+1)))
    ;

  // Wakeup consumer
  m_wait_for_events_mutex->lock();
  m_wait_for_events->signal();
  m_wait_for_events_mutex->unlock();
}


void Gcs_ext_logger_impl::consume_events()
{
  int cycles= 0;
  int index= 0;
  int current_max_read_index;
  struct timespec ts;
  long wait_ms= 500;

  m_max_read_index_mutex->lock();
  current_max_read_index= m_max_read_index;
  m_max_read_index_mutex->unlock();
  do
  {
    // Wait for new events to be pushed
    if(current_max_read_index == m_read_index)
    {
      m_wait_for_events_mutex->lock();
      My_xp_util::set_timespec_nsec(&ts, wait_ms*1000000);
      m_wait_for_events->timed_wait(m_wait_for_events_mutex->get_native_mutex(), &ts);
      m_wait_for_events_mutex->unlock();
    }
    else
    {
      while(m_read_index < current_max_read_index)
      {
        index= m_read_index & BUF_MASK;
        if(m_buffer[index].process())
        {
          m_read_index++;
        }
      }
    }

    cycles++;
    m_max_read_index_mutex->lock();
    current_max_read_index= m_max_read_index;
    m_max_read_index_mutex->unlock();
  }
  while(!is_terminated() || current_max_read_index > m_read_index);
}


bool Gcs_ext_logger_impl::my_cas(int *ptr, int expected_value, int new_value)
{
    if(*ptr != expected_value)
    {
        return false;
    }
    else
    {
        *ptr= new_value;
        return true;
    }
}


bool Gcs_ext_logger_impl::my_read_cas(int old_value, int new_value)
{
  bool ret= false;
  m_max_read_index_mutex->lock();
  ret= my_cas(&m_max_read_index, old_value, new_value);
  m_max_read_index_mutex->unlock();

  return ret;
}


void *consumer_function(void *ptr)
{
  Gcs_ext_logger_impl *l= (Gcs_ext_logger_impl *)ptr;
  l->consume_events();

  return NULL;
}


// GCS Simple Logger
enum_gcs_error Gcs_simple_ext_logger_impl::initialize()
{
  int ret_out= setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
  int ret_err= setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

  if((ret_out == 0) && (ret_err == 0))
    return GCS_OK;
  else
  {
#if defined(WIN_32) || defined(WIN_64)
    int errno= WSAGetLastError();
#endif
    std::cerr << "Unable to invoke setvbuf correctly! " << strerror(errno)
      << std::endl;

    return GCS_NOK;
  }
}


enum_gcs_error Gcs_simple_ext_logger_impl::finalize()
{
  return GCS_OK;
}


void Gcs_simple_ext_logger_impl::log_event(gcs_log_level_t level, const char *msg)
{
  // Print message adding timestamp and level before message
  if(level < GCS_INFO)
    std::cerr << My_xp_util::getsystime() << " "
      << gcs_log_levels[level] << msg << std::endl;
  else
    std::cout << My_xp_util::getsystime() << " "
      << gcs_log_levels[level] << msg << std::endl;
}
/* purecov: end */