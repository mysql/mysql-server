/*
  Copyright (c) 2003 Novell, Inc. All Rights Reserved. 

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

#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <screen.h>
#include <proc.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "my_manage.h"

/******************************************************************************

	macros
	
******************************************************************************/

/******************************************************************************

	global variables
	
******************************************************************************/

/******************************************************************************

	functions
	
******************************************************************************/

/******************************************************************************

	init_args()
	
	Init an argument list.

******************************************************************************/
void init_args(arg_list_t *al)
{
  ASSERT(al != NULL);
  
  al->argc = 0;
  al->size = ARG_BUF;
  al->argv = malloc(al->size * sizeof(char *));
  ASSERT(al->argv != NULL);

  return;
}

/******************************************************************************

	add_arg()
	
	Add an argument to a list.

******************************************************************************/
void add_arg(arg_list_t *al, const char *format, ...)
{
  va_list ap;
  char temp[PATH_MAX];

  ASSERT(al != NULL);

  // increase size
  if (al->argc >= al->size)
  {
    al->size += ARG_BUF;
    al->argv = realloc(al->argv, al->size * sizeof(char *));
    ASSERT(al->argv != NULL);
  }

  if (format)
  {
    va_start(ap, format);
    vsprintf(temp, format, ap);
    va_end(ap);

    al->argv[al->argc] = malloc(strlen(temp)+1);
    ASSERT(al->argv[al->argc] != NULL);
    strcpy(al->argv[al->argc], temp);

    ++(al->argc);
  }
  else
  {
    al->argv[al->argc] = NULL;
  }

  return;
}

/******************************************************************************

	free_args()
	
	Free an argument list.

******************************************************************************/
void free_args(arg_list_t *al)
{
  int i;

  ASSERT(al != NULL);

  for(i = 0; i < al->argc; i++)
  {
    ASSERT(al->argv[i] != NULL);
    free(al->argv[i]);
    al->argv[i] = NULL;
  }

  free(al->argv);
  al->argc = 0;
  al->argv = NULL;

  return;
}

/******************************************************************************

	sleep_until_file_deleted()
	
	Sleep until the given file is no longer found.

******************************************************************************/
int sleep_until_file_deleted(char *pid_file)
{
	struct stat buf;
	int i, err;
	
	for(i = 0; (i < TRY_MAX) && (err = !stat(pid_file, &buf)); i++) sleep(1);
	
	if (err != 0) err = errno;
	
	return err;
}

/******************************************************************************

	sleep_until_file_exists()

	Sleep until the given file exists.

******************************************************************************/
int sleep_until_file_exists(char *pid_file)
{
	struct stat buf;
	int i, err;
	
	for(i = 0; (i < TRY_MAX) && (err = stat(pid_file, &buf)); i++) sleep(1);
	
	if (err != 0) err = errno;
	
	return err;
}

/******************************************************************************

	wait_for_server_start()
	
	Wait for the server on the given port to start.

******************************************************************************/
int wait_for_server_start(char *bin_dir, char *user, char *password, int port,char *tmp_dir)
{
  arg_list_t al;
  int err, i;
  char mysqladmin_file[PATH_MAX];
  char trash[PATH_MAX];
  
	// mysqladmin file
  snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
  snprintf(trash, PATH_MAX, "%s/trash.out",tmp_dir);
	
  // args
  init_args(&al);
  add_arg(&al, "%s", mysqladmin_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--port=%u", port);
  add_arg(&al, "--user=%s", user);
  add_arg(&al, "--password=%s", password);
  add_arg(&al, "--silent");

#ifdef NOT_USED
  add_arg(&al, "-O");
  add_arg(&al, "connect_timeout=10");
  add_arg(&al, "-w");
#endif

  add_arg(&al, "--host=localhost");
  add_arg(&al, "ping");

	// NetWare does not support the connect timeout in the TCP/IP stack
	// -- we will try the ping multiple times
	for(i = 0; (i < TRY_MAX)
       && (err = spawn(mysqladmin_file, &al, TRUE, NULL,
                       trash, NULL)); i++) sleep(1);

  // free args
  free_args(&al);

  return err;
}

/******************************************************************************

	spawn()
	
	Spawn the given path with the given arguments.

******************************************************************************/
int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error)
{
	pid_t pid;
  int result = 0;
  wiring_t wiring = { FD_UNUSED, FD_UNUSED, FD_UNUSED };
  unsigned long flags = PROC_CURRENT_SPACE | PROC_INHERIT_CWD;

  // open wiring
  if (input)
    wiring.infd = open(input, O_RDONLY);

  if (output)
    wiring.outfd = open(output, O_WRONLY | O_CREAT | O_TRUNC);

  if (error)
    wiring.errfd = open(error, O_WRONLY | O_CREAT | O_TRUNC);

  // procve requires a NULL
  add_arg(al, NULL);

  // go
  pid = procve(path, flags, NULL, &wiring, NULL, NULL, 0,
               NULL, (const char **)al->argv);

	if (pid == -1)
  {
    result = -1;
  }
  else if (join)
  {
    waitpid(pid, &result, 0);
  }
	
  // close wiring
  if (wiring.infd != -1)
    close(wiring.infd);

  if (wiring.outfd != -1)
    close(wiring.outfd);

  if (wiring.errfd != -1)
    close(wiring.errfd);

	return result;
}

