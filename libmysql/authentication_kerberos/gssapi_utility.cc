/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "gssapi_utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Logger_client *g_logger_client;

static void gssapi_errmsg(OM_uint32 major, OM_uint32 minor, char *buf,
                          size_t size) {
  OM_uint32 message_context{0};
  OM_uint32 status_code{0};
  OM_uint32 maj_status{0};
  OM_uint32 min_status{0};
  gss_buffer_desc status{0, nullptr};
  char *t_message = buf;
  char *end = t_message + size - 1;
  int types[] = {GSS_C_GSS_CODE, GSS_C_MECH_CODE};

  for (int i = 0; i < 2; i++) {
    message_context = 0;
    status_code = types[i] == GSS_C_GSS_CODE ? major : minor;

    if (!status_code) continue;
    do {
      maj_status = gss_display_status(&min_status, status_code, types[i],
                                      GSS_C_NO_OID, &message_context, &status);
      if (maj_status) break;

      if (t_message + status.length + 2 < end) {
        memcpy(t_message, status.value, status.length);
        t_message += status.length;
        *t_message++ = '.';
        *t_message++ = ' ';
      }
      gss_release_buffer(&min_status, &status);
    } while (message_context != 0);
  }
  *t_message = 0;
}

/* This sends the error to the client */
void log_client_gssapi_error(OM_uint32 major, OM_uint32 minor,
                             const char *msg) {
  std::stringstream log_stream;
  if (GSS_ERROR(major)) {
    char sysmsg[1024]{'\0'};
    gssapi_errmsg(major, minor, sysmsg, sizeof(sysmsg));
    log_stream << "Client GSSAPI error major: " << major << " minor: " << minor;
    log_stream << "  " << msg << sysmsg;
    log_client_info(log_stream.str());
  } else {
    log_stream.str("");
    log_stream << "Client GSSAPI error : " << msg;
  }
}
