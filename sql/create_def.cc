/*****************************************************************************

  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

*****************************************************************************/

// This program creates a def file with all exports from specified libraries.
// This work is based on a JS script that was doing this before this program was
// created, and tries to closely copy the way the script was working. In essence
// we create a input file for link /dump, call link on this input with stdout
// redirected to big pipe that we read and process into set of unique symbols
// that matches our criteria. After link completes, we write out the gathered
// unique symbols. On Debug the link /dump generates ~280MB of output which is
// transformed into 8MB of the result def file, so it's quite important to have
// the processing fast. Currently the create_def consumes the data provided
// by the link two times faster than it takes to generate them, so the total
// execution time is still bound exactly by the link execution time.
// The original script removed the VS_UNICODE_OUTPUT environment variable used
// by the VC++ tools to communicate a need to use special pipes inside VS. This
// was used only sometime around VS2005-2008 and was removed.

#ifndef __clang__
// Disable runtime checks, iterator checks and debugs, and turn on
// optimizations. This is done to make debug version as fast as release one.
// Without this the create_def works even slower than original JS script. We
// can't enable this on CMake level (at least no way was found) because adding
// /O2 requires absence of /RTC1, which in turn can't be removed with CMake, and
// can't be overridden to be disabled by adding any compile options. Thus we
// have to disable /RTC1 and enable /O2 (or any level of optimization) using
// pragmas.
#ifdef _ITERATOR_DEBUG_LEVEL
#undef _ITERATOR_DEBUG_LEVEL
#endif

#define _ITERATOR_DEBUG_LEVEL 0
#pragma runtime_checks("", off)
#pragma optimize("g", on)
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <vector>

/** Prints an error message supplied and attaches GetLastError with formatted
 * message. */
void error(std::string message) {
  DWORD last_error = GetLastError();
  std::cerr << "Error during generating .def file: " << message << "\n"
            << "Last OS error code: " << last_error
            << ", msg: " << std::system_category().message((int)last_error)
            << std::endl;
  exit(1);
}

/** Gathers and prints out the unique symbols. */
class Unique_symbol_map {
 public:
  Unique_symbol_map() { std::cout << "EXPORTS\n"; }
  /** Processes a new symbol candidate in form of a single line of link /dump
   * output. */
  void insert(const std::string &symbol_line);

 private:
  /** List of symbols seen and printed out so far. */
  std::unordered_set<std::string> m_symbols_seen;
};

/** Buffers input and runs a specified callback on single complete lines found.
 */
template <class TCallback>
class Line_buffer {
 public:
  Line_buffer(TCallback &&line_handler) : m_line_handler(line_handler) {}
  /** Runs callback for last incomplete line if present. */
  ~Line_buffer();
  /** Adds a raw buffer bytes to our buffer and finds any new completed lines to
   * call callback on them. */
  void insert(const char *buffer, size_t size);

 private:
  TCallback m_line_handler;
  /** Current incomplete line. */
  std::string m_curr_buffer;
};

template <class TCallback>
Line_buffer<TCallback> create_line_buffer(TCallback &&callback) {
  return Line_buffer<TCallback>(std::forward<TCallback>(callback));
}

/** Runs a specified command line and calls a callback for all data that is
 * written by the child program to its standard output. The standard error is
 * redirected to parent standard error stream. There is no input stream
 * redirected. */
template <class TCallback>
class Process {
 public:
  Process(std::string cmd_line, TCallback &&input_handler, DWORD pipe_size);
  ~Process();

  double get_cpu_time() const;

 private:
  /** Creates a big pipe that will receive and buffer data coming from the
   * child process. */
  void create_pipe(DWORD pipe_size);
  /** Runs the actual child process */
  void create_process(std::string cmd_line);
  /** Reads the child results until the pipe is not closed. Runs callback for
   * all received data. */
  void read_output(TCallback &&input_handler);

  HANDLE m_stdout_read_pipe = INVALID_HANDLE_VALUE;
  HANDLE m_stdout_write_pipe = INVALID_HANDLE_VALUE;
  HANDLE m_process_handle = INVALID_HANDLE_VALUE;
};

template <class TCallback>
Process<TCallback> create_process(std::string cmd_line,
                                  TCallback &&input_handler, DWORD pipe_size) {
  return Process<TCallback>(cmd_line, std::forward<TCallback>(input_handler),
                            pipe_size);
}

template <class TCallback>
Process<TCallback>::Process(std::string cmd_line, TCallback &&input_handler,
                            DWORD pipe_size) {
  create_pipe(pipe_size);
  create_process(cmd_line);
  read_output(std::forward<TCallback>(input_handler));
}

template <class TCallback>
Process<TCallback>::~Process() {
  if (m_stdout_read_pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(m_stdout_read_pipe);
  }
  if (m_stdout_write_pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(m_stdout_write_pipe);
  }
  if (m_process_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(m_process_handle);
  }
}

