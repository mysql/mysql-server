/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

// These tests are specific to Windows Eventlog

#ifdef _WIN32

#include <windows.h>

#include <winevt.h>

#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <system_error>

#include <gmock/gmock.h>

#include "harness_assert.h"
#include "my_compiler.h"
#include "mysqlrouter/utils.h"
#include "router_component_test.h"

#pragma comment( \
    lib, "wevtapi.lib")  // needed for linker to see stuff from winevt.h

/**
 * @file
 * @brief Component Tests that verify error reporting from Windows Service
 *        wrapper via Eventlog
 */

using testing::HasSubstr;
using namespace std::chrono_literals;
Path g_origin_path;

namespace {
// throws std::runtime_error
std::string wchar_to_string(const wchar_t *text) {
  char buf[16 * 1024];
  size_t converted = wcstombs(buf, text, sizeof(buf));
  if (converted == size_t(-1))
    throw std::runtime_error(
        "Converting wchar_t[] to char[] failed: wcstombs() returned error");
  if (converted == sizeof(buf))  // message got truncated, no zero-terminator
    throw std::runtime_error(
        "Converting wchar_t[] to char[] failed: buffer too small");
  return std::string(buf);
}

// noexcept wrapper (useful when you are building an error message and you don't
// want to throw yet another exception)
std::string wchar_to_string_noexcept(const wchar_t *text) noexcept {
  try {
    return wchar_to_string(text);
  } catch (const std::runtime_error &e) {
    return std::string("<") + e.what() + ">";
  }
}

std::error_code last_win32_error_code() {
  return {static_cast<int>(GetLastError()), std::system_category()};
}
}  // namespace

class RouterEventlogTest : public RouterComponentTest {};

/** @class EventlogSubscription
 *
 * This class abstracts the complexities of subscribing and processing Eventlog
 * log events coming from our Router via Windows Eventlog API. To use, just
 * instantiate this class and provide the callback function which will receive
 * all the notifications as a single line of XML. This callback can be provided
 * as a parameter in the constructor, or by calling `set_user_handler()`.
 *
 * Docs used when developing this class, they may help understanding it:
 * https://docs.microsoft.com/en-gb/windows/desktop/WES/subscribing-to-events
 */
class EventlogSubscription {
 public:
  using UserHandler = std::function<void(const std::string &xml)>;

  /** @brief Eventlog Subscription object
   *
   * Starts subscription to Router-generated log events coming from Eventlog,
   * and forwards their payload in form of one-line XML strings to
   * `user_handler`. This handler can be changed mid-flight if desired, please
   * see `set_user_handler()` documentation.
   *
   * @param user_handler user-defined handler that will be called every time a
   *                     log event is received - see `set_user_handler()` docs
   *                     for more information
   *
   * @throws std::runtime_error if subscribing to Eventlog fails
   */
  EventlogSubscription(const UserHandler &user_handler = default_user_handler) {
    set_ourselves_as_active_subscriber();
    set_user_handler(user_handler);
    subscribe_to_eventlog();  // throws std::runtime_error
  }

  /** @brief Stop receiving Eventlog log events */
  ~EventlogSubscription() {
    clear_active_subscriber();
    unsubscribe_from_eventlog();
  }

  /** @brief Sets user log handler
   *
   * @param user_handler user-defined handler that will receive one-line XML
   *        string for every log event that gets pushed to us. It may only
   *        throw std::runtime_error.
   *
   * @note Handler may be changed while Eventlog subscription is active
   */
  void set_user_handler(const UserHandler &user_handler) noexcept {
    std::lock_guard<std::mutex> lk(user_handler_mtx_);
    user_handler_ = user_handler;
  }
  /** @overload */
  void set_user_handler(UserHandler &&user_handler) noexcept {
    std::lock_guard<std::mutex> lk(user_handler_mtx_);
    user_handler_ = std::move(user_handler);
  }

 private:
  void set_ourselves_as_active_subscriber() {
    // see struct ActiveSubscription documentation for info
    std::lock_guard<std::mutex> lk(callback_context_.mtx);
    if (callback_context_.ptr != nullptr)
      throw std::runtime_error("Only one instance currently supported");
    callback_context_.ptr = this;
  }

