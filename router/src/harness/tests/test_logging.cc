/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#define MYSQL_ROUTER_LOG_DOMAIN "my_domain"

////////////////////////////////////////
// Standard include files
#include <ctime>
#include <stdexcept>

////////////////////////////////////////
// Third-party include'files
#include <gmock/gmock.h>
#include <gtest/gtest.h>

////////////////////////////////////////
// Internal interfaces
#include "dim.h"
#include "include/magic.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/stdx/process.h"
#include "mysql/harness/utility/string.h"  // string_format()
#include "test/helpers.h"
#include "test/temp_directory.h"

using mysql_harness::Path;
using mysql_harness::logging::FileHandler;
using mysql_harness::logging::Handler;
using mysql_harness::logging::log_debug;
using mysql_harness::logging::log_error;
using mysql_harness::logging::log_info;
using mysql_harness::logging::log_note;
using mysql_harness::logging::log_system;
using mysql_harness::logging::log_warning;
using mysql_harness::logging::Logger;
using mysql_harness::logging::LogLevel;
using mysql_harness::logging::LogTimestampPrecision;
using mysql_harness::logging::Record;
using mysql_harness::logging::StreamHandler;

using testing::Combine;
using testing::ContainsRegex;
using testing::EndsWith;
using testing::Eq;
using testing::Ge;
using testing::Gt;
using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;
using testing::StrEq;
using testing::Test;
using testing::Values;
using testing::ValuesIn;
using testing::WithParamInterface;

const std::string kDateRegex =
#ifdef GTEST_USES_SIMPLE_RE
    "\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d";
#else
    "[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}";
#endif

const std::chrono::time_point<std::chrono::system_clock> kDefaultTimepoint =
    std::chrono::system_clock::from_time_t(0);

// TODO move this and ASSERT_THROW_LIKE from:
//   tests/helpers/router_test_helpers.h
// to:
//   mysql_harness/shared/include/test/helpers.h
#define EXPECT_THROW_LIKE(expr, exc, msg)                                     \
  try {                                                                       \
    expr;                                                                     \
    ADD_FAILURE() << "Expected exception of type " #exc << " but got none\n"; \
  } catch (exc & e) {                                                         \
    if (std::string(e.what()).find(msg) == std::string::npos) {               \
      ADD_FAILURE() << "Expected exception with message: " << msg             \
                    << "\nbut got: " << e.what() << "\n";                     \
    }                                                                         \
  } catch (...) {                                                             \
    ADD_FAILURE() << "Expected exception of type " #exc                       \
                  << " but got another\n";                                    \
  }

namespace {
Path g_here;
mysql_harness::logging::Registry *g_registry;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Registry tests
////////////////////////////////////////////////////////////////////////////////

class LoggingLowLevelTest : public Test {
 public:
  void SetUp() override {
    clear_registry(*g_registry);
    g_registry->add_handler("handler_1",
                            std::make_shared<StreamHandler>(std::cerr));
    g_registry->add_handler("handler_2",
                            std::make_shared<StreamHandler>(std::cerr));
    g_registry->add_handler("handler_3",
                            std::make_shared<StreamHandler>(std::cerr));
  }

