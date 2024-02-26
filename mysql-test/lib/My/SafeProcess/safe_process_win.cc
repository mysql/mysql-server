// Copyright (c) 2000, 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @file safe_process_win.cc
///
/// Utility program that encapsulates process creation, monitoring
/// and bulletproof process cleanup.
///
/// Usage:
///   safe_process [options to safe_process] -- progname arg1 ... argn
///
/// To safeguard mysqld you would invoke safe_process with a few options
/// for safe_process itself followed by a double dash to indicate start
/// of the command line for the program you really want to start.
///
/// $> safe_process --output=output.log -- mysqld --datadir=var/data1 ...
///
/// This would redirect output to output.log and then start mysqld,
/// once it has done that it will continue to monitor the child as well
/// as the parent.
///
/// The safe_process then checks the following things:
/// 1. Child exits, propagate the child's return code to the parent
///    by exiting with the same return code as the child.
///
/// 2. Parent dies, immediately kill the child and exit, thus the
///    parent does not need to properly cleanup any child, it is handled
///    automatically.
///
/// 3. Signal's recieced by the process will trigger same action as 2)
///
/// 4. The named event "safe_process[pid]" can be signaled and will
///    trigger same action as 2)
///
/// WARNING
///   Be careful when using ProcessExplorer, since it will open a handle
///   to each process(and maybe also the Job), the process spawned by
///   safe_process will not be closed off when safe_process is killed.

#include <signal.h>
#include <windows.h>

// Needs to be included after <windows.h>.
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>
#include <string>

enum { PARENT, CHILD, EVENT, NUM_HANDLES };
HANDLE shutdown_event;

static char safe_process_name[32];
static int verbose = 0;

static void message(const char *fmt, ...) {
  if (!verbose) return;
  va_list args;
  std::fprintf(stderr, "%s: ", safe_process_name);
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  std::fprintf(stderr, "\n");
  va_end(args);
  fflush(stderr);
}

[[noreturn]] static void die(const char *fmt, ...) {
  DWORD last_err = GetLastError();

  va_list args;
  std::fprintf(stderr, "%s: FATAL ERROR, ", safe_process_name);
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  std::fprintf(stderr, "\n");
  va_end(args);

  if (last_err) {
    char *message_text;
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, last_err, 0, (LPSTR)&message_text, 0, NULL)) {
      std::fprintf(stderr, "error: %lu, %s\n", (unsigned long)last_err,
                   message_text);
      LocalFree(message_text);
    } else {
      // FormatMessage failed, print error code only
      std::fprintf(stderr, "error:%lu\n", (unsigned long)last_err);
    }
  }

  fflush(stderr);
  std::exit(1);
}

DWORD get_parent_pid(DWORD pid) {
  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) die("CreateToolhelp32Snapshot failed");

  if (!Process32First(snapshot, &pe32)) {
    CloseHandle(snapshot);
    die("Process32First failed");
  }

  DWORD parent_pid = -1;
  do {
    if (pe32.th32ProcessID == pid) parent_pid = pe32.th32ParentProcessID;
  } while (Process32Next(snapshot, &pe32));

  CloseHandle(snapshot);

  if (parent_pid == static_cast<DWORD>(-1)) die("Could not find parent pid");

  return parent_pid;
}

void handle_signal(int signal) {
  message("Got signal: %d", signal);
  if (SetEvent(shutdown_event) == 0) {
    // exit safe_process and (hopefully) kill off the child
    die("Failed to SetEvent");
  }
}

/** Sets the append flag (FILE_APPEND_DATA) so that the handle inherited by the
 * child process will be in append mode. This is in contrast to the C runtime
 * flag that is set by f(re)open methods and is not communicated to OS, i.e. is
 * local to process and is lost in the context of the child process. This method
 * allows several processes to append a common file concurrently, without the
 * safe_process child overwriting what others append.
 * A bug to MSVCRT was submitted:
 https://developercommunity.visualstudio.com/content/problem/921279/createprocess-does-not-inherit-fappend-flags-set-b.html
 */
