/*
  Copyright (c) 2002 Novell, Inc. All Rights Reserved. 

  This program is free software; you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation; either version 2 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/ 

#ifndef _MY_MANAGE
#define _MY_MANAGE

/******************************************************************************

	includes
	
******************************************************************************/

#include <stdlib.h>
#include <unistd.h>

/******************************************************************************

	macros
	
******************************************************************************/

#define ARG_MAX			50
#define TRY_MAX			5

#define init_args(al)   _init_args(&al);
#define free_args(al)   _free_args(&al);

/******************************************************************************

	structures
	
******************************************************************************/

typedef struct
{
  
  int argc;
  char *argv[ARG_MAX];

} arg_list_t, * arg_list;

/******************************************************************************

	global variables
	
******************************************************************************/

/******************************************************************************

	prototypes
	
******************************************************************************/

void _init_args(arg_list *);
void add_arg(arg_list, char *, ...);
void _free_args(arg_list *);
int sleep_until_file_exists(char *);
int sleep_until_file_deleted(char *);
int wait_for_server_start(char *, char *, char *, int);
int spawn(char *, arg_list, int, char *, char *, char *);
int stop_server(char *, char *, char *, int, char *);
pid_t get_server_pid(char *);
void kill_server(pid_t pid);
void del_tree(char *);
int removef(char *, ...);
void get_basedir(char *, char *);

#endif /* _MY_MANAGE */
