/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_disk_forecaster.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "health_monitor.h"
#include "health_monitor_log.h"
#include "health_monitor_util.h"

namespace hm {

/**
  Initialize the unit test.
  Assumes run_mode = TEST
  @return 0 if test initialized, 1 otherwise
*/
int Disk_Forecaster::initialize_test() {
  if (m_test_filename.empty()) {
    log_info_message("[TEST] Error: Invalid unit test filename");
    return 1;
  }
  return load_unit_test_data();
}

/**
  Load the unit test data into the disk metrics buffer.
  @return 0 if successful, non-zero otherwise
*/
int Disk_Forecaster::load_unit_test_data() {
  const std::string base =
      std::filesystem::path(m_test_filename).filename().string();
  log_info_message("[TEST] Loading unit test file: " + base, false);
  // Read the unit test file and load into a vector of Disk_Metric
  std::vector<Disk_Metric> disk_metrics;
  int ret{read_unit_test_file(disk_metrics)};
  if (ret == 0) {
    // Disable writes from the Disk Collector and clear old data
    disk_metric_buffer()->set_writeable(false);
    {
      rwlock_guard metric_lock(disk_metric_buffer()->get_rwlock(), Lock_Write,
                               __FILE__, __LINE__);
      disk_metric_buffer()->reset_no_lock();
      // Load the test data into the disk_metric_buffer
      log_info_message("[TEST] Writing " + std::to_string(disk_metrics.size()) +
                           " disk metrics to the time series buffer",
                       false);
      for (const auto &disk_metric : disk_metrics) {
        disk_metric_buffer()->write_impl(disk_metric);
      }
    }
  }
  return ret;
}

/**
  Read the unit test file and populate the disk metrics buffer.

  @param disk_metrics  Vector to hold the parsed disk metrics
  @return 0 if successful, non-zero otherwise
*/
int Disk_Forecaster::read_unit_test_file(
    std::vector<Disk_Metric> &disk_metrics) {
  std::ifstream file(m_test_filename);
  if (!file) {
    log_info_message(
        "[TEST] Error: Cannot open test data file " + m_test_filename, false);
    return 1;
  }

  auto trim = [](std::string &s) {
    if (s.empty()) {
      return;
    }
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    s = (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
  };

  // Parsed header fields
  std::string test_name;
  std::string test_type;
  std::string timezone;
  std::string description;
  int format_version{1};
  int resolution_minutes{0};
  int season_minutes{0};
  double alpha{0.0}, beta{0.0}, gamma{0.0};

  bool found_test_type{false};
  bool found_resolution{false};
  bool found_season{false};
  bool found_alpha{false}, found_beta{false}, found_gamma{false};
  bool in_data{false};

  disk_metrics.clear();

  std::string line;
  while (std::getline(file, line)) {
    // Trim line
    trim(line);
    if (line.empty()) {
      continue;
    }
    // Data region handling
    if (line == "START_DATA") {
      in_data = true;
      continue;
    }
    if (line == "END_DATA") {
      break;  // stop reading after END_DATA
    }

    if (!in_data) {
      // Header section:
      // Ignore comment/section header lines
      if (!line.empty() && line[0] == '#') {
        continue;
      }

      // Expect "KEY: VALUE" format
      auto pos = line.find(':');
      if (pos == std::string::npos) {
        // Unknown free-form line in header; ignore
        continue;
      }
      std::string key = line.substr(0, pos);
      std::string val = line.substr(pos + 1);
      trim(key);
      trim(val);

      // Parse recognized keys (ignore unknown keys)
      if (key == "TEST_NAME") {
        test_name = val;
      } else if (key == "TEST_TYPE") {
        test_type = val;
        found_test_type = true;
      } else if (key == "DESCRIPTION") {
        description = val;
      } else if (key == "TIMEZONE") {
        timezone = val;  // advisory; assumes UTC as agreed
      } else if (key == "TEST_FORMAT_VERSION") {
        // Best-effort parse
        try {
          format_version = std::stoi(val);
        } catch (...) { /* ignore */
        }
      } else if (key == "RESOLUTION_MINUTES") {
        try {
          resolution_minutes = std::stoi(val);
          found_resolution = true;
        } catch (...) {
          found_resolution = false;
        }
      } else if (key == "SEASON_MINUTES") {
        try {
          season_minutes = std::stoi(val);
          found_season = true;
        } catch (...) {
          found_season = false;
        }
      } else if (key == "ALPHA") {
        try {
          alpha = std::stod(val);
          found_alpha = true;
        } catch (...) {
          found_alpha = false;
        }
      } else if (key == "BETA") {
        try {
          beta = std::stod(val);
          found_beta = true;
        } catch (...) {
          found_beta = false;
        }
      } else if (key == "GAMMA") {
        try {
          gamma = std::stod(val);
          found_gamma = true;
        } catch (...) {
          found_gamma = false;
        }
      } else if (key == "SERIES_LENGTH") {
        // Optional; can validate against data row count later
      }

      continue;
    }  // !in_data

    // Data section: parse CSV "timestamp, volume_size_mb, used_mb"
    {
      std::istringstream iss(line);
      std::string ts_str, vol_str, used_str;
      if (std::getline(iss, ts_str, ',') && std::getline(iss, vol_str, ',') &&
          std::getline(iss, used_str)) {
        trim(ts_str);
        trim(vol_str);
        trim(used_str);
        time_t ts{};
        if (utl::convert_timestamp(ts_str, ts) != 0) {
          log_info_message(
              std::string("[TEST] Error: Bad timestamp: ") + ts_str, false);
          return 1;
        }
        double volume_size_mb{};
        double used_mb{};
        try {
          volume_size_mb = std::stod(vol_str);
          used_mb = std::stod(used_str);
        } catch (...) {
          log_info_message(
              std::string("[TEST] Error: Non-numeric volume/used on line: ") +
                  line,
              false);
          return 1;
        }
        Disk_Metric metric;
        metric.timestamp = ts;
        metric.total_mb = volume_size_mb;
        metric.used_mb = used_mb;
        disk_metrics.push_back(metric);
      } else {
        log_info_message(
            std::string("[TEST] Error: Malformed data line: ") + line, false);
        return 1;
      }
    }
  }  // while getline

  // Validate required header fields
  if (!found_test_type || test_type != "UNIT") {
    log_info_message("[TEST] Error: TEST_TYPE must be present and 'UNIT'",
                     false);
    return 1;
  }
  if (!found_resolution || resolution_minutes != 1) {
    log_info_message(
        "[TEST] Error: RESOLUTION_MINUTES must be present and equal to 1",
        false);
    return 1;
  }
  if (!found_season || season_minutes <= 0) {
    log_info_message("[TEST] Error: SEASON_MINUTES missing or invalid", false);
    return 1;
  }
  if (!(found_alpha && found_beta && found_gamma)) {
    log_info_message(
        "[TEST] Error: ALPHA/BETA/GAMMA must be present and numeric", false);
    return 1;
  }
  if (disk_metrics.empty()) {
    log_info_message(
        "[TEST] Error: No data rows found between START_DATA and END_DATA",
        false);
    return 1;
  }

// TODO_LATER: Apply parsed params to the forecaster (useful for deterministic
// tests) Acquire write lock if needed; this just sets parameters before
// initialize/advance.
#if 0
  {
    // No lock needed to set static fields; if you share state across threads, guard appropriately.
    // Adjust the model's seasonal period and smoothing parameters for the test.
    rwlock_wrlock(Disk_Forecaster::model_lock(), **FILE**, __LINE__);
    forecaster().state().m = season_minutes;
    forecaster().state().alpha = alpha;
    forecaster().state().beta = beta;
    forecaster().state().gamma = gamma;
    rwlock_unlock(Disk_Forecaster::model_lock());
  }
#endif

  // Build and emit a summary log
  const time_t first_ts = disk_metrics.front().timestamp;
  std::tm *tm_ptr = std::gmtime(&first_ts);
  char buf[32];
  if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_ptr) == 0) {
    strncpy(buf, "<invalid time>", sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
  }
  std::ostringstream msg;
  msg << "[TEST] Data Load "
      << "Start: " << buf
      << ", Name: " << (test_name.empty() ? "<unnamed>" : test_name)
      << ", Type: " << test_type << ", Version: " << format_version
      << ", Resolution: " << resolution_minutes << " min"
      << ", Season: " << season_minutes << " min"
      << ", Parameterss: alpha=" << alpha << ", beta=" << beta
      << ", gamma=" << gamma << ", Rows: " << disk_metrics.size();
  log_info_message(msg.str(), false);

  return 0;
}

/**
  Run the unit test: Generate forecasts from the uploaded disk metric data,
  update the forecast tables with results, write the forecast report to the
  diagnostics log.
*/
void Disk_Forecaster::run_unit_test() {
  rwlock_guard metric_lock(disk_metric_buffer()->get_rwlock(), Lock_Read,
                           __FILE__, __LINE__);
  std::vector<Disk_Forecast> forecasts;
  Disk_Forecast_Summary summary;

  log_info_message("[TEST] Resetting forecast model", false);
  {
    rwlock_guard model_lock(Disk_Forecaster::model_lock(), Lock_Write, __FILE__,
                            __LINE__);
    forecaster().reset_model();
  }

  log_info_message("[TEST] Generating forecast", false);
  generate_forecast(forecasts, summary);

  log_info_message("[TEST] Updating forecast tables", false);
  update_forecast_tables(forecasts, summary);
  save_forecast_status(forecasts, summary);

  log_forecast_status(forecasts, summary);

  // Send results to error log
  for (const auto &forecast : forecasts) {
    std::string forecast_msg = format_disk_forecast(forecast);
    log_info_message(forecast_msg, false);
  }
  std::string summary_msg = format_forecast_summary(summary);
  log_info_message(summary_msg, false);

  // Send report to diagnostics log
  // TODO: Format for error log
  // std::string report = generate_report(forecasts, summary);
  // log_diag(report.c_str());
  std::ignore = generate_report(forecasts, summary);

  log_info_message("[TEST] Resetting forecast model", false);
  {
    rwlock_guard model_lock(Disk_Forecaster::model_lock(), Lock_Write, __FILE__,
                            __LINE__);
    forecaster().reset_model();
  }
}

/**
  @brief Runs the unit test driven by the test data file.

  @return 0 if successful, non-zero otherwise
*/
int Disk_Forecaster::process_test_cycle() {
  std::lock_guard<std::mutex> series_lock(m_series_mutex);
  // Set the test state status
  set_test_state(Dfx_Test_State::RUNNING);

  // Read and process test data input files
  int test_init = initialize_test();
  if (test_init != 0) {
    // Test initialization failed, back to normal operation
    log_info_message("[TEST] Test initialization failed", false);
    set_test_state(Dfx_Test_State::IDLE);
    set_run_mode(Dfx_Run_Mode::NORMAL);
    return 1;
  }

  // Run the test then go to STANDBY mode
  run_unit_test();
  log_info_message("[TEST] Setting run_mode: STANDBY", false);
  set_run_mode(Dfx_Run_Mode::STANDBY);

  // Indicate that the test has completed
  set_test_state(Dfx_Test_State::IDLE);
  return 0;
}

}  // namespace hm
