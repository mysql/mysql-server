// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/resource_detectors/detail/process_detector_utils.h"

#include <fstream>
#include <string>
#include <vector>

#ifdef _MSC_VER
// clang-format off
#  include <windows.h>
#  include <psapi.h>
#  include <shellapi.h>
#  pragma comment(lib, "shell32.lib")
// clang-format on
#else
#  include <sys/types.h>
#  include <unistd.h>
#  include <cstdio>
#endif

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace resource_detector
{
namespace detail
{

constexpr const char *kExecutableName = "exe";
constexpr const char *kCmdlineName    = "cmdline";

std::string GetExecutablePath(const int32_t &pid)
{
#ifdef _MSC_VER
  HANDLE hProcess =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
  if (!hProcess)
  {
    return std::string();
  }

  WCHAR wbuffer[MAX_PATH];
  DWORD len = GetProcessImageFileNameW(hProcess, wbuffer, MAX_PATH);
  CloseHandle(hProcess);

  if (len == 0)
  {
    return std::string();
  }

  // Convert UTF-16 to UTF-8
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wbuffer, len, NULL, 0, NULL, NULL);
  std::string utf8_path(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wbuffer, len, &utf8_path[0], size_needed, NULL, NULL);

  return utf8_path;
#else
  std::string path = FormFilePath(pid, kExecutableName);
  char buffer[4096];

  ssize_t len = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
  if (len != -1)
  {
    buffer[len] = '\0';
    return std::string(buffer);
  }

  return std::string();
#endif
}

std::vector<std::string> ExtractCommandWithArgs(const std::string &command_line_path)
{
  std::vector<std::string> commands;
  std::ifstream command_line_file(command_line_path, std::ios::in | std::ios::binary);
  std::string command;
  while (std::getline(command_line_file, command, '\0'))
  {
    if (!command.empty())
    {
      commands.push_back(command);
    }
  }
  return commands;
}

std::vector<std::string> GetCommandWithArgs(const int32_t &pid)
{
#ifdef _MSC_VER
  int argc      = 0;
  LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argvW)
  {
    return {};  // returns an empty vector if CommandLineToArgvW fails
  }

  std::vector<std::string> args;
  for (int i = 0; i < argc; i++)
  {
    // Convert UTF-16 to UTF-8
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
    if (size_needed > 0)
    {
      std::string arg(size_needed - 1, 0);
      WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &arg[0], size_needed, NULL, NULL);
      args.push_back(arg);
    }
  }

  LocalFree(argvW);
  return args;
#else
  std::string command_line_path = FormFilePath(pid, kCmdlineName);
  return ExtractCommandWithArgs(command_line_path);
#endif
}

std::string FormFilePath(const int32_t &pid, const char *process_type)
{
  char buff[64];
  int len = std::snprintf(buff, sizeof(buff), "/proc/%d/%s", pid, process_type);
  if (len < 0)
  {
    // in case snprintf fails
    return std::string();
  }
  if (len >= static_cast<int>(sizeof(buff)))
  {
    return std::string(buff, sizeof(buff) - 1);
  }
  return std::string(buff, len);
}

}  // namespace detail
}  // namespace resource_detector
OPENTELEMETRY_END_NAMESPACE
