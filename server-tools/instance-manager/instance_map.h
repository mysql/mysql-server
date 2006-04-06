#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H
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

#include "protocol.h"
#include "guardian.h"

#include <my_sys.h>
#include <hash.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class Instance;
extern int load_all_groups(char ***groups, const char *filename);
extern void free_groups(char **groups);


/*
  Instance_map - stores all existing instances
*/

class Instance_map
{
public:
  /* Instance_map iterator */
  class Iterator
  {
  private:
    uint current_instance;
    Instance_map *instance_map;
  public:
    Iterator(Instance_map *instance_map_arg) :
     current_instance(0), instance_map(instance_map_arg)
    {}

    void go_to_first();
    Instance *next();
  };
  friend class Iterator;
public:
  /* returns a pointer to the instance or NULL, if there is no such instance */
  Instance *find(const char *name, uint name_len);

  int flush_instances();
  void lock();
  void unlock();
  int init();
  /*
    Process a given option and assign it to appropricate instance. This is
    required for the option handler, passed to my_search_option_files().
  */
  int process_one_option(const char *group, const char *option);

  Instance_map(const char *default_mysqld_path_arg);
  ~Instance_map();

public:
  const char *mysqld_path;
  Guardian_thread *guardian;

private:
  /* loads options from config files */
  int load();
  /* inits instances argv's after all options have been loaded */
  int complete_initialization();
private:
  enum { START_HASH_SIZE = 16 };
  pthread_mutex_t LOCK_instance_map;
  HASH hash;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H */
