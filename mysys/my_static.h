#ifndef MYSYS_MY_STATIC_INCLUDED
#define MYSYS_MY_STATIC_INCLUDED

/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/*
  Static variables for mysys library. All definied here for easy making of
  a shared library
*/

C_MODE_START
#include <signal.h>

#define MAX_SIGNALS	10		/* Max signals under a dont-allow */

struct st_remember {
  int number;
  sig_handler (*func)(int number);
};

extern char curr_dir[FN_REFLEN], home_dir_buff[FN_REFLEN];

extern volatile int _my_signals;
extern struct st_remember _my_sig_remember[MAX_SIGNALS];

extern const char *soundex_map;

extern USED_MEM* my_once_root_block;
extern uint	 my_once_extra;

extern struct st_my_file_info my_file_info_default[MY_NFILE];

C_MODE_END

#endif /* MYSYS_MY_STATIC_INCLUDED */
