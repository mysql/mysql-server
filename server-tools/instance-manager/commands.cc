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

#include "command.h"
#include "commands.h"
#include "instance.h"
#include "instance_map.h"
#include "messages.h"
#include "protocol.h"
#include "buffer.h"
#include <m_string.h>


/* implementation for Show_instances: */


/*
  The method sends a list of instances in the instance map to the client.

  SYNOPSYS
    Show_instances::do_command()
    net               The network connection to the client.

  RETURN
    0 - ok
    1 - error occured
*/

int Show_instances::do_command(struct st_net *net)
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
    Imap_iterator iterator(instance_map);

    instance_map->lock();
    while (instance= iterator.next())
    {
      position= 0;
      store_to_string(&send_buff, instance->options.instance_name, &position);
      if (instance->is_running())
        store_to_string(&send_buff, (char *) "online", &position);
      else
        store_to_string(&send_buff, (char *) "offline", &position);
      if (my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }
    instance_map->unlock();
  }
  if (send_eof(net))
    goto err;
  if (net_flush(net))
    goto err;

  return 0;
err:
  return 1;
}


int Show_instances::execute(struct st_net *net, ulong connection_id)
{
  if (do_command(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/* implementation for Flush_instances: */

int Flush_instances::execute(struct st_net *net, ulong connection_id)
{
  if (instance_map->flush_instances())
    return ER_OUT_OF_RESOURCES;

  net_send_ok(net, connection_id);
  return 0;
}


/* implementation for Show_instance_status: */

Show_instance_status::Show_instance_status(Instance_map *imap_arg,
                                           const char *name, uint len)
  :Command(imap_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want t store the name */
  if (instance= instance_map->find(name, len))
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


/*
  The method sends a table with a status of requested instance to the client.

  SYNOPSYS
    Show_instance_status::do_command()
    net               The network connection to the client.
    instance_name     The name of the instance.

  RETURN
    0 - ok
    1 - error occured
*/


int Show_instance_status::do_command(struct st_net *net,
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
    if ((instance= instance_map->find(instance_name, strlen(instance_name))) == NULL)
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


int Show_instance_status::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    if (do_command(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* Implementation for Show_instance_options */

Show_instance_options::Show_instance_options(Instance_map *imap_arg,
                                             const char *name, uint len):
  Command(imap_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want t store the name */
  if (instance= instance_map->find(name, len))
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


int Show_instance_options::do_command(struct st_net *net,
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

    if ((instance= instance_map->
                   find(instance_name, strlen(instance_name))) == NULL)
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


int Show_instance_options::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    if (do_command(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* Implementation for Start_instance */

Start_instance::Start_instance(Instance_map *imap_arg,
                               const char *name, uint len)
  :Command(imap_arg)
{
  /* we make a search here, since we don't want t store the name */
  if (instance= instance_map->find(name, len))
    instance_name= instance->options.instance_name;
}


int Start_instance::execute(struct st_net *net, ulong connection_id)
{
  uint err_code;
  if (instance == 0)
  {
    return ER_BAD_INSTANCE_NAME; /* haven't found an instance */
  }
  else
  {
    if (err_code= instance->start())
      return err_code;

    if (instance->options.is_guarded != NULL)
        instance_map->guardian->guard(instance);

    net_send_ok(net, connection_id);
    return 0;
  }
}


/* Implementation for Stop_instance: */

Stop_instance::Stop_instance(Instance_map *imap_arg,
                               const char *name, uint len)
  :Command(imap_arg)
{
  /* we make a search here, since we don't want t store the name */
  if (instance= instance_map->find(name, len))
    instance_name= instance->options.instance_name;
}


int Stop_instance::execute(struct st_net *net, ulong connection_id)
{
  uint err_code;

  if (instance == 0)
  {
    return ER_BAD_INSTANCE_NAME; /* haven't found an instance */
  }
  else
  {
    if (instance->options.is_guarded != NULL)
        instance_map->guardian->
               stop_guard(instance);
    if (err_code= instance->stop())
      return err_code;

    net_send_ok(net, connection_id);
    return 0;
  }
}


int Syntax_error::execute(struct st_net *net, ulong connection_id)
{
  return ER_SYNTAX_ERROR;
}
