/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <gtest/gtest.h>
#include <stdarg.h>
#include <string>

using testing::TestEventListeners;
using testing::TestCase;
using testing::TestEventListener;
using testing::TestInfo;
using testing::TestPartResult;
using testing::UnitTest;
using testing::UnitTest;


/**
   Receives events from googletest, and outputs interesting events
   in TAP compliant format.
   Implementation is inspired by PrettyUnitTestResultPrinter.
   See documentation for base class.
 */
class TapEventListener : public TestEventListener
{
public:
  TapEventListener() : m_test_number(0) {}
  virtual ~TapEventListener() {}

  virtual void OnTestProgramStart(const UnitTest& /*unit_test*/) {}
  virtual void OnTestIterationStart(const UnitTest& unit_test, int iteration);
  virtual void OnEnvironmentsSetUpStart(const UnitTest& unit_test);
  virtual void OnEnvironmentsSetUpEnd(const UnitTest& /*unit_test*/) {}
  virtual void OnTestCaseStart(const TestCase& test_case);
  virtual void OnTestStart(const TestInfo& test_info);
  virtual void OnTestPartResult(const TestPartResult& test_part_result);
  virtual void OnTestEnd(const TestInfo& test_info);
  virtual void OnTestCaseEnd(const TestCase& /*test_case*/) {};
  virtual void OnEnvironmentsTearDownStart(const UnitTest& unit_test);
  virtual void OnEnvironmentsTearDownEnd(const UnitTest& /*unit_test*/) {}
  virtual void OnTestIterationEnd(const UnitTest& unit_test, int iteration);
  virtual void OnTestProgramEnd(const UnitTest& /*unit_test*/) {}
private:
  int m_test_number;
  std::string m_test_case_name;
};


/**
   Prints arguments to stdout using vprintf, but prefixes with "# ".
*/
static void tap_diagnostic_printf(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  printf("# ");
  vprintf(fmt, args);
  va_end(args);
}


/**
   Formats a list of arguments to a string, using the same format
   spec as for printf.
 */
static std::string format_string(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char buffer[4096];
  const int buffer_size = sizeof(buffer)/sizeof(buffer[0]);
  const int size = vsnprintf(buffer, buffer_size, fmt, args);
  va_end(args);

  if (size < 0 || size >= buffer_size)
  {
    return std::string("<formatting error or buffer exceeded>");
  } else {
    return std::string(buffer, size);
  }

}


/**
   Formats a countable noun.  Depending on its quantity, either the
   singular form or the plural form is used. e.g.

   FormatCountableNoun(1, "formula", "formuli") returns "1 formula".
   FormatCountableNoun(5, "book", "books") returns "5 books".   
 */
static std::string format_countable_noun(int count,
                                         const char *singular_form,
                                         const char *plural_form)
{
  return
    format_string("%d %s", count, count == 1 ? singular_form : plural_form);
}


/**
   Formats the count of tests.
 */
static std::string format_test_count(int test_count)
{
  return format_countable_noun(test_count, "test", "tests");
}


/**
   Formats the count of test cases.
*/
static std::string format_testcase_count(int test_case_count)
{
  return format_countable_noun(test_case_count, "test case", "test cases");
}


/**
   Converts a TestPartResult::Type enum to human-friendly string
   representation.
*/
static std::string test_part_result_type_tostring(TestPartResult::Type type)
{
  switch (type) {
  case TestPartResult::kSuccess:
    return "Success";

  case TestPartResult::kNonFatalFailure:
  case TestPartResult::kFatalFailure:
    return "Failure";
  }
}


/**
   Formats a source file path and a line number as they would appear
   in a compiler error message.
*/
static std::string format_file_location(const TestPartResult &test_part_result)
{
  const char* const file= test_part_result.file_name();
  const char* const file_name = file == NULL ? "unknown file" : file;
  const int line= test_part_result.line_number();
  if (line < 0)
    return format_string("%s:", file_name);
  return format_string("%s:%d:", file_name, line);
}


