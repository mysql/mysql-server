/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_SHM_DEFAULTS_INCLUDED
#define MY_SHM_DEFAULTS_INCLUDED

/**
  @file include/my_shm_defaults.h
  A few default values for shared memory, used in multiple places.
*/

#if defined (_WIN32)

/* Shared memory and named pipe connections are supported. */
#define shared_memory_buffer_length 16000
#define default_shared_memory_base_name "MYSQL"
#endif /* _WIN32*/

#endif  // MY_SHM_DEFAULTS_INCLUDED