  void clear_active_subscriber() {
    // prevent eventlog_log_cb() from referencing this object;
    // see struct ActiveSubscription documentation for info
    std::lock_guard<std::mutex> lk(callback_context_.mtx);
    callback_context_.ptr = nullptr;
  }

  /** @brief Subscribe to Eventlog events
   *
   * After calling this function, eventlog_event_cb() will be called once for
   * every Eventlog event that appears, until unsubscribe_from_eventlog() is
   * called. Note that event != log entry, as there may also be other events.
   *
   * @throws std::runtime_error on subscription failure
   */
  void subscribe_to_eventlog() {
    // On Windows 10 (and probably many others), 'Application' is that thing you
    // see in Event Viewer program, under 'Windows Logs' tree (next to
    // 'Security', 'Setup', 'System', etc)
    const wchar_t *channel = L"Application";

    // clang-format off
    // This is an XPath query which "greps for" messages that are of interest to us.
    //
    // @note: To understand the fields used in our XPath query, see example XML
    //        shown in eventlog_log_cb().
    //
    // @note: At the time of writing, expression "Provider[@Name = 'MySQL Router']"
    //        could also be replaced by "(Provider/@Name = 'MySQL Router')" and
    //        it seems to work - if one day the below fails to run, try this
    //        other expression (unsure if they're equivalent).
    // clang-format on
    const wchar_t *query =
        L"*[System[(Level <= 3)"  // log level, 3 = ERROR
        " and Provider[@Name = 'MySQL Router']"
        " and TimeCreated[timediff(@SystemTime) <= 10000]]]";  // backlog, value
                                                               // in ms

    // Subscribe to eventlog events. eventlog_event_cb() will be called once
    // for:
    // - every event dating back <value defined in TimeCreated[...] expression>
    //   before subscribing
    // - every new event that appears after subscribing
    subscription_ =
        EvtSubscribe(NULL, NULL, channel, query, NULL, &callback_context_,
                     (EVT_SUBSCRIBE_CALLBACK)eventlog_event_cb,
                     EvtSubscribeStartAtOldestRecord);

    if (!subscription_) {
      DWORD status = GetLastError();
      if (status == ERROR_EVT_CHANNEL_NOT_FOUND)
        throw std::runtime_error(
            std::string("EvtSubscribe() failed: Channel '") +
            wchar_to_string_noexcept(channel) + "' not found");
      else if (status == ERROR_EVT_INVALID_QUERY)
        // if we need, we could call EvtGetExtendedStatus() to get more info why
        // the query failed
        throw std::runtime_error(
            std::string("EvtSubscribe() failed: Invalid query '" +
                        wchar_to_string_noexcept(query) + "'"));
      else
        throw std::runtime_error(
            std::string("EvtSubscribe() failed, error code = ") +
            std::to_string(status));
    }
  }

  /** @brief Unsubscribe from Eventlog events */
  void unsubscribe_from_eventlog() noexcept {
    BOOL ok = EvtClose(subscription_);
    if (!ok) {
      std::cerr << "WARNING: EvtClose() failed: " << last_win32_error_code()
                << std::endl;
    }
  }

