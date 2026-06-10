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

#include "mrs/database/metadata_logger.h"

#include <iomanip>

#include "helper/json/rapid_json_to_struct.h"
#include "mrs/database/query_version.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysqlrouter/utils_sqlstring.h"
#include "scope_guard.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

namespace {

constexpr std::string_view kSinkName = "mysql_rest_service";

mysqlrouter::sqlstring get_insert_sql(size_t records_num) {
  const std::string values = "(?, ?, ?, ?, ?, ?)";
  std::string result =
      "INSERT INTO mysql_rest_service_metadata.router_general_log(router_id, "
      "log_time, log_type, domain, message, thread_id) VALUES ";

  for (size_t i = 0; i < records_num; ++i) {
    if (i > 0) {
      result += ", ";
    }

    result += values;
  }

  return result.c_str();
}

std::string to_string(std::chrono::time_point<std::chrono::system_clock> tp) {
  using namespace std::chrono;
  auto tp_t = system_clock::to_time_t(tp);

  // get the fractional seconds
  auto now_ms = duration_cast<microseconds>(tp.time_since_epoch()) % 1'000'000;

  std::tm now_tm = *std::localtime(&tp_t);

  std::ostringstream oss;
  oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
      << std::setw(6) << now_ms.count();

  return oss.str();
}

}  // namespace

void BufferedLogger::log(const LogRecord &record) {
  std::unique_lock<std::mutex> l{mtx_};
  // if the flushing thread is running wait until there is a space in the buffer
  // (with a 0,5 timeout)
  buffer_not_full_cv_.wait_for(l, std::chrono::milliseconds(500), [this]() {
    return buffered_records_.size() < get_buffer_size() ||
           !flush_thread_is_running_;
  });

  // the buffer is full and timed out waiting, need to drop the record
  if (buffered_records_.size() >= get_buffer_size()) {
    // the buffer is full and we timed out waiting, we need to drop the record
    ++dropped_logs_;
    return;
  }

  buffered_records_.push(record);

  if (buffered_records_.size() >= get_buffer_size()) {
    flush_thread_cv_.notify_one();
  }
}

void BufferedLogger::start_flush_thread() {
  std::lock_guard<std::mutex> lock(mtx_);
  flush_thread_is_running_ = true;
  flush_thread_ = std::thread{&BufferedLogger::flush_thread_run, this};
}

void BufferedLogger::stop_flush_thread() {
  if (!flush_thread_is_running_) return;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    flush_thread_is_running_ = false;
  }
  flush_thread_cv_.notify_one();
  if (flush_thread_.joinable()) flush_thread_.join();
}

void BufferedLogger::flush_thread_run() {
  while (flush_thread_is_running_) {
    LogRecordBuffer records_to_flush;

    {
      std::unique_lock<std::mutex> lock(mtx_);
      flush_thread_cv_.wait_for(lock, get_flush_interval(), [this]() {
        return buffered_records_.size() >= get_buffer_size() ||
               !flush_thread_is_running_;
      });

      if (buffered_records_.empty()) {
        if (!flush_thread_is_running_) break;
        continue;
      }

      records_to_flush = std::move(buffered_records_);
      buffered_records_ = LogRecordBuffer{};

      buffer_not_full_cv_.notify_all();
    }

    // the actual flush I/O is done outside of the lock to not block the log
    // producers
    const auto num_of_records_to_flush = records_to_flush.size();
    if (!flush_records(records_to_flush)) {
      std::unique_lock<std::mutex> lock(mtx_);
      dropped_logs_ += num_of_records_to_flush;
    }
  }
}

