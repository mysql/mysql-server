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
#ifndef __WIN__
#include <dirent.h>
#endif
#include <string.h>
#ifdef __NETWARE__
#include <screen.h>
#include <proc.h>
#else
#include <sys/types.h>
#ifndef __WIN__
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fnmatch.h>                            /* FIXME HAVE_FNMATCH_H or something */
#else
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#endif
#endif
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "my_manage.h"

#ifndef __NETWARE__
#define ASSERT assert
extern char **environ;
#endif



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

  al->argc= 0;
  al->size= ARG_BUF;
  al->argv= malloc(al->size * sizeof(char *));
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
  char temp[FN_REFLEN];

  ASSERT(al != NULL);

  /* increase size */
  if (al->argc >= (int)al->size)
  {
    al->size+= ARG_BUF;
    al->argv= realloc(al->argv, al->size * sizeof(char *));
    ASSERT(al->argv != NULL);
  }

  if (format)
  {
    va_start(ap, format);
    vsprintf(temp, format, ap);
    va_end(ap);

    al->argv[al->argc]= malloc(strlen(temp)+1);
    ASSERT(al->argv[al->argc] != NULL);
    strcpy(al->argv[al->argc], temp);
    
    ++(al->argc);
  }
  else
  {
    al->argv[al->argc]= NULL;
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

  for (i= 0; i < al->argc; i++)
  {
    ASSERT(al->argv[i] != NULL);
    free(al->argv[i]);
    al->argv[i]= NULL;
  }

  free(al->argv);
  al->argc= 0;
  al->argv= NULL;

  return;
}

/******************************************************************************

        sleep_until_file_deleted()

        Sleep until the given file is no longer found.

******************************************************************************/

#ifndef __WIN__
int sleep_until_file_deleted(char *pid_file)
#else
int sleep_until_file_deleted(HANDLE pid_file)
#endif
{
  int err= 0;            /* Initiate to supress warning */
#ifndef __WIN__
  struct stat buf;
  int i;

  for (i= 0; (i < TRY_MAX) && (err= !stat(pid_file, &buf)); i++) sleep(1);

  if (err != 0) err= errno;
#else
  err= (WaitForSingleObject(pid_file, TRY_MAX*1000) == WAIT_TIMEOUT);
#endif
  return err;
}

/******************************************************************************

        sleep_until_file_exists()

        Sleep until the given file exists.

******************************************************************************/

#ifndef __WIN__
int sleep_until_file_exists(char *pid_file)
#else
int sleep_until_file_exists(HANDLE pid_file)
#endif
{
  int err= 0;            /* Initiate to supress warning */
#ifndef __WIN__
  struct stat buf;
  int i;

  for (i= 0; (i < TRY_MAX) && (err= stat(pid_file, &buf)); i++) sleep(1);

  if (err != 0) err= errno;
#else
  err= (WaitForSingleObject(pid_file, TRY_MAX*1000) == WAIT_TIMEOUT);
#endif
  return err;
}

/******************************************************************************

        wait_for_server_start()

        Wait for the server on the given port to start.

******************************************************************************/

int wait_for_server_start(char *bin_dir __attribute__((unused)),
                          char *mysqladmin_file,
                          char *user, char *password, int port,char *tmp_dir)
{
  arg_list_t al;
  int err= 0;
#ifndef __WIN__
  int i;
#endif
  char trash[FN_REFLEN];

  /* mysqladmin file */
  snprintf(trash, FN_REFLEN, "%s/trash.out",tmp_dir);

  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqladmin_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--port=%u", port);
  add_arg(&al, "--user=%s", user);
  add_arg(&al, "--password=%s", password);
  add_arg(&al, "--silent");

/* #ifdef NOT_USED */
#ifndef __NETWARE__
  add_arg(&al, "-O");
  add_arg(&al, "connect_timeout=10");
  add_arg(&al, "-w");
#endif

  add_arg(&al, "--host=localhost");
#ifndef __NETWARE__
  add_arg(&al, "--protocol=tcp");
#endif
  add_arg(&al, "ping");

  /*
    NetWare does not support the connect timeout in the TCP/IP stack
    -- we will try the ping multiple times
  */
#ifndef __WIN__
  for (i= 0; (i < TRY_MAX)
         && (err= spawn(mysqladmin_file, &al, TRUE, NULL,
                        trash, NULL, NULL)); i++) sleep(1);
#else
  err= spawn(mysqladmin_file, &al, TRUE, NULL,trash, NULL, NULL);
#endif

  /* free args */
  free_args(&al);

  return err;
}

