/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab
   Copyright (c) 2010, 2011, Oracle and/or its affiliates.

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

/**
  @file

  examples for dialog client authentication plugin

  Two examples are provided: two_questions server plugin, that asks
  the password and an "Are you sure?" question with a reply "yes, of course".
  It demonstrates the usage of "password" (input is hidden) and "ordinary"
  (input can be echoed) questions, and how to mark the last question,
  to avoid an extra roundtrip.

  And three_attempts plugin that gives the user three attempts to enter
  a correct password. It shows the situation when a number of questions
  is not known in advance.
*/

#include <mysql/plugin_auth.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mysql/auth_dialog_client.h>

/********************* SERVER SIDE ****************************************/

/**
  dialog demo with two questions, one password and one, the last, ordinary.
*/
static int two_questions(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  int pkt_len;

  /* send a password question */
  if (vio->write_packet(vio,
                        (const unsigned char *) PASSWORD_QUESTION "Password, please:",
                        18))
    return CR_ERROR;

  /* read the answer */
  if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
    return CR_ERROR;

  info->password_used= PASSWORD_USED_YES;

  /* fail if the password is wrong */
  if (strcmp((const char *) pkt, info->auth_string))
    return CR_ERROR;

  /* send the last, ordinary, question */
  if (vio->write_packet(vio,
                        (const unsigned char *) LAST_QUESTION "Are you sure ?",
                        15))
    return CR_ERROR;

  /* read the answer */
  if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
    return CR_ERROR;

  /* check the reply */
  return strcmp((const char *) pkt, "yes, of course") ? CR_ERROR : CR_OK;
}

static struct st_mysql_auth two_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "dialog", /* requires dialog client plugin */
  two_questions
};

/* dialog demo where the number of questions is not known in advance */
static int three_attempts(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  int pkt_len, i;

  for (i= 0; i < 3; i++)
  {
    /* send the prompt */
    if (vio->write_packet(vio, 
		(const unsigned char *) PASSWORD_QUESTION "Password, please:", 18))
      return CR_ERROR;

    /* read the password */
    if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
      return CR_ERROR;

    info->password_used= PASSWORD_USED_YES;

    /*
      finish, if the password is correct.
      note, that we did not mark the prompt packet as "last"
    */
    if (strcmp((const char *) pkt, info->auth_string) == 0)
      return CR_OK;
  }

  return CR_ERROR;
}

static struct st_mysql_auth three_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  "dialog", /* requires dialog client plugin */
  three_attempts 
};

mysql_declare_plugin(dialog)
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &two_handler,
  "two_questions",
  "Sergei Golubchik",
  "Dialog plugin demo 1",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0100,
  NULL,
  NULL,
  NULL,
  0,
},
{
  MYSQL_AUTHENTICATION_PLUGIN,
  &three_handler,
  "three_attempts",
  "Sergei Golubchik",
  "Dialog plugin demo 2",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0100,
  NULL,
  NULL,
  NULL,
  0,
}
mysql_declare_plugin_end;