  void TearDown() override { clear_registry(*g_registry); }
};

TEST_F(LoggingLowLevelTest, test_handler_CRUD) {
  // remove default stuff
  clear_registry(*g_registry);

  // handler doesn't exist yet
  ASSERT_EQ(0u, g_registry->get_handler_names().size());
  EXPECT_THROW_LIKE(g_registry->get_handler("foo"), std::logic_error,
                    "Accessing non-existant handler 'foo'");

  // add it
  g_registry->add_handler("foo", std::make_shared<StreamHandler>(std::cerr));

  // try adding it again
  EXPECT_THROW_LIKE(g_registry->add_handler(
                        "foo", std::make_shared<StreamHandler>(std::cerr)),
                    std::logic_error, "Duplicate handler 'foo'");

  // it exists now
  ASSERT_EQ(1u, g_registry->get_handler_names().size());
  EXPECT_NO_THROW(g_registry->get_handler("foo"));
  EXPECT_STREQ("foo", g_registry->get_handler_names().begin()->c_str());

  // remove it
  EXPECT_NO_THROW(g_registry->remove_handler("foo"));

  // try removing it again
  EXPECT_THROW_LIKE(g_registry->remove_handler("foo"), std::logic_error,
                    "Removing non-existant handler 'foo'");

  // it doesn't exist now
  EXPECT_EQ(0u, g_registry->get_handler_names().size());
  EXPECT_THROW_LIKE(g_registry->get_handler("foo"), std::logic_error,
                    "Accessing non-existant handler 'foo'");
}

TEST_F(LoggingLowLevelTest, test_logger_CRUD) {
  // logger doesn't exist yet
  ASSERT_EQ(0u, g_registry->get_logger_names().size());
  EXPECT_THROW_LIKE(g_registry->get_logger("foo"), std::logic_error,
                    "Accessing non-existant logger 'foo'");
  EXPECT_THROW_LIKE(g_registry->update_logger("foo", Logger()),
                    std::logic_error, "Updating non-existant logger 'foo'");

  // add it
  g_registry->create_logger("foo", LogLevel::kError);

  // try adding it again
  EXPECT_THROW_LIKE(g_registry->create_logger("foo", LogLevel::kError),
                    std::logic_error, "Duplicate logger 'foo'");

  // it exists now
  ASSERT_EQ(1u, g_registry->get_logger_names().size());
  EXPECT_NO_THROW(g_registry->get_logger("foo"));
  EXPECT_STREQ("foo", g_registry->get_logger_names().begin()->c_str());

  // update it
  EXPECT_NO_THROW(g_registry->update_logger("foo", Logger()));

  // remove it
  EXPECT_NO_THROW(g_registry->remove_logger("foo"));

  // try removing it again
  EXPECT_THROW_LIKE(g_registry->remove_logger("foo"), std::logic_error,
                    "Removing non-existant logger 'foo'");

  // it doesn't exist now
  EXPECT_EQ(0u, g_registry->get_logger_names().size());
  EXPECT_THROW_LIKE(g_registry->get_logger("foo"), std::logic_error,
                    "Accessing non-existant logger 'foo'");
}

TEST_F(LoggingLowLevelTest, test_logger_update) {
  g_registry->create_logger("foo");
  EXPECT_EQ(0u, g_registry->get_logger("foo").get_handler_names().size());

  // valid update
  {
    Logger l = g_registry->get_logger("foo");
    EXPECT_NO_THROW(l.attach_handler("handler_1"));
    EXPECT_NO_THROW(l.attach_handler("handler_2"));
    EXPECT_NO_THROW(l.attach_handler("handler_3"));
    EXPECT_NO_THROW(g_registry->update_logger("foo", l));

    // handlers should have been successfully added
    EXPECT_EQ(3u, g_registry->get_logger("foo").get_handler_names().size());
  }

  // not all handlers exist
  {
    Logger l = g_registry->get_logger("foo");
    EXPECT_NO_THROW(l.attach_handler("handler_1"));
    EXPECT_NO_THROW(l.attach_handler("unicorn"));
    EXPECT_NO_THROW(l.attach_handler("handler_3"));
    EXPECT_THROW_LIKE(g_registry->update_logger("foo", l), std::logic_error,
                      "Attaching unknown handler 'unicorn'");

    // failed update should not change the logger in the registry
    EXPECT_EQ(3u, g_registry->get_logger("foo").get_handler_names().size());
  }

  // detaching non-existent handlers is optionally verified by detach_handler().
  // It is not something that concerns update_logger(), since it only sees
  // the Logger object after all the handlers that were supposed to be detached,
  // got detached
  {
    Logger l = g_registry->get_logger("foo");
    EXPECT_NO_THROW(l.detach_handler("handler_1", true));  // true = verify
    EXPECT_NO_THROW(
        l.detach_handler("unicorn", false));  // false = don't verify, no-op
    EXPECT_THROW_LIKE(l.detach_handler("unicorn", true), std::logic_error,
                      "Detaching unknown handler 'unicorn'");
    EXPECT_THROW_LIKE(l.detach_handler("unicorn"),  // true is default
                      std::logic_error, "Detaching unknown handler 'unicorn'");

    // logger object should still be valid after failed detach_handler() and
    // contain the two handlers we did not remove
    EXPECT_EQ(2u, l.get_handler_names().size());
    EXPECT_EQ(1, std::count(l.get_handler_names().begin(),
                            l.get_handler_names().end(), "handler_2"));
    EXPECT_EQ(1, std::count(l.get_handler_names().begin(),
                            l.get_handler_names().end(), "handler_3"));

    // logger should update successfully
    EXPECT_NO_THROW(g_registry->update_logger("foo", l));
    EXPECT_EQ(2u, g_registry->get_logger("foo").get_handler_names().size());
  }
}

////////////////////////////////////////////////////////////////////////////////
// higher-level tests
////////////////////////////////////////////////////////////////////////////////

TEST(FunctionalTest, ThisMustRunAsFirst) { init_test_logger(); }

TEST(FunctionalTest, LogFromUnregisteredModule) {
  // Test a scenario when no domain logger has been added yet.
  //
  // Logging should fall back to using application ("main") logger's
  // configuration, which is always added by the application (init_log() in
  // main(), in our case), but use the "log domain"

  std::stringstream buffer;
  auto handler = std::make_shared<StreamHandler>(buffer);
  g_registry->add_handler(StreamHandler::kDefaultName, handler);
  attach_handler_to_all_loggers(*g_registry, StreamHandler::kDefaultName);

  log_info("Test message from an unregistered module");
  std::string log = buffer.str();

  // log message should be something like:
  // 2017-04-12 14:05:31 my_domain INFO [7ffff7fd5780] Test message from an
  // unregistered module
  EXPECT_THAT(log, ::testing::Not(::testing::HasSubstr(" main ERROR")));

  EXPECT_THAT(log, ::testing::HasSubstr(" my_domain INFO"));
  EXPECT_THAT(
      log, ::testing::HasSubstr(" Test message from an unregistered module\n"));

  // clean up
  g_registry->remove_handler(StreamHandler::kDefaultName);
}

TEST(FunctionalTest, LogOnDanglingHandlerReference) {
  // NOTE: "a_gonner" and "z_stayer" are named like that to ensure that
  // iterating over the container (std::set<std::string>) inside
  // Logger::handler() will process "a_gonner" first. std::set makes guarantee
  // that iterating over its elements will be in ascending element order, which
  // means alphabetical order in case of std::string. By having those two named
  // like that, we additionally verify that logging to a valid handler will
  // still work AFTER trying to log to a removed handler.

  // add 2 new handlers
  std::stringstream buffer;
  auto handler = std::make_shared<StreamHandler>(buffer);
  g_registry->add_handler("a_gonner",
                          std::make_shared<StreamHandler>(std::cerr));
  g_registry->add_handler("z_stayer", handler);

  // create a logger with the new handlers attached
  g_registry->create_logger("my_logger");
  Logger l(*g_registry);
  l.attach_handler("z_stayer");
  l.attach_handler("a_gonner");
  g_registry->update_logger("my_logger", l);

  // now remove first handler
  g_registry->remove_handler("a_gonner");

  // and try to log with the logger still holding a reference to it.
  // Logger::handle() should deal with it properly - it should log
  // to all (still existing) handlers ("z_stayer" in this case).
  EXPECT_NO_THROW(
      l.handle(Record{LogLevel::kWarning, stdx::this_process::get_id(),
                      kDefaultTimepoint, "my_logger", "Test message"}));
  std::string log = buffer.str();

  // log message should be something like:
  // 2017-04-12 14:05:31 my_logger WARNING [7ffff7fd5780] Test message
  EXPECT_NE(log.npos, log.find(" my_logger WARNING"));
  EXPECT_NE(log.npos, log.find(" Test message\n"));

  // clean up
  g_registry->remove_handler("z_stayer");
  g_registry->remove_logger("my_logger");
}

TEST(TestBasic, Setup) {
  // Test that creating a logger will give it a name and a default log
  // level.
  Logger logger(*g_registry);
  EXPECT_EQ(logger.get_level(), LogLevel::kWarning);

  logger.set_level(LogLevel::kDebug);
  EXPECT_EQ(logger.get_level(), LogLevel::kDebug);
}

class LoggingTest : public Test {
 public:
  // Here we are just testing that messages are written and in the
  // right format. We use kNotSet log level, which will print all
  // messages.
  Logger logger{*g_registry, LogLevel::kNotSet};
};

TEST_F(LoggingTest, StreamHandler) {
  std::stringstream buffer;

  g_registry->add_handler("TestStreamHandler",
                          std::make_shared<StreamHandler>(buffer));
  logger.attach_handler("TestStreamHandler");

  // A bunch of casts to int for tellp to avoid C2666 in MSVC
  ASSERT_THAT((int)buffer.tellp(), Eq(0));
  logger.handle(Record{LogLevel::kInfo, stdx::this_process::get_id(),
                       kDefaultTimepoint, "my_module", "Message"});
  EXPECT_THAT((int)buffer.tellp(), Gt(0));

  // message should be logged after applying format (timestamp, etc)
  EXPECT_THAT(buffer.str(),
              ContainsRegex(kDateRegex + " my_module INFO.*Message\n"));

  // clean up
  g_registry->remove_handler("TestStreamHandler");
}

TEST_F(LoggingTest, FileHandler) {
  // Check that an exception is thrown for a path that cannot be
  // opened.
  EXPECT_ANY_THROW(FileHandler("/something/very/unlikely/to/exist"));

  // We do not use mktemp or friends since we want this to work on
  // Windows as well.
  Path log_file(g_here.join(
      "log4-" + std::to_string(stdx::this_process::get_id()) + ".log"));
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    std::error_code ec;
    stdx::filesystem::remove(log_file.str(), ec);
  });

  g_registry->add_handler("TestFileHandler",
                          std::make_shared<FileHandler>(log_file));
  logger.attach_handler("TestFileHandler");

  // Log one record
  logger.handle(Record{LogLevel::kInfo, stdx::this_process::get_id(),
                       kDefaultTimepoint, "my_module", "Message"});

  // Open and read the entire file into memory.
  std::vector<std::string> lines;
  {
    std::ifstream ifs_log(log_file.str());
    std::string line;
    while (std::getline(ifs_log, line)) lines.push_back(line);
  }

  // We do the assertion here to ensure that we can do as many tests
  // as possible and report issues.
  ASSERT_THAT(lines.size(), Ge(1));

  // Check basic properties for the first line.
  EXPECT_THAT(lines.size(), Eq(1));

  // message should be logged after applying format (timestamp, etc)
  EXPECT_THAT(lines.at(0),
              ContainsRegex(kDateRegex + " my_module INFO.*Message"));

  // clean up
  g_registry->remove_handler("TestFileHandler");
}

