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
#include <sstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

template <typename Out>
void split_str(const std::string &input, Out result, char delim = ' ') {
  std::stringstream ss;
  ss.str(input);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

std::vector<std::string> split_str(const std::string &s, char delim = ' ') {
  std::vector<std::string> elems;
  split_str(s, std::back_inserter(elems), delim);
  return elems;
}

}  // namespace

int ProcessWrapper::wait_for_exit(std::chrono::milliseconds timeout) {
  if (exit_code_set_) return exit_code();

  // wait_for_exit() is a convenient short name, but a little unclear with
  // respect to what this function actually does
  return wait_for_exit_while_reading_and_autoresponding_to_output(timeout);
}

int ProcessWrapper::kill() {
  try {
    exit_code_ = launcher_.kill();
    exit_code_set_ = true;
  } catch (std::exception &e) {
    fprintf(stderr, "failed killing process %s: %s\n",
            launcher_.get_cmd_line().c_str(), e.what());
    return 1;
  }

  return exit_code_;
}

int ProcessWrapper::wait_for_exit_while_reading_and_autoresponding_to_output(
    std::chrono::milliseconds timeout) {
  namespace ch = std::chrono;
  auto step = 1ms;
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
    step *= 200;
  }
  ch::time_point<ch::steady_clock> timeout_timestamp =
      ch::steady_clock::now() + timeout;

  // We alternate between non-blocking read() and non-blocking waitpid() here.
  // Reading/autoresponding must be done, because the child might be blocked on
  // them (for example, it might block on password prompt), and therefore won't
  // exit until we deal with its output.
  std::exception_ptr eptr;
  exit_code_set_ = false;
  do {
    read_and_autorespond_to_output(0ms);

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
    while (read_and_autorespond_to_output(step, false))
      ;  // false = disable autoresponder
    return exit_code_;
  } else {
    // we timed out waiting for child
    std::rethrow_exception(eptr);
  }
}

bool ProcessWrapper::expect_output(const std::string &str, bool regex,
                                   std::chrono::milliseconds timeout) {
  auto now = std::chrono::steady_clock::now();
  auto until = now + timeout;
  for (;;) {
    if (output_contains(str, regex)) return true;

    now = std::chrono::steady_clock::now();

    if (now > until) {
      return false;
    }

    if (!read_and_autorespond_to_output(
            std::chrono::duration_cast<std::chrono::milliseconds>(until - now)))
      return false;
  }
}

bool ProcessWrapper::output_contains(const std::string &str, bool regex) const {
  if (!regex) {
    return execute_output_raw_.find(str) != std::string::npos;
  }

  // regex
  return pattern_found(execute_output_raw_, str);
}

bool ProcessWrapper::read_and_autorespond_to_output(
    std::chrono::milliseconds timeout, bool autoresponder_enabled /*= true*/) {
  char cmd_output[kReadBufSize] = {
      0};  // hygiene (cmd_output[bytes_read] = 0 would suffice)

  // blocks until timeout expires (very likely) or until at least one byte is
  // read (unlikely) throws std::runtime_error on read error
  int bytes_read =
      launcher_.read(cmd_output, kReadBufSize - 1,
                     timeout);  // cmd_output may contain multiple lines

  if (bytes_read > 0) {
#ifdef _WIN32
    // On Windows we get \r\n instead of \n, so we need to get rid of the \r
    // everywhere. As surprising as it is, WIN32API doesn't provide the
    // automatic conversion:
    // https://stackoverflow.com/questions/18294650/win32-changing-to-binary-mode-childs-stdout-pipe
    {
      char *new_end = std::remove(cmd_output, cmd_output + bytes_read, '\r');
      *new_end = '\0';
      bytes_read = new_end - cmd_output;
    }
#endif

    execute_output_raw_ += cmd_output;

    if (autoresponder_enabled)
      autorespond_to_matching_lines(bytes_read, cmd_output);

    return true;
  } else {
    return false;
  }
}

void ProcessWrapper::autorespond_to_matching_lines(int bytes_read,
                                                   char *cmd_output) {
  // returned lines do not contain the \n
  std::vector<std::string> lines =
      split_str(std::string(cmd_output, cmd_output + bytes_read), '\n');
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
  for (const auto &response : output_responses_) {
    const std::string &output = response.first;
    if (line.substr(0, output.size()) == output) {
      const char *resp = response.second.c_str();
      launcher_.write(resp, strlen(resp));
      return true;
    }
  }

  return false;
}
