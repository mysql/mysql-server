/*
  Copyright 2009 Sun Microsystems, Inc.

   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


#ifndef NDB_PROCESS_HPP
#define NDB_PROCESS_HPP

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
    return GetCurrentProcessid();
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

    const Vector<BaseString>& args(void) const
    {
      return m_args;
    }

  };

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
  }

  bool wait(int& ret, int timeout = 0)
  {
    int retries = 0;
    int status;
    while (true)
    {
      pid_t ret_pid = waitpid(m_pid, &status, WNOHANG);
      if (ret_pid == -1)
      {
        fprintf(stderr,
                "Error occured when waiting for process %d, ret: %d, errno: %d\n",
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
    assert(false); // Never reached
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
    assert(tmp == 0);

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