TEST_F(LoggingTest, FileHandlerRotate) {
  // Check that the FileHandler can rotate to supplied filename

  // We do not use mktemp or friends since we want this to work on
  // Windows as well.
  Path log_file(g_here.join(
      "log4-" + std::to_string(stdx::this_process::get_id()) + ".log"));
  Path renamed_log_file(g_here.join(
      "rotated-log4-" + std::to_string(stdx::this_process::get_id()) + ".log"));
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    std::error_code ec;
    stdx::filesystem::remove(log_file.str(), ec);
    stdx::filesystem::remove(renamed_log_file.str(), ec);
  });

  g_registry->add_handler("TestFileHandler",
                          std::make_shared<FileHandler>(log_file));
  logger.attach_handler("TestFileHandler");

  // Log one record
  logger.handle(Record{LogLevel::kInfo, stdx::this_process::get_id(),
                       kDefaultTimepoint, "my_module", "Message"});

  // Verify only the original logfile exists
  ASSERT_TRUE(log_file.exists());

  // Open and read the entire file into memory.
  std::vector<std::string> lines;
  {
    std::ifstream ifs_log(log_file.str());
    std::string line;
    while (std::getline(ifs_log, line)) lines.push_back(line);
  }

  // We do the assertion here to ensure that we can do as many tests
  // as possible and report issues.
  ASSERT_THAT(lines.size(), Ge(1));

  // Check basic properties for the first line.
  EXPECT_THAT(lines.size(), Eq(1));

  // Message should be logged after applying format (timestamp, etc)
  EXPECT_THAT(lines.at(0),
              ContainsRegex(kDateRegex + " my_module INFO.*Message"));

  // Rotate existing file to old filename
  g_registry->flush_all_loggers(renamed_log_file.str());

  // Verify the renamed file exists
  ASSERT_TRUE(renamed_log_file.exists());

  // Log one record after rotation
  logger.handle(Record{LogLevel::kInfo, stdx::this_process::get_id(),
                       kDefaultTimepoint, "my_module", "Another message"});

  // Verify the original log file once again gets logged to
  ASSERT_TRUE(log_file.exists());

  // Open and read the new file into memory after rotation
  std::vector<std::string> lines2;
  {
    std::ifstream ifs_log(log_file.str());
    std::string line;
    while (std::getline(ifs_log, line)) lines2.push_back(line);
  }

  // We do the assertion here to ensure that we can do as many tests
  // as possible and report issues.
  ASSERT_THAT(lines2.size(), Ge(1));

  // Check basic properties for the first line.
  EXPECT_THAT(lines2.size(), Eq(1));

  // Message should be logged after rotation and applying format (timestamp,
  // etc)
  EXPECT_THAT(lines2.at(0),
              ContainsRegex(kDateRegex + " my_module INFO.*Another message"));

  // clean up
  g_registry->remove_handler("TestFileHandler");
}

