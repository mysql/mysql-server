/* Copyright (c) 2007, 2011, Oracle and/or its affiliates. All rights reserved.

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
  Utility program that encapsulates process creation, monitoring
  and bulletproof process cleanup

  Usage:
    safe_process [options to safe_process] -- progname arg1 ... argn

  To safeguard mysqld you would invoke safe_process with a few options
  for safe_process itself followed by a double dash to indicate start
  of the command line for the program you really want to start

  $> safe_process --output=output.log -- mysqld --datadir=var/data1 ...

  This would redirect output to output.log and then start mysqld,
  once it has done that it will continue to monitor the child as well
  as the parent.

  The safe_process then checks the follwing things:
  1. Child exits, propagate the childs return code to the parent
     by exiting with the same return code as the child.

  2. Parent dies, immediately kill the child and exit, thus the
     parent does not need to properly cleanup any child, it is handled
     automatically.

  3. Signal's recieced by the process will trigger same action as 2)

  4. The named event "safe_process[pid]" can be signaled and will
     trigger same action as 2)

  WARNING! Be careful when using ProcessExplorer, since it will open
           a handle to each process(and maybe also the Job), the process
           spawned by safe_process will not be closed off when safe_process
           is killed.
*/

#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <signal.h>
#include <stdlib.h>

static int verbose= 0;
static char safe_process_name[32]= {0};

static void message(const char* fmt, ...)
{
  if (!verbose)
    return;
  va_list args;
  fprintf(stderr, "%s: ", safe_process_name);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  fflush(stderr);
}


static void die(const char* fmt, ...)
{
  DWORD last_err= GetLastError();
  va_list args;
  fprintf(stderr, "%s: FATAL ERROR, ", safe_process_name);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  if (last_err)
  {
    char *message_text;
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER
        |FORMAT_MESSAGE_IGNORE_INSERTS, NULL, last_err , 0, (LPSTR)&message_text,
        0, NULL))
    {
      fprintf(stderr,"error: %d, %s\n",last_err, message_text);
      LocalFree(message_text);
    }
    else
    {
      /* FormatMessage failed, print error code only */
      fprintf(stderr,"error:%d\n", last_err);
    }
  }
  fflush(stderr);
  exit(1);
}


DWORD get_parent_pid(DWORD pid)
{
  HANDLE snapshot;
  DWORD parent_pid= -1;
  PROCESSENTRY32 pe32;
  pe32.dwSize= sizeof(PROCESSENTRY32);

  snapshot= CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    die("CreateToolhelp32Snapshot failed");

  if (!Process32First(snapshot, &pe32))
  {
    CloseHandle(snapshot);
    die("Process32First failed");
  }

  do
  {
    if (pe32.th32ProcessID == pid)
      parent_pid= pe32.th32ParentProcessID;
  } while(Process32Next( snapshot, &pe32));
  CloseHandle(snapshot);

  if (parent_pid == -1)
    die("Could not find parent pid");

  return parent_pid;
}


enum {
  PARENT,
  CHILD,
  EVENT,
  NUM_HANDLES
};


HANDLE shutdown_event;
void handle_signal (int signal)
{
  message("Got signal: %d", signal);
  if(SetEvent(shutdown_event) == 0) {
    /* exit safe_process and (hopefully) kill off the child */
    die("Failed to SetEvent");
  }
}


int main(int argc, const char** argv )
{
  char child_args[4096]= {0};
  DWORD pid= GetCurrentProcessId();
  DWORD parent_pid= get_parent_pid(pid);
  HANDLE job_handle;
  HANDLE wait_handles[NUM_HANDLES]= {0};
  PROCESS_INFORMATION process_info= {0};
  BOOL nocore= FALSE;

  sprintf(safe_process_name, "safe_process[%d]", pid);

  /* Create an event for the signal handler */
  if ((shutdown_event=
       CreateEvent(NULL, TRUE, FALSE, safe_process_name)) == NULL)
    die("Failed to create shutdown_event");
  wait_handles[EVENT]= shutdown_event;

  signal(SIGINT,   handle_signal);
  signal(SIGBREAK, handle_signal);
  signal(SIGTERM,  handle_signal);

  message("Started");

  /* Parse arguments */
  for (int i= 1; i < argc; i++) {
    const char* arg= argv[i];
	char* to= child_args;
    if (strcmp(arg, "--") == 0 && strlen(arg) == 2) {
      /* Got the "--" delimiter */
      if (i >= argc)
        die("No real args -> nothing to do");
      /* Copy the remaining args to child_arg */
      for (int j= i+1; j < argc; j++) {
        arg= argv[j];
        if (strchr (arg, ' ') &&
            arg[0] != '\"' &&
            arg[strlen(arg)] != '\"')
        {
          /* Quote arg that contains spaces and are not quoted already */
          to+= _snprintf(to, child_args + sizeof(child_args) - to,
                         "\"%s\" ", arg);
        }
        else
        {
          to+= _snprintf(to, child_args + sizeof(child_args) - to,
          "%s ", arg);
        }
      }
      break;
    } else {
      if (strcmp(arg, "--verbose") == 0)
        verbose++;
      else if (strncmp(arg, "--parent-pid", 10) == 0)
      {
            /* Override parent_pid with a value provided by user */
        const char* start;
        if ((start= strstr(arg, "=")) == NULL)
          die("Could not find start of option value in '%s'", arg);
        start++; /* Step past = */
        if ((parent_pid= atoi(start)) == 0)
          die("Invalid value '%s' passed to --parent-id", start);
      }
      else if (strcmp(arg, "--nocore") == 0)
      {
        nocore= TRUE;
      }
      else if ( strncmp (arg, "--env ", 6) == 0 )
      {
	putenv(strdup(arg+6));
      }
      else
        die("Unknown option: %s", arg);
    }
  }
  if (*child_args == '\0')
    die("nothing to do");

  /* Open a handle to the parent process */
  message("parent_pid: %d", parent_pid);
  if (parent_pid == pid)
    die("parent_pid is equal to own pid!");

  if ((wait_handles[PARENT]=
       OpenProcess(SYNCHRONIZE, FALSE, parent_pid)) == NULL)
    die("Failed to open parent process with pid: %d", parent_pid);

  /* Create the child process in a job */
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
  STARTUPINFO                          si   = { 0 };
  si.cb = sizeof(si);

  /*
    Create the job object to make it possible to kill the process
    and all of it's children in one go
  */
  if ((job_handle= CreateJobObject(NULL, NULL)) == NULL)
    die("CreateJobObject failed");

  /*
    Make all processes associated with the job terminate when the
    last handle to the job is closed.
  */
#ifndef JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE  0x00002000
#endif

  jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation,
                              &jeli, sizeof(jeli)) == 0)
    message("SetInformationJobObject failed, continue anyway...");

				/* Avoid popup box */
  if (nocore)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
                 | SEM_NOOPENFILEERRORBOX);

