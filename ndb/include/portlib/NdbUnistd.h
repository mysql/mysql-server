/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_UNISTD_H
#define NDB_UNISTD_H

#ifdef NDB_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <limits.h>

#define DIR_SEPARATOR "\\"
#define PATH_MAX 256

#pragma warning(disable: 4503 4786)

#else
#include <unistd.h>
#include <limits.h>

#define DIR_SEPARATOR "/"

#endif

#endif
