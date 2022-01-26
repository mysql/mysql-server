/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "process_wrapper.h"

#include <algorithm>
#include <array>
#include <thread>
#include <vector>

#include "mysql/harness/string_utils.h"  // split_string

using namespace std::chrono_literals;

ProcessWrapper::ProcessWrapper(
    const std::string &app_cmd, const std::vector<std::string> &args,
    const std::vector<std::pair<std::string, std::string>> &env_vars,
    bool include_stderr, OutputResponder &output_responder)
    : launcher_(app_cmd.c_str(), args, env_vars, include_stderr),
      output_responder_(output_responder) {
  launcher_.start();
  output_reader_ = std::thread([&]() {
    while (!output_reader_stop_) {
      try {
        read_and_autorespond_to_output(5ms);
        if (output_reader_stop_) break;
        std::this_thread::sleep_for(5ms);
      } catch (const std::system_error &e) {
        if (std::errc::resource_unavailable_try_again == e.code() ||
            std::errc::permission_denied == e.code()) {
          continue;
        }

        // if the underlying process went away we may get "Bad file descriptor"
        // exception here
        if (std::errc::bad_file_descriptor == e.code() ||
            std::errc::invalid_argument == e.code()) {
          break;
        }
        throw;
      }
    }
  });
}

int ProcessWrapper::kill() {
  try {
    exit_code_ = launcher_.kill();
    stop_output_reader_thread();
    exit_code_set_ = true;
  } catch (std::exception &e) {
    fprintf(stderr, "failed killing process %s: %s\n",
            launcher_.get_cmd_line().c_str(), e.what());
    return 1;
  }

  return exit_code_;
}

int ProcessWrapper::wait_for_exit(std::chrono::milliseconds timeout) {
  if (exit_code_set_) return exit_code();
  namespace ch = std::chrono;
  auto step = 1ms;
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
    step *= 200;
  }
  ch::time_point<ch::steady_clock> timeout_timestamp =
      ch::steady_clock::now() + timeout;

  // The child might be blocked on input/output (for example password prompt),
  // so we wait giving a output thread a change to deal with it
  std::exception_ptr eptr;
  exit_code_set_ = false;
  do {
    try {
      // throws std::runtime_error or std::system_error
      exit_code_ = launcher_.wait(0ms);
      exit_code_set_ = true;
      break;
    } catch (const std::system_error &e) {
      eptr = std::current_exception();

      if (e.code() != std::errc::timed_out) {
        break;
      }
    } catch (const std::runtime_error &) {
      eptr = std::current_exception();
      break;
    }

    std::this_thread::sleep_for(step);
  } while (ch::steady_clock::now() < timeout_timestamp);

  if (exit_code_set_) {
    // the child exited, but there might still be some data left in the pipe to
    // read, so let's consume it all
    stop_output_reader_thread();
    while (read_and_autorespond_to_output(step,
                                          /*autoresponder_enabled=*/false))
      ;
    return exit_code_;
  } else {
    // we timed out waiting for child
    std::rethrow_exception(eptr);
  }
}

bool ProcessWrapper::expect_output(const std::string &str, bool regex,
                                   std::chrono::milliseconds timeout) {
  auto step = 5ms;
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
    step *= 10;
  }

  auto now = std::chrono::steady_clock::now();
  auto until = now + timeout;
  for (;;) {
    if (output_contains(str, regex)) return true;

    now = std::chrono::steady_clock::now();

    if (now > until) {
      return false;
    }

    std::this_thread::sleep_for(step);
  }
}

bool ProcessWrapper::output_contains(const std::string &str, bool regex) const {
  std::lock_guard<std::mutex> output_lock(output_mtx_);

  if (!regex) {
    return execute_output_raw_.find(str) != std::string::npos;
  }

  // regex
  return pattern_found(execute_output_raw_, str);
}

bool ProcessWrapper::read_and_autorespond_to_output(
    std::chrono::milliseconds timeout, bool autoresponder_enabled /*= true*/) {
  std::array<char, kReadBufSize> read_buf = {0};

  // blocks until timeout expires (very likely) or until at least one byte is
  // read (unlikely) throws std::runtime_error on read error
  int bytes_read =
      launcher_.read(read_buf.data(), read_buf.size() - 1,
                     timeout);  // cmd_output may contain multiple lines

  if (bytes_read <= 0) return false;

#ifdef _WIN32
  // On Windows we get \r\n instead of \n, so we need to get rid of the \r
  // everywhere. As surprising as it is, WIN32API doesn't provide the
  // automatic conversion:
  // https://stackoverflow.com/questions/18294650/win32-changing-to-binary-mode-childs-stdout-pipe
  {
    char *new_end =
        std::remove(read_buf.data(), read_buf.data() + bytes_read, '\r');
    *new_end = '\0';
    bytes_read = new_end - read_buf.data();
  }
#endif

  std::string_view cmd_output(read_buf.data(), bytes_read);
  {
    std::lock_guard<std::mutex> output_lock(output_mtx_);
    execute_output_raw_ += cmd_output;
  }

  if (autoresponder_enabled) {
    autorespond_to_matching_lines(cmd_output);
  }

  return true;
}

void ProcessWrapper::autorespond_to_matching_lines(
    const std::string_view &cmd_output) {
  // returned lines do not contain the \n
  std::vector<std::string> lines =
      mysql_harness::split_string(cmd_output, '\n');
  if (lines.empty()) return;

  // it is possible that the last line from the previous call did not match
  // because it arrived incomplete. Here we try an assumption that the first
  // line is a continuation of last line from previous call.
  if (last_line_read_.size() &&
      autorespond_on_matching_pattern(last_line_read_ + lines.front())) {
    // indeed, it was a continuation of previous line. So now we must prevent
    // both fragments from being used again
    lines.erase(lines.begin());
    last_line_read_.clear();

    if (lines.empty()) return;
  }

  // try matching all but last line
  for (auto it = lines.cbegin(); it != lines.cend() - 1; ++it)
    autorespond_on_matching_pattern(*it);

  // try matching the last line
  if (autorespond_on_matching_pattern(lines.back()))
    last_line_read_.clear();
  else
    // last line failed to match, it may be because it arrived incomplete. Save
    // it for the next time
    last_line_read_ = lines.back();
}

bool ProcessWrapper::autorespond_on_matching_pattern(const std::string &line) {
  const std::string resp = output_responder_(line);
  if (!resp.empty()) {
    launcher_.write(resp.c_str(), resp.length());
    return true;
  }

  return false;
}
