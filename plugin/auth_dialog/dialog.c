/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#if !defined (_GNU_SOURCE)
# define _GNU_SOURCE /* for RTLD_DEFAULT */
#endif

#include <mysql/client_plugin.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mysql.h>
#include <dlfcn.h>

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

