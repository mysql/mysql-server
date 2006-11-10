#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H
/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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


/*
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
  /*
    The following two constants defines name of the default mysqld-instance
    ("mysqld").
  */
  static const LEX_STRING DFLT_INSTANCE_NAME;

public:
  /*
    The operation is intended to check whether string is a well-formed
    instance name or not.
  */
  static bool is_name_valid(const LEX_STRING *name);

  /*
    The operation is intended to check if the given instance name is
    mysqld-compatible or not.
  */
  static bool is_mysqld_compatible_name(const LEX_STRING *name);

public:
  Instance(Thread_registry &thread_registry_arg);

  ~Instance();
  int init(const LEX_STRING *name_arg);
  int complete_initialization(Instance_map *instance_map_arg,
                              const char *mysqld_path);

  bool is_running();
  int start();
  int stop();
  /* send a signal to the instance */
  void kill_instance(int signo);
  bool is_crashed();
  void set_crash_flag_n_wake_all();
  Instance_map *get_map();

  /*
    The operation is intended to check if the instance is mysqld-compatible
    or not.
  */
  inline bool is_mysqld_compatible() const;

  /*
    The operation is intended to check if the instance is configured properly
    or not. Misconfigured instances are not managed.
  */
  inline bool is_configured() const;

  inline const LEX_STRING *get_name() const;

public:
  enum { DEFAULT_SHUTDOWN_DELAY= 35 };
  Instance_options options;
  Thread_registry &thread_registry;

private:
  /* This attributes is a flag, specifies if the instance has been crashed. */
  bool crashed;

  /*
    This attribute specifies if the instance is configured properly or not.
    Misconfigured instances are not managed.
  */
  bool configured;

  /*
    This attribute specifies whether the instance is mysqld-compatible or not.
    Mysqld-compatible instances can contain only mysqld-specific options.
    At the moment an instance is mysqld-compatible if its name is "mysqld".

    The idea is that [mysqld] section should contain only mysqld-specific
    options (no Instance Manager-specific options) to be readable by mysqld
    program.
  */
  bool mysqld_compatible;

  /*
    Mutex protecting the instance. Currently we use it to avoid the
    double start of the instance. This happens when the instance is starting
    and we issue the start command once more.
  */
  pthread_mutex_t LOCK_instance;
  /*
    This condition variable is used to wake threads waiting for instance to
    stop in Instance::stop()
  */
  pthread_cond_t COND_instance_stopped;
  Instance_map *instance_map;

  void  remove_pid();
};


inline bool Instance::is_mysqld_compatible() const
{
  return mysqld_compatible;
}


inline bool Instance::is_configured() const
{
  return configured;
}


inline const LEX_STRING *Instance::get_name() const
{
  return &options.instance_name;
}

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H */
