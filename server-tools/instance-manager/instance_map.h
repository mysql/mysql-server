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
#include <my_sys.h>
#include <hash.h>

#ifdef __GNUC__
#pragma interface
#endif

#include "protocol.h"
#include "guardian.h"

class Instance;
extern int load_all_groups(char ***groups, const char *filename);
extern void free_groups(char **groups);


/*
  Instance_map - stores all existing instances
*/

class Instance_map
{
public:
  /* returns a pointer to the instance or NULL, if there is no such instance */
  Instance *find(const char *name, uint name_len);
  Instance *find(uint instance_number);

  int flush_instances();
  int cleanup();
  int lock();
  int unlock();
  Instance *get_instance(uint instance_number);

  Instance_map();
  ~Instance_map();

  /* loads options from config files */
  int load();
  /* adds instance to internal hash */
  int add_instance(Instance *instance);
  /* inits instances argv's after all options have been loaded */
  void complete_initialization();

public:
  const char *mysqld_path;
  /* user an password to shutdown MySQL */
  const char *user;
  const char *password;
  Guardian_thread *guardian;

private:
  enum { START_HASH_SIZE = 16 };
  pthread_mutex_t LOCK_instance_map;
  HASH hash;
};


/* Instance_map iterator */

class Imap_iterator
{
private:
  uint current_instance;
  Instance_map *instance_map;
public:
  Imap_iterator(Instance_map *instance_map_arg) :
    instance_map(instance_map_arg), current_instance(0)
  {}

  void go_to_first();
  Instance *next();
};


#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_MAP_H */
