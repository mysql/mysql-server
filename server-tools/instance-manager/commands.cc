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
#include "factory.h"
#include "commands.h"
#include "instance.h"
#include "instance_map.h"
#include "messages.h"


/* implementation for Show_instances: */

int Show_instances::execute(struct st_net *net, ulong connection_id)
{
  if (factory->instance_map.show_instances(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/* implementation for Flush_instances: */

int Flush_instances::execute(struct st_net *net, ulong connection_id)
{
  if (factory->instance_map.flush_instances())
    return ER_OUT_OF_RESOURCES;

  net_send_ok(net, connection_id);
  return 0;
}


/* implementation for Show_instance_status: */

Show_instance_status::Show_instance_status(Command_factory *factory,
                                           const char *name, uint len)
  :Command(factory)
{
  Instance *instance;

  /* we make a search here, since we don't want t store the name */
  if (instance= factory->instance_map.find(name, len))
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


int Show_instance_status::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    if (factory->instance_map.show_instance_status(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* Implementation for Show_instance_options */

Show_instance_options::Show_instance_options(Command_factory *factory,
                                             const char *name, uint len):
  Command(factory)
{
  Instance *instance;

  /* we make a search here, since we don't want t store the name */
  if (instance= (factory->instance_map).find(name, len))
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


int Show_instance_options::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    if (factory->instance_map.show_instance_options(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* Implementation for Start_instance */

Start_instance::Start_instance(Command_factory *factory,
                               const char *name, uint len)
  :Command(factory)
{
  /* we make a search here, since we don't want t store the name */
  if (instance= factory->instance_map.find(name, len))
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
      factory->instance_map.guardian->guard(instance->options.instance_name,
                                         instance->options.instance_name_len);

    net_send_ok(net, connection_id);
    return 0;
  }
}


/* Implementation for Stop_instance: */

Stop_instance::Stop_instance(Command_factory *factory,
                               const char *name, uint len)
  :Command(factory)
{
  /* we make a search here, since we don't want t store the name */
  if (instance= factory->instance_map.find(name, len))
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
      factory->instance_map.guardian->
               stop_guard(instance_name, instance->options.instance_name_len);
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
