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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifdef _WIN32
#define SERVICE_VERSION __declspec(dllexport) void *
#else
#define SERVICE_VERSION void *
#endif

#define VERSION_my_snprintf     0x0100
#define VERSION_thd_alloc       0x0100
#define VERSION_thd_wait        0x0100
#define VERSION_my_thread_scheduler 0x0100
