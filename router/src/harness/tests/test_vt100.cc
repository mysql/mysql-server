/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/vt100.h"
#include "mysql/harness/vt100_filter.h"

#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>

#define ESC "\x1b"

struct Vt100Param {
  std::string test_name;
  std::string seq;
  std::vector<std::string> candidates;
};

class Vt100Test : public ::testing::Test,
                  public ::testing::WithParamInterface<Vt100Param> {};

/**
 * @test Vt100::${name} generates the expected sequence.
 */
TEST_P(Vt100Test, ensure_generates) {
  EXPECT_THAT(GetParam().candidates, ::testing::Contains(GetParam().seq));
}

/**
 * @test Vt100Filter(..., false) doesn't filter out Vt100 sequences.
 */
TEST_P(Vt100Test, ensure_filter_ignores) {
  std::ostringstream out_stream;
  Vt100Filter filtered_streambuf(out_stream.rdbuf(), false);
  std::ostream filtered_stream(&filtered_streambuf);

  filtered_stream << GetParam().seq;
  filtered_stream.flush();

  EXPECT_THAT(out_stream.str(), GetParam().seq);
}

/**
 * @test Vt100Filter(..., true) removes Vt100 sequences.
 */
TEST_P(Vt100Test, ensure_filter_filters) {
  std::ostringstream out_stream;
  Vt100Filter filtered_streambuf(out_stream.rdbuf(), true);
  std::ostream filtered_stream(&filtered_streambuf);

  filtered_stream << GetParam().seq;
  filtered_stream.flush();

  EXPECT_THAT(out_stream.str(), "");
}

const Vt100Param vt100_params[]{
//
#define G(FUNC, LETTER)                                                     \
  Vt100Param{                                                               \
      #FUNC, Vt100::FUNC(), {ESC LETTER, ESC "[" LETTER, ESC "[1" LETTER}}, \
      Vt100Param{#FUNC "_1",                                                \
                 Vt100::FUNC(1),                                            \
                 {ESC LETTER, ESC "[" LETTER, ESC "[1" LETTER}},            \
      Vt100Param {                                                          \
#FUNC "_n", Vt100::FUNC(2), { ESC "[2" LETTER }                         \
  }
    //
    G(cursor_up, "A"),
    //
    G(cursor_down, "B"),
    //
    G(cursor_forward, "C"),
    //
    G(cursor_back, "D"),
    //
    G(cursor_next_line, "E"),
    //
    G(cursor_prev_line, "F"),
    //
    G(scroll_up, "S"),
    //
    G(scroll_down, "T"),
    //
    G(cursor_abs_col, "G"),
    //
    G(cursor_abs_row, "d"),

    // colors
    // - (foreground, background) x (col, ndx, rgb)

    Vt100Param{"format_foreground_red",
               Vt100::foreground(Vt100::Color::Red),
               {ESC "[31m"}},
    Vt100Param{"format_background_red",
               Vt100::background(Vt100::Color::Red),
               {ESC "[41m"}},
    Vt100Param{"format_foreground_brightblue",
               Vt100::foreground(Vt100::Color::BrightBlue),
               {ESC "[94m"}},
    Vt100Param{"format_background_brightblue",
               Vt100::background(Vt100::Color::BrightBlue),
               {ESC "[104m"}},
    Vt100Param{"format_foreground_rgb_red",
               Vt100::foreground(Vt100::Rgb{255u, 0u, 0u}),
               {ESC "[38;2;255;0;0m"}},
    Vt100Param{"format_background_rgb_blue",
               Vt100::background(Vt100::Rgb{0u, 0u, 255u}),
               {ESC "[48;2;0;0;255m"}},
    Vt100Param{
        "format_foreground_index_red", Vt100::foreground(1), {ESC "[38;5;1m"}},
    Vt100Param{
        "format_background_index_red", Vt100::background(1), {ESC "[48;5;1m"}},

    Vt100Param{"save_cursor_pos", Vt100::save_cursor_pos(), {ESC "[s"}},
    Vt100Param{"restore_cursor_pos", Vt100::restore_cursor_pos(), {ESC "[u"}},

    Vt100Param{"cursor_abs_pos", Vt100::cursor_abs_pos(), {ESC "[;H"}},
    Vt100Param{"cursor_abs_pos_col_1", Vt100::cursor_abs_pos(1), {ESC "[;H"}},
    Vt100Param{"cursor_abs_pos_col_n", Vt100::cursor_abs_pos(2), {ESC "[2;H"}},
    Vt100Param{
        "cursor_abs_pos_row_1", Vt100::cursor_abs_pos(1, 1), {ESC "[;H"}},
    Vt100Param{
        "cursor_abs_pos_row_n", Vt100::cursor_abs_pos(1, 2), {ESC "[;2H"}},
    Vt100Param{
        "cursor_abs_pos_row_col_n", Vt100::cursor_abs_pos(2, 2), {ESC "[2;2H"}},

    Vt100Param{"erase_in_line", Vt100::erase_in_line(), {ESC "[0K", ESC "[K"}},
    Vt100Param{"erase_in_line_0",
               Vt100::erase_in_line(Vt100::Erase::LeftAndCur),
               {ESC "[0K", ESC "[K"}},
    Vt100Param{"erase_in_line_1",
               Vt100::erase_in_line(Vt100::Erase::RightAndCur),
               {ESC "[1K"}},
    Vt100Param{"erase_in_line_2",
               Vt100::erase_in_line(Vt100::Erase::All),
               {ESC "[2K"}},

    Vt100Param{
        "erase_in_display", Vt100::erase_in_display(), {ESC "[0J", ESC "[J"}},
    Vt100Param{"erase_in_display_0",
               Vt100::erase_in_display(Vt100::Erase::LeftAndCur),
               {ESC "[0J", ESC "[J"}},
    Vt100Param{"erase_in_display_1",
               Vt100::erase_in_display(Vt100::Erase::RightAndCur),
               {ESC "[1J"}},
    Vt100Param{"erase_in_display_2",
               Vt100::erase_in_display(Vt100::Erase::All),
               {ESC "[2J"}},

    Vt100Param{"reset", Vt100::reset(), {ESC "c"}}  //
};

INSTANTIATE_TEST_SUITE_P(Spec, Vt100Test, ::testing::ValuesIn(vt100_params),
                         [](const testing::TestParamInfo<Vt100Param> &info) {
                           return info.param.test_name + "_works";
                         });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
