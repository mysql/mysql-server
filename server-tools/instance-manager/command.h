#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_COMMAND_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_COMMAND_H
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

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

/* Class responsible for allocation of im commands. */

class Instance_map;

/*
  Command - entry point for any command.
  GangOf4: 'Command' design pattern
*/

class Command
{
public:
  Command(Instance_map *instance_map_arg= 0);
  virtual ~Command();

  /* method of executing: */
  virtual int execute(struct st_net *net, ulong connection_id) = 0;

protected:
  Instance_map *instance_map;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_COMMAND_H */
