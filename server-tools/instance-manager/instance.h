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
#include <my_sys.h>
#include <mysql.h>
#include "instance_options.h"

#ifdef __GNUC__
#pragma interface
#endif

class Instance_map;

class Instance
{
public:
  Instance();

  ~Instance();
  int init(const char *name);
  int complete_initialization(Instance_map *instance_map_arg,
                              const char *mysqld_path);

  bool is_running();
  int start();
  int stop();
  /* send a signal to the instance */
  void kill_instance(int signo);
  int is_crashed();
  void fork_and_monitor();

public:
  enum { DEFAULT_SHUTDOWN_DELAY= 35 };
  Instance_options options;

private:
  /*
    Mutex protecting the instance. Currently we use it to avoid the
    double start of the instance. This happens when the instance is starting
    and we issue the start command once more.
  */
  int crashed;
  pthread_mutex_t LOCK_instance;
  pthread_cond_t COND_instance_restarted;
  Instance_map *instance_map;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H */
