/* Copyright (C) 2003 MySQL AB

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

#ifndef __CPCD_COMMON_HPP_INCLUDED__
#define __CPCD_COMMON_HPP_INCLUDED__

#include <ndb_global.h>
#include <logger/Logger.hpp>
#if 0
#include <getarg.h>
#endif

extern int debug;

extern Logger logger;

int runas(const char * user);
int insert(const char * pair, class Properties & p);

int insert_file(const char * filename, class Properties&);
int insert_file(FILE *, class Properties&, bool break_on_empty = false);

#endif /* ! __CPCD_COMMON_HPP_INCLUDED__ */
