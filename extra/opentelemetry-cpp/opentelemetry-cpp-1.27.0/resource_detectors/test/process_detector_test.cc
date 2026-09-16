// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <fstream>
#include <string>
#include <vector>

#ifdef _MSC_VER
// clang-format off
#  include <process.h>
#  include <windows.h>
#  include <psapi.h>
#  define getpid _getpid
// clang-format on
#else
#  include <sys/types.h>
#  include <unistd.h>
#  include <cstdio>
#endif

#include "opentelemetry/resource_detectors/detail/process_detector_utils.h"

TEST(ProcessDetectorUtilsTest, FormFilePath)
{
  int32_t pid              = 1234;
  std::string cmdline_path = opentelemetry::resource_detector::detail::FormFilePath(pid, "cmdline");
  std::string exe_path     = opentelemetry::resource_detector::detail::FormFilePath(pid, "exe");

  EXPECT_EQ(cmdline_path, "/proc/1234/cmdline");
  EXPECT_EQ(exe_path, "/proc/1234/exe");
}

TEST(ProcessDetectorUtilsTest, ExtractCommandWithArgs)
{
  std::string filename{"test_command_args.txt"};

  {
    std::ofstream outfile(filename, std::ios::binary);
    const char raw_data[] = "test_command\0arg1\0arg2\0arg3\0";
    outfile.write(raw_data, sizeof(raw_data) - 1);
  }

  std::vector<std::string> args =
      opentelemetry::resource_detector::detail::ExtractCommandWithArgs(filename);
  EXPECT_EQ(args, (std::vector<std::string>{"test_command", "arg1", "arg2", "arg3"}));

  std::remove(filename.c_str());  // Cleanup
}

TEST(ProcessDetectorUtilsTest, EmptyCommandWithArgsFile)
{
  std::string filename{"empty_command_args.txt"};
  std::ofstream outfile(filename, std::ios::binary);
  outfile.close();

  std::vector<std::string> args =
      opentelemetry::resource_detector::detail::ExtractCommandWithArgs(filename);
  EXPECT_TRUE(args.empty());

  std::remove(filename.c_str());  // Cleanup
}

TEST(ProcessDetectorUtilsTest, GetExecutablePathTest)
{
  int32_t pid = getpid();
  std::string path;
#ifdef _MSC_VER
  HANDLE hProcess =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
  if (!hProcess)
  {
    path = std::string();
  }
  else
  {

    WCHAR wbuffer[MAX_PATH];
    DWORD len = GetProcessImageFileNameW(hProcess, wbuffer, MAX_PATH);
    CloseHandle(hProcess);

    if (len == 0)
    {
      path = std::string();
    }
    else
    {
      int size_needed = WideCharToMultiByte(CP_UTF8, 0, wbuffer, len, NULL, 0, NULL, NULL);
      std::string utf8_path(size_needed, 0);
      WideCharToMultiByte(CP_UTF8, 0, wbuffer, len, &utf8_path[0], size_needed, NULL, NULL);

      path = utf8_path;
    }
  }
#else
  std::string exe_path = opentelemetry::resource_detector::detail::FormFilePath(pid, "exe");
  char buffer[4096];

  ssize_t len = readlink(exe_path.c_str(), buffer, sizeof(buffer) - 1);
  if (len != -1)
  {
    buffer[len] = '\0';
    path        = std::string(buffer);
  }
  else
  {
    path = std::string();
  }
#endif
  std::string expected_path = opentelemetry::resource_detector::detail::GetExecutablePath(pid);
  EXPECT_EQ(path, expected_path);
}

TEST(ProcessDetectorUtilsTest, CommandTest)
{
  int32_t pid = getpid();
  std::string command;
#ifdef _MSC_VER
  int argc      = 0;
  LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);

  if (argvW && argc > 0)
  {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, argvW[0], -1, NULL, 0, NULL, NULL);
    if (size_needed > 0)
    {
      std::string arg(size_needed - 1, 0);
      WideCharToMultiByte(CP_UTF8, 0, argvW[0], -1, &arg[0], size_needed, NULL, NULL);
      command = arg;
    }

    LocalFree(argvW);
  }
  else
  {
    command = std::string();
  }
#else
  std::string command_line_path =
      opentelemetry::resource_detector::detail::FormFilePath(pid, "cmdline");
  std::ifstream command_line_file(command_line_path, std::ios::in | std::ios::binary);
  std::getline(command_line_file, command, '\0');
#endif
  std::vector<std::string> expected_command_with_args =
      opentelemetry::resource_detector::detail::GetCommandWithArgs(pid);
  std::string expected_command;
  if (!expected_command_with_args.empty())
  {
    expected_command = expected_command_with_args[0];
  }
  EXPECT_EQ(command, expected_command);
}

TEST(ProcessDetectorUtilsTest, GetCommandWithArgsTest)
{
  int32_t pid = getpid();
  std::vector<std::string> args;
#ifdef _MSC_VER
  int argc      = 0;
  LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argvW)
  {
    args = {};
  }
  else
  {
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
  }

  LocalFree(argvW);
#else
  std::string command_line_path =
      opentelemetry::resource_detector::detail::FormFilePath(pid, "cmdline");
  args = opentelemetry::resource_detector::detail::ExtractCommandWithArgs(command_line_path);
#endif
  std::vector<std::string> expected_args =
      opentelemetry::resource_detector::detail::GetCommandWithArgs(pid);
  EXPECT_EQ(args, expected_args);
}