void fix_file_append_flag_inheritance(DWORD std_handle) {
  HANDLE old_handle = GetStdHandle(std_handle);
  HANDLE new_handle = ReOpenFile(old_handle, FILE_APPEND_DATA,
                                 FILE_SHARE_WRITE | FILE_SHARE_READ, 0);
  if (new_handle != INVALID_HANDLE_VALUE) {
    SetHandleInformation(new_handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetStdHandle(std_handle, new_handle);
    CloseHandle(old_handle);
  }
}

int main(int argc, const char **argv) {
  DWORD pid = GetCurrentProcessId();
  sprintf(safe_process_name, "safe_process[%lu]", (unsigned long)pid);

  // Create an event for the signal handler
  if ((shutdown_event = CreateEvent(NULL, TRUE, FALSE, safe_process_name)) ==
      NULL)
    die("Failed to create shutdown_event");

  HANDLE wait_handles[NUM_HANDLES] = {0};
  wait_handles[EVENT] = shutdown_event;

  signal(SIGINT, handle_signal);
  signal(SIGBREAK, handle_signal);
  signal(SIGTERM, handle_signal);

  message("Started");

  BOOL nocore = FALSE;
  DWORD parent_pid = get_parent_pid(pid);
  char child_args[8192] = {0};
  std::string exe_name;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    char *to = child_args;

    if (strcmp(arg, "--") == 0 && strlen(arg) == 2) {
      // Got the "--" delimiter
      if (i >= argc) die("No real args -> nothing to do");

      // Copy the remaining args to child_arg
      for (int j = i + 1; j < argc; j++) {
        arg = argv[j];
        if (strchr(arg, ' ') && arg[0] != '\"' && arg[strlen(arg)] != '\"') {
          // Quote arg that contains spaces and are not quoted already
          to += std::snprintf(to, child_args + sizeof(child_args) - to,
                              "\"%s\" ", arg);
        } else {
          to += std::snprintf(to, child_args + sizeof(child_args) - to, "%s ",
                              arg);
        }
      }

      // Check if executable is mysqltest.exe client
      if (exe_name.compare("mysqltest") == 0) {
        char safe_process_pid[32];

        // Pass safeprocess PID to mysqltest which is used to create an event
        std::sprintf(safe_process_pid, "--safe-process-pid=%lu",
                     (unsigned long)pid);
        to += std::snprintf(to, child_args + sizeof(child_args) - to, "%s ",
                            safe_process_pid);
      }
      break;
    } else {
      if (strcmp(arg, "--verbose") == 0)
        verbose++;
      else if (strncmp(arg, "--parent-pid", 10) == 0) {
        // Override parent_pid with a value provided by user
        const char *start;
        if ((start = strstr(arg, "=")) == NULL)
          die("Could not find start of option value in '%s'", arg);
        // Step past '='
        start++;
        if ((parent_pid = atoi(start)) == 0)
          die("Invalid value '%s' passed to --parent-id", start);
      } else if (strcmp(arg, "--nocore") == 0) {
        nocore = TRUE;
      } else if (strncmp(arg, "--env ", 6) == 0) {
        putenv(strdup(arg + 6));
      } else if (std::strncmp(arg, "--safe-process-name", 19) == 0) {
        exe_name = arg + 20;
      } else
        die("Unknown option: %s", arg);
    }
  }

  if (*child_args == '\0') die("nothing to do");

  // Open a handle to the parent process
  message("parent_pid: %lu", (unsigned long)parent_pid);
  if (parent_pid == pid) die("parent_pid is equal to own pid!");

  if ((wait_handles[PARENT] = OpenProcess(SYNCHRONIZE, FALSE, parent_pid)) ==
      NULL)
    die("Failed to open parent process with pid: %lu",
        (unsigned long)parent_pid);

  // Create the child process in a job
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
  STARTUPINFO si = {};
  si.cb = sizeof(si);

  // Create the job object to make it possible to kill the process
  // and all of it's children in one go.
  HANDLE job_handle = CreateJobObject(NULL, NULL);
  if (job_handle == NULL) die("CreateJobObject failed");

  // Create a completion port for the job object.
  HANDLE port_handle =
      CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
  if (port_handle == NULL) die("CreateIoCompletionPort failed");

  JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_port_info;
  job_port_info.CompletionKey = job_handle;
  job_port_info.CompletionPort = port_handle;
  if (!SetInformationJobObject(job_handle,
                               JobObjectAssociateCompletionPortInformation,
                               &job_port_info, sizeof(job_port_info))) {
    die("SetInformationJobObject failed");
  }

// Make all processes associated with the job terminate when the
// last handle to the job is closed.
#ifndef JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 0x00002000
#endif

  jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation,
                              &jeli, sizeof(jeli)) == 0)
    message("SetInformationJobObject failed, continue anyway...");

  // Avoid popup box
  if (nocore)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);

  fix_file_append_flag_inheritance(STD_OUTPUT_HANDLE);
  fix_file_append_flag_inheritance(STD_ERROR_HANDLE);

