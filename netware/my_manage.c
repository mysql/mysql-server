/*
  Copyright (c) 2003 MySQL AB
  Copyright (c) 2003 Novell, Inc.

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

/*****************************************************************************
** Utility functions for support programs
*****************************************************************************/

/* MySQL library headers */
#include <my_global.h>
#include <my_sys.h>
#include <my_dir.h>
#include <m_string.h>

/* These 'should' be POSIX or ANSI */
#include <assert.h> /* ASSERT */
#include <stdarg.h> /* vsprintf, va_* */
#include <sys/types.h> /* pid_t */
#ifndef __WIN__
#include <unistd.h> /* fork, rmdir, execve */
#endif
#include <stdio.h> /* freopen */
#include <stdlib.h> /* FILE */
#ifndef __WIN__
#include <dirent.h> /* opendir, readdir */
#endif

#if !defined(__NETWARE__) && !defined(__WIN__)
#include <sys/wait.h>
#endif

#if !defined(__NETWARE__)
#include <signal.h>
#endif

/* For ASSERT -- Not totally sure about this one: */
#if !defined(ASSERT)
#define ASSERT(A) assert(A)
#endif

#include "my_manage.h"
#define __STDC__ 1
#include "process.h"
/******************************************************************************

  init_args()

  Init an argument list.

******************************************************************************/
void init_args(arg_list_t *al)
{
#ifndef __WIN__
  ASSERT(al != NULL);

  al->argc = 0;
  al->size = ARG_BUF;
  al->argv = (char **)my_malloc(al->size * sizeof(char *), MYF(MY_WME));
  ASSERT(al->argv != NULL);
#else
  win_args[0]= '\0';
  skip_first_param= TRUE;
#endif
  return;
}

/******************************************************************************

  add_arg()

  Add an argument to a list.

******************************************************************************/
void add_arg(arg_list_t *al, const char *format, ...)
{
#ifndef __WIN__
  va_list ap;
  char temp[PATH_MAX];

  ASSERT(al != NULL);

  /* increase size */
  if (al->argc >= (int)al->size)
  {
    al->size += ARG_BUF;
    al->argv = (char **)my_realloc((char *)al->argv, al->size * sizeof(char *), MYF(MY_WME));
    ASSERT(al->argv != NULL);
  }

  if (format)
  {
    va_start(ap, format);
    vsprintf(temp, format, ap);
    va_end(ap);

    al->argv[al->argc] = my_malloc(strlen(temp)+1, MYF(MY_WME));
    ASSERT(al->argv[al->argc] != NULL);
    strcpy(al->argv[al->argc], temp);

    ++(al->argc);
  }
  else
  {
    al->argv[al->argc] = NULL;
  }
#else
  va_list ap;
  char param[PATH_MAX];

  if (!skip_first_param)
  {
    va_start(ap, format);
    vsprintf(&param, format, ap);
    va_end(ap);
    strcat(win_args," ");
    strcat(win_args,param);
  }
  else
  {
    skip_first_param= FALSE;
  }
#endif
  return;
}

/******************************************************************************

  free_args()

  Free an argument list.

******************************************************************************/
void free_args(arg_list_t *al)
{
#ifndef __WIN__
  int i;

  ASSERT(al != NULL);

  for(i = 0; i < al->argc; i++)
  {
    ASSERT(al->argv[i] != NULL);
    my_free(al->argv[i], MYF(MY_WME));
    al->argv[i] = NULL;
  }

  my_free((char *)al->argv, MYF(MY_WME));
  al->argc = 0;
  al->argv = NULL;
#endif
  return;
}

/******************************************************************************

  sleep_until_file_deleted()

  Sleep until the given file is no longer found.

******************************************************************************/
int sleep_until_file_deleted(char *pid_file)
{
  MY_STAT stat_info;
  int i, err = 0;
#ifndef __WIN__
  for(i = 0; i < TRY_MAX; i++)
  {
    if (my_stat(pid_file, &stat_info, MYF(0)) == (MY_STAT *) NULL)
    {
      err = errno;
      break;
    }
    my_sleep(1);
  }
#else
  switch (pid_mode)
  {
    case MASTER_PID:
      err= (WaitForSingleObject(master_server, TRY_MAX*1000) == WAIT_TIMEOUT);
      pid_mode= 0;
      break;
    case SLAVE_PID:
      err= (WaitForSingleObject(slave_server, TRY_MAX*1000) == WAIT_TIMEOUT);
      pid_mode= 0;
      break;
  };
#endif
  return err;
}

/******************************************************************************

  sleep_until_file_exists()

  Sleep until the given file exists.

******************************************************************************/
int sleep_until_file_exists(char *pid_file)
{
  MY_STAT stat_info;
  int i, err = 0;
  
#ifndef __WIN__  
  for(i = 0; i < TRY_MAX; i++)
  {
    if (my_stat(pid_file, &stat_info, MYF(0)) == (MY_STAT *) NULL)
    {
      err = errno;
      break;
    }
    my_sleep(1);
  }
#else
  switch (pid_mode)
  {
    case MASTER_PID:
      WaitForSingleObject(master_server, TRY_MAX*1000);
      pid_mode= 0;
      break;
    case SLAVE_PID:
      WaitForSingleObject(slave_server, TRY_MAX*1000);
      pid_mode= 0;
      break;
  };
#endif

  return err;
}

