/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file include/caching_sha2_passwordopt-longopts.h
*/

{"server-public-key-path", OPT_SERVER_PUBLIC_KEY,
 "File path to the server public RSA key in PEM format.",
 &opt_server_public_key, &opt_server_public_key, 0,
 GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
{"get-server-public-key", 0,
 "Get server public key",
 &opt_get_server_public_key,
 &opt_get_server_public_key, 0, GET_BOOL,
 NO_ARG, 0, 0, 0, 0, 0, 0},
