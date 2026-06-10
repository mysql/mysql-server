/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_METADATA_LOGGER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_METADATA_LOGGER_H_

#include <list>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include "collector/mysql_cache_manager.h"
#include "helper/json/text_to.h"
#include "mrs/configuration.h"
#include "mrs/database/query_version.h"
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"

namespace mrs {
namespace database {

class BufferedLogger {
 public:
  using LogRecord = mysql_harness::logging::Record;
  using LogRecordBuffer = std::queue<LogRecord>;

  void log(const LogRecord &record);

  virtual ~BufferedLogger() {}

 protected:
  virtual bool flush_records(LogRecordBuffer &records) = 0;

  void start_flush_thread();
  void stop_flush_thread();

  void flush_thread_run();

  virtual size_t get_buffer_size() const = 0;
  virtual std::chrono::seconds get_flush_interval() const = 0;

  LogRecordBuffer buffered_records_;
  std::mutex mtx_;
  std::condition_variable buffer_not_full_cv_;
  std::condition_variable flush_thread_cv_;
  std::thread flush_thread_;
  bool flush_thread_is_running_{false};

  uint64_t dropped_logs_{0};
};

class MetadataLogger : public BufferedLogger {
 public:
  using LogLevel = mysql_harness::logging::LogLevel;

  struct Options {
    static constexpr size_t kMinBufferSize = 1;
    static constexpr size_t kMaxBufferSize = 10'000;
    static constexpr size_t kDefaultBufferSize = 500;
    static constexpr auto kMinFlushInterval = std::chrono::seconds(1);
    static constexpr auto kMaxFlushInterval = std::chrono::seconds(86'400);
    static constexpr auto kDefaultFlushInterval = std::chrono::seconds(10);

    std::optional<LogLevel> log_level;
    std::optional<size_t> buffer_size{kDefaultBufferSize};
    std::optional<std::chrono::seconds> flush_interval{kDefaultFlushInterval};

    bool operator==(const Options &) const = default;
  };

  void init(mysql_harness::logging::LogLevel log_level);
  void deinit();

  void on_metadata_available(const mrs::database::MrsSchemaVersion &schema_ver,
                             mysqlrouter::MySQLSession *session);

  void on_metadata_version_change(
      const mrs::database::MrsSchemaVersion &schema_ver);

  void start(const mrs::Configuration *configuration,
             collector::MysqlCacheManager *cache);
  void stop();

  static MetadataLogger &instance() {
    static MetadataLogger instance_;

    return instance_;
  }

 protected:
  LogLevel get_log_level() const {
    return logger_options_.log_level.value_or(static_log_level_);
  }

  size_t get_buffer_size() const override {
    return logger_options_.buffer_size.value_or(Options::kDefaultBufferSize);
  }

  std::chrono::seconds get_flush_interval() const override {
    return logger_options_.flush_interval.value_or(
        Options::kDefaultFlushInterval);
  }

 private:
  MetadataLogger();
  bool flush_records(LogRecordBuffer &records) override;
  void reconfigure(const Options &config);
  void report_dropped_logs(mysqlrouter::MySQLSession *session);
  void check_dynamic_config(mysqlrouter::MySQLSession *session);
  bool check_metadata_version_supported(
      const mrs::database::MrsSchemaVersion &schema_ver);

  // whether the logging to metadata is enabled in the configuration
  [[nodiscard]] bool is_enabled() const { return initialized_; }

  // log level configured statically
  LogLevel static_log_level_;

  // current logger configuration
  Options logger_options_;

  std::shared_ptr<mysql_harness::logging::ExternalHandler> handler_;
  collector::MysqlCacheManager *cache_manager_;
  const mrs::Configuration *configuration_;

  bool initialized_{false};
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_METADATA_LOGGER_H_
