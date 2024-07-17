/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "test/helpers.h"

#include "dim.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/registry.h"

::testing::AssertionResult AssertLoaderSectionAvailable(
    const char *loader_expr, const char *section_expr,
    mysql_harness::Loader *loader, const std::string &section_name) {
  auto lst = loader->available();
  auto match_example =
      [&section_name](const std::pair<std::string, std::string> &elem) {
        return elem.first == section_name;
      };

  if (std::count_if(lst.begin(), lst.end(), match_example) > 0)
    return ::testing::AssertionSuccess();

  std::ostringstream sections;
  for (auto &name : lst) {
    sections << " " << name.first;
    if (!name.second.empty()) {
      sections << ":" << name.second;
    }
  }
  return ::testing::AssertionFailure()
         << "Loader '" << loader_expr << "' did not contain section '"
         << section_name << "' (from expression '" << section_expr << "')\n"
         << "Sections were: " << sections.str();
}

void register_test_logger() {
  static mysql_harness::logging::Registry static_registry;

  mysql_harness::DIM::instance().set_static_LoggingRegistry(&static_registry);
}

void init_test_logger(
    const std::list<std::string> &additional_log_domains /* = {} */,
    const std::string &log_folder /* = "" */,
    const std::string &log_filename /* = "" */) {
  register_test_logger();

  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

  if (!dim.has_Config()) {
    auto config = std::make_unique<mysql_harness::LoaderConfig>(
        mysql_harness::Config::allow_keys);
    config->add(mysql_harness::logging::kConfigSectionLogger);
    config->get(mysql_harness::logging::kConfigSectionLogger, "")
        .add(mysql_harness::logging::options::kLevel, "debug");

    dim.set_Config(config.release(),
                   std::default_delete<mysql_harness::LoaderConfig>());
  }

  std::list<std::string> log_domains(additional_log_domains.begin(),
                                     additional_log_domains.end());
  log_domains.push_back(mysql_harness::logging::kMainLogger);

  mysql_harness::logging::clear_registry(registry);
  mysql_harness::logging::create_module_loggers(
      registry,
      mysql_harness::logging::get_default_log_level(
          mysql_harness::DIM::instance().get_Config()),
      log_domains, mysql_harness::logging::kMainLogger);
  mysql_harness::logging::create_main_log_handler(registry, log_filename,
                                                  log_folder, true);

  registry.set_ready();
}
