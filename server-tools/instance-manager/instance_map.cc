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
#include "log.h"
#include "options.h"

#include <m_ctype.h>
#include <mysql_com.h>
#include <m_string.h>

/*
  Note:  As we are going to suppost different types of connections,
  we shouldn't have connection-specific functions. To avoid it we could
  put such functions to the Command-derived class instead.
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

static int process_option(void *ctx, const char *group, const char *option)
{
  Instance_map *map= NULL;
  Instance *instance= NULL;
  static const char prefix[]= { 'm', 'y', 's', 'q', 'l', 'd' };

  map = (Instance_map*) ctx;
  if (strncmp(group, prefix, sizeof prefix) == 0 &&
      ((my_isdigit(default_charset_info, group[sizeof prefix]))
       || group[sizeof(prefix)] == '\0'))
    {
      if ((instance= map->find(group, strlen(group))) == NULL)
      {
        if ((instance= new Instance) == 0)
          goto err;
        if (instance->init(group) || map->add_instance(instance))
          goto err_instance;
      }

      if (instance->options.add_option(option))
        goto err;   /* the instance'll be deleted when we destroy the map */
    }

  return 0;

err_instance:
  delete instance;
err:
  return 1;
}

C_MODE_END


Instance_map::Instance_map(const char *default_mysqld_path_arg):
mysqld_path(default_mysqld_path_arg)
{
  pthread_mutex_init(&LOCK_instance_map, 0);
}


int Instance_map::init()
{
  return hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
                   get_instance_key, delete_instance, 0);
}

Instance_map::~Instance_map()
{
  pthread_mutex_lock(&LOCK_instance_map);
  hash_free(&hash);
  pthread_mutex_unlock(&LOCK_instance_map);
  pthread_mutex_destroy(&LOCK_instance_map);
}


void Instance_map::lock()
{
  pthread_mutex_lock(&LOCK_instance_map);
}


void Instance_map::unlock()
{
  pthread_mutex_unlock(&LOCK_instance_map);
}


int Instance_map::flush_instances()
{
  int rc;

  guardian->lock();
  pthread_mutex_lock(&LOCK_instance_map);
  hash_free(&hash);
  hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
            get_instance_key, delete_instance, 0);
  pthread_mutex_unlock(&LOCK_instance_map);
  rc= load();
  guardian->init();
  guardian->unlock();
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


int Instance_map::complete_initialization()
{
  Instance *instance;
  uint i= 0;


  if (hash.records == 0)                        /* no instances found */
  {
    if ((instance= new Instance) == 0)
      goto err;

    if (instance->init("mysqld") || add_instance(instance))
      goto err_instance;


    /*
      After an instance have been added to the instance_map,
      hash_free should handle it's deletion => goto err, not
      err_instance.
    */
    if (instance->complete_initialization(this, mysqld_path,
                                          DEFAULT_SINGLE_INSTANCE))
      goto err;
  }
  else
    while (i < hash.records)
    {
      instance= (Instance *) hash_element(&hash, i);
      if (instance->complete_initialization(this, mysqld_path, USUAL_INSTANCE))
        goto err;
      i++;
    }

  return 0;
err_instance:
  delete instance;
err:
  return 1;
}


/* load options from config files and create appropriate instance structures */

int Instance_map::load()
{
  int argc= 1;
  /* this is a dummy variable for search_option_files() */
  uint args_used= 0;
  const char *argv_options[3];
  char **argv= (char **) &argv_options;


  /* the name of the program may be orbitrary here in fact */
  argv_options[0]= "mysqlmanager";
  argv_options[1]= '\0';

  /*
    If the routine failed, we'll simply fallback to defaults in
    complete_initialization().
  */
  if (my_search_option_files(Options::config_file, &argc,
                             (char ***) &argv, &args_used,
                             process_option, (void*) this))
    log_info("Falling back to compiled-in defaults");

  if (complete_initialization())
    return 1;

  return 0;
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

  return NULL;
}