/******************************************************************************

	stop_server()
	
	Stop the server with the given port and pid file.

******************************************************************************/
int stop_server(char *bin_dir, char *user, char *password, int port,
                char *pid_file,char *tmp_dir)
{
	arg_list_t al;
	int err, i, argc = 0;
  char mysqladmin_file[PATH_MAX];
  char trash[PATH_MAX];
  
	// mysqladmin file
  snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
  snprintf(trash, PATH_MAX, "%s/trash.out",tmp_dir);
	
  // args
  init_args(&al);
	add_arg(&al, "%s", mysqladmin_file);
	add_arg(&al, "--no-defaults");
	add_arg(&al, "--port=%u", port);
	add_arg(&al, "--user=%s", user);
	add_arg(&al, "--password=%s", password);
	add_arg(&al, "-O");
	add_arg(&al, "shutdown_timeout=20");
	add_arg(&al, "shutdown");

	// spawn
	if ((err = spawn(mysqladmin_file, &al, TRUE, NULL,
                   trash, NULL)) == 0)
	{
		sleep_until_file_deleted(pid_file);
	}
	else
	{
    pid_t pid = get_server_pid(pid_file);
		
    // shutdown failed - kill server
		kill_server(pid);
	
  	sleep(TRY_MAX);
    
    // remove pid file if possible
    err = remove(pid_file);
  }
  
  // free args
  free_args(&al);

	return err;
}

/******************************************************************************

	get_server_pid()
	
	Get the VM id with the given pid file.

******************************************************************************/
pid_t get_server_pid(char *pid_file)
{
	char buf[PATH_MAX];
	int fd, err;
	char *p;
	pid_t id;
	
	// discover id
	fd = open(pid_file, O_RDONLY);
	
	err = read(fd, buf, PATH_MAX);
	
	close(fd);
	
	if (err > 0)
	{
		// terminate string
		if ((p = strchr(buf, '\n')) != NULL)
		{
			*p = NULL;
			
			// check for a '\r'
			if ((p = strchr(buf, '\r')) != NULL)
			{
				*p = NULL;
			}
		}
		else
		{
			buf[err] = NULL;
		}
		
		id = strtol(buf, NULL, 0);
	}
  
  return id;
}

/******************************************************************************

	kill_server()
	
	Force a kill of the server with the given pid.

******************************************************************************/
void kill_server(pid_t pid)
{
  if (pid > 0)
  {
    // destroy vm
    NXVmDestroy(pid);
  }
}

/******************************************************************************

	del_tree()
	
	Delete the directory and subdirectories.

******************************************************************************/
void del_tree(char *dir)
{
	DIR *parent = opendir(dir);
	DIR *entry;
	char temp[PATH_MAX];
	
	if (parent == NULL)
	{
		return;
	}

	while((entry = readdir(parent)) != NULL)
	{
		// create long name
		snprintf(temp, PATH_MAX, "%s/%s", dir, entry->d_name);

		if (entry->d_name[0] == '.')
		{
			// Skip
		}
		else if (S_ISDIR(entry->d_type))
		{
			// delete subdirectory
			del_tree(temp);
		}
		else
		{
			// remove file
			remove(temp);
		}
	}

	// remove directory
	rmdir(dir);
}

/******************************************************************************

	removef()
	
******************************************************************************/
int removef(const char *format, ...)
{
	va_list ap;
  char path[PATH_MAX];
	
	va_start(ap, format);

	vsnprintf(path, PATH_MAX, format, ap);
	
	va_end(ap);
  
  return remove(path);
}

/******************************************************************************

	get_basedir()
	
******************************************************************************/
void get_basedir(char *argv0, char *basedir)
{
	char temp[PATH_MAX];
	char *p;
	
	ASSERT(argv0 != NULL);
  ASSERT(basedir != NULL);

	strcpy(temp, strlwr(argv0));
	while((p = strchr(temp, '\\')) != NULL) *p = '/';
	
	if ((p = strindex(temp, "/bin/")) != NULL)
	{
		*p = NULL;
		strcpy(basedir, temp);
	}
}