/**
   Formats a TestPartResult as a string.
 */
static std::string test_part_result_tostring(const TestPartResult
                                             &test_part_result)
{
  return format_file_location(test_part_result)
    + " "
    + test_part_result_type_tostring(test_part_result.type())
    + test_part_result.message();
}


void TapEventListener::OnTestIterationStart(const UnitTest& unit_test,
                                            int iteration)
{
  const std::string num_tests=
    format_test_count(unit_test.test_to_run_count()).c_str();
  const std::string num_test_cases=
    format_testcase_count(unit_test.test_case_to_run_count()).c_str();
  tap_diagnostic_printf("Running %s from %s.\n",
                        num_tests.c_str(),
                        num_test_cases.c_str());
  printf("%d..%d\n", 1, unit_test.test_to_run_count());
  fflush(stdout);
}


void TapEventListener::OnEnvironmentsSetUpStart(const UnitTest& unit_test)
{
  tap_diagnostic_printf("Global test environment set-up.\n");
  fflush(stdout);
}


void TapEventListener::OnTestCaseStart(const TestCase& test_case) {
  m_test_case_name = test_case.name();
}


void TapEventListener::OnTestStart(const TestInfo& test_info)
{
  ++m_test_number;
  tap_diagnostic_printf("Run %d %s.%s\n", m_test_number, 
                        m_test_case_name.c_str(), test_info.name());
  fflush(stdout);
}


void TapEventListener::OnTestPartResult(const TestPartResult& test_part_result)
{
  if (test_part_result.passed())
    return;
  std::string error_message= test_part_result_tostring(test_part_result);
  size_t pos = 0;
  while((pos = error_message.find("\n", pos)) != std::string::npos) {
    error_message.replace(pos, 1, "\n# ");
    pos += 1;
  }
  tap_diagnostic_printf("%s\n", error_message.c_str());
}


void TapEventListener::OnTestEnd(const TestInfo& test_info)
{
  if (test_info.result()->Passed()) {
    printf("ok %d\n", m_test_number);
  } else {
    printf("not ok %d\n", m_test_number);
  }
  fflush(stdout);
}


void TapEventListener::OnEnvironmentsTearDownStart(const UnitTest& unit_test)
{
  tap_diagnostic_printf("Global test environment tear-down\n");
  fflush(stdout);
}


void TapEventListener::OnTestIterationEnd(const UnitTest& unit_test,
                                          int iteration)
{
  const std::string num_tests=
    format_test_count(unit_test.test_to_run_count());
  const std::string num_test_cases=
    format_testcase_count(unit_test.test_case_to_run_count());
  tap_diagnostic_printf("Ran %s from %s.\n",
                        num_tests.c_str(),
                        num_test_cases.c_str());
  const std::string num_successful_tests=
    format_test_count(unit_test.successful_test_count());
  tap_diagnostic_printf("Passed: %s.\n", num_successful_tests.c_str());

  if (!unit_test.Passed()) {
    const int num_failures = unit_test.failed_test_count();
    tap_diagnostic_printf("Failed: %s.\n",
                          format_test_count(num_failures).c_str());
  }
  
  const int num_disabled = unit_test.disabled_test_count();
  if (num_disabled && !testing::GTEST_FLAG(also_run_disabled_tests)) {
    tap_diagnostic_printf("YOU HAVE %d disabled %s\n",
                          num_disabled,
                          num_disabled == 1 ? "TEST" : "TESTS");
  }
  fflush(stdout);
}


/**
   Removes the default googletest listener (a PrettyUnitTestResultPrinter),
   and installs our own TAP compliant pretty printer instead.
 */
void install_tap_listener()
{
  TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
  delete listeners.Release(listeners.default_result_printer());
  listeners.Append(new TapEventListener);
}
