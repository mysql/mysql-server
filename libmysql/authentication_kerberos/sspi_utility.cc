/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#define SECURITY_WIN32

#include "sspi_utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

extern Logger_client *g_logger_client;

/* Log MySQL client SSPI error into error stream.  */
void log_client_sspi_error(SECURITY_STATUS err, const char *msg) {
  if (!succeeded(err)) {
    std::stringstream log_stream;
    char sspi_err_smsg[1024]{'\0'};
    std::string symbol{"#"};
    symbol += err;
    strcpy(sspi_err_smsg, symbol.c_str());
    size_t length{0};
    log_stream << "Client SSPI error: ";
    length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        err, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        static_cast<LPSTR>(sspi_err_smsg), sizeof(sspi_err_smsg), nullptr);
    if (length) {
      log_stream << msg << " , Error number " << err << ":  " << sspi_err_smsg;
    } else {
      log_stream << msg << " , Error number " << err << ":  "
                 << "Failed to get detailed error message.";
    }
    log_client_info(log_stream.str());
  }
}

/*
  This function checks if InitializeSecurityContext succeeded or not.
  If succeeded, perform next step like continue or authentication completed.
  If failed, exit the SSPI authentication loop.

  https://docs.microsoft.com/en-us/windows/win32/secauthn/initializesecuritycontext--kerberos
*/
bool succeeded(SECURITY_STATUS status) {
  if ((status == SEC_E_OK) || (status == SEC_I_COMPLETE_AND_CONTINUE) ||
      (status == SEC_I_COMPLETE_NEEDED) || (status == SEC_I_CONTINUE_NEEDED) ||
      (status == SEC_I_INCOMPLETE_CREDENTIALS)) {
    return true;
  } else {
    return false;
  }
}