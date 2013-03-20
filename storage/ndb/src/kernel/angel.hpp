/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef ANGEL_HPP
#define ANGEL_HPP

#include <util/BaseString.hpp>

void
angel_run(const char* progname,
          const Vector<BaseString>& original_args,
          const char* connect_str,
          int force_nodeid,
          const char* bind_address,
          bool initial,
          bool no_start,
          bool daemon);

void
angel_stop(void);

#endif
