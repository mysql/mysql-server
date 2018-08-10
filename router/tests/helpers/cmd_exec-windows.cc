/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "cmd_exec-windows.h"
#include "cmd_exec.h"
#include "router_test_helpers.h"

#include <string>

#include <stdio.h>
#include <tchar.h>
#include <windows.h>

static std::string get_env_string() {
  LPCH env_str = GetEnvironmentStrings();
  if (!env_str) return "";
  LPCH next_env_var = env_str;
  size_t len_total = 0;
  size_t len_item;
  while ((len_item = strlen(next_env_var)) > 0) {
    len_total += len_item + 1;
    next_env_var += len_item + 1;
  }
  std::string result(env_str, len_total);
  FreeEnvironmentStrings(env_str);
  return result;
}

void Process_launcher::start() {
  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&child_out_rd, &child_out_wr, &saAttr, 0))
    report_error("Failed to create child_out_rd");

  if (!SetHandleInformation(child_out_rd, HANDLE_FLAG_INHERIT, 0))
    report_error("Failed to create child_out_rd");

  // force non blocking IO in Windows
  // DWORD mode = PIPE_NOWAIT;
  // BOOL res = SetNamedPipeHandleState(child_out_rd, &mode, NULL, NULL);

  if (!CreatePipe(&child_in_rd, &child_in_wr, &saAttr, 0))
    report_error("Failed to create child_in_rd");

  if (!SetHandleInformation(child_in_wr, HANDLE_FLAG_INHERIT, 0))
    report_error("Failed to created child_in_wr");

  // Create Process
  BOOL bSuccess = FALSE;

  ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  if (redirect_stderr_) si.hStdError = child_out_wr;
  si.hStdOutput = child_out_wr;
  si.hStdInput = child_in_rd;
  si.dwFlags |= STARTF_USESTDHANDLES;

  // Environment strings must inherit all the existing ones (like PATH changes).
  std::string myenv;
  if (!env_.empty()) {
    myenv = get_env_string();
    myenv.append(env_);
    myenv.push_back('\0');
  }

  bSuccess = CreateProcess(
      NULL,                                   // lpApplicationName
      const_cast<char *>(cmd_line_.c_str()),  // lpCommandLine
      NULL,                                   // lpProcessAttributes
      NULL,                                   // lpThreadAttributes
      TRUE,                                   // bInheritHandles
      0,                                      // dwCreationFlags
      myenv.empty()
          ? NULL
          : reinterpret_cast<LPVOID>(const_cast<char *>(myenv.c_str())),
      // lpEnvironment
      NULL,  // lpCurrentDirectory
      &si,   // lpStartupInfo
      &pi);  // lpProcessInformation

  if (!bSuccess) report_error(NULL);

  CloseHandle(child_out_wr);
  CloseHandle(child_in_rd);

  // DWORD res1 = WaitForInputIdle(pi.hProcess, 100);
  // res1 = WaitForSingleObject(pi.hThread, 100);
}

uint64_t Process_launcher::get_pid() { return (uint64_t)pi.hProcess; }

int Process_launcher::wait() {
  DWORD dwExit = 0;

  for (;;) {
    if (!GetExitCodeProcess(pi.hProcess, &dwExit)) {
      DWORD dwError = GetLastError();
      if (dwError != ERROR_INVALID_HANDLE)  // not closed already?
        report_error(NULL);
      return -1;
    }
    if (dwExit == STILL_ACTIVE)
      WaitForSingleObject(pi.hProcess, INFINITE);
    else
      break;
  }
  return dwExit;
}

void Process_launcher::close() {
  DWORD dwExit;
  if (GetExitCodeProcess(pi.hProcess, &dwExit)) {
    if (dwExit == STILL_ACTIVE) {
      if (!TerminateProcess(pi.hProcess, 0)) report_error(NULL);
      // TerminateProcess is async, wait for process to end.
      WaitForSingleObject(pi.hProcess, INFINITE);
    }
  } else {
    report_error(NULL);
  }

  if (!CloseHandle(pi.hProcess)) report_error(NULL);
  if (!CloseHandle(pi.hThread)) report_error(NULL);

  if (!CloseHandle(child_out_rd)) report_error(NULL);
  if (!CloseHandle(child_in_wr)) report_error(NULL);

  is_alive_ = false;
}

int Process_launcher::read_one_char() {
  char buf;
  int ret = Process_launcher::read(&buf, 1);
  return ret == EOF ? EOF : buf;
}

int Process_launcher::read(char *buf, size_t count) {
  BOOL bSuccess = FALSE;
  DWORD dwBytesRead, dwCode;
  int i = 0;

  while (!(bSuccess = ReadFile(child_out_rd, buf, count, &dwBytesRead, NULL))) {
    dwCode = GetLastError();
    if (dwCode == ERROR_NO_DATA) continue;
    if (dwCode == ERROR_BROKEN_PIPE)
      return EOF;
    else
      report_error(NULL);
  }

  return dwBytesRead;
}

int Process_launcher::write_one_char(char c) {
  return Process_launcher::write(&c, 1);
}

int Process_launcher::write(const char *buf, size_t count) {
  DWORD dwBytesWritten;
  BOOL bSuccess = FALSE;
  bSuccess = WriteFile(child_in_wr, buf, count, &dwBytesWritten, NULL);
  if (!bSuccess) {
    if (GetLastError() != ERROR_NO_DATA)  // otherwise child process just died.
      report_error(NULL);
  } else {
    // When child input buffer is full, this returns zero in NO_WAIT mode.
    return dwBytesWritten;
  }
  return 0;  // so the compiler does not cry
}

void Process_launcher::report_error(const char *msg) {
  DWORD dwCode = GetLastError();
  LPTSTR lpMsgBuf;

  if (msg != NULL) {
    throw std::runtime_error(msg);
  } else {
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, dwCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf, 0, NULL);
    std::string msgerr = "SystemError: ";
    msgerr += lpMsgBuf;
    msgerr += "with error code " + std::to_string(dwCode);
    LocalFree(lpMsgBuf);
    throw std::runtime_error(msgerr);
  }
}

uint64_t Process_launcher::get_fd_write() { return (uint64_t)child_in_wr; }

uint64_t Process_launcher::get_fd_read() { return (uint64_t)child_out_rd; }

void Process_launcher::kill() { close(); }

CmdExecResult cmd_exec(const std::string &cmd, bool include_stderr,
                       std::string working_dir, const std::string &env) {
  std::string orig_cwd;
  if (!working_dir.empty()) {
    orig_cwd = change_cwd(working_dir);
  }

  Process_launcher launcher(cmd.c_str(), include_stderr, env);

  char cmd_output[256];
  int nbytes;
  std::string output{};

  for (;;) {
    if ((nbytes = launcher.read(cmd_output, 256)) == EOF) {
      break;
    }
    output.append(cmd_output, nbytes);
  }

  if (!orig_cwd.empty()) {
    change_cwd(orig_cwd);
  }

  int code = launcher.wait();
  return CmdExecResult{output, code, 0};
}
