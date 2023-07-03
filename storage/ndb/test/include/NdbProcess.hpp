/*
  Copyright (c) 2009, 2022, Oracle and/or its affiliates.


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


#ifndef NDB_PROCESS_HPP
#define NDB_PROCESS_HPP

#include "util/require.h"
#include <portlib/NdbSleep.h>

class NdbProcess
{
#ifdef _WIN32
  typedef DWORD pid_t;
#endif
  pid_t m_pid;
  BaseString m_name;
public:

  static pid_t getpid()
  {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return ::getpid();
#endif
  }

  class Args
  {
    Vector<BaseString> m_args;
  public:

    void add(const char* str)
    {
      m_args.push_back(str);
    }

    void add(const char* str, const char* str2)
    {
      BaseString tmp;
      tmp.assfmt("%s%s", str, str2);
      m_args.push_back(tmp);
    }

    void add(const char* str, int val)
    {
      BaseString tmp;
      tmp.assfmt("%s%d", str, val);
      m_args.push_back(tmp);
    }

    void add(const Args & args)
    {
      for (unsigned i = 0; i < args.m_args.size(); i++)
        add(args.m_args[i].c_str());
    }

    const Vector<BaseString>& args(void) const
    {
      return m_args;
    }

  };

#ifdef _WIN32
  static void printerror()
  {
    char* message;
    DWORD err = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&message, 0, NULL);

    fprintf(stderr, "Function failed, error: %d, message: '%s'", err, message);
    LocalFree(message);
  }
#endif

  static NdbProcess* create(const BaseString& name,
                            const BaseString& path,
                            const BaseString& cwd,
                            const Args& args)
  {
    NdbProcess* proc = new NdbProcess(name);
    if (!proc)
    {
      fprintf(stderr, "Failed to allocate memory for new process\n");
      return NULL;
    }

    // Check existence of cwd
    if (cwd.c_str() && access(cwd.c_str(), F_OK) != 0)
    {
      fprintf(stderr,
              "The specified working directory '%s' does not exist\n",
              cwd.c_str());
      delete proc;
      return NULL;
    }

    if (!start_process(proc->m_pid, path.c_str(), cwd.c_str(), args))
    {
      fprintf(stderr,
              "Failed to create process '%s'\n", name.c_str());
      delete proc;
      return NULL;
    }
    return proc;
  }

  bool stop(void)
  {
#ifdef _WIN32
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pid);
    if(!TerminateProcess(processHandle,9999))
    {
      printerror();
      return false;
    }
    CloseHandle(processHandle);
    return true;
#else
    int ret = kill(m_pid, 9);
    if (ret != 0)
    {
      fprintf(stderr,
              "Failed to kill process %d, ret: %d, errno: %d\n",
              m_pid, ret, errno);
      return false;
    }
    printf("Stopped process %d\n", m_pid);
    return true;
#endif
  }

  bool wait(int& ret, int timeout = 0)
  {
#ifdef _WIN32
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pid);
    const DWORD result = WaitForSingleObject(processHandle, timeout*100);
    bool fun_ret = true;
    if (result == WAIT_TIMEOUT) {
      fprintf(stderr,
        "Timeout when waiting for process %d\n", m_pid);
      fun_ret = false;
    }
    else if (result == WAIT_FAILED) {
      printerror();
      fun_ret = false;
    }
    DWORD exitCode = 0;
    if (GetExitCodeProcess(processHandle, &exitCode) == FALSE)
    {
      fprintf(stderr,
        "Error occurred when getting exit code of process %d\n", m_pid);
      return false;
    }
    if (exitCode != 9999)
    {
      ret = static_cast<int>(exitCode);
    }
    return fun_ret;

#else
    int retries = 0;
    int status;
    while (true)
    {
      pid_t ret_pid = waitpid(m_pid, &status, WNOHANG);
      if (ret_pid == -1)
      {
        fprintf(stderr,
                "Error occurred when waiting for process %d, ret: %d, errno: %d\n",
                m_pid, status, errno);
        return false;
      }

      if (ret_pid == m_pid)
      {
        if (WIFEXITED(status))
          ret = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
          ret = WTERMSIG(status);
        else
          ret = 37; // Unknown exit status

        printf("Got process %d, status: %d, ret: %d\n", m_pid, status, ret);
        return true;
      }

      if (timeout == 0)
        return false;

      if (retries++ > timeout*10)
      {
        fprintf(stderr,
                "Timeout when waiting for process %d\n", m_pid);
        return false;
      }
      NdbSleep_MilliSleep(10);
    }
    require(false); // Never reached
#endif
  }

private:

  NdbProcess(BaseString name) :
  m_name(name)
  {
    m_pid = -1;
  }

  static bool start_process(pid_t& pid, const char* path,
                            const char* cwd,
                            const Args& args)
  {
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    //LPSTR r = (LPSTR)args.args().getBase()->c_str();
    BaseString args_str;

    args_str.assign(args.args(), " ");
    std::string final_arg(path);
    final_arg.append(" ");
    final_arg.append(args_str.c_str());
    LPSTR r = (LPSTR)final_arg.c_str();



    // Start the child process.
    if (!CreateProcess(path,
      (LPSTR)final_arg.c_str(),
      NULL,
      NULL,
      FALSE,
      0,
      NULL,
      cwd,
      &si,
      &pi)
      ) {
      printerror();
      return false;
    }
    else
    {
      pid = pi.dwProcessId;
      fprintf(stderr, "Started process: %d\n", pid);
    }
    return true;
#else
    int retries = 5;
    pid_t tmp;
    while ((tmp = fork()) == -1)
    {
      fprintf(stderr, "Warning: 'fork' failed, errno: %d - ", errno);
      if (retries--)
      {
        fprintf(stderr, "retrying in 1 second...\n");
        NdbSleep_SecSleep(1);
        continue;
      }
      fprintf(stderr, "giving up...\n");
      return false;
    }

    if (tmp)
    {
      pid = tmp;
      printf("Started process: %d\n", pid);
      return true;
    }
    require(tmp == 0);

    if (cwd && chdir(cwd) != 0)
    {
      fprintf(stderr, "Failed to change directory to '%s', errno: %d\n", cwd, errno);
      exit(1);
    }

    // Concatenate arguments
    BaseString args_str;
    args_str.assign(args.args(), " ");

    char **argv = BaseString::argify(path, args_str.c_str());
    //printf("name: %s\n", path);
    execv(path, argv);

    fprintf(stderr, "execv failed, errno: %d\n", errno);
    exit(1);
#endif
  }
};

#endif
