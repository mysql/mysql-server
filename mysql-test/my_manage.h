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
#ifndef __NETWARE__
#include <string.h>
#include <my_global.h>
#include <m_string.h>

#ifndef __WIN__
#define strnicmp strncasecmp
#define strlwr(STRARG) (STRARG)
#else
int my_vsnprintf_(char *to, size_t n, const char* value, ...);
#endif
#endif

/******************************************************************************

        macros
        
******************************************************************************/

#define ARG_BUF                 10
#define TRY_MAX                 5

#ifdef __WIN__
#define kill(A,B) TerminateProcess((HANDLE)A,0)
#define NOT_NEED_PID 0
#define MASTER_PID   1
#define SLAVE_PID    2
#define mysqld_timeout 60000

int pid_mode;
bool run_server;
bool skip_first_param;

#define snprintf _snprintf
#define vsnprintf _vsnprintf
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

#ifdef __WIN__
typedef int pid_t;
#endif
/******************************************************************************

        global variables
        
******************************************************************************/

/******************************************************************************

        prototypes
        
******************************************************************************/

void init_args(arg_list_t *);
void add_arg(arg_list_t *, const char *, ...);
void free_args(arg_list_t *);

#ifndef __WIN__
int sleep_until_file_exists(char *);
int sleep_until_file_deleted(char *);
#else
int sleep_until_file_exists(HANDLE);
int sleep_until_file_deleted(HANDLE);
#endif
int wait_for_server_start(char *, char *, char *, char *, int,char *);

#ifndef __WIN__
int spawn(char *, arg_list_t *, int, char *, char *, char *, char *);
#else
int spawn(char *, arg_list_t *, int , char *, char *, char *, HANDLE *);
#endif

#ifndef __WIN__
int stop_server(char *, char *, char *, char *, int, char *,char *);
pid_t get_server_pid(char *);
void kill_server(pid_t pid);
#else
int stop_server(char *, char *, char *, char *, int, HANDLE,char *);
#endif
void del_tree(char *);
int removef(const char *, ...);

void get_basedir(char *, char *);
void remove_empty_file(const char *file_name);

#endif /* _MY_MANAGE */
