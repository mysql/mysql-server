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
#ifndef __WIN__
#include <unistd.h>
#endif

/******************************************************************************

	macros
	
******************************************************************************/
#ifdef __WIN__
#define PATH_MAX _MAX_PATH
#define NAME_MAX _MAX_FNAME
#define kill(A,B) TerminateProcess((HANDLE)A,0)
#define NOT_NEED_PID 0
#define MASTER_PID   1
#define SLAVE_PID    2
#define mysqld_timeout 60000

intptr_t master_server;
intptr_t slave_server;
int pid_mode;
bool run_server;
char win_args[1024];
bool skip_first_param;
#endif


#define ARG_BUF			10
#define TRY_MAX			 5
#define NULL              (char) 0

#ifdef __NETWARE__
#define strstr(A,B) strindex(A,B)
#endif


/******************************************************************************

	structures
	
******************************************************************************/

typedef struct
{
  
  int argc;
  char **argv;

  size_t size;

} arg_list_t;


typedef int pid_t;
/******************************************************************************

	global variables
	
******************************************************************************/

/******************************************************************************

	prototypes
	
******************************************************************************/

void init_args(arg_list_t *);
void add_arg(arg_list_t *, const char *, ...);
void free_args(arg_list_t *);

int sleep_until_file_exists(char *);
int sleep_until_file_deleted(char *);
int wait_for_server_start(char *, char *, char *, int,char *);

int spawn(char *, arg_list_t *, int, char *, char *, char *);

int stop_server(char *, char *, char *, int, char *,char *);
pid_t get_server_pid(char *);
void kill_server(pid_t pid);

void del_tree(char *);
int removef(const char *, ...);

void get_basedir(char *, char *);

char mysqladmin_file[PATH_MAX]; 

#endif /* _MY_MANAGE */


