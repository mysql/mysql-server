/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>

using std::chrono::duration;
using std::chrono::nanoseconds;
using std::chrono::steady_clock;

static bool timer_running = false;
static double seconds_used;
static steady_clock::time_point timer_start;
static size_t bytes_processed = 0;

void StartBenchmarkTiming() {
  assert(!timer_running);
  timer_running = true;
  timer_start = steady_clock::now();
}

void StopBenchmarkTiming() {
  if (timer_running) {
    auto used = steady_clock::now() - timer_start;
    seconds_used += duration<double>(used).count();
    timer_running = false;
  }
}

void SetBytesProcessed(size_t bytes) { bytes_processed = bytes; }

void internal_do_microbenchmark(const char *name, void (*func)(size_t)) {
  // There's no point in timing in debug mode, so just run 10 times
  // so that we don't waste build time (this should give us enough runs
  // to verify that we don't crash).
  // Similarly for Running in pushbuild or Jenkins: results are irrelevant.
  bool skip_benchmarking = false;
#if !defined(NDEBUG)
  printf(
      "WARNING: Running microbenchmark in debug mode. "
      "Timings will be misleading.\n");
  skip_benchmarking = true;
#endif
  char *env_pb2workdir = std::getenv("PB2WORKDIR");
  char *env_jenkins_url = std::getenv("JENKINS_URL");
  if (env_pb2workdir != nullptr) {
    printf("WARNING: running in PB2, skipping benchmarking.\n");
    skip_benchmarking = true;
  }
  if (env_jenkins_url != nullptr) {
    printf("WARNING: running in Jenkins, skipping benchmarking.\n");
    skip_benchmarking = true;
  }

  // Do 100 iterations as rough calibration. (Often, this will over- or
  // undershoot by as much as 50%, but that's fine.)
  static constexpr size_t calibration_iterations = 100;
  static constexpr size_t num_skip_iterations = 10;
  seconds_used = 0.0;
  StartBenchmarkTiming();
  if (skip_benchmarking)
    func(num_skip_iterations);
  else
    func(calibration_iterations);
  StopBenchmarkTiming();
  double seconds_used_per_iteration = seconds_used / calibration_iterations;

  size_t num_iterations;

  // Do the actual run, unless we already took more than one second.
  if (!skip_benchmarking && seconds_used < 1.0) {
    // Scale so that we end up around one second per benchmark
    // (but never less than 100).
    num_iterations =
        std::max<size_t>(lrint(1.0 / seconds_used_per_iteration), 100);
    seconds_used = 0.0;
    StartBenchmarkTiming();
    func(num_iterations);
    StopBenchmarkTiming();
  } else {
    // The calibration already took too long, so just reuse its results.
    if (skip_benchmarking)
      num_iterations = num_skip_iterations;
    else
      num_iterations = calibration_iterations;
  }
  printf("%-40s %10ld iterations %10.0f ns/iter", name,
         static_cast<long>(num_iterations),
         1e9 * seconds_used / double(num_iterations));

  if (bytes_processed > 0) {
    double bytes_per_second = bytes_processed / seconds_used;
    if (bytes_per_second > (512 << 20))  // 0.5 GB/sec.
      printf(" %8.2f GB/sec", bytes_per_second / (1 << 30));
    else
      printf(" %8.2f MB/sec", bytes_per_second / (1 << 20));
    bytes_processed = 0;  // Reset for next test.
  }

  printf("\n");
}
