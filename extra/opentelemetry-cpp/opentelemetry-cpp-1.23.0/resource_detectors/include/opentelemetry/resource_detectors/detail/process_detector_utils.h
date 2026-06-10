// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace resource_detector
{
namespace detail
{

/**
 * Forms a file path for a process type based on the given PID.
 * for example - /proc/<pid>/cmdline, /proc/<pid>/exe
 */
std::string FormFilePath(const int32_t &pid, const char *process_type);

/**
 * Retrieves the absolute file system path to the executable for a given PID.
 * Platform-specific behavior:
 *   - Windows: Uses OpenProcess() + GetProcessImageFileNameW().
 *   - Linux/Unix: Reads the /proc/<pid>/exe symbolic link.
 *   - TODO: Need to implement for Darwin
 *
 * @param pid Process ID.
 */
std::string GetExecutablePath(const int32_t &pid);

/**
 * Extracts the command-line arguments and the command.
 * Platform-specific behavior:
 *   - Windows: Uses CommandLineToArgvW() to parse the command line.
 *   - Linux/Unix: Reads the /proc/<pid>/cmdline file and splits it into command and arguments.
 *   - TODO: Need to implement for Darwin
 */
std::vector<std::string> ExtractCommandWithArgs(const std::string &command_line_path);

/**
 * Retrieves the command-line arguments and the command used to launch the process for a given PID.
 * This function is a wrapper around ExtractCommandWithArgs() and is provided for convenience and
 * testability of ExtractCommandWithArgs().
 */
std::vector<std::string> GetCommandWithArgs(const int32_t &pid);

}  // namespace detail
}  // namespace resource_detector
OPENTELEMETRY_END_NAMESPACE
