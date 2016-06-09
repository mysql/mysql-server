/*  Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <my_global.h>
#include <mysql/plugin_auth.h>
#include <mysql/client_plugin.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
  first byte of the question string is the question "type".
  It can be a "ordinary" or a "password" question.
  The last bit set marks a last question in the authentication exchange.
*/
#define ORDINARY_QUESTION       "\2"
#define LAST_QUESTION           "\3"
#define LAST_PASSWORD           "\4"
#define PASSWORD_QUESTION       "\5"

/********************* SERVER SIDE ****************************************/

static int qa_auth_interface (MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  int pkt_len, err= CR_OK;

  /* send a password question */
  if (vio->write_packet(vio, (const unsigned char *) PASSWORD_QUESTION, 1))
    return CR_ERROR;

  /* read the answer */
  if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
    return CR_ERROR;

  info->password_used= PASSWORD_USED_YES;

  /* fail if the password is wrong */
  if (strcmp((const char *) pkt, info->auth_string))
    return CR_ERROR;

/* Test of default_auth */
  if (strcmp(info->user_name, "qa_test_11_user")== 0)
  {
     strcpy(info->authenticated_as, "qa_test_11_dest");
  }
  else
     err= CR_ERROR;
  return err;
}

int generate_auth_string_hash(char *outbuf, unsigned int *buflen,
                              const char *inbuf, unsigned int inbuflen)
{
  /*
    if buffer specified by server is smaller than the buffer given
    by plugin then return error
  */
  if (*buflen < inbuflen)
    return 1;
  strncpy(outbuf, inbuf, inbuflen);
  *buflen= strlen(inbuf);
  return 0;
}

int validate_auth_string_hash(char* const inbuf  MY_ATTRIBUTE((unused)),
                              unsigned int buflen  MY_ATTRIBUTE((unused)))
{
  return 0;
}

int set_salt(const char* password MY_ATTRIBUTE((unused)),
             unsigned int password_len MY_ATTRIBUTE((unused)),
             unsigned char* salt MY_ATTRIBUTE((unused)),
             unsigned char* salt_len)
{
  *salt_len= 0;
  return 0;
}

static struct st_mysql_auth qa_auth_test_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "qa_auth_interface", /* requires test_plugin client's plugin */
  qa_auth_interface,
  generate_auth_string_hash,
  validate_auth_string_hash,
  set_salt,
  AUTH_FLAG_PRIVILEGED_USER_FOR_PASSWORD_CHANGE
};

mysql_declare_plugin(test_plugin)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &qa_auth_test_handler,
  "qa_auth_server",
  "Horst Hunger",
  "plugin API test plugin",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0101,
  NULL,
  NULL,
  NULL,
  0,
}
mysql_declare_plugin_end;
