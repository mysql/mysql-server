#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H
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
#include <my_sys.h>
#include <m_string.h>
#include <hash.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class Guardian;
class Instance;
class Named_value_arr;
class Thread_registry;

extern int load_all_groups(char ***groups, const char *filename);
extern void free_groups(char **groups);

extern int create_instance_in_file(const LEX_STRING *instance_name,
                                   const Named_value_arr *options);


/**
  Instance_map - stores all existing instances
*/

class Instance_map
{
public:
  /**
    Instance_map iterator
  */

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

public:
  Instance *find(const LEX_STRING *name);

  bool is_there_active_instance();

  void lock();
  void unlock();

  bool init();
  bool reset();

  int load();

  int process_one_option(const LEX_STRING *group, const char *option);

  int add_instance(Instance *instance);

  int remove_instance(Instance *instance);

  int create_instance(const LEX_STRING *instance_name,
                      const Named_value_arr *options);

public:
  Instance_map();
  ~Instance_map();

private:
  bool complete_initialization();

private:
  enum { START_HASH_SIZE = 16 };
  pthread_mutex_t LOCK_instance_map;
  HASH hash;

private:
  friend class Iterator;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H */
