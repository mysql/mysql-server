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

#include "command.h"

#include <my_global.h>
#include <my_sys.h>
#include <m_ctype.h>
#include <m_string.h>
#include <mysql_com.h>
#include <mysqld_error.h>

#include "log.h"
#include "protocol.h"
#include "instance_map.h"

Command::Command(Command_factory *factory_arg)
  :factory(factory_arg)
{}

Command::~Command()
{}

#ifdef __GNUC__
FIX_GCC_LINKING_PROBLEM
#endif
