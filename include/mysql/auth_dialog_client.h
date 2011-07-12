#ifndef MYSQL_AUTH_DIALOG_CLIENT_INCLUDED
/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file

  Definitions needed to use Dialog client authentication plugin
*/

struct st_mysql;

#define MYSQL_AUTH_DIALOG_CLIENT_INCLUDED

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

/**
  first byte of the question string is the question "type".
  It can be an "ordinary" or a "password" question.
  The last bit set marks a last question in the authentication exchange.
*/
#define ORDINARY_QUESTION       "\2"
#define LAST_QUESTION           "\3"
#define PASSWORD_QUESTION       "\4"
#define LAST_PASSWORD           "\5"

#endif
