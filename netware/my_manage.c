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
#include <nks/vm.h>
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
void _init_args(arg_list *al)
{
  int i;
  
  *al = malloc(sizeof(arg_list_t));

  (*al)->argc = 0;
  
  for(i = 0; i < ARG_MAX; i++)
  {
    (*al)->argv[i] = NULL;
  }
}

/******************************************************************************

	add_arg()
	
	Add an argument to a list.

******************************************************************************/
void add_arg(arg_list al, char *format, ...)
{
  va_list ap;

  ASSERT(al != NULL);
  ASSERT(al->argc < ARG_MAX);

  al->argv[al->argc] = malloc(PATH_MAX);

  ASSERT(al->argv[al->argc] != NULL);

  va_start(ap, format);

  vsprintf(al->argv[al->argc], format, ap);

  va_end(ap);

  ++(al->argc);
}

/******************************************************************************

	_free_args()
	
	Free an argument list.

******************************************************************************/
void _free_args(arg_list *al)
{
  int i;

  ASSERT(al != NULL);
  ASSERT(*al != NULL);

  for(i = 0; i < (*al)->argc; i++)
  {
    ASSERT((*al)->argv[i] != NULL);
    free((*al)->argv[i]);
    (*al)->argv[i] = NULL;
  }

  free(*al);
  *al = NULL;
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
int wait_for_server_start(char *bin_dir, char *user, char *password, int port)
{
  arg_list al;
  int err, i;
  char mysqladmin_file[PATH_MAX];
  char trash[PATH_MAX];
  
	// mysqladmin file
  snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
  snprintf(trash, PATH_MAX, "/tmp/trash.out");
	
  // args
  init_args(al);
  add_arg(al, "%s", mysqladmin_file);
  add_arg(al, "--no-defaults");
  add_arg(al, "--port=%u", port);
  add_arg(al, "--user=%s", user);
  add_arg(al, "--password=%s", password);
  add_arg(al, "--silent");
  add_arg(al, "-O");
  add_arg(al, "connect_timeout=10");
  add_arg(al, "-w");
  add_arg(al, "--host=localhost");
  add_arg(al, "ping");

	// NetWare does not support the connect timeout in the TCP/IP stack
	// -- we will try the ping multiple times
	for(i = 0; (i < TRY_MAX)
       && (err = spawn(mysqladmin_file, al, TRUE, NULL,
                       trash, NULL)); i++) sleep(1);
	
  // free args
  free_args(al);

  return err;
}

/******************************************************************************

	spawn()
	
	Spawn the given file with the given arguments.

******************************************************************************/
int spawn(char *file, arg_list al, int join, char *input,
          char *output, char *error)
{
	NXNameSpec_t name;
	NXExecEnvSpec_t env;
	NXVmId_t vm, ignore;
	int result;
	
	// name
	name.ssType = NX_OBJ_FILE;
	name.ssPathCtx = 0;
	name.ssPath = file;
	
	// env
	env.esArgc = al->argc;
	env.esArgv = al->argv;
	env.esEnv = NULL;
	
	env.esStdin.ssPathCtx = 0;
	env.esStdout.ssPathCtx = 0;
	env.esStderr.ssPathCtx = 0;
	
  if (input == NULL)
	{
		env.esStdin.ssType = NX_OBJ_DEFAULT;
		env.esStdin.ssPath = NULL;
	}
	else
	{
		env.esStdin.ssType = NX_OBJ_FILE;
		env.esStdin.ssPath = input;
	}
	
	if (output == NULL)
  {
    env.esStdout.ssType = NX_OBJ_DEFAULT;
    env.esStdout.ssPath = NULL;
  }
  else
  {
    env.esStdout.ssType = NX_OBJ_FILE;
    env.esStdout.ssPath = output;
  }
	
	if (error == NULL)
  {
    env.esStderr.ssType = NX_OBJ_DEFAULT;
    env.esStderr.ssPath = NULL;
  }
  else
  {
    env.esStderr.ssType = NX_OBJ_FILE;
    env.esStderr.ssPath = error;
  }
	
	result = NXVmSpawn(&name, &env, NX_VM_SAME_ADDRSPACE | NX_VM_INHERIT_ENV, &vm);
	
	if (!result && join)
	{
		NXVmJoin(vm, &ignore, &result);
	}
	
	return result;
}

/******************************************************************************

	stop_server()
	
	Stop the server with the given port and pid file.

******************************************************************************/
int stop_server(char *bin_dir, char *user, char *password, int port,
                char *pid_file)
{
	arg_list al;
	int err, i, argc = 0;
  char mysqladmin_file[PATH_MAX];
  char trash[PATH_MAX];
  
	// mysqladmin file
  snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
  snprintf(trash, PATH_MAX, "/tmp/trash.out");
	
  // args
  init_args(al);
	add_arg(al, "%s", mysqladmin_file);
	add_arg(al, "--no-defaults");
	add_arg(al, "--port=%u", port);
	add_arg(al, "--user=%s", user);
	add_arg(al, "--password=%s", password);
	add_arg(al, "-O");
	add_arg(al, "shutdown_timeout=20");
	add_arg(al, "shutdown");

	// spawn
	if ((err = spawn(mysqladmin_file, al, TRUE, NULL,
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
  free_args(al);

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
int removef(char *format, ...)
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

