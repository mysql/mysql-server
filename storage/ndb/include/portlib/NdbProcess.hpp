/*
  Copyright (c) 2009, 2024, Oracle and/or its affiliates.


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include <filesystem>
#include <memory>
#include <optional>

#include "util/BaseString.hpp"
#include "util/require.h"

#include "portlib/NdbSleep.h"
#include "portlib/ndb_socket.h"
#include "portlib/ndb_socket_poller.h"

class NdbProcess {
 public:
#ifdef _WIN32
  using process_handle_t = PROCESS_INFORMATION;
  using pipe_handle_t = HANDLE;
  inline static const pipe_handle_t InvalidHandle = INVALID_HANDLE_VALUE;
  inline static bool S_ISDIR(int mode) { return (mode & _S_IFDIR); }
#else
  using process_handle_t = pid_t;
  using pipe_handle_t = socket_t;
  static constexpr pipe_handle_t InvalidHandle = -1;
  inline static void CloseHandle(int fd) { (void)close(fd); }
#endif

  class Pipes {
    std::array<pipe_handle_t, 4> fd;
    bool is_setup{false};

   public:
    enum { parent_read, child_write, child_read, parent_write };
    Pipes();
    ~Pipes();
    pipe_handle_t operator[](size_t idx) const { return fd[idx]; }
    bool connected() const { return is_setup; }

    pipe_handle_t parentRead() const { return fd[parent_read]; }
    pipe_handle_t childWrite() const { return fd[child_write]; }
    pipe_handle_t childRead() const { return fd[child_read]; }
    pipe_handle_t parentWrite() const { return fd[parent_write]; }

    static FILE *open(pipe_handle_t, const char *mode);
    void closePipe(size_t);
    void closeChildHandles();
    void closeParentHandles();
  };

  class Args {
    Vector<BaseString> m_args;

   public:
    void add(const char *str);
    /*
     * For '--name=value' options which will be passed as one argument.
     * Example: args.add("--id=", 7);
     */
    void add(const char *str, const char *str2);
    void add(const char *str, int val);
    /*
     * For '-name value' options which will be passed as two arguments.
     * Example: args.add2("-id",7);
     */
    void add2(const char *str, const char *str2);
    void add2(const char *str, int val);

    /*
     * Add arguments one by one from another argument list.
     */
    void add(const Args &args);

    const Vector<BaseString> &args(void) const { return m_args; }
    void clear() { m_args.clear(); }
  };

  ~NdbProcess() { assert(!running()); }
  void closeHandles();
  static void printerror();
  bool stop(void);
  bool wait(int &ret, int timeout_msec = 0);
  bool running() const;

  static std::unique_ptr<NdbProcess> create(const BaseString &name,
                                            const BaseString &path,
                                            const BaseString &cwd,
                                            const Args &args,
                                            Pipes *const fds = nullptr) {
    std::unique_ptr<NdbProcess> proc(new NdbProcess(name, fds));

    if (!proc) {
      fprintf(stderr, "Failed to allocate memory for new process\n");
    }

    // Check cwd
    if (cwd.c_str()) {
      struct stat cwdstat;
      if ((access(cwd.c_str(), F_OK) != 0) ||
          (stat(cwd.c_str(), &cwdstat) != 0) || (!S_ISDIR(cwdstat.st_mode))) {
        fprintf(stderr, "The specified working directory '%s' cannot be used\n",
                cwd.c_str());
        proc.reset(nullptr);
      }
    }

    if (proc && !start_process(proc->m_proc, path.c_str(), cwd.c_str(), args,
                               proc->m_pipes)) {
      fprintf(stderr, "Failed to create process '%s'\n", name.c_str());
      proc.reset(nullptr);
    }

    return proc;
  }

  static std::unique_ptr<NdbProcess> create_via_ssh(
      const BaseString &name, const BaseString &host, const BaseString &path,
      const BaseString &cwd, const Args &args, Pipes *const fds = nullptr);

 private:
#ifdef _WIN32
  process_handle_t m_proc{InvalidHandle, InvalidHandle, 0, 0};
#else
  process_handle_t m_proc{InvalidHandle};
