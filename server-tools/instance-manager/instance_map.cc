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


Instance_map::Instance_map()
{
  hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
            get_instance_key, delete_instance, 0);
  pthread_mutex_init(&LOCK_instance_map, 0);
}


Instance_map::~Instance_map()
{
  pthread_mutex_lock(&LOCK_instance_map);
  hash_free(&hash);
  pthread_mutex_unlock(&LOCK_instance_map);
  pthread_mutex_destroy(&LOCK_instance_map);
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


int Instance_map::show_instance_options(struct st_net *net,
                                        const char *instance_name)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name, option;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, option_field;
  uint position=0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char *) "option_name";
  name_field.length= 20;
  name.data= &name_field;
  option_field.name= (char *) "value";
  option_field.length= 20;
  option.data= &option_field;
  field_list= list_add(NULL, &option);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;

    if ((instance= find(instance_name, strlen(instance_name))) == NULL)
      goto err;
    store_to_string(&send_buff, (char *) "instance_name", &position);
    store_to_string(&send_buff, (char *) instance_name, &position);
    my_net_write(net, send_buff.buffer, (uint) position);
    if (instance->options.mysqld_path != NULL)
    {
      position= 0;
      store_to_string(&send_buff, (char *) "mysqld_path", &position);
      store_to_string(&send_buff,
                     (char *) instance->options.mysqld_path,
                     &position);
      my_net_write(net, send_buff.buffer, (uint) position);
    }

    if (instance->options.mysqld_user != NULL)
    {
      position= 0;
      store_to_string(&send_buff, (char *) "admin_user", &position);
      store_to_string(&send_buff,
                      (char *) instance->options.mysqld_user,
                      &position);
      my_net_write(net, send_buff.buffer, (uint) position);
    }

    if (instance->options.mysqld_password != NULL)
    {
      position= 0;
      store_to_string(&send_buff, (char *) "admin_password", &position);
      store_to_string(&send_buff,
                      (char *) instance->options.mysqld_password,
                      &position);
      my_net_write(net, send_buff.buffer, (uint) position);
    }

    /* loop through the options stored in DYNAMIC_ARRAY */
    for (int i= 0; i < instance->options.options_array.elements; i++)
    {
      char *tmp_option, *option_value;
      get_dynamic(&(instance->options.options_array), (gptr) &tmp_option, i);
      option_value= strchr(tmp_option, '=');
      /* split the option string into two parts */
      *option_value= 0;
      position= 0;
      store_to_string(&send_buff, tmp_option + 2, &position);
      store_to_string(&send_buff, option_value + 1, &position);
      /* join name and the value into the same option again */
      *option_value= '=';
      my_net_write(net, send_buff.buffer, (uint) position);
    }
  }

  send_eof(net);
  net_flush(net);

  return 0;

err:
  return 1;
}

/* return the list of running guarded instances */
int Instance_map::init_guardian()
{
  Instance *instance;
  uint i= 0;

  while (i < hash.records)
  {
    instance= (Instance *) hash_element(&hash, i);
    if ((instance->options.is_guarded != NULL) && (instance->is_running()))
      if (guardian->guard(instance->options.instance_name,
                          instance->options.instance_name_len))
        return 1;
    i++;
  }

  return 0;
}


/*
  The method sends a list of instances in the instance map to the client.

  SYNOPSYS
    show_instances()
    net               The network connection to the client.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_map::show_instances(struct st_net *net)
{
  Buffer send_buff;  /* buffer for packets */
  LIST name, status;
  NAME_WITH_LENGTH name_field, status_field;
  LIST *field_list;
  uint position=0;

  name_field.name= (char *) "instance_name";
  name_field.length= 20;
  name.data= &name_field;
  status_field.name= (char *) "status";
  status_field.length= 20;
  status.data= &status_field;
  field_list= list_add(NULL, &status);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;
    uint i= 0;

    pthread_mutex_lock(&LOCK_instance_map);
    while (i < hash.records)
    {
      position= 0;
      instance= (Instance *) hash_element(&hash, i);
      store_to_string(&send_buff, instance->options.instance_name, &position);
      if (instance->is_running())
        store_to_string(&send_buff, (char *) "online", &position);
      else
        store_to_string(&send_buff, (char *) "offline", &position);
      if (my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
      i++;
    }
    pthread_mutex_unlock(&LOCK_instance_map);
  }
  if (send_eof(net))
    goto err;
  if (net_flush(net))
    goto err;

  return 0;
err:
  return 1;
}


/*
  The method sends a table with a status of requested instance to the client.

  SYNOPSYS
    show_instance_status()
    net               The network connection to the client.
    instance_name     The name of the instance.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_map::show_instance_status(struct st_net *net,
                                       const char *instance_name)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name, status, version;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, status_field, version_field;
  uint position=0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char *) "instance_name";
  name_field.length= 20;
  name.data= &name_field;
  status_field.name= (char *) "status";
  status_field.length= 20;
  status.data= &status_field;
  version_field.name= (char *) "version";
  version_field.length= MAX_VERSION_LENGTH;
  version.data= &version_field;
  field_list= list_add(NULL, &version);
  field_list= list_add(field_list, &status);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;

    store_to_string(&send_buff, (char *) instance_name, &position);
    if ((instance= find(instance_name, strlen(instance_name))) == NULL)
      goto err;
    if (instance->is_running())
    {
      store_to_string(&send_buff, (char *) "online", &position);
      store_to_string(&send_buff, mysql_get_server_info(&(instance->mysql)), &position);
    }
    else
    {
      store_to_string(&send_buff, (char *) "offline", &position);
      store_to_string(&send_buff, (char *) "unknown", &position);
    }


    my_net_write(net, send_buff.buffer, (uint) position);
  }

  send_eof(net);
  net_flush(net);

err:
  return 0;
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