/**
 * @test
 *      Verify if no exception is throw when file can be opened for writing.
 */
TEST_F(LoggingTest, DontThrowIfOpenedLogFileForWriting) {
  TempDirectory tmp_dir;
  Path dir_path(tmp_dir.name());
  Path file_path(dir_path.join("test_file.log").str());

  ASSERT_TRUE(dir_path.exists());
  { std::ofstream file(file_path.str()); }
  ASSERT_TRUE(file_path.exists());
  ASSERT_NO_THROW(FileHandler{file_path.str()});
}

#ifndef _WIN32
/**
 * @test
 *       Verify if appropriate system_error is thrown when cannot
 *       create file in directory.
 *
 * There is component test: RouterLoggingTest, bad_logging_folder
 * that uses file as a directory to verify scenario when file cannot
 * be created in directory.
 */
TEST_F(LoggingTest, FileHandlerThrowsNoPermissionToCreateFileInDirectory) {
  TempDirectory tmp_dir;
  Path dir_path(tmp_dir.name());
  Path file_path(dir_path.join("test_file.log").str());

  ASSERT_TRUE(dir_path.exists());

  // set permissions
  chmod(dir_path.c_str(), S_IRUSR);

  ASSERT_FALSE(file_path.exists());
  EXPECT_THROW_LIKE(FileHandler(file_path.str()), std::system_error,
                    "Cannot create file in directory " +
                        file_path.dirname().str() + ": Permission denied");
}
#endif

