/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef AUTHENTICATION_WEBAUTHN_CLIENTOPT_LONGOPTS_H
#define AUTHENTICATION_WEBAUTHN_CLIENTOPT_LONGOPTS_H

{"plugin_authentication_webauthn_client_preserve_privacy",
 OPT_AUTHENTICATION_WEBAUTHN_CLIENT_PRESERVE_PRIVACY,
 "Allows selection of discoverable credential to be used for signing "
 "challenge. "
 "default is false - implies challenge is signed by all credentials for "
 "given relying party.",
 &opt_authentication_webauthn_client_preserve_privacy,
 &opt_authentication_webauthn_client_preserve_privacy,
 nullptr,
 GET_BOOL,
 NO_ARG,
 0,
 0,
 0,
 nullptr,
 0,
 nullptr},
    {"plugin_authentication_webauthn_device",
     0,
     "Specifies what libfido2 device to use. 0 (the first device) is the "
     "default.",
     &opt_authentication_webauthn_client_device,
     &opt_authentication_webauthn_client_device,
     nullptr,
     GET_UINT,
     REQUIRED_ARG,
     0,
     0,
     0,
     nullptr,
     0,
     nullptr},
#endif /* AUTHENTICATION_WEBAUTHN_CLIENTOPT_LONGOPTS_H */
