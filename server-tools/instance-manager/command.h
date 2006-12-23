#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_COMMAND_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_COMMAND_H
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

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

/* Class responsible for allocation of IM commands. */

class Guardian;
class Instance_map;

struct st_net;

/*
  Command - entry point for any command.
  GangOf4: 'Command' design pattern
*/

class Command
{
public:
  Command();
  virtual ~Command();

  /*
    This operation incapsulates behaviour of the command.

    SYNOPSIS
      net             The network connection to the client.
      connection_id   Client connection ID

    RETURN
      0               On success
      non 0           On error. Client error code is returned.
  */
  virtual int execute(st_net *net, ulong connection_id) = 0;

protected:
  Guardian *guardian;
  Instance_map *instance_map;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_COMMAND_H */