#if 0
  /* Setup stdin, stdout and stderr redirect */
  si.dwFlags= STARTF_USESTDHANDLES;
  si.hStdInput= GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput= GetStdHandle(STD_OUTPUT_HANDLE);
  si.hStdError= GetStdHandle(STD_ERROR_HANDLE);
#endif

  /*
    Create the process suspended to make sure it's assigned to the
    Job before it creates any process of it's own

    Allow the new process to break away from any job that this
    process is part of so that it can be assigned to the new JobObject
    we just created. This is safe since the new JobObject is created with
    the JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flag, making sure it will be
    terminated when the last handle to it is closed(which is owned by
    this process).

    If breakaway from job fails on some reason, fallback is to create a
    new process group. Process groups also allow to kill process and its 
    descedants, subject to some restrictions (processes have to run within
    the same console,and must not ignore CTRL_BREAK)
  */
  DWORD create_flags[]= {CREATE_BREAKAWAY_FROM_JOB, CREATE_NEW_PROCESS_GROUP, 0};
  BOOL process_created= FALSE;
  BOOL jobobject_assigned= FALSE;

  for (int i=0; i < sizeof(create_flags)/sizeof(create_flags[0]); i++)
  { 
    process_created= CreateProcess(NULL, (LPSTR)child_args,
                    NULL,
                    NULL,
                    TRUE, /* inherit handles */
                    CREATE_SUSPENDED | create_flags[i],
                    NULL,
                    NULL,
                    &si,
                    &process_info);
    if (process_created)
    {
     jobobject_assigned= AssignProcessToJobObject(job_handle, process_info.hProcess);
     break;
    }
  }

  if (!process_created)
  {
    die("CreateProcess failed");
  }
  ResumeThread(process_info.hThread);
  CloseHandle(process_info.hThread);

  wait_handles[CHILD]= process_info.hProcess;

  message("Started child %d", process_info.dwProcessId);

  /* Monitor loop */
  DWORD child_exit_code= 1;
  DWORD wait_res= WaitForMultipleObjects(NUM_HANDLES, wait_handles,
                                         FALSE, INFINITE);
  switch (wait_res)
  {
    case WAIT_OBJECT_0 + PARENT:
      message("Parent exit");
      break;
    case WAIT_OBJECT_0 + CHILD:
      if (GetExitCodeProcess(wait_handles[CHILD], &child_exit_code) == 0)
        message("Child exit: could not get exit_code");
      else
        message("Child exit: exit_code: %d", child_exit_code);
      break;
    case WAIT_OBJECT_0 + EVENT:
      message("Wake up from shutdown_event");
      break;

    default:
      message("Unexpected result %d from WaitForMultipleObjects", wait_res);
      break;
  }
  message("Exiting, child: %d", process_info.dwProcessId);

  if (TerminateJobObject(job_handle, 201) == 0)
    message("TerminateJobObject failed");
  CloseHandle(job_handle);
  message("Job terminated and closed");

  if (!jobobject_assigned)
  {
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process_info.dwProcessId);
    TerminateProcess(process_info.hProcess, 202);
  }

  if (wait_res != WAIT_OBJECT_0 + CHILD)
  {
    /* The child has not yet returned, wait for it */
    message("waiting for child to exit");
    if ((wait_res= WaitForSingleObject(wait_handles[CHILD], INFINITE))
        != WAIT_OBJECT_0)
    {
      message("child wait failed: %d", wait_res);
    }
    else
    {
      message("child wait succeeded");
    }
    /* Child's exit code should now be 201, no need to get it */
  }

  message("Closing handles");
  for (int i= 0; i < NUM_HANDLES; i++)
    CloseHandle(wait_handles[i]);

  message("Exiting, exit_code: %d", child_exit_code);
  exit(child_exit_code);
}