/**
 * @test
 *      Verify if appropriate system_error is thrown when file cannot
 *      be opened for writing.
 */
TEST_F(LoggingTest,
       FileHandlerThrowsFileExistsButCannotOpenToWriteReadOnlyFile) {
  TempDirectory tmp_dir;
  Path dir_path(tmp_dir.name());
  Path file_path(dir_path.join("test_file.log").str());

  // create empty log file
  ASSERT_TRUE(dir_path.exists());
  { std::ofstream file(file_path.str()); }

  // set file read-only
#ifdef _WIN32
  // set file read-only
  if (SetFileAttributes(file_path.c_str(), FILE_ATTRIBUTE_READONLY) == FALSE)
    FAIL() << "cannot set read-only attribute to file\n";
#else
  chmod(file_path.c_str(), S_IRUSR);
#endif

#ifdef _WIN32
  EXPECT_THROW_LIKE(
      FileHandler(file_path.str()), std::system_error,
      "File exists, but cannot open for writing " + file_path.str() + ": " +
          std::error_code(ERROR_ACCESS_DENIED, std::system_category())
              .message());
#else
  EXPECT_THROW_LIKE(FileHandler(file_path.str()), std::system_error,
                    "File exists, but cannot open for writing " +
                        file_path.str() + ": Permission denied");
#endif
}

TEST_F(LoggingTest, HandlerWithDisabledFormatting) {
  std::stringstream buffer;

  g_registry->add_handler("TestStreamHandler",
                          std::make_shared<StreamHandler>(buffer, false));
  logger.attach_handler("TestStreamHandler");

  // A bunch of casts to int for tellp to avoid C2666 in MSVC
  ASSERT_THAT((int)buffer.tellp(), Eq(0));
  logger.handle(Record{LogLevel::kInfo, stdx::this_process::get_id(),
                       kDefaultTimepoint, "my_module", "Message"});
  EXPECT_THAT((int)buffer.tellp(), Gt(0));

  // message should be logged verbatim
  EXPECT_THAT(buffer.str(), StrEq("Message\n"));

  // clean up
  g_registry->remove_handler("TestStreamHandler");
}

TEST_F(LoggingTest, Messages) {
  std::stringstream buffer;

  g_registry->add_handler("TestStreamHandler",
                          std::make_shared<StreamHandler>(buffer));
  logger.attach_handler("TestStreamHandler");

  std::chrono::time_point<std::chrono::system_clock> now =
      std::chrono::system_clock::now();

  const auto pid = stdx::this_process::get_id();

  auto check_message = [this, &buffer, now, pid](const std::string &message,
                                                 LogLevel level,
                                                 const std::string &level_str) {
    buffer.str("");
    ASSERT_THAT((int)buffer.tellp(), Eq(0));

    Record record{level, pid, now, "my_module", message};
    logger.handle(record);

    EXPECT_THAT(buffer.str(), EndsWith(message + "\n"));
    EXPECT_THAT(buffer.str(), HasSubstr(level_str));
  };

  check_message("Slippery spaghetti", LogLevel::kSystem, " SYSTEM ");
  check_message("Crazy noodles", LogLevel::kError, " ERROR ");
  check_message("Sloth tantrum", LogLevel::kWarning, " WARNING ");
  check_message("Russel's teapot", LogLevel::kInfo, " INFO ");
  check_message("Rabbit hole", LogLevel::kNote, " NOTE ");
  check_message("Bugs galore", LogLevel::kDebug, " DEBUG ");

  // Ensure no truncation of long messages
  check_message(std::string(512, 'a'), LogLevel::kInfo, " INFO ");

  // clean up
  g_registry->remove_handler("TestStreamHandler");
}