template <class TCallback>
void Process<TCallback>::create_pipe(DWORD pipe_size) {
  SECURITY_ATTRIBUTES sec_attributes;

  // The write side of the pipe needs to be inheritable.
  sec_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_attributes.bInheritHandle = TRUE;
  sec_attributes.lpSecurityDescriptor = NULL;

  // We create buffer big enough to not make child process stall while we are
  // processing its older output.
  if (!CreatePipe(&m_stdout_read_pipe, &m_stdout_write_pipe, &sec_attributes,
                  pipe_size)) {
    error("CreatePipe failed");
  }

  // The read side does not have to be inheritable.
  if (!SetHandleInformation(m_stdout_read_pipe, HANDLE_FLAG_INHERIT, 0)) {
    error("SetHandleInformation failed on read pipe");
  }
}

template <class TCallback>
void Process<TCallback>::create_process(std::string cmd_line) {
  PROCESS_INFORMATION proc_info;
  STARTUPINFO start_info;

  ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
  ZeroMemory(&start_info, sizeof(STARTUPINFO));

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  start_info.hStdOutput = m_stdout_write_pipe;
  start_info.hStdInput = INVALID_HANDLE_VALUE;
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  // We cast away the constness as CreateProcess expects the non-const argument.
  // However, only the UNICODE CreateProcessW variant actually modifies the
  // memory supplied. Since this program assumes it is compiled with ANSI
  // support only (no use of TCHAR) we can just cast the const away.
  if (!CreateProcess(NULL, const_cast<char *>(cmd_line.c_str()), NULL, NULL,
                     TRUE, 0, NULL, NULL, &start_info, &proc_info)) {
    error("CreateProcess failed");
  } else {
    m_process_handle = proc_info.hProcess;
    CloseHandle(proc_info.hThread);
    CloseHandle(m_stdout_write_pipe);
    m_stdout_write_pipe = INVALID_HANDLE_VALUE;
  }
}

// A quite small buffer for reading incoming data. It doesn't have to be big as
// the pipe itself does the buffering for more incoming data.
constexpr DWORD buf_size = 16 * 1024;
char raw_buffer[buf_size];

template <class TCallback>
void Process<TCallback>::read_output(TCallback &&input_handler) {
  DWORD bytes_read;
  DWORD bytes_available;

  for (;;) {
    // Check if there is any data to be read.
    if (!PeekNamedPipe(m_stdout_read_pipe, NULL, 0, NULL, &bytes_available,
                       NULL)) {
      // This error is reported when the pipe is closed.
      if (GetLastError() != 109) {
        error("PeekNamedPipe failed");
      }
      return;
    }
    if (bytes_available) {
      // Read actual data from pipe, but no more than our small buffer.
      if (!ReadFile(m_stdout_read_pipe, raw_buffer,
                    std::min(bytes_available, buf_size), &bytes_read, NULL)) {
        error("ReadFile on child process output pipe failed");
      }
      input_handler(raw_buffer, bytes_read);
    } else {
      Sleep(1);
    }
  }
}

double filetime_to_sec(const FILETIME &f) {
  return (f.dwLowDateTime + (((uint64_t)f.dwHighDateTime) << 32)) / 10.0 /
         1000 / 1000;
}

double get_cpu_time(HANDLE process) {
  FILETIME kernel_time;
  FILETIME user_time;
  FILETIME creation_time;
  FILETIME exit_time;
  GetProcessTimes(process, &creation_time, &exit_time, &kernel_time,
                  &user_time);
  return filetime_to_sec(kernel_time) + filetime_to_sec(user_time);
}

template <class TCallback>
double Process<TCallback>::get_cpu_time() const {
  return ::get_cpu_time(m_process_handle);
}

double our_cpu_time() { return get_cpu_time(GetCurrentProcess()); }

template <class TCallback>
Line_buffer<TCallback>::~Line_buffer() {
  if (!m_curr_buffer.empty()) {
    m_line_handler(m_curr_buffer);
  }
}

template <class TCallback>
void Line_buffer<TCallback>::insert(const char *buffer, size_t size) {
  size_t last_index_added = 0;
  for (size_t i = 0; i < size; ++i) {
    if (buffer[i] == '\r' || buffer[i] == '\n') {
      m_curr_buffer.append(buffer + last_index_added, i - last_index_added);
      if (!m_curr_buffer.empty()) {
        m_line_handler(m_curr_buffer);
        m_curr_buffer.clear();
      }
      last_index_added = i + 1;
    }
  }
  m_curr_buffer.append(buffer + last_index_added, size - last_index_added);
}

