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

#include "common.h"

#include <openssl/evp.h> /* EVP_* */

#include <mysql_com.h> /* CHALLENGE_LENGTH */

#undef MYSQL_DYNAMIC_PLUGIN
#include <mysql/service_mysql_alloc.h>
#define MYSQL_DYNAMIC_PLUGIN

/*
  Helper method to redirect plugin specific messages to a registered callback
  method or to stdout/stderr.
*/
void get_plugin_messages(const std::string &msg, message_type type) {
  /* if callback is registered, pass msg to callback function */
  if (mc) {
    mc(msg.c_str());
  } else {
    if (type == message_type::ERROR) {
      std::cerr << msg.c_str() << std::endl;
      std::cerr.flush();
    } else if (type == message_type::INFO) {
      std::cout << msg.c_str() << std::endl;
    }
  }
}

/*
  Helper method to redirect plugin specific input requirement to a registered
  callback or to stdin
*/
int get_user_input(const std::string &msg, input_type type, void *arg,
                   const unsigned int *optional_arg_size /* = false */) {
  int retval = 1;
  switch (type) {
    case input_type::UINT: {
      unsigned int *uint_input = pointer_cast<unsigned int *>(arg);
      get_plugin_messages(msg, message_type::INFO);
      if (mc_get_uint) {
        retval = mc_get_uint(uint_input);
      } else {
        std::cin >> *uint_input;
        retval = 0;
      }
      break;
    }
    case input_type::PASSWORD: {
      if (optional_arg_size != nullptr) {
        char *char_input = pointer_cast<char *>(arg);
        if (mc_get_password) {
          get_plugin_messages(msg, message_type::INFO);
          retval = mc_get_password(char_input, *optional_arg_size);
        } else {
          char *password = get_tty_password(msg.c_str());
          auto password_len = strlen(password);
          if (password_len < *optional_arg_size) {
            strcpy(char_input, password);
            retval = 0;
          }
          memset(password, 1, password_len);
          my_free(password);
        }
      }
    }
    default:
      break;
  }
  return retval;
}

/**
  Helper method to convert base64 string to url safe base64.
  Using standard Base64 in URL requires replacing of '+', '/' and '='
  characters.
  '+' will be replaced with '-'
  '/' will be replaced with '_'
  '=' is used for padding which will be removed.
  @param [out] url_compatible_str url safe base64 string
  @param [in] len length of base64 string
  @param [in] base64_str  base64 string
*/
void url_compatible_base64(char *url_compatible_str, size_t len,
                           char *base64_str) {
  for (size_t i = 0; i < len; i++) {
    switch (base64_str[i]) {
      case '+':
        url_compatible_str[i] = '-';
        break;
      case '/':
        url_compatible_str[i] = '_';
        break;
      case '=':
        url_compatible_str[i] = 0;
        return;
      default:
        url_compatible_str[i] = base64_str[i];
    }
  }
}

bool generate_sha256(const unsigned char *in_key, unsigned int in_key_length,
                     unsigned char *hash, unsigned int &hash_length) {
  bool ret = true;
  EVP_MD *md_sha256 = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  md_sha256 = EVP_MD_fetch(nullptr, "sha256", nullptr);
#endif
  EVP_MD_CTX *context = EVP_MD_CTX_create();
  if (context != nullptr) {
    if (EVP_DigestInit_ex(context, md_sha256 ? md_sha256 : EVP_sha256(),
                          nullptr)) {
      if (EVP_DigestUpdate(context, in_key, in_key_length)) {
        if (EVP_DigestFinal_ex(context, hash, &hash_length)) {
          ret = false;
        }
      }
    }
    EVP_MD_CTX_destroy(context);
  }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (md_sha256) EVP_MD_free(md_sha256);
  md_sha256 = nullptr;
#endif
  return ret;
}