TEST_F(LoggingTest, TimestampPrecision) {
  std::stringstream buffer;

  g_registry->add_handler("TestStreamHandler",
                          std::make_shared<StreamHandler>(buffer));
  logger.attach_handler("TestStreamHandler");

  std::chrono::time_point<std::chrono::system_clock> now =
      std::chrono::system_clock::now();
  time_t cur = std::chrono::system_clock::to_time_t(now);
  struct tm cur_localtime;
#ifdef _WIN32
  localtime_s(&cur_localtime, &cur);
#else
  localtime_r(&cur, &cur_localtime);
#endif

  // we deliberately calculate the nanoseconds part slightly different here, to
  // ensure the handler gets the calculation correct
  const auto nsec_part = std::chrono::duration_cast<std::chrono::nanoseconds>(
      now - std::chrono::system_clock::from_time_t(cur));

  const auto pid = stdx::this_process::get_id();

  auto check_precision = [this, &buffer, now, pid, cur_localtime, nsec_part](
                             const std::string &message,
                             LogTimestampPrecision precision) {
    std::shared_ptr<Handler> handler =
        g_registry->get_handler("TestStreamHandler");
    buffer.str("");
    ASSERT_THAT((int)buffer.tellp(), Eq(0));

    // Format according to precision
    std::string dt{mysql_harness::utility::string_format(
        "%04d-%02d-%02d %02d:%02d:%02d", cur_localtime.tm_year + 1900,
        cur_localtime.tm_mon + 1, cur_localtime.tm_mday, cur_localtime.tm_hour,
        cur_localtime.tm_min, cur_localtime.tm_sec)};
    switch (precision) {
      case LogTimestampPrecision::kMilliSec:
        dt.append(mysql_harness::utility::string_format(
            ".%03lld",
            static_cast<long long int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(nsec_part)
                    .count())));
        break;
      case LogTimestampPrecision::kMicroSec:
        dt.append(mysql_harness::utility::string_format(
            ".%06lld",
            static_cast<long long int>(
                std::chrono::duration_cast<std::chrono::microseconds>(nsec_part)
                    .count())));
        break;
      case LogTimestampPrecision::kNanoSec:
        dt.append(mysql_harness::utility::string_format(
            ".%09lld",
            static_cast<long long int>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(nsec_part)
                    .count())));
        break;
      case LogTimestampPrecision::kSec:
      case LogTimestampPrecision::kNotSet:
      default:
        break;
    }

    handler->set_timestamp_precision(precision);

    Record record{LogLevel::kDebug, pid, now, "my_module", message};
    logger.handle(record);

    EXPECT_THAT(buffer.str(), StartsWith(dt));
  };

  check_precision("Crazy noodles", LogTimestampPrecision::kNotSet);
  check_precision("Sloth tantrum", LogTimestampPrecision::kSec);
  check_precision("Russel's teapot", LogTimestampPrecision::kMilliSec);
  check_precision("Bugs galore", LogTimestampPrecision::kMicroSec);
  check_precision("Kings knife", LogTimestampPrecision::kNanoSec);

  // clean up
  g_registry->remove_handler("TestStreamHandler");
}

class LogLevelTest : public LoggingTest,
                     public WithParamInterface<std::tuple<LogLevel, LogLevel>> {
};

// Check that messages are not emitted when the level is set higher.
TEST_P(LogLevelTest, Level) {
  LogLevel logger_level = std::get<0>(GetParam());
  LogLevel handler_level = std::get<1>(GetParam());

  std::stringstream buffer;
  g_registry->add_handler(
      "TestStreamHandler",
      std::make_shared<StreamHandler>(buffer, true, handler_level));
  logger.attach_handler("TestStreamHandler");

  std::chrono::time_point<std::chrono::system_clock> now =
      std::chrono::system_clock::now();

  const auto pid = stdx::this_process::get_id();

  // Set the log level of the logger.
  logger.set_level(logger_level);

  // Some handy shorthands for the levels as integers.
  const int min_level =
      std::min(static_cast<int>(logger_level), static_cast<int>(handler_level));
  const int max_level = static_cast<int>(LogLevel::kNotSet);

  // Loop over all levels below or equal to the provided level and
  // make sure that something is printed.
  for (int lvl = 0; lvl < min_level + 1; ++lvl) {
    buffer.str("");
    ASSERT_THAT((int)buffer.tellp(), Eq(0));
    logger.handle(Record{static_cast<LogLevel>(lvl), pid, now, "my_module",
                         "Some message"});
    auto output = buffer.str();
    EXPECT_THAT(output.size(), Gt(0));
  }

  // Loop over all levels above the provided level and make sure
  // that nothing is printed.
  for (int lvl = min_level + 1; lvl < max_level; ++lvl) {
    buffer.str("");
    ASSERT_THAT((int)buffer.tellp(), Eq(0));
    logger.handle(Record{static_cast<LogLevel>(lvl), pid, now, "my_module",
                         "Some message"});
    auto output = buffer.str();
    EXPECT_THAT(output.size(), Eq(0));
  }

  // clean up
  g_registry->remove_handler("TestStreamHandler");
}

const LogLevel all_levels[]{
    LogLevel::kFatal, LogLevel::kSystem, LogLevel::kError, LogLevel::kWarning,
    LogLevel::kInfo,  LogLevel::kNote,   LogLevel::kDebug};

INSTANTIATE_TEST_SUITE_P(CheckLogLevel, LogLevelTest,
                         Combine(ValuesIn(all_levels), ValuesIn(all_levels)));

