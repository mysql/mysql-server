#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_FACTORY_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_FACTORY_H
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
#include "instance_map.h"

/*
  This class could be used to handle various protocols. We could pass to
  the parser various derived classes. I.e Mylsq_command_factory,
  Http_command_factory e.t.c. Also see comment in the instance_map.cc
*/

class Command_factory
{
public:
  Command_factory(Instance_map &instance_map): instance_map(instance_map)
  {}

  Show_instances        *new_Show_instances        ();
  Show_instance_status  *new_Show_instance_status  (const char *name, uint len);
  Show_instance_options *new_Show_instance_options (const char *name, uint len);
  Start_instance        *new_Start_instance        (const char *name, uint len);
  Stop_instance         *new_Stop_instance         (const char *name, uint len);
  Flush_instances       *new_Flush_instances       ();
  Syntax_error          *new_Syntax_error          ();

  Instance_map &instance_map;
};
#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_FACTORY_H */
