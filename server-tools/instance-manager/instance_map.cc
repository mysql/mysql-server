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

#ifdef __GNUC__
#pragma implementation
#endif

#include "instance_map.h"
#include "buffer.h"
#include "instance.h"
#include <m_ctype.h>
#include <my_sys.h>
#include <mysql_com.h>
#include <m_string.h>

/*
  TODO: Currently there are some mysql-connection specific functions.
  As we are going to suppost different types of connections, we shouldn't
  have them here in future. To avoid it we could put such
  connection-specific functions to the Command-derived class instead.
  The command could be easily constructed for a specific connection if
  we would provide a special factory for each connection.
*/

C_MODE_START

/* Procedure needed for HASH initialization */

static byte* get_instance_key(const byte* u, uint* len,
                          my_bool __attribute__((unused)) t)
{
  const Instance *instance= (const Instance *) u;
  *len= instance->options.instance_name_len;
  return (byte *) instance->options.instance_name;
}

static void delete_instance(void *u)
{
  Instance *instance= (Instance *) u;
  delete instance;
}

/*
  The option handler to pass to the process_default_option_files finction.

  SYNOPSYS
    process_option()
    ctx               Handler context. Here it is an instance_map structure.
    group_name        The name of the group the option belongs to.
    option            The very option to be processed. It is already
                      prepared to be used in argv (has -- prefix)

  DESCRIPTION

    This handler checks whether a group is an instance group and adds
    an option to the appropriate instance class. If this is the first
    occurence of an instance name, we'll also create the instance
    with such name and add it to the instance map.

  RETURN
    0 - ok
    1 - error occured
*/

static int process_option(void * ctx, const char *group, const char *option)
{
  Instance_map *map= NULL;
  Instance *instance= NULL;
  static const char prefix[]= { 'm', 'y', 's', 'q', 'l', 'd' };

  map = (Instance_map*) ctx;
  if (strncmp(group, prefix, sizeof prefix) == 0 &&
      (my_isdigit(default_charset_info, group[sizeof prefix])))
    {
      if ((instance= map->find(group, strlen(group))) == NULL)
      {
        if ((instance= new Instance) == 0)
          goto err_new_instance;
        if (instance->init(group))
          goto err;
        if (map->add_instance(instance))
          goto err;
      }

      if (instance->options.add_option(option))
        goto err;
    }

  return 0;

err:
  delete instance;
err_new_instance:
  return 1;
}

C_MODE_END


Instance_map::Instance_map(const char *default_mysqld_path_arg,
                           const char *default_admin_user_arg,
                           const char *default_admin_password_arg)
{
  mysqld_path= default_mysqld_path_arg;
  user= default_admin_user_arg;
  password= default_admin_password_arg;

  pthread_mutex_init(&LOCK_instance_map, 0);
}


int Instance_map::init()
{
  if (hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
                get_instance_key, delete_instance, 0))
    return 1;
  return 0;
}

Instance_map::~Instance_map()
{
  pthread_mutex_lock(&LOCK_instance_map);
  hash_free(&hash);
  pthread_mutex_unlock(&LOCK_instance_map);
  pthread_mutex_destroy(&LOCK_instance_map);
}


int Instance_map::lock()
{
  return pthread_mutex_lock(&LOCK_instance_map);
}


int Instance_map::unlock()
{
  return pthread_mutex_unlock(&LOCK_instance_map);
}


int Instance_map::flush_instances()
{
  int rc;

  pthread_mutex_lock(&LOCK_instance_map);
  hash_free(&hash);
  hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
            get_instance_key, delete_instance, 0);
  rc= load();
  pthread_mutex_unlock(&LOCK_instance_map);
  return rc;
}


int Instance_map::add_instance(Instance *instance)
{
  return my_hash_insert(&hash, (byte *) instance);
}


Instance *
Instance_map::find(const char *name, uint name_len)
{
  Instance *instance;
  pthread_mutex_lock(&LOCK_instance_map);
  instance= (Instance *) hash_search(&hash, (byte *) name, name_len);
  pthread_mutex_unlock(&LOCK_instance_map);
  return instance;
}


void Instance_map::complete_initialization()
{
  Instance *instance;
  uint i= 0;

  while (i < hash.records)
  {
    instance= (Instance *) hash_element(&hash, i);
    instance->options.complete_initialization(mysqld_path, user, password);
    i++;
  }
}


int Instance_map::cleanup()
{
  Instance *instance;
  uint i= 0;

  while (i < hash.records)
  {
    instance= (Instance *) hash_element(&hash, i);
    instance->cleanup();
    i++;
  }
}


Instance *
Instance_map::find(uint instance_number)
{
  Instance *instance;
  char name[80];

  sprintf(name, "mysqld%i", instance_number);
  pthread_mutex_lock(&LOCK_instance_map);
  instance= (Instance *) hash_search(&hash, (byte *) name, strlen(name));
  pthread_mutex_unlock(&LOCK_instance_map);
  return instance;
}


/* load options from config files and create appropriate instance structures */

int Instance_map::load()
{
  int error;

  error= process_default_option_files("my", process_option, (void *) this);

  complete_initialization();

  return error;
}


/*--- Implementaton of the Instance map iterator class  ---*/


void Instance_map::Iterator::go_to_first()
{
  current_instance=0;
}


Instance *Instance_map::Iterator::next()
{
  if (current_instance < instance_map->hash.records)
    return (Instance *) hash_element(&instance_map->hash, current_instance++);
  else
    return NULL;
}