namespace {
class MetadataLogHandler final
    : public mysql_harness::logging::ExternalHandler {
 public:
  MetadataLogHandler(MetadataLogger &metadata_logger)
      : metadata_logger_(metadata_logger) {}

 protected:
  void do_log(const mysql_harness::logging::Record &record) noexcept override {
    metadata_logger_.log(record);
  }

 private:
  MetadataLogger &metadata_logger_;
};

class ParseMetadataLoggerOptions
    : public helper::json::RapidReaderHandlerToStruct<MetadataLogger::Options> {
 public:
  using Options = MetadataLogger::Options;
  template <typename ValueType>
  std::optional<MetadataLogger::LogLevel> to_loglevel(const ValueType &value,
                                                      std::string &err) {
    try {
      return mysql_harness::logging::log_level_from_string(value);
    } catch (std::exception &e) {
      err = e.what();
      return std::nullopt;
    }
  }

  template <typename ValueType>
  std::optional<uint32_t> to_uint(const ValueType &value) noexcept {
    try {
      return std::stoul(value.c_str());
    } catch (...) {
      return std::nullopt;
    }
  }

  template <typename ValueType>
  std::optional<std::chrono::seconds> to_seconds(
      const ValueType &value) noexcept {
    try {
      return std::chrono::seconds(std::stoul(value.c_str()));
    } catch (...) {
      return std::nullopt;
    }
  }

  template <typename ValueType>
  void handle_object_value(const std::string &key, const ValueType &vt) {
    if (key == "logLevel") {
      std::string err;
      const auto val = to_loglevel(vt, err);
      if (!val) {
        log_warning("mrsMetadataLoggerOptions.logLevel: %s", err.c_str());
      } else {
        result_.log_level = val;
      }
    } else if (key == "bufferSize") {
      const auto val = to_uint(vt);
      if (!val || *val < Options::kMinBufferSize ||
          *val > Options::kMaxBufferSize) {
        log_warning(
            "mrsMetadataLoggerOptions.bufferSize must be integer value from "
            "range [%zu, %zu] was '%s'",
            Options::kMinBufferSize, Options::kMaxBufferSize, vt.c_str());
      } else {
        result_.buffer_size = val;
      }
    } else if (key == "flushInterval") {
      const auto val = to_seconds(vt);
      if (!val || *val < Options::kMinFlushInterval ||
          *val > Options::kMaxFlushInterval) {
        log_warning(
            "mrsMetadataLoggerOptions.flushInterval must be integer value from "
            "range [%s, %s] was '%s'",
            std::to_string(Options::kMinFlushInterval.count()).c_str(),
            std::to_string(Options::kMaxFlushInterval.count()).c_str(),
            vt.c_str());
      } else {
        result_.flush_interval = val;
      }
    }
  }

  void handle_value(const std::string &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      handle_object_value(key, vt);
    }
  }

  bool String(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool RawNumber(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }
};

auto parse_json_options(const std::string &options) {
  auto result =
      helper::json::text_to_handler<ParseMetadataLoggerOptions>(options);

  if (!result) {
    log_error(
        "Failed to parse 'MetadataLogger' options from global JSON "
        "configuration");
  }

  return result.value_or(ParseMetadataLoggerOptions::Result{});
}

}  // namespace

MetadataLogger::MetadataLogger() {}

void MetadataLogger::init(mysql_harness::logging::LogLevel log_level) {
  logger_options_.log_level = static_log_level_ = log_level;

  handler_ = std::make_shared<MetadataLogHandler>(*this);
  register_external_logging_handler(std::string(kSinkName), handler_);

  initialized_ = true;
}

void MetadataLogger::deinit() {
  if (!initialized_) return;

  unregister_external_logging_handler(std::string(kSinkName));
  if (handler_) handler_.reset();

  initialized_ = false;
}

void MetadataLogger::check_dynamic_config(mysqlrouter::MySQLSession *session) {
  mysqlrouter::sqlstring query{
      "SELECT JSON_MERGE_PATCH("
      "  IFNULL((select JSON_EXTRACT(data, '$.mrsMetadataLogger') from "
      "mysql_rest_service_metadata.config), JSON_OBJECT()),"
      "  IFNULL((select JSON_EXTRACT(options, '$.mrsMetadataLogger') from "
      "mysql_rest_service_metadata.router where id = ?), JSON_OBJECT())"
      ") as mrsMetadataLoggerOptions"};

  query << configuration_->router_id_;

  auto result{session->query_one(query.str())};

  if (nullptr == result.get() || !(*result)[0]) return;

  const auto json_str = std::string((*result)[0]);

  const auto options = parse_json_options(json_str);

  if (options != logger_options_) {
    reconfigure(options);
  }
}

void MetadataLogger::reconfigure(const Options &options) {
  const auto prev_log_level = get_log_level();
  logger_options_ = options;
  if (prev_log_level != get_log_level()) {
    set_log_level_for_handler(std::string(kSinkName), get_log_level());
  }
}