/******************************************************************************

  wait_for_server_start()
  
  Wait for the server on the given port to start.

******************************************************************************/
int wait_for_server_start(char *bin_dir, char *user, char *password, int port)
{
  arg_list_t al;
  int err = 0, i;
  char trash[PATH_MAX];
  
  /* mysqladmin file */
  my_snprintf(trash, PATH_MAX, "/tmp/trash.out");

  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqladmin_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--port=%u", port);
  add_arg(&al, "--user=%s", user);
  add_arg(&al, "--password=%s", password);
  add_arg(&al, "--silent");
  add_arg(&al, "-O");
  add_arg(&al, "connect_timeout=10");
  add_arg(&al, "-w");
  add_arg(&al, "--host=localhost");
  add_arg(&al, "ping");

  /* NetWare does not support the connect timeout in the TCP/IP stack
       -- we will try the ping multiple times */
  for(i = 0; (i < TRY_MAX)
    && (err = spawn(mysqladmin_file, &al, TRUE, NULL,
                             trash, NULL, NOT_NEED_PID)); i++) sleep(1);
  
  /* free args */
  free_args(&al);

  return err;
}

/******************************************************************************

  spawn()

  Spawn the executable at the given path with the given arguments.

******************************************************************************/

#ifdef __NETWARE__

int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error)
{
  pid_t pid;
  int result = 0;
  wiring_t wiring = { FD_UNUSED, FD_UNUSED, FD_UNUSED };
  unsigned long flags = PROC_CURRENT_SPACE | PROC_INHERIT_CWD;

  /* open wiring */
  if (input)
    wiring.infd = open(input, O_RDONLY);

  if (output)
    wiring.outfd = open(output, O_WRONLY | O_CREAT | O_TRUNC);

  if (error)
    wiring.errfd = open(error, O_WRONLY | O_CREAT | O_TRUNC);

  /* procve requires a NULL */
  add_arg(al, NULL);

  /* go */
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
  
  /* close wiring */
  if (wiring.infd != -1)
    close(wiring.infd);

  if (wiring.outfd != -1)
    close(wiring.outfd);

  if (wiring.errfd != -1)
    close(wiring.errfd);

  return result;
}

#else /* NOT __NETWARE__ */

#ifdef __WIN__

int my_vsnprintf_(char *to, size_t n, const char* value, ...)
{
  char *start=to, *end=to+n-1;
  uint length, num_state, pre_zero;
  reg2 char *par;// = value;
  va_list args;
  va_start(args,value);

  par = va_arg(args, char *);
  while (par != NULL)
  {
    uint plen,left_len = (uint)(end-to)+1;
    if (!par) par = (char*)"(null)";
    plen = (uint) strlen(par);
    if (left_len <= plen)
    plen = left_len - 1;
    to=strnmov(to+strlen(to),par,plen);
    par = va_arg(args, char *);
  }
  va_end(args);
  DBUG_ASSERT(to <= end);
  *to='\0';
  return (uint) (to - start);
}

int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error)
{
  char *cl;
  char *arg;
  intptr_t result;
  int j;
  int err;
  STARTUPINFO startup_info;
  PROCESS_INFORMATION process_information;
  ULONG dosretval;
  int retval;
  DWORD exit_code;
  SECURITY_ATTRIBUTES process_attributes, thread_attributes;
  char command_line[1024]= "";


  memset(&startup_info,0,sizeof(STARTUPINFO));
  startup_info.cb = sizeof(STARTUPINFO);

  if (input)
    freopen(input, "rb", stdin);

  if (output)
    freopen(output, "wb", stdout);

  if (error)
    freopen(error, "wb", stderr);

  result= CreateProcess(
    path,
    &win_args,
    NULL,
    NULL,
    TRUE,
    0,
    NULL,
    NULL,
    &startup_info,
    &process_information
  );

  if (process_information.hProcess)
  {
    if (join)
    {
      if (WaitForSingleObject(process_information.hProcess, mysqld_timeout) == WAIT_TIMEOUT)
      {
        exit_code= -1;
      }
      else
      {
        GetExitCodeProcess(process_information.hProcess, &exit_code);
      }
      CloseHandle(process_information.hProcess);
    }
    else
    {
      exit_code= 0;
    }
    if (run_server)
    {
      switch (pid_mode)
      {
      case MASTER_PID:
        master_server= process_information.hProcess;
        break;
      case SLAVE_PID:
        slave_server= process_information.hProcess;
        break;
      };
      pid_mode= 0;
      run_server= FALSE;
    };
  }
  else
  {
    exit_code= -1;
  }
  if (input)
    freopen("CONIN$","rb",stdin);
  if (output)
    freopen("CONOUT$","wb",stdout);
  if (error)
    freopen("CONOUT$","wb",stderr);

  return exit_code;
}

