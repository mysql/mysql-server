#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H
/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <m_string.h>

#include "instance_options.h"
#include "priv.h"

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class Instance_map;
class Thread_registry;


/**
  Instance_name -- the class represents instance name -- a string of length
  less than MAX_INSTANCE_NAME_SIZE.

  Generally, this is just a string with self-memory-management and should be
  eliminated in the future.
*/

class Instance_name
{
public:
  Instance_name(const LEX_STRING *name);

public:
  inline const LEX_STRING *get_str() const
  {
    return &str;
  }

  inline const char *get_c_str() const
  {
    return str.str;
  }

  inline uint get_length() const
  {
    return str.length;
  }

private:
  LEX_STRING str;
  char str_buffer[MAX_INSTANCE_NAME_SIZE];
};


class Instance
{
public:
  /* States of an instance. */
  enum enum_instance_state
  {
    STOPPED,
    NOT_STARTED,
    STARTING,
    STARTED,
    JUST_CRASHED,
    CRASHED,
    CRASHED_AND_ABANDONED,
    STOPPING
  };

public:
  /**
    The constant defines name of the default mysqld-instance ("mysqld").
  */
  static const LEX_STRING DFLT_INSTANCE_NAME;

public:
  static bool is_name_valid(const LEX_STRING *name);
  static bool is_mysqld_compatible_name(const LEX_STRING *name);

public:
  Instance();
  ~Instance();

  bool init(const LEX_STRING *name_arg);
  bool complete_initialization();

public:
  bool is_active();

  bool is_mysqld_running();

  bool start_mysqld();
  bool stop_mysqld();
  bool kill_mysqld(int signo);

  void lock();
  void unlock();

  const char *get_state_name();

  void reset_stat();

public:
  /**
    The operation is intended to check if the instance is mysqld-compatible
    or not.
  */
  inline bool is_mysqld_compatible() const;

  /**
    The operation is intended to check if the instance is configured properly
    or not. Misconfigured instances are not managed.
  */
  inline bool is_configured() const;

  /**
    The operation returns TRUE if the instance is guarded and FALSE otherwise.
  */
  inline bool is_guarded() const;

  /**
    The operation returns name of the instance.
  */
  inline const LEX_STRING *get_name() const;

  /**
    The operation returns the current state of the instance.

    NOTE: At the moment should be used only for guarded instances.
  */
  inline enum_instance_state get_state() const;

  /**
    The operation changes the state of the instance.

    NOTE: At the moment should be used only for guarded instances.
    TODO: Make private.
  */
  inline void set_state(enum_instance_state new_state);

  /**
    The operation returns crashed flag.
  */
  inline bool is_crashed();

public:
  /**
    This attributes contains instance options.

    TODO: Make private.
  */
  Instance_options options;

private:
  /**
    monitoring_thread_active is TRUE if there is a thread that monitors the
    corresponding mysqld-process.
  */
  bool monitoring_thread_active;

  /**
    crashed is TRUE when corresponding mysqld-process has been died after
    start.
  */
  bool crashed;

  /**
    configured is TRUE when the instance is configured and FALSE otherwise.
    Misconfigured instances are not managed.
  */
  bool configured;

  /*
    mysqld_compatible specifies whether the instance is mysqld-compatible
    or not. Mysqld-compatible instances can contain only mysqld-specific
    options. At the moment an instance is mysqld-compatible if its name is
    "mysqld".

    The idea is that [mysqld] section should contain only mysqld-specific
    options (no Instance Manager-specific options) to be readable by mysqld
    program.
  */
  bool mysqld_compatible;

  /*
    Mutex protecting the instance.
  */
  pthread_mutex_t LOCK_instance;

private:
  /* Guarded-instance attributes. */

  /* state of an instance (i.e. STARTED, CRASHED, etc.) */
  enum_instance_state state;

public:
  /* the amount of attemts to restart instance (cleaned up at success) */
  int restart_counter;

  /* triggered at a crash */
  time_t crash_moment;

  /* General time field. Used to provide timeouts (at shutdown and restart) */
  time_t last_checked;

private:
  static const char *get_instance_state_name(enum_instance_state state);

private:
  void remove_pid();

  bool wait_for_stop();

private:
  friend class Instance_monitor;
};


inline bool Instance::is_mysqld_compatible() const
{
  return mysqld_compatible;
}


inline bool Instance::is_configured() const
{
  return configured;
}


inline bool Instance::is_guarded() const
{
  return !options.nonguarded;
}


inline const LEX_STRING *Instance::get_name() const
{
  return &options.instance_name;
}


inline Instance::enum_instance_state Instance::get_state() const
{
  return state;
}


inline void Instance::set_state(enum_instance_state new_state)
{
  state= new_state;
}


inline bool Instance::is_crashed()
{
  return crashed;
}

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H */
