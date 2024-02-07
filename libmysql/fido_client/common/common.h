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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMMON_H_
#define COMMON_H_

#include <cstring>
#include <iostream>
#include <sstream>

#include "my_byteorder.h"
#include "my_inttypes.h"

/* Length of random salt in base64 format */
#define BASE64_CHALLENGE_LENGTH 45

typedef void (*plugin_messages_callback)(const char *msg);
typedef int (*plugin_messages_callback_get_uint)(unsigned int *val);
typedef int (*plugin_messages_callback_get_password)(
    char *buffer, const unsigned int buffer_len);
extern plugin_messages_callback mc;
extern plugin_messages_callback_get_uint mc_get_uint;
extern plugin_messages_callback_get_password mc_get_password;

/* Define type of message */
enum class message_type {
  INFO,  /* directed to stdout */
  ERROR, /* directed to stderr */
};

enum class input_type {
  UINT,    /* get unsigned int input */
  PASSWORD /* get password */
};
/*
  Helper method to redirect plugin specific messages to a registered callback
  method or to stdout/stderr.
*/
void get_plugin_messages(const std::string &msg, message_type type);
int get_user_input(const std::string &msg, input_type type, void *arg,
                   const unsigned int *optional_arg_size = nullptr);

/**
  Helper method to convert base64 string to url safe base64.
*/
void url_compatible_base64(char *url_compatible_str, size_t len,
                           char *base64_str);
bool generate_sha256(const unsigned char *in_key, unsigned int in_key_length,
                     unsigned char *hash, unsigned int &hash_length);

#endif  // COMMON_H_