bool MetadataLogger::check_metadata_version_supported(
    const mrs::database::MrsSchemaVersion &schema_ver) {
  if (schema_ver < MrsSchemaVersion{4, 0, 2}) {
    log_warning(
        "Logging to mysql_rest_service_metadata metadata was configured but "
        "MRS metadata version %s does not support metadata logging",
        schema_ver.str().c_str());
    return false;
  }

  return true;
}

void MetadataLogger::on_metadata_available(
    const mrs::database::MrsSchemaVersion &schema_ver,
    mysqlrouter::MySQLSession *session) {
  if (!is_enabled()) {
    return;
  }

  if (!flush_thread_is_running_ &&
      check_metadata_version_supported(schema_ver)) {
    start_flush_thread();
  }

  check_dynamic_config(session);
}

void MetadataLogger::on_metadata_version_change(
    const mrs::database::MrsSchemaVersion &schema_ver) {
  if (!is_enabled()) {
    return;
  }

  const bool version_supported = check_metadata_version_supported(schema_ver);

  if (version_supported && !flush_thread_is_running_) {
    start_flush_thread();
  }

  if (!version_supported && flush_thread_is_running_) {
    stop_flush_thread();
  }
}

void MetadataLogger::start(const mrs::Configuration *configuration,
                           collector::MysqlCacheManager *cache) {
  if (!is_enabled()) {
    return;
  }

  cache_manager_ = cache;
  configuration_ = configuration;

  auto session =
      cache_manager_->get_instance(collector::kMySQLConnectionMetadataRW, true);

  QueryVersion q;
  const auto md_version = q.query_version(session.get());

  if (check_metadata_version_supported(md_version)) {
    start_flush_thread();
  }
}

void MetadataLogger::stop() {
  if (!is_enabled()) {
    return;
  }

  stop_flush_thread();
}

bool MetadataLogger::flush_records(LogRecordBuffer &records) {
  if (records.empty()) {
    // nothing to store
    return true;
  }

  try {
    auto session = cache_manager_->get_instance(
        collector::kMySQLConnectionMetadataRW, true);
    const auto old_log_queries = session->log_queries();
    session->log_queries(false);
    Scope_guard restore_session_logging(
        [&]() { session->log_queries(old_log_queries); });

    const size_t max_batch_size = 10;
    const size_t num_of_batches =
        (records.size() + max_batch_size - 1) / max_batch_size;

    report_dropped_logs(session.get());

    mysqlrouter::MySQLSession::Transaction transaction(session.get());
    for (size_t batch = 0; batch < num_of_batches; ++batch) {
      const size_t batch_size = std::min(max_batch_size, records.size());

      auto insert_sql = get_insert_sql(batch_size);

      for (size_t i = 0; i < batch_size; ++i) {
        const auto &record = records.front();
        insert_sql << configuration_->router_id_ << to_string(record.created)
                   << mysql_harness::logging::log_level_to_string(record.level)
                   << record.domain << record.message << record.process_id;

        records.pop();
      }

      session->execute(insert_sql.str());
    }
    transaction.commit();
  } catch (const mysqlrouter::MySQLSession::Error &) {
    return false;
  }

  return true;
}

void MetadataLogger::report_dropped_logs(mysqlrouter::MySQLSession *session) {
  uint64_t dropped_logs_num;

  {
    std::lock_guard<std::mutex> lk{mtx_};
    dropped_logs_num = dropped_logs_;
    // nothing to report
    if (dropped_logs_num == 0) {
      return;
    }
    dropped_logs_ = 0;
  }

  auto insert_sql = get_insert_sql(1);

  std::string message{"Metadata logger could not log " +
                      std::to_string(dropped_logs_num) +
                      " messages. They were dropped."};

  insert_sql << configuration_->router_id_
             << to_string(std::chrono::system_clock::now())
             << mysql_harness::logging::log_level_to_string(
                    mysql_harness::logging::LogLevel::kWarning)
             << MYSQL_ROUTER_LOG_DOMAIN << message
             << stdx::this_process::get_id();

  bool insert_ok{true};
  try {
    session->execute(insert_sql.str());
  } catch (const mysqlrouter::MySQLSession::Error &) {
    insert_ok = false;
  }

  // if failed to report, add it back to the counter to try again later
  if (!insert_ok) {
    std::lock_guard<std::mutex> lk{mtx_};
    dropped_logs_ += dropped_logs_num;
  }
}

}  // namespace database
}  // namespace mrs