////////////////////////////////////////////////////////////////
// Tests of the functional interface to the logger.
////////////////////////////////////////////////////////////////

TEST(FunctionalTest, CreateRemove) {
  // Test that creating two modules with different names succeed.
  EXPECT_NO_THROW(g_registry->create_logger("my_first"));
  EXPECT_NO_THROW(g_registry->create_logger("my_second"));

  // Test that trying to create two loggers for the same module fails.
  EXPECT_THROW(g_registry->create_logger("my_first"), std::logic_error);
  EXPECT_THROW(g_registry->create_logger("my_second"), std::logic_error);

  // Check that we can remove one of the modules and that removing it
  // a second time fails (mostly to get full coverage).
  ASSERT_NO_THROW(g_registry->remove_logger("my_second"));
  EXPECT_THROW(g_registry->remove_logger("my_second"), std::logic_error);

  // Clean up after the tests
  ASSERT_NO_THROW(g_registry->remove_logger("my_first"));
}

void expect_no_log(void (*func)(const char *, ...)
                       ATTRIBUTE_GCC_FORMAT(printf, 1, 2),
                   std::stringstream &buffer) {
  // Clear the buffer first and ensure that it was cleared to avoid
  // triggering other errors.
  buffer.str("");
  ASSERT_THAT((int)buffer.tellp(), Eq(0));

  // Write a simple message with a variable
  const int x = 3;
  func("Just a test of %d", x);

  // Log should be empty
  EXPECT_THAT((int)buffer.tellp(), Eq(0));
}

void expect_log(void (*func)(const char *, ...)
                    ATTRIBUTE_GCC_FORMAT(printf, 1, 2),
                std::stringstream &buffer, const char *kind) {
  // Clear the buffer first and ensure that it was cleared to avoid
  // triggering other errors.
  buffer.str("");
  ASSERT_THAT((int)buffer.tellp(), Eq(0));

  // Write a simple message with a variable
  const int x = 3;
  func("Just a test of %d", x);

  auto log = buffer.str();

  // Check that only one line was generated for the message. If the
  // message was sent to more than one logger, it could result in
  // multiple messages.
  EXPECT_THAT(std::count(log.begin(), log.end(), '\n'), Eq(1));

  // Check that the log contain the (expanded) message, the correct
  // indication (e.g., ERROR or WARNING), and the module name.
  EXPECT_THAT(log, HasSubstr("Just a test of 3"));
  EXPECT_THAT(log, HasSubstr(kind));
  EXPECT_THAT(log, HasSubstr(MYSQL_ROUTER_LOG_DOMAIN));
}

// Macros for log expectance at different log levels
#define EXPECT_LOG_LEVEL_DEBUG(buffer)        \
  expect_log(log_system, buffer, "SYSTEM");   \
  expect_log(log_error, buffer, "ERROR");     \
  expect_log(log_warning, buffer, "WARNING"); \
  expect_log(log_info, buffer, "INFO");       \
  expect_log(log_note, buffer, "NOTE");       \
  expect_log(log_debug, buffer, "DEBUG");

#define EXPECT_LOG_LEVEL_NOTE(buffer)         \
  expect_log(log_system, buffer, "SYSTEM");   \
  expect_log(log_error, buffer, "ERROR");     \
  expect_log(log_warning, buffer, "WARNING"); \
  expect_log(log_info, buffer, "INFO");       \
  expect_log(log_note, buffer, "NOTE");       \
  expect_no_log(log_debug, buffer);

#define EXPECT_LOG_LEVEL_INFO(buffer)         \
  expect_log(log_system, buffer, "SYSTEM");   \
  expect_log(log_error, buffer, "ERROR");     \
  expect_log(log_warning, buffer, "WARNING"); \
  expect_log(log_info, buffer, "INFO");       \
  expect_no_log(log_note, buffer);            \
  expect_no_log(log_debug, buffer);

#define EXPECT_LOG_LEVEL_WARNING(buffer)      \
  expect_log(log_system, buffer, "SYSTEM");   \
  expect_log(log_error, buffer, "ERROR");     \
  expect_log(log_warning, buffer, "WARNING"); \
  expect_no_log(log_info, buffer);            \
  expect_no_log(log_note, buffer);            \
  expect_no_log(log_debug, buffer);

#define EXPECT_LOG_LEVEL_ERROR(buffer)      \
  expect_log(log_system, buffer, "SYSTEM"); \
  expect_log(log_error, buffer, "ERROR");   \
  expect_no_log(log_warning, buffer);       \
  expect_no_log(log_info, buffer);          \
  expect_no_log(log_note, buffer);          \
  expect_no_log(log_debug, buffer);