  /** @brief Eventlog event handler
   *
   * This callback receives events that match our XPath query criteria.
   * Note that log entry is just one type of an event, and those events are
   * forwarded to eventlog_log_cb().
   *
   * @param action event type
   * @param context ActiveSubscription ptr
   * @param event event handle
   */
  static DWORD WINAPI eventlog_event_cb(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                        PVOID context,  // ActiveSubscripton*
                                        EVT_HANDLE event) noexcept {
    // Extract EventlogSubscription object which subscribed to log events.
    // Note that the EventlogSubscription object might no longer exist at the
    // time of the call (race between EvtClose() in ~EventlogSubscription() and
    // this function), so before we proceed we must ensure that:
    //   1. EventlogSubscription object still exists
    //   2. that ~EventlogSubscription() doesn't destroy it while we execute
    // We take care of #1 by checking if the ptr is still valid
    // (~EventlogSubscription() will set it to NULL) and #2 is taken care of
    // the by mutex, which will block ~EventlogSubscription() until this
    // callback finishes.
    ActiveSubscription &as = *static_cast<ActiveSubscription *>(context);
    std::lock_guard<std::mutex> lk(as.mtx);
    EventlogSubscription *self = as.ptr;
    if (self == nullptr)
      return ERROR_SUCCESS;  // EventlogSubscription is dead, do nothing

    try {
      switch (action) {
        case EvtSubscribeActionError:
          // (DWORD)(event) is what the docs say to do. However, DWORD is
          // unsigned long (32-bit) while `event` is void* (64-bit), so this
          // triggers two warnings:
          //   warning C4311: 'type cast': pointer truncation from 'EVT_HANDLE'
          //   to 'DWORD' warning C4302: 'type cast': truncation from
          //   'EVT_HANDLE' to 'DWORD'
          // So we disable them here
          MY_COMPILER_DIAGNOSTIC_PUSH()
          MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4311)
          MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4302)
          MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wvoid-pointer-to-int-cast")
          throw std::runtime_error(
              "Eventlog callback received an error: %lu\n" +
              std::to_string(
                  (DWORD)(event)));  // c-style cast perscribed by docs
          MY_COMPILER_DIAGNOSTIC_POP()
          break;

        case EvtSubscribeActionDeliver:
          self->eventlog_log_cb(event);  // throws std::runtime_error
          break;

        default:
          throw std::runtime_error(
              "Eventlog callback received unrecognized action");
      }
    } catch (const std::runtime_error &e) {
      // Since this is a WIN32API callback, we can't simply bubble up the
      // exception. The best we can do is display the error so that the user can
      // find out why the test failed.
      std::cerr << "Querying Eventlog from OS failed: " << e.what()
                << std::endl;
    }

    return ERROR_SUCCESS;  // According to docs, returned status is ignored
  }

  /** @brief Extract Eventlog log event payload to XML string, and forward it
   *         to user's handler
   *
   * @param event Eventlog log event handle
   *
   * @throws std::runtime_error if:
   *         - rendering to XML fails
   *         - conversion to std::string fails
   *         - user callback function throws std::runtime_error
   */
  void eventlog_log_cb(EVT_HANDLE event) {
    // Obtain log entry as XML string.  Here is an example of how that XML might
    // looks like:
    // clang-format off
    //
    //   <Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>
    //     <System>
    //       <Provider Name='MySQL Router'/>
    //       <EventID Qualifiers='0'>0</EventID>
    //       <Level>2</Level>
    //       <Task>0</Task>
    //       <Keywords>0x80000000000000</Keywords>
    //       <TimeCreated SystemTime='2019-01-28T15:03:16.459313800Z'/>
    //       <EventRecordID>87695</EventRecordID>
    //       <Channel>Application</Channel>
    //       <Computer>somehost.example.com</Computer><Security/>
    //     </System>
    //     <EventData>
    //       <Data>MySQL Router</Data>
    //       <Data>Some example message</Data> <---- log message appears here
    //     </EventData>
    //   </Event>
    //
    // clang-format on
    // Note that the above example is nicely formatted - the actual XML string
    // we get from EvtRenderEventXml() is just a single line.
    constexpr DWORD buf_size = 16 * 1024;
    wchar_t buf[buf_size];  // one-line XML will show up here
    {
      DWORD buf_used = 0;      // \_ we have no use for them but
      DWORD property_cnt = 0;  // /  they must be supplied to EvtRender
      if (!EvtRender(NULL, event, EvtRenderEventXml, buf_size, buf, &buf_used,
                     &property_cnt)) {
        DWORD status = GetLastError();
        if (status == ERROR_INSUFFICIENT_BUFFER)
          throw std::runtime_error("EvtRender() failed: buffer is too small");
        else
          throw std::runtime_error("EvtRender() failed: error code = " +
                                   std::to_string(status));
      }
    }

    // convert resulting wchar_t[] to std::string and call user's callback with
    // it
    std::string xml = wchar_to_string(buf);  // throws std::runtime_error
    try {
      std::lock_guard<std::mutex> lk(user_handler_mtx_);
      user_handler_(xml);
    } catch (std::runtime_error &) {
      // throwing std::runtime_error is allowed, rethrow
      throw;
    } catch (...) {
      // API expects only std::runtime_error to be thrown
      harness_assert_this_should_not_execute();
    }
  }

  /** @brief Default log handler - no-op */
  static void default_user_handler(const std::string & /* xml */) noexcept {}

  EVT_HANDLE subscription_;
  UserHandler user_handler_;
  std::mutex user_handler_mtx_;

  /** @brief Controlling EventlogSubscription info, passed to user log handler
   * via "context" argument
   *
   * We need WIN32API to pass EventlogSubscription ptr to eventlog_event_cb()
   * when it calls it, and we must ensure it doesn't try to use it if when this
   * object no longer exists. This can happen, because there's a race between
   * EvtClose()/~EventlogSubscription and the eventlog_event_cb(). Therefore to
   * protect against a segfault, instead of passing this ptr directly, we pass
   * a static struct (static = always exists) with this ptr in it, and we
   * enforce a policy that we always set this ptr to NULL in
   * ~EventlogSubscription(). eventlog_event_cb() checks this ptr for NULL
   * before trying to touch our object.
   * We also have a mutex to ensure that this assignment to NULL doesn't happen
   * while eventlog_event_cb() is already executing - ~EventlogSubscription()
   * will wait for it to complete, before proceeding.
   *
   * This mechanism has two limitations (both irrelevant at the time of
   * writing):
   * 1. only 1 EventlogSubscription object can exist at any one time
   * 2. If 2nd EventlogSubscription object was created quickly enough after 1st
   *    one got destroyed, a callback destined for 1st object could be serviced
   *    by the 2nd.
   */
  struct ActiveSubscription {
    EventlogSubscription *ptr;
    std::mutex mtx;
  };
  static ActiveSubscription callback_context_;
};