void Unique_symbol_map::insert(const std::string &symbol_line) {
  // Some magic list of symbols we don't want to be exported.
  static const char *compiler_symbols[] = {
      "__real@",  //
      "__xmm@",   // SSE instruction set constants
      "_CTA2?",   // std::bad_alloc
      "_CTA3?",   // std::length_error
      "_CTA4?",   // std::ios_base::failure
      "_CTA5?",   // std::ios_base::failure
      "_CTA6?",  // boost::exception_detail::clone_impl<boost::exception_detail::error_info_injector<boost::bad_get>>
      "_CTA7?",  // boost::exception_detail::clone_impl<boost::exception_detail::error_info_injector<boost::bad_lexical_cast>>
      "_CTA8?AV?",  // bad_rational
      "_TI2?",      // std::bad_alloc
      "_TI3?",      // std::length_error
      "_TI4?",      // std::ios_base::failure
      "_TI5?",      // std::ios_base::failure
      "_TI6?",  // boost::exception_detail::clone_impl<boost::exception_detail::error_info_injector<boost::bad_get>>
      "_TI7?",  // boost::exception_detail::clone_impl<boost::exception_detail::error_info_injector<boost::bad_lexical_cast>>
      "_TI8?AV?",    // bad_rational
      "_RTC_",       //
      "??_C@_",      //
      "??_R",        //
      "??_7",        //
      "?_G",         // scalar deleting destructor
      "_VInfreq_?",  // special label (exception handler?) for Intel compiler
      "?_E",         // vector deleting destructor
      "<lambda_",    // anything that is lambda-related
  };
  if (symbol_line.find("External") == std::string::npos) {
    return;
  }
  // Parse the line into tokens separated by a space.
  std::vector<std::string> columns;
  size_t current_pos = 0;
  for (;;) {
    auto pos = symbol_line.find(' ', current_pos);
    if (pos == std::string::npos) break;
    if (pos != 0) {
      columns.push_back(symbol_line.substr(current_pos, pos - current_pos));
    }
    current_pos = pos + 1;
  }
  if (current_pos != symbol_line.size()) {
    columns.push_back(symbol_line.substr(current_pos));
  }

  if (columns.size() < 3) {
    return;
  }

  // Magic copied from JS script:
  // If the third column of link /dump /symbols output contains SECTx, the
  // symbol is defined in that section of the object file. If UNDEF appears, it
  // is not defined in that object and must be resolved elsewhere. BSS symbols
  // (like uninitialized arrays) appear to have non-zero second column.
  if (columns[2].substr(0, 4) != "SECT") {
    if (columns[2] == "UNDEF" && atol(columns[1].c_str()) == 0) {
      return;
    }
  }

  // Extract undecorated symbol names between "|" and next whitespace after it.
  size_t index = 0;
  while (index < columns.size() && columns[index] != "|") {
    index++;
  }
  if (index + 1 >= columns.size()) {
    error("Unexpected symbol line format: " + symbol_line);
  }
  // Extract the actual symbol name we care about and check it's not on list of
  // compiler's symbols.
  auto &symbol = columns[index + 1];
  for (auto &compiler_symbol : compiler_symbols) {
    if (symbol.find(compiler_symbol) != std::string::npos) {
      return;
    }
  }

  // Check if we have function or data.
  if (symbol_line.find("notype () ") == std::string::npos) {
    symbol.append(" DATA");
  }

  // Check if this is a function inside the std namespace
  if (symbol_line.find(" __cdecl std::") != std::string::npos) {
    return;
  }

  // Check if this symbol was seen before.
  auto res = m_symbols_seen.emplace(symbol);
  if (res.second) {
    std::cout << symbol << "\n";
  }
}

template <class TCallback>
double measure_execution_time(TCallback &&callback) {
  auto start_time = std::chrono::high_resolution_clock::now();

  callback();
  // Print info about the time used.
  auto end_time = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                               start_time)
             .count() /
         1000.0;
}

class Resp_file {
 public:
  Resp_file(int arguments_count, const char **arguments) {
    std::ofstream rspFile(get_name().c_str());
    rspFile << "/symbols \n";
    for (int i = 0; i < arguments_count; ++i) {
      std::string input(arguments[i]);
      if (input.size() > 4 && (input.substr(input.size() - 4) == ".lib" ||
                               input.substr(input.size() - 4) == ".obj")) {
        rspFile << "\"" << input << "\"\n";
      }
    }
  }
  ~Resp_file() {
    // Cleanup.
    _unlink(get_name().c_str());
  }
  std::string get_name() { return "dumpsymbols.rsp"; }
};

int main(int argc, const char *argv[]) {
  double link_cpu_time;
  auto time_in_sec = measure_execution_time([argc, argv, &link_cpu_time] {
    // Prepare the input file for the link /dump.
    std::cerr << "Creating def file..." << std::endl;
    Resp_file resp_file(argc - 1, argv + 1);

    // This should speed-up printing the result a little.
    std::iostream::sync_with_stdio(false);

    // Call the actual link /dump and process the data.
    Unique_symbol_map symbol_map;
    auto buffer = create_line_buffer(
        [&symbol_map](std::string &line) { symbol_map.insert(line); });
    auto process = create_process(
        "link /dump @" + resp_file.get_name(),
        [&buffer](const char *buf, size_t bytes_count) {
          buffer.insert(buf, bytes_count);
        },
        // use bigger pipe buffer to let link /dump to buffer data in case we
        // have some lag. The data comes from linker in rates of 10s MB/s.
        16 * 1024 * 1024);
    link_cpu_time = process.get_cpu_time();
  });

  std::cerr << std::setprecision(3) << "Creating def file finished in "
            << time_in_sec << "s. (We used " << our_cpu_time()
            << "s, link used " << link_cpu_time << "s CPU time)" << std::endl;

  return 0;
}