#endif
  BaseString m_name;
  Pipes *m_pipes;

  NdbProcess(BaseString name, Pipes *fds) : m_name(name), m_pipes(fds) {}

  /*
   * Quoting function to be used for passing program name and arguments to a
   * Windows program that follows the quoting of command line supported by
   * Microsoft C/C++ runtime. See for example description in:
   * https://learn.microsoft.com/en-us/cpp/cpp/main-function-command-line-args
   *
   * Note this quoting is not always suitable when calling other programs since
   * they are free to interpret the command line as they wish and the possible
   * quoting done may mess up.  For example cmd.exe treats unquoted ^
   * differently.
   */
  static std::optional<BaseString> quote_for_windows_crt(
      const char *str) noexcept;

  static std::optional<BaseString> quote_for_windows_cmd_crt(
      const char *str) noexcept;

  static std::optional<BaseString> quote_for_posix_sh(const char *str) noexcept;

  static std::optional<BaseString> quote_for_unknown_shell(
      const char *str) noexcept;

  static bool start_process(process_handle_t &pid, const char *path,
                            const char *cwd, const Args &args, Pipes *pipes);
};

inline NdbProcess::Pipes::Pipes() {
  fd[0] = fd[1] = fd[2] = fd[3] = InvalidHandle;
  bool r1, r2;
#ifdef _WIN32
  r1 = CreatePipe(&fd[parent_read], &fd[child_write], nullptr, 0);
  r2 = CreatePipe(&fd[child_read], &fd[parent_write], nullptr, 0);
#else
  r1 = (pipe(&fd[0]) == 0);
  r2 = (pipe(&fd[2]) == 0);
#endif
  is_setup = r1 && r2;
}

inline void NdbProcess::Pipes::closePipe(size_t i) {
  assert(i < 4);
  CloseHandle(fd[i]);
  fd[i] = InvalidHandle;
}

inline void NdbProcess::Pipes::closeChildHandles() {
  closePipe(child_read);
  closePipe(child_write);
}

inline void NdbProcess::Pipes::closeParentHandles() {
  closePipe(parent_read);
  closePipe(parent_write);
}

inline NdbProcess::Pipes::~Pipes() {
  closeParentHandles();
  closeChildHandles();
}

inline FILE *NdbProcess::Pipes::open(pipe_handle_t p, const char *mode) {
#ifdef _WIN32
  return _fdopen(_open_osfhandle((intptr_t)p, _O_TEXT), mode);
#else
  return fdopen(p, mode);
#endif
}

inline void NdbProcess::Args::add(const char *str) { m_args.push_back(str); }

inline void NdbProcess::Args::add(const char *str, const char *str2) {
  BaseString tmp;
  tmp.assfmt("%s%s", str, str2);
  m_args.push_back(tmp);
}

inline void NdbProcess::Args::add2(const char *str, const char *str2) {
  BaseString tmp;
  m_args.push_back(str);
  m_args.push_back(str2);
}

inline void NdbProcess::Args::add(const char *str, int val) {
  BaseString tmp;
  tmp.assfmt("%s%d", str, val);
  m_args.push_back(tmp);
}

inline void NdbProcess::Args::add2(const char *str, int val) {
  m_args.push_back(str);
  BaseString tmp;
  tmp.assfmt("%d", val);
  m_args.push_back(tmp);
}

inline void NdbProcess::Args::add(const Args &args) {
  for (unsigned i = 0; i < args.m_args.size(); i++) add(args.m_args[i].c_str());
}

std::unique_ptr<NdbProcess> NdbProcess::create_via_ssh(
    const BaseString &name, const BaseString &host, const BaseString &path,
    const BaseString &cwd, const Args &args, Pipes *const fds) {
  BaseString ssh_name = "ssh";
  Args ssh_args;
  ssh_args.add(host.c_str());
  /*
   * Arguments need to be quoted. What kind of quoting depends on what shell is
   * used on remote host when ssh executes the command.
   *
   * And for Windows also what quoting the command itself requires on the
   * command line.
   *
   * As a rough heuristic the type of quoting needed on remote host we look at
   * command path and arguments. If backslash (\) is in any of them it is
   * assumed that ssh executes command via cmd.exe and that command is a C/C++
   * program. And if slash (/) is found it is assumed that ssh execute command
   * via Bourne shell, sh, or compatible on remote host. This is not perfect but
   * is a simple rule to document.
   *
   * On Windows it is assumed that ssh follows the quoting rules for Microsoft
   * C/C++ runtime.
   */
  bool has_backslash = (strchr(path.c_str(), '\\'));
  bool has_slash = (strchr(path.c_str(), '/'));
  for (size_t i = 0; i < args.args().size(); i++) {
    if (strchr(args.args()[i].c_str(), '\\')) has_backslash = true;
    if (strchr(args.args()[i].c_str(), '/')) has_slash = true;
  }
  std::optional<BaseString> (*quote_func)(const char *str);
  if (has_backslash && !has_slash)
    quote_func = quote_for_windows_cmd_crt;
  else if (!has_backslash && has_slash)
    quote_func = quote_for_posix_sh;
  else
    quote_func = quote_for_unknown_shell;

  auto qpath = quote_func(path.c_str());
  if (!qpath) {
    fprintf(stderr, "Function failed, could not quote command name: %s\n",
            path.c_str());
    return {};
  }
  ssh_args.add(qpath.value().c_str());
  for (size_t i = 0; i < args.args().size(); i++) {
    auto &arg = args.args()[i];
    auto qarg = quote_func(arg.c_str());
    if (!qarg) {
      fprintf(stderr, "Function failed, could not quote command argument: %s\n",
              arg.c_str());
      return {};
    }
    ssh_args.add(qarg.value().c_str());
  }
  return create(name, ssh_name, cwd, ssh_args, fds);
}