EventlogSubscription::ActiveSubscription
    EventlogSubscription::callback_context_;

class EventlogMatcher {
 public:
  /** @brief Ctor - initialises object and logs marker
   *
   * @throws std::runtime_error if logging marker fails
   */
  EventlogMatcher(const std::string &message, bool debug_mode = false)
      : message_(message),
        timestamp_marker_(make_start_marker()),
        found_log_beginning_(false),
        found_message_(false),
        debug_mode_(debug_mode) {
    // mark logs start, throws std::runtime_error
    mysqlrouter::write_windows_event_log(timestamp_marker_);
  }

  /** @brief Sniffs Eventlog entries (XML string) for message of interest */
  void operator()(const std::string xml) noexcept {
    if (debug_mode_) std::cerr << "INCOMING EVENTLOG:\n" << xml << std::endl;

    if (xml.find(timestamp_marker_) != xml.npos) {
      found_log_beginning_ = true;
    } else if (found_log_beginning_ && xml.find(message_) != xml.npos)
      found_message_ = true;
  }

  /** @brief Returns search results */
  const bool &found() noexcept { return found_message_; }

  /** @brief Returns end marker
   *
   * This is a useful utility function that generates a string that could be
   * used as an end marker in logs. It is analogous (but opposite) to
   * `make_start_marker()`.
   *
   * @return Marker string
   */
  static std::string make_end_marker() noexcept {
    return std::string("## END ") +
           std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()) +
           " ##";
  }

 private:
  /** @brief Return log start marker
   *
   * Generates a marker which we use to find beginning of logs generated by
   * current test. Log entries appearing before this marker will be ignored, as
   * they were written before current test ran. It is assmed that the marker is
   * unique enough to never match against anything else in the Eventlog, and
   * particularly the message we're seeking.
   *
   * @return Marker string
   */
  static std::string make_start_marker() noexcept {
    return std::string("## START ") +
           std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()) +
           " ##";
  }

  const std::string message_;
  const std::string timestamp_marker_;
  bool found_log_beginning_;
  bool found_message_;
  const bool debug_mode_;
};

/** @brief wait up to 10 seconds for the predicate to turn true
 *
 * Typically used to wait for log entry to appear. Return should be almost
 * instantenaous under normal conditions, but we give it full 10 seconds in
 * case we're running on a very busy machine.
 *
 * @returns True if condition was meat, false if it timed out
 */
static bool wait_until_true(const std::function<bool(void)> &pred) noexcept {
  for (int i = 0; i <= 1000; i++) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}
/** @overload */
static bool wait_until_true(const bool &pred) noexcept {
  return wait_until_true([&]() { return pred; });
}

/**
 * @test
 * Verify that errors:
 * - reported by the Windows Service wrapper code (proxy_main() and friends)
 * - BEFORE we are certain if we are running as a Service
 * get written to:
 * - Windows Eventlog
 * - STDERR
 */
