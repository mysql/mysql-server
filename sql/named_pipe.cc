/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"
#include "log.h"
#include "named_pipe.h"

/**
  Creates an instance of a named pipe and returns a handle.

  @param sec_attr    Security attributes for the pipe.
  @param sec_descr   Security descriptor for the pipe.
  @param buffer_size Number of bytes to reserve for input and output buffers.
  @param name        The name of the pipe.
  @param name_buff   Output argument: null-terminated concatenation of
                     "\\.\pipe\" and name.
  @param buflen      The size of name_buff.

  @returns           Pipe handle, or INVALID_HANDLE_VALUE in case of error.

  @note  The entire pipe name string can be up to 256 characters long.
         Pipe names are not case sensitive.
 */
HANDLE create_server_named_pipe(SECURITY_ATTRIBUTES *sec_attr,
                                SECURITY_DESCRIPTOR *sec_descr,
                                DWORD buffer_size,
                                const char *name,
                                char *name_buf,
                                size_t buflen)
{
  HANDLE ret_handle= INVALID_HANDLE_VALUE;

  strxnmov(name_buf, buflen - 1,
           "\\\\.\\pipe\\", name, NullS);
  memset(sec_attr, 0, sizeof(SECURITY_ATTRIBUTES));
  memset(sec_descr, 0, sizeof(SECURITY_DESCRIPTOR));
  if (!InitializeSecurityDescriptor(sec_descr, SECURITY_DESCRIPTOR_REVISION))
  {
    sql_print_error("Can't start server : Initialize security descriptor: %s",
                    strerror(errno));
    return INVALID_HANDLE_VALUE;
  }
  if (!SetSecurityDescriptorDacl(sec_descr, TRUE, NULL, FALSE))
  {
    sql_print_error("Can't start server : Set security descriptor: %s",
                    strerror(errno));
    return INVALID_HANDLE_VALUE;
  }
  sec_attr->nLength= sizeof(SECURITY_ATTRIBUTES);
  sec_attr->lpSecurityDescriptor= sec_descr;
  sec_attr->bInheritHandle= FALSE;
  ret_handle= CreateNamedPipe(name_buf,
                              PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                              FILE_FLAG_FIRST_PIPE_INSTANCE,
                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                              PIPE_UNLIMITED_INSTANCES,
                              buffer_size,
                              buffer_size,
                              NMPWAIT_USE_DEFAULT_WAIT,
                              sec_attr);
  
  if (ret_handle == INVALID_HANDLE_VALUE)
  {
    int error= GetLastError();

    if (error == ERROR_ACCESS_DENIED)
    {
      sql_print_error("Can't start server : Named Pipe \"%s\" already in use.",
                      name);
    }
    else
    {
      LPVOID msg_buff= NULL;

      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR) &msg_buff, 0, NULL );
      
      if (msg_buff != NULL)
      {
        sql_print_error("%s: %s", (char*)msg_buff, strerror(errno));
        LocalFree(msg_buff);
      }
    }
  }

  return ret_handle;
}