std::optional<BaseString> NdbProcess::quote_for_windows_crt(
    const char *str) noexcept {
  /*
   * Assuming program file names can not include " or end with a \ this
   * function should be usable also for quoting the command part of command
   * line when calling a C program via CreateProcess.
   */
  const char *p = str;
  while (isprint(*p) && *p != ' ' && *p != '"' && *p != '*' && *p != '?') p++;
  if (*p == '\0' && str[0] != '\0') return str;
  BaseString ret;
  ret.append('"');
  size_t backslashes = 0;
  while (*str) {
    switch (*str) {
      case '"':
        if (backslashes) {
          // backslashes preceding double quote needs quoting
          ret.append(backslashes, '\\');
          backslashes = 0;
        }
        // use double double quotes to quote double quote
        ret.append('"');
        break;
      case '\\':
        // Count backslashes in case they will be followed by double quote
        backslashes++;
        break;
      default:
        backslashes = 0;
    }
    ret.append(*str);
    str++;
  }
  if (backslashes) ret.append(backslashes, '\\');
  ret.append('"');
  return ret;
}

std::optional<BaseString> NdbProcess::quote_for_windows_cmd_crt(
    const char *str) noexcept {
  /*
   * Quoting of % is not handled and likely not possible when using double
   * quotes. If for %xxx% there is an environment variable named xxx defined,
   * that will be replaced in string by cmd.exe. If there is no such environment
   * variable the %xxx% will be intact as is.
   *
   * Since cmd.exe do not allow any quoting of a double quote within double
   * quotes we should make sure each argument have an even number of double
   * quotes, else cmd.exe may treat the last double quote from one argument as
   * beginning of a quotation ending at the first quote of next argument. That
   * can be accomplished using "" when quoting a single " within an argument.
   * But to make it more likely that a argument containing an even number of "
   * be quoted in the same way for windows as for posix shell the alternate
   * quoting method \" will be used in those cases. But only if none of ^ < > &
   * | appear between even and odd double quotes since cmd.exe will interpret
   * them specially.
   */
  const char *p = str;
  bool dquote = true;  // Assume quoted will started with "
  bool need_dquote = false;
  bool need_quote = (*p == '\0');
  while (!need_dquote && *p) {
    switch (*p) {
      case '^':
      case '<':
      case '>':
      case '|':
      case '&':
        if (!dquote)
          need_dquote = true;
        else
          need_quote = true;
        break;
      case '"':
        dquote = !dquote;
        need_quote = true;
        break;
      case ' ':
      case '*':
      case '?':
        need_dquote = true;
        break;
      default:
        if (!isprint(*p)) need_dquote = true;
    }
    p++;
  }
  if (!need_quote && !need_dquote) return str;
  // If argument had even number of double quotes, dquote should be true.
  if (!dquote) need_dquote = true;
  BaseString ret;
  ret.append('"');
  int backslashes = 0;
  while (*str) {
    switch (*str) {
      case '"':
        if (backslashes) ret.append(backslashes, '\\');
        backslashes = 0;
        if (need_dquote)
          ret.append('"');
        else
          ret.append('\\');
        break;
      case '\\':
        backslashes++;
        break;
      default:
        backslashes = 0;
    }
    ret.append(*str);
    str++;
  }
  if (backslashes) ret.append(backslashes, '\\');
  ret.append('"');
  return ret;
}

