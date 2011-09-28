/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301  USA.
*/

#ifndef yaSSL_transport_types_h__
#define yaSSL_transport_types_h__

/* Type of transport functions used for sending and receiving data. */
typedef long (*yaSSL_recv_func_t) (void *, void *, size_t);
typedef long (*yaSSL_send_func_t) (void *, const void *, size_t);

#endif
