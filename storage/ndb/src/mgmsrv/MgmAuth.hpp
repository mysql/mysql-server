/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MGM_AUTH_HPP
#define MGM_AUTH_HPP

class MgmAuth {
 public:
  typedef unsigned short level;

  enum {
    serverRequiresTls = 0x001,  // Server requires TLS past bootstrap stage
    clientHasTls = 0x010,       // Client session is using TLS
    clientHasCert = 0x020,      // Client session is authenticated via cert
    cmdIsBootstrap = 0x100,     // Command is used to bootstrap a client
  };

  enum result { Ok, ServerRequiresTls, END_ERRORS };

  static int checkAuth(int cmdAuthLevel, int serverOpt, int sessionAuthLevel);

  static const char *message(int code) {
    if (code >= 0 && code < result::END_ERRORS) return _message[code];
    return "(MgmAuth unexpected error code)";
  }

 private:
  static constexpr const char *_message[result::END_ERRORS] = {
      "(no error)",
      "Requires TLS",
  };
};

#endif
