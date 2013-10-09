/*  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

  dialog client authentication plugin with examples

  dialog is a general purpose client authentication plugin, it simply
  asks the user the question, as provided by the server and reports
  the answer back to the server. No encryption is involved,
  the answers are sent in clear text.

  Two examples are provided: two_questions server plugin, that asks
  the password and an "Are you sure?" question with a reply "yes, of course".
  It demonstrates the usage of "password" (input is hidden) and "ordinary"
  (input can be echoed) questions, and how to mark the last question,
  to avoid an extra roundtrip.

  And three_attempts plugin that gives the user three attempts to enter
  a correct password. It shows the situation when a number of questions
  is not known in advance.
*/
#if defined (WIN32) && !defined (RTLD_DEFAULT)
# define RTLD_DEFAULT GetModuleHandle(NULL)
#endif

#include <my_global.h>
#include <mysql.h>
#include <mysql/plugin_auth.h>
#include <mysql/client_plugin.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined (_GNU_SOURCE)
# define _GNU_SOURCE /* for RTLD_DEFAULT */
#endif

/**
  first byte of the question string is the question "type".
  It can be an "ordinary" or a "password" question.
  The last bit set marks a last question in the authentication exchange.
*/
#define ORDINARY_QUESTION       "\2"
#define LAST_QUESTION           "\3"
#define PASSWORD_QUESTION       "\4"
#define LAST_PASSWORD           "\5"

/********************* SERVER SIDE ****************************************/

/**
  dialog demo with two questions, one password and one, the last, ordinary.
*/
static int two_questions(MYSQL_PLUGIN_VIO *vio, MYSQL_SERVER_AUTH_INFO *info)
{
  unsigned char *pkt;
  int pkt_len;

  /* send a password question */
  if (vio->write_packet(vio, (const unsigned char *) PASSWORD_QUESTION "Password, please:", 18))
    return CR_ERROR;

  /* read the answer */
  if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
    return CR_ERROR;

  info->password_used= PASSWORD_USED_YES;

  /* fail if the password is wrong */
  if (strcmp((const char *) pkt, info->auth_string))
    return CR_ERROR;

  /* send the last, ordinary, question */
  if (vio->write_packet(vio, (const unsigned char *) LAST_QUESTION "Are you sure ?", 15))
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

/********************* CLIENT SIDE ***************************************/
/*
  This plugin performs a dialog with the user, asking questions and
  reading answers. Depending on the client it may be desirable to do it
  using GUI, or console, with or without curses, or read answers
  from a smartcard, for example.

  To support all this variety, the dialog plugin has a callback function
  "authentication_dialog_ask". If the client has a function of this name
  dialog plugin will use it for communication with the user. Otherwise
  a default fgets() based implementation will be used.
*/

/**
  type of the mysql_authentication_dialog_ask function

  @param mysql          mysql
  @param type           type of the input
                        1 - ordinary string input
                        2 - password string
  @param prompt         prompt
  @param buf            a buffer to store the use input
  @param buf_len        the length of the buffer

  @retval               a pointer to the user input string.
                        It may be equal to 'buf' or to 'mysql->password'.
                        In all other cases it is assumed to be an allocated
                        string, and the "dialog" plugin will free() it.
*/
typedef char *(*mysql_authentication_dialog_ask_t)(struct st_mysql *mysql,
                      int type, const char *prompt, char *buf, int buf_len);

static mysql_authentication_dialog_ask_t ask;

static char *builtin_ask(MYSQL *mysql __attribute__((unused)),
                         int type __attribute__((unused)),
                         const char *prompt,
                         char *buf, int buf_len)
{
  char *ptr;
  fputs(prompt, stdout);
  fputc(' ', stdout);
  if (fgets(buf, buf_len, stdin) == NULL)
    return NULL;
  if ((ptr= strchr(buf, '\n')))
    *ptr= 0;

  return buf;
}

/**
  The main function of the dialog plugin.

  Read the prompt, ask the question, send the reply, repeat until
  the server is satisfied.

  @note
   1. this plugin shows how a client authentication plugin
      may read a MySQL protocol OK packet internally - which is important
      where a number of packets is not known in advance.
   2. the first byte of the prompt is special. it is not
      shown to the user, but signals whether it is the last question
      (prompt[0] & 1 == 1) or not last (prompt[0] & 1 == 0),
      and whether the input is a password (not echoed).
   3. the prompt is expected to be sent zero-terminated
*/
static int perform_dialog(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  unsigned char *pkt, cmd= 0;
  int pkt_len, res;
  char reply_buf[1024], *reply;

  do
  {
    /* read the prompt */
    pkt_len= vio->read_packet(vio, &pkt);
    if (pkt_len < 0)
      return CR_ERROR;

    if (pkt == 0)
    {
      /*
        in mysql_change_user() the client sends the first packet, so
        the first vio->read_packet() does nothing (pkt == 0).

        We send the "password", assuming the client knows what it's doing.
        (in other words, the dialog plugin should be only set as a default
        authentication plugin on the client if the first question
        asks for a password - which will be sent in clear text, by the way)
      */
      reply= mysql->passwd;
    }
    else
    {
      cmd= *pkt++;

      /* is it MySQL protocol packet ? */
      if (cmd == 0 || cmd == 254)
        return CR_OK_HANDSHAKE_COMPLETE; /* yes. we're done */

      /*
        asking for a password with an empty prompt means mysql->password
        otherwise we ask the user and read the reply
      */
      if ((cmd >> 1) == 2 && *pkt == 0)
        reply= mysql->passwd;
      else
        reply= ask(mysql, cmd >> 1, (const char *) pkt, 
				   reply_buf, sizeof(reply_buf));
      if (!reply)
        return CR_ERROR;
    }
    /* send the reply to the server */
    res= vio->write_packet(vio, (const unsigned char *) reply, 
						   strlen(reply)+1);

    if (reply != mysql->passwd && reply != reply_buf)
      free(reply);

    if (res)
      return CR_ERROR;

    /* repeat unless it was the last question */
  } while ((cmd & 1) != 1);

  /* the job of reading the ok/error packet is left to the server */
  return CR_OK;
}

/**
  initialization function of the dialog plugin

  Pick up the client's authentication_dialog_ask() function, if exists,
  or fall back to the default implementation.
*/

static int init_dialog(char *unused1   __attribute__((unused)), 
                       size_t unused2  __attribute__((unused)), 
                       int unused3     __attribute__((unused)), 
                       va_list unused4 __attribute__((unused)))
{
  void *sym= dlsym(RTLD_DEFAULT, "mysql_authentication_dialog_ask");
  ask= sym ? (mysql_authentication_dialog_ask_t) sym : builtin_ask;
  return 0;
}

mysql_declare_client_plugin(AUTHENTICATION)
  "dialog",
  "Sergei Golubchik",
  "Dialog Client Authentication Plugin",
  {0,1,0},
  "GPL",
  NULL,
  init_dialog,
  NULL,
  NULL,
  perform_dialog
mysql_end_client_plugin;

