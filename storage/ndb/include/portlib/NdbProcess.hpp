/*
  Copyright (c) 2009, 2023, Oracle and/or its affiliates.


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

#include <array>

#include "util/BaseString.hpp"
#include "util/require.h"

#include "portlib/ndb_socket.h"
#include "portlib/NdbSleep.h"

#ifdef _WIN32
inline bool S_ISDIR(int mode) { return (mode & _S_IFDIR); }
#endif

class NdbProcess
{
public:
#ifdef _WIN32
  using process_handle_t = PROCESS_INFORMATION;
  using pipe_handle_t = HANDLE;
#else
  using process_handle_t = pid_t;
  using pipe_handle_t = socket_t;
#endif

  class Pipes {
    std::array<pipe_handle_t, 4> fd;
    bool is_setup {false};

  public:
    enum { parent_read, child_write, child_read, parent_write };
    Pipes();
    pipe_handle_t operator [](size_t idx) const { return fd[idx]; }
    bool connected() const                      { return is_setup; }

    pipe_handle_t parentRead() const            { return fd[parent_read];  }
    pipe_handle_t childWrite() const            { return fd[child_write];  }
    pipe_handle_t childRead() const             { return fd[child_read];   }
    pipe_handle_t parentWrite() const           { return fd[parent_write]; }

    static FILE * open(pipe_handle_t, const char * mode);
  };

  class Args {
    Vector<BaseString> m_args;

  public:
    void add(const char* str);
    void add(const char* str, const char* str2);
    void add(const char* str, int val);
    void add(Uint64 val);
    void add(const Args & args);
    const Vector<BaseString>& args(void) const  { return m_args; }
  };

  void closeHandles();
  static void printerror();
  bool stop(void);
  bool wait(int& ret, int timeout_msec = 0);

  static NdbProcess* create(const BaseString& name,
                            const BaseString& path,
                            const BaseString& cwd,
                            const Args& args,
                            Pipes * const fds = nullptr)
  {
    NdbProcess* proc = new NdbProcess(name, fds);
    if (!proc)
    {
      fprintf(stderr, "Failed to allocate memory for new process\n");
      return NULL;
    }

    // Check cwd
    if (cwd.c_str())
    {
      struct stat cwdstat;
      if((access(cwd.c_str(), F_OK) != 0) ||
         (stat(cwd.c_str(), &cwdstat) != 0) ||
         (!S_ISDIR(cwdstat.st_mode)))
      {
        fprintf(stderr, "The specified working directory '%s' cannot be used\n",
                cwd.c_str());
        delete proc;
        return nullptr;
      }
    }

    if (!start_process(proc->m_proc, path.c_str(), cwd.c_str(),
                       args, proc->m_pipes))
    {
      fprintf(stderr, "Failed to create process '%s'\n", name.c_str());
      delete proc;
      proc = nullptr;
    }
    return proc;
  }

private:
  process_handle_t m_proc;
  BaseString m_name;
  Pipes * m_pipes;

  NdbProcess(BaseString name, Pipes * fds) : m_name(name), m_pipes(fds) {}

  static bool start_process(process_handle_t & pid, const char* path,
                            const char* cwd, const Args& args, Pipes * pipes);
};


inline NdbProcess::Pipes::Pipes() {
  bool r1, r2;
#ifdef _WIN32
  r1 = CreatePipe(& fd[0], & fd[1], nullptr, 0);
  r2 = CreatePipe(& fd[2], & fd[3], nullptr, 0);
#else
  r1 = (pipe(&fd[0]) == 0);
  r2 = (pipe(&fd[2]) == 0);
#endif
  is_setup = r1 && r2;
}

inline FILE * NdbProcess::Pipes::open(pipe_handle_t p, const char * mode) {
#ifdef _WIN32
  return _fdopen(_open_osfhandle((intptr_t) p, _O_TEXT), mode);
#else
  return fdopen(p, mode);
#endif
}

inline void NdbProcess::Args::add(const char* str)
{
  m_args.push_back(str);
}

inline void NdbProcess::Args::add(const char* str, const char* str2)
{
  BaseString tmp;
  tmp.assfmt("%s%s", str, str2);
  m_args.push_back(tmp);
}

inline void NdbProcess::Args::add(const char* str, int val)
{
  BaseString tmp;
  tmp.assfmt("%s%d", str, val);
  m_args.push_back(tmp);
}

inline void NdbProcess::Args::add(Uint64 val)
{
  BaseString tmp;
  tmp.assfmt("%ju", uintmax_t{val});
  m_args.push_back(tmp);
}

inline void NdbProcess::Args::add(const Args & args)
{
  for (unsigned i = 0; i < args.m_args.size(); i++)
    add(args.m_args[i].c_str());
}

#ifdef _WIN32

/******
        NdbProcess Win32 implementation
                                        *********/

inline void NdbProcess::closeHandles()
{
  CloseHandle(m_proc.hProcess);
  CloseHandle(m_proc.hThread);
}

inline void NdbProcess::printerror()
{
  char* message;
  DWORD err = GetLastError();

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&message, 0, NULL);

  fprintf(stderr, "Function failed, error: %lu, message: '%s'", err, message);
  LocalFree(message);
}

inline bool NdbProcess::stop()
{
  if (!TerminateProcess(m_proc.hProcess,9999))
  {
    printerror();
    closeHandles();
    return false;
  }
  closeHandles();
  return true;
}