std::optional<BaseString> NdbProcess::quote_for_posix_sh(
    const char *str) noexcept {
  const char *p = str;
  while (*p && !strchr("\t\n \"#$&'()*;<>?\\`|~", *p)) p++;
  if (*p == '\0' && p != str) return str;
  BaseString ret;
  ret.append('"');
  while (*str) {
    char ch = *str;
    switch (ch) {
      case '"':
      case '$':
      case '\\':
      case '`':
        ret.append('\\');
        break;
    }
    ret.append(ch);
    str++;
  }
  ret.append('"');
  return ret;
}

std::optional<BaseString> NdbProcess::quote_for_unknown_shell(
    const char *str) noexcept {
  auto windows = quote_for_windows_cmd_crt(str);
  auto posix = quote_for_posix_sh(str);
  if (windows != posix) return {};
  return windows;
}

#ifdef _WIN32

/******
        NdbProcess Win32 implementation
                                        *********/

inline void NdbProcess::closeHandles() {
  CloseHandle(m_proc.hProcess);
  m_proc.hProcess = InvalidHandle;
  CloseHandle(m_proc.hThread);
  m_proc.hThread = InvalidHandle;
}

inline bool NdbProcess::running() const {
  return (m_proc.hProcess != InvalidHandle);
}

inline void NdbProcess::printerror() {
  char *message;
  DWORD err = GetLastError();

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&message, 0, nullptr);

  fprintf(stderr, "Function failed, error: %lu, message: '%s'", err, message);
  LocalFree(message);
}

inline bool NdbProcess::stop() {
  bool r = TerminateProcess(m_proc.hProcess, 9999);
  if (!r) printerror();
  return r;
}

inline bool NdbProcess::wait(int &ret, int timeout) {
  require(m_proc.hProcess != INVALID_HANDLE_VALUE);

  const DWORD result = WaitForSingleObject(m_proc.hProcess, timeout);
  if (result != WAIT_OBJECT_0) {
    if (result == WAIT_TIMEOUT)
      fprintf(stderr, "Timeout when waiting for process\n");
    else
      printerror();
    return false;
  }

  DWORD exitCode = 0;
  if (!GetExitCodeProcess(m_proc.hProcess, &exitCode)) {
    printerror();
    return false;
  }
  assert(exitCode != STILL_ACTIVE);

  closeHandles();
  ret = (int)exitCode;
  return true;
}

inline bool NdbProcess::start_process(process_handle_t &pid, const char *path,
                                      const char *cwd, const Args &args,
                                      Pipes *pipes) {
  // If program without path need to lookup path, CreateProcess will not do
  // that.
  char full_path[PATH_MAX];
  if (strpbrk(path, "/\\") == nullptr) {
    DWORD len = SearchPath(nullptr, path, ".EXE", sizeof(full_path), full_path,
                           nullptr);
    if (len > 0) path = full_path;
  }

  std::optional<BaseString> quoted = quote_for_windows_crt(path);
  if (!quoted) {
    fprintf(stderr, "Function failed, could not quote command name: %s\n",
            path);
    return false;
  }
  BaseString cmdLine(quoted.value().c_str());

  /* Quote each argument, and append it to the command line */
  auto &args_vec = args.args();
  for (size_t i = 0; i < args_vec.size(); i++) {
    auto *arg = args_vec[i].c_str();
    std::optional<BaseString> quoted = quote_for_windows_crt(arg);
    if (!quoted) {
      fprintf(stderr, "Function failed, could not quote command argument: %s\n",
              arg);
      return false;
    }
    cmdLine.append(' ');
    cmdLine.append(quoted.value().c_str());
  }
  char *command_line = strdup(cmdLine.c_str());

  STARTUPINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  if (pipes) {
    static constexpr int Inherit = HANDLE_FLAG_INHERIT;
    SetHandleInformation(pipes->childRead(), Inherit, Inherit);
    SetHandleInformation(pipes->childWrite(), Inherit, Inherit);
    si.hStdOutput = pipes->childWrite();
    si.hStdInput = pipes->childRead();
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags = STARTF_USESTDHANDLES;
  }

  // Start the child process.
  if (!CreateProcess(
          path,  // "the full path and file name of the module to execute"
          command_line,  // lpCommandLine: command line including arguments
          nullptr,       // lpProcessAttributes: process security attributes
          nullptr,  // lpThreadAttributes: primary thread security attributes
          (bool)pipes,  // bInheritHandles: allow pipe handles to be inherited
          0,            // dwCreationFlags
          nullptr,      // lpEnvironment: use parent's environment
          cwd,          // lpCurrentDirectory: current directory
          &si,          // lpStartupInfo
          &pid)         // lpProcessInformation
  ) {
    printerror();
    if (pipes) pipes->closeChildHandles();
    free(command_line);
    // CreateProcess zero fills pid on failure, reinitialize it
    pid.hProcess = INVALID_HANDLE_VALUE;
    pid.hThread = INVALID_HANDLE_VALUE;
    pid.dwProcessId = 0;
    pid.dwThreadId = 0;
    return false;
  }

  fprintf(stderr, "Started process.\n");
  if (pipes) pipes->closeChildHandles();

  free(command_line);
  return true;
}

