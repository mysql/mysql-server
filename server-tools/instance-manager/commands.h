#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_COMMANDS_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_COMMANDS_H
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

#include "instance.h"
#include "my_global.h"

/*
  Print all instances of this instance manager.
  Grammar: SHOW ISTANCES
*/

class Show_instances : public Command
{
public:
  Show_instances(Instance_map *instance_map_arg): Command(instance_map_arg)
  {}

  int do_command(struct st_net *net);
  int execute(struct st_net *net, ulong connection_id);
};


/*
  Reread configuration file and refresh instance map.
  Grammar: FLUSH INSTANCES
*/

class Flush_instances : public Command
{
public:
  Flush_instances(Instance_map *instance_map_arg): Command(instance_map_arg)
  {}

  int execute(struct st_net *net, ulong connection_id);
};


/*
  Print status of an instance.
  Grammar: SHOW ISTANCE STATUS <instance_name>
*/

class Show_instance_status : public Command
{
public:

  Show_instance_status(Instance_map *instance_map_arg, const char *name, uint len);
  int do_command(struct st_net *net, const char *instance_name);
  int execute(struct st_net *net, ulong connection_id);
  const char *instance_name;
};


/*
  Print options if chosen instance.
  Grammar: SHOW INSTANCE OPTIONS <instance_name>
*/

class Show_instance_options : public Command
{
public:

  Show_instance_options(Instance_map *instance_map_arg, const char *name, uint len);

  int execute(struct st_net *net, ulong connection_id);
  int do_command(struct st_net *net, const char *instance_name);
  const char *instance_name;
};


/*
  Start an instance.
  Grammar: START INSTANCE <instance_name>
*/

class Start_instance : public Command
{
public:
  Start_instance(Instance_map *instance_map_arg, const char *name, uint len);

  int execute(struct st_net *net, ulong connection_id);
  const char *instance_name;
  Instance *instance;
};


/*
  Stop an instance.
  Grammar: STOP INSTANCE <instance_name>
*/

class Stop_instance : public Command
{
public:
  Stop_instance(Instance_map *instance_map_arg, const char *name, uint len);

  Instance *instance;
  int execute(struct st_net *net, ulong connection_id);
  const char *instance_name;
};


/*
  Syntax error command. This command is issued if parser reported a syntax error.
  We need it to distinguish the parse error and the situation when parser internal
  error occured. E.g. parsing failed because we hadn't had enought memory. In the
  latter case parse_command() should return an error.
*/

class Syntax_error : public Command
{
public:
  int execute(struct st_net *net, ulong connection_id);
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_COMMANDS_H */
