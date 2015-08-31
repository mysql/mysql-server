/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

/* Defines to make different thread packages compatible */

#ifndef THREAD_TYPE_INCLUDED
#define THREAD_TYPE_INCLUDED

#ifdef  __cplusplus
extern "C"{
#endif

/* Flags for the THD::system_thread variable */
enum enum_thread_type
{
  NON_SYSTEM_THREAD= 0,
  SYSTEM_THREAD_SLAVE_IO= 1,
  SYSTEM_THREAD_SLAVE_SQL= 2,
  SYSTEM_THREAD_NDBCLUSTER_BINLOG= 4,
  SYSTEM_THREAD_EVENT_SCHEDULER= 8,
  SYSTEM_THREAD_EVENT_WORKER= 16,
  SYSTEM_THREAD_INFO_REPOSITORY= 32,
  SYSTEM_THREAD_SLAVE_WORKER= 64,
  SYSTEM_THREAD_COMPRESS_GTID_TABLE= 128,
  SYSTEM_THREAD_BACKGROUND= 256
};

#ifdef  __cplusplus
}
#endif

#endif /* THREAD_TYPE_INCLUDED */