TEST_F(RouterEventlogTest, wrapper_running_as_unknown) {
  // Test method:
  //   Run `mysqlrouter.exe --service`
  // Explanation:
  //   Starting with `--service` is only allowed from Windows Service
  //   Controller; if started from cmd.exe or another process, it will exit
  //   with error.

  // Error message will be different depending on whether MySQLRouter service
  // is registered on this machine or not.
  constexpr char expected_message_registered[] =
      "Starting service failed (are you trying to run a service from "
      "command-line?): The service process "
      "could not connect to the service controller.";
  constexpr char expected_message_not_registered[] =
      "Could not find service 'MySQLRouter'!\nUse --install-service or "
      "--install-service-manual option to install the service first.";

  // start sniffing Eventlog
  EventlogMatcher matcher_registered(expected_message_registered);
  EventlogMatcher matcher_not_registered(expected_message_not_registered);
  EventlogSubscription sub([&](const std::string &xml) {
    matcher_registered(xml);
    matcher_not_registered(xml);
  });

  // run the router and wait for it to exit
  auto &router = launch_router({"--service"}, EXIT_FAILURE, true, false, -1s);
  check_exit_code(router, EXIT_FAILURE);

  // verify the message WAS written to STDERR
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), AnyOf(HasSubstr(expected_message_registered),
                                 HasSubstr(expected_message_not_registered)));

  // verify the message WAS written to Eventlog
  EXPECT_TRUE(wait_until_true([&]() {
    return matcher_registered.found() || matcher_not_registered.found();
  }));
}

/**
 * @test
 * Verify that errors:
 * - reported by the Windows Service wrapper code (proxy_main() and friends)
 * - AFTER we are certain that we ARE running as a Service
 * get written to:
 * - Windows Eventlog
 * - (they may also additionally land in STDERR, we don't care, and therefore
 *    don't test for that)
 */
TEST_F(RouterEventlogTest, wrapper_running_as_service) {
  // This test is a stub that we might implement one day. Two prerequisites
  // must be fulfilled to implement it:
  // - we'd need to register Router as a Service
  // - have some error message to test on. Right now, the moment between
  //   launching as a service and entering main() is very narrow, and there's
  //   nothing in between that logs.
}

/**
 * @test
 * Verify that errors:
 * - reported by the Windows Service wrapper code (proxy_main() and friends)
 * - AFTER we are certain that we ARE NOT running as a Service
 * get written to:
 * - STDERR
 * but not to:
 * - Windows Eventlog
 */
TEST_F(RouterEventlogTest, wrapper_running_as_process) {
  // run mysqlrouter.exe  --install (no -c)
  // Test method:
  //   Run `mysqlrouter.exe --install-service` without the required
  //   `-c path/to/conf/file` to trigger error message
  // Explanation:
  //   Service will never try running with `--install-service` parameter,
  //   therefore if we are running with such, it must be from the
  //   command-line/script. And if so, we should not log to Eventlog.

  // expected message
  constexpr char expected_message[] =
      "Service install option requires an existing configuration file to be "
      "specified (-c";
  EventlogMatcher error_matcher(expected_message);

  // Since we're NOT expecting the error message to appear in Eventlog, we need
  // another way to tell we finished scanning the log. We do this by writing
  // this message after Router finishes, and then once we find it, we know we
  // scanned the entire log.
  const std::string log_end_marker = EventlogMatcher::make_end_marker();
  EventlogMatcher end_matcher(log_end_marker);

  // start sniffing Eventlog
  EventlogSubscription sub([&](const std::string &xml) {
    error_matcher(xml);
    end_matcher(xml);
  });

  // run the router and wait for it to exit
  auto &router = launch_router({"--install-service"}, EXIT_FAILURE, true, false,
                               -1s);  // missing -c <config>
  check_exit_code(router, EXIT_FAILURE);

  // mark the end of log
  mysqlrouter::write_windows_event_log(log_end_marker);

  // verify the message WAS written to STDERR
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr(expected_message));

  // verify the message WAS NOT written to Eventlog
  wait_until_true(end_matcher.found());
  EXPECT_FALSE(error_matcher.found());
  EXPECT_TRUE(end_matcher.found());
}