#else /* NOT __NETWARE__, NOT __WIN__ */

/* This assumes full POSIX.1 compliance */
int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error)
{
  int result = 0;
  pid_t pid;

  if ((pid = fork()))
  {
    /* Remains in parent process */
    if (join && (pid != -1))
      waitpid(pid, &result, 0);
  }
  else
  {
    /* Child process */

    /* Reassign streams */
    if (input)
      freopen(input, "r", stdin);

    if (output)
      freopen(output, "w", stdout);

    if (error)
      freopen(error, "w", stderr);

    /* Spawn the process */
    execve(path, al->argv, environ);
  }

  return result;
}

#endif /* __WIN__ */

#endif /* __NETWARE__ */

/******************************************************************************

  stop_server()
  
  Stop the server with the given port and pid file.

******************************************************************************/
int stop_server(char *bin_dir, char *user, char *password, int port,
                char *pid_file)
{
  arg_list_t al;
  int err;
  char trash[PATH_MAX];
  
  my_snprintf(trash, PATH_MAX, "/tmp/trash.out");

  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqladmin_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--port=%u", port);
  add_arg(&al, "--user=%s", user);
  add_arg(&al, "--password=%s", password);
  add_arg(&al, "-O");
  add_arg(&al, "shutdown_timeout=20");
  add_arg(&al, "shutdown");

  /* spawn */
  if ((err = spawn(mysqladmin_file, &al, TRUE, NULL,
                  trash, NULL)) == 0)
  {
    sleep_until_file_deleted(pid_file);
  }
  else
  {
    pid_t pid = get_server_pid(pid_file);
    
    /* shutdown failed - kill server */
    kill_server(pid);
  
    sleep(TRY_MAX);
    
    /* remove pid file if possible */
    err = my_delete(pid_file, MYF(MY_WME));
  }
  
  /* free args */
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
  int err;
  File fd;
  char *p;
  pid_t id = 0;

  /* discover id */
  fd = my_open(pid_file, O_RDONLY, MYF(MY_WME));

  err = my_read(fd, buf, PATH_MAX, MYF(MY_WME));

  my_close(fd, MYF(MY_WME));

  if (err > 0)
  {
    /* terminate string */
    if ((p = strchr(buf, '\n')) != NULL)
    {
      *p = '\0';

      /* check for a '\r' */
      if ((p = strchr(buf, '\r')) != NULL)
      {
      *p = '\0';
      }
    }
    else
    {
      buf[err] = '\0';
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

#if !defined(__NETWARE__)
    /* Send SIGTERM to pid */
    kill(pid, SIGTERM);
#else /* __NETWARE__ */
    /* destroy vm */
    NXVmDestroy(pid);

#endif 

  }
}

/******************************************************************************

  del_tree()
  
  Delete the directory and subdirectories.

******************************************************************************/
void del_tree(char *dir)
{
  MY_DIR *current;
  uint i;
  char temp[PATH_MAX];
  
  current = my_dir(dir, MYF(MY_WME | MY_WANT_STAT));
  
  /* current is NULL if dir does not exist */
  if (current == NULL)
    return;
  
  for (i = 0; i < current->number_off_files; i++)
  {
    /* create long name */
    my_snprintf(temp, PATH_MAX, "%s/%s", dir, current->dir_entry[i].name);
    
    if (current->dir_entry[i].name[0] == '.')
    {
      /* Skip */
    }
    else if (MY_S_ISDIR(current->dir_entry[i].mystat.st_mode))
    {
      /* delete subdirectory */
      del_tree(temp);
    }
    else
    {
      /* remove file */
      my_delete(temp, MYF(MY_WME));
    }
  }

  my_dirend(current);

  /* remove directory */
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

  my_vsnprintf(path, PATH_MAX, format, ap);
  
  va_end(ap);
#ifdef __WIN__
  {
    MY_DIR *current; 
    uint i;
    struct _finddata_t find;
    char temp[PATH_MAX];
#ifdef _WIN64
    __int64 handle;
#else
    long handle;
#endif
    char *p;

    p= strrchr(path,'\\');
    if (p == NULL)
    {
      p= strrchr(path,'/');
      if (p == NULL)
        p= &path;      
      else
        p++;
    }
    else
      p++;
  
    if ((handle=_findfirst(path,&find)) == -1L)
      return 0;
    do
    {
      strcpy(p,find.name);
      my_delete(path, MYF(MY_WME));
    } while (!_findnext(handle,&find));
    _findclose(handle);
  }
#else
  return my_delete(path, MYF(MY_WME));
#endif
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

  strcpy(temp, argv0);
#ifndef __WIN__
  casedn_str(temp);
#endif
  while((p = strchr(temp, '\\')) != NULL) *p = '/';

  if ((p = strstr(temp, "/bin/")) != NullS)
  {
    *p = '\0';
    strcpy(basedir, temp);
  }
}