#if 0
  // Setup stdin, stdout and stderr redirect
  si.dwFlags= STARTF_USESTDHANDLES;
  si.hStdInput= GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput= GetStdHandle(STD_OUTPUT_HANDLE);
  si.hStdError= GetStdHandle(STD_ERROR_HANDLE);
#endif

  // Create the process suspended to make sure it's assigned to the
  // Job before it creates any process of it's own.
  //
  // Allow the new process to break away from any job that this
  // process is part of so that it can be assigned to the new JobObject
  // we just created. This is safe since the new JobObject is created with
  // the JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flag, making sure it will be
  // terminated when the last handle to it is closed(which is owned by
  // this process).
  //
  // If breakaway from job fails on some reason, fallback is to create a
  // new process group. Process groups also allow to kill process and its
  // descendants, subject to some restrictions (processes have to run within
  // the same console,and must not ignore CTRL_BREAK)
  DWORD create_flags[] = {CREATE_BREAKAWAY_FROM_JOB, CREATE_NEW_PROCESS_GROUP,
                          0};

  BOOL jobobject_assigned = FALSE;
  BOOL process_created = FALSE;
  PROCESS_INFORMATION process_info = {};

  for (unsigned int i = 0; i < sizeof(create_flags) / sizeof(create_flags[0]);
       i++) {
    process_created = CreateProcess(
        NULL, (LPSTR)child_args, NULL, NULL, TRUE,  // Inherit handles
        CREATE_SUSPENDED | create_flags[i], NULL, NULL, &si, &process_info);
    if (process_created) {
      jobobject_assigned =
          AssignProcessToJobObject(job_handle, process_info.hProcess);
      break;
    }
  }

  if (!process_created) {
    die("CreateProcess failed");
  }

  ResumeThread(process_info.hThread);
  CloseHandle(process_info.hThread);
  wait_handles[CHILD] = process_info.hProcess;

  message("Started child %lu", (unsigned long)process_info.dwProcessId);

  // Monitor loop
  DWORD child_exit_code = 1;
  DWORD wait_res =
      WaitForMultipleObjects(NUM_HANDLES, wait_handles, FALSE, INFINITE);
  switch (wait_res) {
    case WAIT_OBJECT_0 + PARENT:
      message("Parent exit");
      break;
    case WAIT_OBJECT_0 + CHILD:
      if (GetExitCodeProcess(wait_handles[CHILD], &child_exit_code) == 0)
        message("Child exit: could not get exit_code");
      else
        message("Child exit: exit_code: %lu", (unsigned long)child_exit_code);
      break;
    case WAIT_OBJECT_0 + EVENT:
      message("Wake up from shutdown_event");
      break;
    default:
      message("Unexpected result %lu from WaitForMultipleObjects",
              (unsigned long)wait_res);
      break;
  }

  message("Exiting, child: %lu", (unsigned long)process_info.dwProcessId);

  if (jobobject_assigned) {
    // Send Terminate to all job children processes.
    if (TerminateJobObject(job_handle, 201) == 0)
      message("TerminateJobObject failed");

    message("Waiting for job processes to finish.");

    DWORD completion_code;
    ULONG_PTR completion_key;
    LPOVERLAPPED overlapped;
    while (GetQueuedCompletionStatus(port_handle, &completion_code,
                                     &completion_key, &overlapped, INFINITE) &&
           !((HANDLE)completion_key == job_handle &&
             completion_code == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)) {
    }
  } else {
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process_info.dwProcessId);
    TerminateProcess(process_info.hProcess, 202);

    if (wait_res != WAIT_OBJECT_0 + CHILD) {
      // The child has not yet returned, wait for it
      message("waiting for child to exit");
      if ((wait_res = WaitForSingleObject(wait_handles[CHILD], INFINITE)) !=
          WAIT_OBJECT_0) {
        message("child wait failed: %lu", (unsigned long)wait_res);
      } else {
        message("child wait succeeded");
      }
    }
  }
  // Child's exit code should now be 201 or 202, no need to get it

  CloseHandle(job_handle);
  CloseHandle(port_handle);

  message("Job terminated and closed");

  message("Closing handles");
  for (int i = 0; i < NUM_HANDLES; i++) CloseHandle(wait_handles[i]);

  message("Exiting, exit_code: %lu", (unsigned long)child_exit_code);
  std::exit(child_exit_code);
}