inline bool NdbProcess::wait(int & ret, int timeout)
{
  const DWORD result = WaitForSingleObject(m_proc.hProcess, timeout);
  bool fun_ret = true;
  if (result == WAIT_TIMEOUT)
  {
    fprintf(stderr, "Timeout when waiting for process\n");
    return false;
  }
  if (result == WAIT_FAILED)
  {
    printerror();
    fun_ret = false;
  }
  DWORD exitCode = 0;
  if (GetExitCodeProcess(m_proc.hProcess, &exitCode) == FALSE)
  {
    fprintf(stderr, "Error occurred when getting exit code of process\n");
    closeHandles();
    return false;
  }
  closeHandles();
  if (exitCode != 9999)
  {
    ret = static_cast<int>(exitCode);
  }
  return fun_ret;
}

inline bool NdbProcess::start_process(process_handle_t & pid,
                                      const char* path,
                                      const char* cwd,
                                      const Args& args,
                                      Pipes * pipes)
{
  STARTUPINFO si;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  BaseString args_str;

  args_str.assign(args.args(), " ");
  std::string final_arg(path);
  final_arg.append(" ");
  final_arg.append(args_str.c_str());

  if(pipes) {
    static constexpr int Inherit = HANDLE_FLAG_INHERIT;
    SetHandleInformation(pipes->childRead(), Inherit, Inherit);
    SetHandleInformation(pipes->childWrite(), Inherit, Inherit);
    si.cb = sizeof(si);
    si.hStdOutput = pipes->childWrite();
    si.hStdInput = pipes->childRead();
    si.dwFlags = STARTF_USESTDHANDLES;
  }

  // Start the child process.
  if (!CreateProcess(
    path,     // Application Name
    (LPSTR)const_cast<char *>(final_arg.c_str()), // command line
    nullptr,  // process security attributes
    nullptr,  // primary thread security attributes
    (bool) pipes, // Flag allowing pipe handles to be inherited
    0,        // creation flags
    nullptr,  // use parent's environment
    cwd,      // current directory
    &si,      // lpStartupInfo
    &pid)     // lpProcessInformation
    ) {
    printerror();
    return false;
  }
  else
  {
    fprintf(stderr, "Started process.\n");
  }
  return true;
}


#else

/******
        NdbProcess Posix implementation
                                        *********/

inline void NdbProcess::closeHandles() { }
inline void NdbProcess::printerror() { }

inline bool NdbProcess::stop()
{
  int ret = kill(m_proc, 9);
  if (ret != 0)
  {
    fprintf(stderr,
            "Failed to kill process %d, ret: %d, errno: %d\n",
            m_proc, ret, errno);
    return false;
  }
  printf("Stopped process %d\n", m_proc);
  return true;
}

inline bool NdbProcess::wait(int & ret, int timeout)
{
  int slept = 0;
  int status;
  while (true)
  {
    pid_t ret_pid = waitpid(m_proc, &status, WNOHANG);
    if (ret_pid == -1)
    {
      fprintf(stderr,
              "Error occurred when waiting for process %d, ret: %d, errno: %d\n",
              m_proc, status, errno);
      return false;
    }

    if (ret_pid == m_proc)
    {
      if (WIFEXITED(status))
        ret = WEXITSTATUS(status);
      else if (WIFSIGNALED(status))
        ret = WTERMSIG(status);
      else
        ret = 37; // Unknown exit status

      printf("Got process %d, status: %d, ret: %d\n", m_proc, status, ret);
      return true;
    }

    if (timeout == 0)
      return false;

    slept += 10;
    if (slept > timeout)
    {
      fprintf(stderr, "Timeout when waiting for process %d\n", m_proc);
      return false;
    }
    NdbSleep_MilliSleep(10);
  }
  require(false); // Never reached
}

inline bool NdbProcess::start_process(process_handle_t & pid,
                                      const char* path,
                                      const char* cwd,
                                      const Args& args,
                                      Pipes * pipes)
{
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
    if(pipes) {
      close(pipes->childRead());
      close(pipes->childWrite());
    }
    return true;
  }
  require(tmp == 0);

  if (cwd && chdir(cwd) != 0)
  {
    fprintf(stderr, "Failed to change directory to '%s', errno: %d\n", cwd, errno);
    exit(1);
  }

  // Dup second half of socketpair to child STDIN & STDOUT
  if(pipes != nullptr) {
    close(pipes->parentRead());
    close(pipes->parentWrite());

    if(dup2(pipes->childRead(), STDIN_FILENO) != STDIN_FILENO) {
      fprintf(stderr, "STDIN dup2() failed\n");
      exit(1);
    }

    if(dup2(pipes->childWrite(), STDOUT_FILENO) != STDOUT_FILENO) {
      fprintf(stderr, "STDIN dup2() failed\n");
      exit(1);
    }
  }

  // Concatenate arguments
  BaseString args_str;
  args_str.assign(args.args(), " ");

  char **argv = BaseString::argify(path, args_str.c_str());
  //printf("name: %s\n", path);
  execvp(path, argv);

  fprintf(stderr, "execv failed, errno: %d\n", errno);
  exit(1);
}

#endif  // Posix

#endif  // NDB_PROCESS_HPP