/**
 * @test
 * Verify that errors:
 * - reported by Router code (real_main() and everything it spawns)
 * - running as a service
 * - BEFORE logging facility is initialised from configuration
 * get written to:
 * - Windows Eventlog
 * - (they may also additionally land in STDERR, we don't care, and therefore
 *    don't test for that)
 */
TEST_F(RouterEventlogTest, application_running_as_service_preconfig) {
  // This test is a stub that we might implement one day. A prerequisite must
  // be fulfilled to implement it:
  // - we'd need to register Router as a Service
}

/**
 * @test
 * Verify that errors:
 * - reported by Router code (real_main() and everything it spawns)
 * - NOT running as a service
 * - BEFORE logging facility is initialised from configuration
 * get written to:
 * - STDERR
 * and not to:
 * - Windows Eventlog
 */
TEST_F(RouterEventlogTest, application_running_as_process_preconfig) {
  // Test method:
  //   Run `mysqlrouter.exe -c <path/to/config/that/does/not/exist>`
  // Explanation:
  //   Starting with `-c <bogus/path/to/config>` will cause the config-reading
  //   code to throw. The throwing happens in application code and the logging
  //   near main().

  // expected message
  constexpr char expected_message[] =
      "Error: The configuration file 'bogus.conf' does not exist.\n";
  EventlogMatcher error_matcher(expected_message);

  // Since we're NOT expecting the error message to appear in Eventlog, we need
  // another way to tell we finished scanning the log. We do this by writing
  // this message after Router finishes, and then once we find it, we know we
  // scanned the entire log.
  const std::string log_end_marker = EventlogMatcher::make_end_marker();
  EventlogMatcher end_matcher(log_end_marker);

  // start sniffing Eventlog
  EventlogSubscription sub([&](const std::string &xml) {
    error_matcher(xml);
    end_matcher(xml);
  });

  // run the router and wait for it to exit
  auto &router =
      launch_router({"-c", "bogus.conf"}, EXIT_FAILURE, true, false, -1s);
  check_exit_code(router, EXIT_FAILURE);

  // mark the end of log
  mysqlrouter::write_windows_event_log(log_end_marker);

  // verify the message WAS written to STDERR
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr(expected_message));

  // verify the message WAS NOT written to Eventlog
  wait_until_true(end_matcher.found());
  EXPECT_FALSE(error_matcher.found());
  EXPECT_TRUE(end_matcher.found());
}

/**
 * @test
 * Verify that errors:
 * - reported by Router code (real_main() and everything it spawns)
 * - NOT running as a service
 * - AFTER logging facility is initialised from configuration
 * get written to:
 * - STDERR (as specified by config)
 * and not to:
 * - Windows Eventlog (as not specified by config)
 */
TEST_F(RouterEventlogTest, application_running_as_process_postconfig) {
  // Test method:
  //   Run `mysqlrouter.exe` against config with bad [routing] section
  // Explanation:
  //   Router will read configuration, then notice that [routing] section is
  //   missing obligatory key (destinations), log error, and exit. The
  //   throwing happens in application code and the logging near main().

  // create config with bad [routing] section (missing "destinations" key)
  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = "";  // log to STDERR
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[routing]", &params);

  // expected message
  constexpr char expected_message[] =
      "Configuration error: option destinations in [routing] is required\n";
  EventlogMatcher error_matcher(expected_message);

  // Since we're NOT expecting the error message to appear in Eventlog, we need
  // another way to tell we finished scanning the log. We do this by writing
  // this message after Router finishes, and then once we find it, we know we
  // scanned the entire log.
  const std::string log_end_marker = EventlogMatcher::make_end_marker();
  EventlogMatcher end_matcher(log_end_marker);

  // start sniffing Eventlog
  EventlogSubscription sub([&](const std::string &xml) {
    error_matcher(xml);
    end_matcher(xml);
  });

  // run the router and wait for it to exit
  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1s);
  check_exit_code(router, EXIT_FAILURE);

  // mark the end of log
  mysqlrouter::write_windows_event_log(log_end_marker);

  // verify the message WAS written to STDERR
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr(expected_message));

  // verify the message WAS NOT written to Eventlog
  wait_until_true(end_matcher.found());
  EXPECT_FALSE(error_matcher.found());
  EXPECT_TRUE(end_matcher.found());
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#else
int main() { return 0; }
#endif
