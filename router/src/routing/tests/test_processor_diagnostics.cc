/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "processors/base/processor.h"
#include "processors/base/processor_diagnostics.h"

namespace {

class TestProcessor : public BasicProcessor {
 public:
  TestProcessor(std::string name, std::optional<std::string_view> stage_name)
      : BasicProcessor(nullptr),
        name_(std::move(name)),
        stage_name_(std::move(stage_name)) {}

  stdx::expected<Result, std::error_code> process() override {
    return Result::Done;
  }

  std::string diagnostic_name() const override { return name_; }

  std::optional<std::string_view> diagnostic_stage_name() const override {
    using namespace std::literals;
    return stage_name_;
  }

 private:
  std::string name_;
  std::optional<std::string_view> stage_name_;
};

}  // namespace

TEST(ProcessorDiagnosticsTest, FormatsProcessorStackWithStateAndName) {
  std::vector<std::unique_ptr<BasicProcessor>> stack;
  stack.emplace_back(
      std::make_unique<TestProcessor>("Bootstrap", std::nullopt));
  stack.emplace_back(std::make_unique<TestProcessor>("AuthFlow", "WaitBoth"));

  const auto ec = make_error_code(std::errc::bad_message);
  const auto msg =
      routing::processor_diagnostics::processor_failed_message(ec, stack);

  std::ostringstream expected;
  expected << "classic::loop() processor failed: " << ec.message() << " ("
           << ec.category().name() << ":" << ec.value()
           << "), top_processor=AuthFlow{state=WaitBoth}, "
              "processor_stack_size=2, "
              "processor_stack_below_top=[Bootstrap{state=n/a}]";

  EXPECT_EQ(msg, expected.str());
}

TEST(ProcessorDiagnosticsTest, FormatsNaWhenTopProcessorHasNoStage) {
  std::vector<std::unique_ptr<BasicProcessor>> stack;
  stack.emplace_back(
      std::make_unique<TestProcessor>("Bootstrap", std::nullopt));

  const auto ec = make_error_code(std::errc::bad_message);
  const auto msg =
      routing::processor_diagnostics::processor_failed_message(ec, stack);

  std::ostringstream expected;
  expected << "classic::loop() processor failed: " << ec.message() << " ("
           << ec.category().name() << ":" << ec.value()
           << "), top_processor=Bootstrap{state=n/a}, "
              "processor_stack_size=1, processor_stack_below_top=[]";

  EXPECT_EQ(msg, expected.str());
}

TEST(ProcessorDiagnosticsTest,
     FormatsThreeProcessorsWithReversedBelowTopOrder) {
  std::vector<std::unique_ptr<BasicProcessor>> stack;
  stack.emplace_back(
      std::make_unique<TestProcessor>("Bootstrap", std::nullopt));
  stack.emplace_back(std::make_unique<TestProcessor>("AuthFlow", "Connect"));
  stack.emplace_back(std::make_unique<TestProcessor>("QueryExec", "Response"));

  const auto ec = make_error_code(std::errc::bad_message);
  const auto msg =
      routing::processor_diagnostics::processor_failed_message(ec, stack);

  std::ostringstream expected;
  expected << "classic::loop() processor failed: " << ec.message() << " ("
           << ec.category().name() << ":" << ec.value()
           << "), top_processor=QueryExec{state=Response}, "
              "processor_stack_size=3, "
              "processor_stack_below_top=[AuthFlow{state=Connect}, "
              "Bootstrap{state=n/a}]";

  EXPECT_EQ(msg, expected.str());
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