#else

/******
        NdbProcess Posix implementation
                                        *********/

inline bool NdbProcess::running() const { return (m_proc != InvalidHandle); }

inline bool NdbProcess::stop() {
  int ret = kill(m_proc, 9);
  if (ret)
    fprintf(stderr, "Failed to kill process %d, ret: %d, errno: %d\n", m_proc,
            ret, errno);
  return (ret == 0);
}

inline bool NdbProcess::wait(int &ret, int timeout) {
  int slept = 0;
  int status;
  while (true) {
    pid_t ret_pid = waitpid(m_proc, &status, WNOHANG);

    if (ret_pid == -1) {
      fprintf(
          stderr,
          "Error occurred when waiting for process %d, ret: %d, errno: %d\n",
          m_proc, status, errno);
      return false;
    }

    if (ret_pid == m_proc) {
      if (WIFEXITED(status))
        ret = WEXITSTATUS(status);
      else if (WIFSIGNALED(status))
        ret = WTERMSIG(status);
      else
        ret = 37;  // Unknown exit status

      printf("Got process %d, status: %d, ret: %d\n", m_proc, status, ret);
      m_proc = InvalidHandle;
      return true;
    }

    if (timeout == 0) return false;

    slept += 10;
    if (slept > timeout) {
      fprintf(stderr, "Timeout when waiting for process %d\n", m_proc);
      return false;
    }
    NdbSleep_MilliSleep(10);
  }
  require(false);  // Never reached
}

inline bool NdbProcess::start_process(process_handle_t &pid, const char *path,
                                      const char *cwd, const Args &args,
                                      Pipes *pipes) {
  pid = InvalidHandle;
  int retries = 5;
  pid_t tmp;
  while ((tmp = fork()) == -1) {
    fprintf(stderr, "Warning: 'fork' failed, errno: %d - ", errno);
    if (retries--) {
      fprintf(stderr, "retrying in 1 second...\n");
      NdbSleep_SecSleep(1);
      continue;
    }
    fprintf(stderr, "giving up...\n");
    return false;
  }

  if (tmp) {
    pid = tmp;
    printf("Started process: %d\n", pid);
    if (pipes) {
      pipes->closeChildHandles();
    }
    return true;
  }
  require(tmp == 0);

  if (cwd && chdir(cwd) != 0) {
    fprintf(stderr, "Failed to change directory to '%s', errno: %d\n", cwd,
            errno);
    exit(1);
  }

  // Dup second half of socketpair to child STDIN & STDOUT
  if (pipes != nullptr) {
    pipes->closeParentHandles();

    if (dup2(pipes->childRead(), STDIN_FILENO) != STDIN_FILENO) {
      fprintf(stderr, "STDIN dup2() failed\n");
      exit(1);
    }

    if (dup2(pipes->childWrite(), STDOUT_FILENO) != STDOUT_FILENO) {
      fprintf(stderr, "STDIN dup2() failed\n");
      exit(1);
    }
  }

  auto &args_vec = args.args();
  size_t arg_cnt = args_vec.size();
  char **argv = new char *[1 + arg_cnt + 1];
  argv[0] = const_cast<char *>(path);
  for (size_t i = 0; i < arg_cnt; i++)
    argv[1 + i] = const_cast<char *>(args_vec[i].c_str());
  argv[1 + arg_cnt] = nullptr;

  execvp(path, argv);

  fprintf(stderr, "execv failed, error %d '%s'\n", errno, strerror(errno));
  exit(1);
}

#endif  // Posix

#endif  // NDB_PROCESS_HPP
