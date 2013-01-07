/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef NAMED_PIPE_INCLUDED
#define NAMED_PIPE_INCLUDED

C_MODE_START

HANDLE create_server_named_pipe(SECURITY_ATTRIBUTES *sec_attr,
                                SECURITY_DESCRIPTOR *sec_descr,
                                DWORD buffer_size,
                                const char *name,
                                char *name_buf,
                                size_t buflen);

C_MODE_END

#endif /* NAMED_PIPE_INCLUDED */