#define EXPECT_LOG_LEVEL_SYSTEM(buffer)     \
  expect_log(log_system, buffer, "SYSTEM"); \
  expect_no_log(log_error, buffer);         \
  expect_no_log(log_warning, buffer);       \
  expect_no_log(log_info, buffer);          \
  expect_no_log(log_note, buffer);          \
  expect_no_log(log_debug, buffer);

#define EXPECT_LOG_LEVEL_NOT_SET(buffer) \
  expect_no_log(log_system, buffer);     \
  expect_no_log(log_error, buffer);      \
  expect_no_log(log_warning, buffer);    \
  expect_no_log(log_info, buffer);       \
  expect_no_log(log_note, buffer);       \
  expect_no_log(log_debug, buffer);

TEST(FunctionalTest, Loggers) {
  // The loader creates these modules during start, so tests of the
  // logger that involves the loader are inside the loader unit
  // test. Here we instead call these functions directly.
  ASSERT_NO_THROW(g_registry->create_logger(MYSQL_ROUTER_LOG_DOMAIN));

  std::stringstream buffer;
  auto handler = std::make_shared<StreamHandler>(buffer);
  g_registry->add_handler(StreamHandler::kDefaultName, handler);
  attach_handler_to_all_loggers(*g_registry, StreamHandler::kDefaultName);

  set_log_level_for_all_loggers(*g_registry, LogLevel::kDebug);
  EXPECT_LOG_LEVEL_DEBUG(buffer);

  set_log_level_for_all_loggers(*g_registry, LogLevel::kNote);
  EXPECT_LOG_LEVEL_NOTE(buffer);

  set_log_level_for_all_loggers(*g_registry, LogLevel::kError);
  EXPECT_LOG_LEVEL_ERROR(buffer);

  set_log_level_for_all_loggers(*g_registry, LogLevel::kWarning);
  EXPECT_LOG_LEVEL_WARNING(buffer);

  set_log_level_for_all_loggers(*g_registry, LogLevel::kSystem);
  EXPECT_LOG_LEVEL_SYSTEM(buffer);

  // Check that nothing is logged when the handler is unregistered.
  g_registry->remove_handler(StreamHandler::kDefaultName);
  set_log_level_for_all_loggers(*g_registry, LogLevel::kNotSet);
  EXPECT_LOG_LEVEL_NOT_SET(buffer);

  ASSERT_NO_THROW(g_registry->remove_logger(MYSQL_ROUTER_LOG_DOMAIN));
}

TEST(FunctionalTest, Handlers) {
  // The loader creates these modules during start, so tests of the
  // logger that involves the loader are inside the loader unit
  // test. Here we instead call these functions directly.
  ASSERT_NO_THROW(g_registry->create_logger(MYSQL_ROUTER_LOG_DOMAIN));

  std::stringstream buffer;
  std::stringstream buffer2;
  auto handler = std::make_shared<StreamHandler>(buffer);
  auto handler2 = std::make_shared<StreamHandler>(buffer2);
  g_registry->add_handler("stream1", handler);
  g_registry->add_handler("stream2", handler2);
  attach_handler_to_all_loggers(*g_registry, "stream1");
  attach_handler_to_all_loggers(*g_registry, "stream2");

  set_log_level_for_all_handlers(*g_registry, LogLevel::kDebug);
  EXPECT_LOG_LEVEL_DEBUG(buffer);
  EXPECT_LOG_LEVEL_DEBUG(buffer2);

  set_log_level_for_all_handlers(*g_registry, LogLevel::kNote);
  EXPECT_LOG_LEVEL_NOTE(buffer);
  EXPECT_LOG_LEVEL_NOTE(buffer2);

  set_log_level_for_all_handlers(*g_registry, LogLevel::kError);
  EXPECT_LOG_LEVEL_ERROR(buffer);
  EXPECT_LOG_LEVEL_ERROR(buffer2);

  set_log_level_for_all_handlers(*g_registry, LogLevel::kWarning);
  EXPECT_LOG_LEVEL_WARNING(buffer);
  EXPECT_LOG_LEVEL_WARNING(buffer2);

  set_log_level_for_all_handlers(*g_registry, LogLevel::kSystem);
  EXPECT_LOG_LEVEL_SYSTEM(buffer);
  EXPECT_LOG_LEVEL_SYSTEM(buffer2);

  // Check that nothing is logged when the handlers are unregistered.
  g_registry->remove_handler("stream1");
  g_registry->remove_handler("stream2");
  set_log_level_for_all_handlers(*g_registry, LogLevel::kNotSet);
  EXPECT_LOG_LEVEL_NOT_SET(buffer);
  EXPECT_LOG_LEVEL_NOT_SET(buffer2);

  ASSERT_NO_THROW(g_registry->remove_logger(MYSQL_ROUTER_LOG_DOMAIN));
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname();
  init_test_logger();
  g_registry = &mysql_harness::DIM::instance().get_LoggingRegistry();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
