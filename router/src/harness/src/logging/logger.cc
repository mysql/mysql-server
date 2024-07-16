/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/logging/logger.h"
#include "dim.h"
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/registry.h"

namespace mysql_harness {

namespace logging {

////////////////////////////////////////////////////////////////
// class Logger

Logger::Logger(Registry &registry, LogLevel level)
    : level_(level), registry_(&registry) {}

void Logger::attach_handler(std::string name) {
  // verification that the handler actually exists will be performed in
  // Registry::update_logger() - it makes no sense to do it earlier, since it
  // can still change between now and the time when we update_logger() is called
  handlers_.emplace(name);
}

void Logger::detach_handler(std::string name,
                            bool handler_must_exist /*= true */) {
  if (handlers_.erase(name) == 0 && handler_must_exist)
    throw std::logic_error(std::string("Detaching unknown handler '") + name +
                           "'");
}

bool Logger::is_handled(LogLevel level) const {
  if (level > level_) return false;

  return registry_->is_handled(level);
}

void Logger::handle(const Record &record) {
  if (record.level > level_) return;

  for (const std::string &handler_id : handlers_) {
    std::shared_ptr<Handler> handler;
    try {
      handler = registry_->get_handler(handler_id);
    } catch (const std::logic_error &) {
      // It may happen that another thread has removed this handler since
      // we got a copy of our Logger object, and we now have a dangling
      // reference. In such case, simply skip it.
      continue;
    }
    if (record.level <= handler->get_level()) handler->handle(record);
  }
}

void Logger::lazy_handle(LogLevel record_level,
                         std::function<Record()> record_creator) const {
  if (record_level > level_) return;

  // build the record once.
  std::optional<Record> record;

  for (const std::string &handler_id : handlers_) {
    std::shared_ptr<Handler> handler;
    try {
      handler = registry_->get_handler(handler_id);
    } catch (const std::logic_error &) {
      // It may happen that another thread has removed this handler since
      // we got a copy of our Logger object, and we now have a dangling
      // reference. In such case, simply skip it.
      continue;
    }
    if (record_level <= handler->get_level()) {
      if (!record) record = record_creator();

      handler->handle(*record);
    }
  }
}

bool DomainLogger::init_logger() const {
  if (logger_) return true;

  // if there is no DIM, don't log anything.
  auto &dim = mysql_harness::DIM::instance();
  if (!dim.has_LoggingRegistry()) return false;

  mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

  logger_ = registry.get_logger_or_default(domain_);

  return true;
}

void DomainLogger::log(LogLevel log_level,
                       std::function<std::string()> producer) const {
  // init logger if needed.
  if (!init_logger()) return;

  logger_->lazy_handle(log_level, [&]() -> Record {
    const auto now = std::chrono::system_clock::now();
    // Build the record for the handler.
    return {log_level, stdx::this_process::get_id(), now, domain_, producer()};
  });
}

void DomainLogger::log(LogLevel log_level, std::string msg) const {
  // init logger if needed.
  if (!init_logger()) return;

  logger_->lazy_handle(log_level, [&]() -> Record {
    const auto now = std::chrono::system_clock::now();
    // Build the record for the handler.
    return {log_level, stdx::this_process::get_id(), now, domain_, msg};
  });
}

}  // namespace logging

}  // namespace mysql_harness