/******************************************************************************

        spawn()

        Spawn the given path with the given arguments.

******************************************************************************/

#ifdef __NETWARE__
int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error, char *pid_file)
{
  pid_t pid;
  int result= 0;
  wiring_t wiring= { FD_UNUSED, FD_UNUSED, FD_UNUSED };
  unsigned long flags= PROC_CURRENT_SPACE | PROC_INHERIT_CWD;

  /* open wiring */
  if (input)
    wiring.infd= open(input, O_RDONLY);

  if (output)
    wiring.outfd= open(output, O_WRONLY | O_CREAT | O_TRUNC);

  if (error)
    wiring.errfd= open(error, O_WRONLY | O_CREAT | O_TRUNC);

  /* procve requires a NULL */
  add_arg(al, NULL);

  /* go */
  pid= procve(path, flags, NULL, &wiring, NULL, NULL, 0,
              NULL, (const char **)al->argv);

  /* close wiring */
  if (wiring.infd != -1)
    close(wiring.infd);

  if (wiring.outfd != -1)
    close(wiring.outfd);

  if (wiring.errfd != -1)
    close(wiring.errfd);

  return result;
}
#elif __WIN__

int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error, HANDLE *pid)
{
  bool result;
  int i;
  STARTUPINFO startup_info;
  PROCESS_INFORMATION process_information;
  DWORD exit_code;
  char win_args[1024]= "";

  /* Skip the first parameter */
  for (i= 1; i < al->argc; i++)
  {
    ASSERT(al->argv[i] != NULL);
    strcat(win_args,al->argv[i]);
    strcat(win_args," ");
  }

  memset(&startup_info,0,sizeof(STARTUPINFO));
  startup_info.cb= sizeof(STARTUPINFO);

  if (input)
    freopen(input, "rb", stdin);

  if (output)
    freopen(output, "wb", stdout);

  if (error)
    freopen(error, "wb", stderr);

  result= CreateProcess(
    path,
    (LPSTR)&win_args,
    NULL,
    NULL,
    TRUE,
    0,
    NULL,
    NULL,
    &startup_info,
    &process_information
  );

  if (result && process_information.hProcess)
  {
    if (join)
    {
      if (WaitForSingleObject(process_information.hProcess, mysqld_timeout)
          == WAIT_TIMEOUT)
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
    if (pid != NULL)
      *pid= process_information.hProcess;
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
#else
int spawn(char *path, arg_list_t *al, int join, char *input,
          char *output, char *error, char *pid_file __attribute__((unused)))
{
  pid_t pid;
  int res_exec= 0;
  int result= 0;

  pid= fork();

  if (pid == -1)
  {
    fprintf(stderr, "fork was't created\n");
    /* We can't create the fork...exit with error */
    return EXIT_FAILURE;
  }

  if (pid  > 0)
  {
    /* The parent process is waiting for child process if join is not zero */
    if (join)
    {
      waitpid(pid, &result, 0);
      if (WIFEXITED(result) != 0)
      {
        result= WEXITSTATUS(result);
      }
      else
      {
        result= EXIT_FAILURE;
      }
    }
  }
  else
  {

    /* Child process */
    add_arg(al, NULL);

    /* Reassign streams */
    if (input)
      freopen(input, "r", stdin);

    if (output)
      freopen(output, "w", stdout);

    if (error)
      freopen(error, "w", stderr);

    /* Spawn the process */
    if ((res_exec= execve(path, al->argv, environ)) < 0)
      exit(EXIT_FAILURE);

    /* Restore streams */
    if (input)
      freopen("/dev/tty", "r", stdin);

    if (output)
      freopen("/dev/tty", "w", stdout);

    if (error)
      freopen("/dev/tty", "w", stderr);

    exit(0);
  }

  return result;
}
#endif
/******************************************************************************

        stop_server()

        Stop the server with the given port and pid file.

******************************************************************************/

int stop_server(char *bin_dir __attribute__((unused)), char *mysqladmin_file,
                char *user, char *password, int port,
#ifndef __WIN__
                char *pid_file,
#else
                HANDLE pid_file,
#endif
                char *tmp_dir)
{
  arg_list_t al;
  int err= 0;
  char trash[FN_REFLEN];

  snprintf(trash, FN_REFLEN, "%s/trash.out",tmp_dir);

  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqladmin_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--port=%u", port);
  add_arg(&al, "--user=%s", user);
  add_arg(&al, "--password=%s", password);
  add_arg(&al, "-O");
  add_arg(&al, "shutdown_timeout=20");
#ifndef __NETWARE__
  add_arg(&al, "--protocol=tcp");
#endif
  add_arg(&al, "shutdown");

  /* spawn */
  if ((err= spawn(mysqladmin_file, &al, TRUE, NULL,
                  trash, NULL, NULL)) == 0)
  {
    sleep_until_file_deleted(pid_file);
  }
  else
  {
#ifndef __WIN__
    pid_t pid= get_server_pid(pid_file);

    /* shutdown failed - kill server */
   kill_server(pid);

   sleep(TRY_MAX);

   /* remove pid file if possible */
   err= remove(pid_file);
#else
  TerminateProcess(pid_file,err);
#endif
  }

  /* free args */
  free_args(&al);

  return err;
}

/******************************************************************************

        get_server_pid()

        Get the VM id with the given pid file.

******************************************************************************/

#ifndef __WIN__
pid_t get_server_pid(char *pid_file)
{
  char buf[FN_REFLEN];
  int fd, err;
  char *p;
  pid_t id= 0;

  /* discover id */
  fd= open(pid_file, O_RDONLY);

  err= read(fd, buf, FN_REFLEN);

  close(fd);

  if (err > 0)
  {
    /* terminate string */
    if ((p= strchr(buf, '\n')) != NULL)
    {
      *p= '\0';

      /* check for a '\r' */
      if ((p= strchr(buf, '\r')) != NULL)
      {
        *p= '\0';
      }
    }
    else
    {
      buf[err]= '\0';
    }

    id= strtol(buf, NULL, 0);
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
#endif
/******************************************************************************

        del_tree()

        Delete the directory and subdirectories.

******************************************************************************/

void del_tree(char *dir)
{
#ifndef __WIN__
  DIR *parent= opendir(dir);
  struct dirent *entry;
  char temp[FN_REFLEN];

  if (parent == NULL)
  {
    return;
  }

  while ((entry= readdir(parent)) != NULL)
  {
    /* create long name */
    snprintf(temp, FN_REFLEN, "%s/%s", dir, entry->d_name);

    if (entry->d_name[0] == '.')
    {
      /* Skip */
    }
    else
    {
/* FIXME missing test in acinclude.m4 */
#ifndef STRUCT_DIRENT_HAS_D_TYPE
      struct stat st;

      if (lstat(entry->d_name, &st) == -1)
      {
        /* FIXME error */
        return;  
      }
      if (S_ISDIR(st.st_mode))
#else
      if (S_ISDIR(entry->d_type))
#endif
      {
        /* delete subdirectory */
        del_tree(temp);
      }
      else
      {
        /* remove file */
        remove(temp);
      }
    }
  }
  /* remove directory */
  rmdir(dir);
#else
  struct _finddata_t parent;
#if defined(_MSC_VER) && _MSC_VER > 1200
  intptr_t handle;
#else
  long handle;
#endif  /* _MSC_VER && _MSC_VER > 1200  */ 
  char temp[FN_REFLEN];
  char mask[FN_REFLEN];

  snprintf(mask,FN_REFLEN,"%s/*.*",dir);

  if ((handle=_findfirst(mask,&parent)) == -1L)
  {
    return;
  }

  do
  {
    /* create long name */
    snprintf(temp, FN_REFLEN, "%s/%s", dir, parent.name);
    if (parent.name[0] == '.')
    {
      /* Skip */
    }
    else
    if (parent.attrib & _A_SUBDIR)
    {
      /* delete subdirectory */
      del_tree(temp);
    }
    else
    {
      /* remove file */
      remove(temp);
    }
  } while (_findnext(handle,&parent) == 0);

   _findclose(handle);

   /* remove directory */
   _rmdir(dir);
#endif
}

/******************************************************************************

        removef()

******************************************************************************/

int removef(const char *format, ...)
{
#ifdef __NETWARE__
  va_list ap;
  char path[FN_REFLEN];

  va_start(ap, format);

  vsnprintf(path, FN_REFLEN, format, ap);

  va_end(ap);
  return remove(path);

#elif __WIN__
  {
    va_list ap;
    char path[FN_REFLEN];
    struct _finddata_t parent;
#if defined(_MSC_VER) && _MSC_VER > 1200
    intptr_t handle;
#else
    long handle;
#endif  /* _MSC_VER && _MSC_VER > 1200  */ 
    char temp[FN_REFLEN];
    char *p;

    va_start(ap, format);

    vsnprintf(path, FN_REFLEN, format, ap);

    va_end(ap);

    p= path + strlen(path);
    while (*p != '\\' && *p != '/' && p > path) p--;

    if ((handle=_findfirst(path,&parent)) == -1L)
    {
      /* if there is not files....it's ok */
      return 0;
    }

    *p= '\0';

    do
    {
      if (! (parent.attrib & _A_SUBDIR))
      {
        snprintf(temp, FN_REFLEN, "%s/%s", path, parent.name);
        remove(temp);
      }
    }while (_findnext(handle,&parent) == 0);

    _findclose(handle);
  }
#else
  DIR *parent;
  struct dirent *entry;
  char temp[FN_REFLEN];
  va_list ap;
  char path[FN_REFLEN];
  char *p;
  /* Get path with mask */
  va_start(ap, format);

  vsnprintf(path, FN_REFLEN, format, ap);

  va_end(ap);

  p= path + strlen(path);
  while (*p != '\\' && *p != '/' && p > path) p--;
  *p= '\0';
  p++;

  parent= opendir(path);

  if (parent == NULL)
  {
    return 1;            /* Error, directory missing */
  }

  while ((entry= readdir(parent)) != NULL)
  {
    /* entry is not directory and entry matches with mask */
#ifndef STRUCT_DIRENT_HAS_D_TYPE
    struct stat st;

    /* create long name */
    snprintf(temp, FN_REFLEN, "%s/%s", path, entry->d_name);

    if (lstat(temp, &st) == -1)
    {
      return 1;  /* Error couldn't lstat file */
    }

    if (!S_ISDIR(st.st_mode) && !fnmatch(p, entry->d_name,0))
#else
    if (!S_ISDIR(entry->d_type) && !fnmatch(p, entry->d_name,0))
#endif
    {
      /* create long name */
      snprintf(temp, FN_REFLEN, "%s/%s", path, entry->d_name);
      /* Delete only files */
      remove(temp);
    }
  }
#endif
  return 0;
}

/******************************************************************************

        get_basedir()

******************************************************************************/

void get_basedir(char *argv0, char *basedir)
{
  char temp[FN_REFLEN];
  char *p;
  int position;

  ASSERT(argv0 != NULL);
  ASSERT(basedir != NULL);

  strcpy(temp, strlwr(argv0));
  while ((p= strchr(temp, '\\')) != NULL) *p= '/';

  if ((position= strinstr(temp, "/bin/")) != 0)
  {
    p= temp + position;
    *p= '\0';
    strcpy(basedir, temp);
  }
}

uint strinstr(reg1 const char *str,reg4 const char *search)
{
  reg2 my_string i,j;
  my_string start= (my_string) str;

 skipp:
  while (*str != '\0')
  {
    if (*str++ == *search)
    {
      i=(my_string) str;
      j= (my_string) search+1;
      while (*j)
        if (*i++ != *j++) goto skipp;
      return ((uint) (str - start));
    }
  }
  return (0);
}

/******************************************************************************

        remove_empty_file()

******************************************************************************/

void remove_empty_file(const char *file_name)
{
  struct stat file;

  if (!stat(file_name,&file))
  {
    if (!file.st_size)
      remove(file_name);
  }
}
