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

class Instance
{
public:
  Instance(): is_connected(FALSE)
  {}
  ~Instance();

  int init(const char *name);

  /* check if the instance is running and set up mysql connection if yes */
  bool is_running();
  int start();
  int stop();
  int cleanup();

public:
  Instance_options options;

  /* connection to the instance */
  MYSQL mysql;

private:
  /*
    Mutex protecting the instance. Currently we use it to avoid the
    double start of the instance. This happens when the instance is starting
    and we issue the start command once more.
  */
  pthread_mutex_t LOCK_instance;
  /* Here we store the state of the following connection */
  bool is_connected;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_H */
