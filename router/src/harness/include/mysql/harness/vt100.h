/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLHARNESS_VT100_INCLUDED
#define MYSQLHARNESS_VT100_INCLUDED

#include "harness_export.h"

#include <array>
#include <cstdint>
#include <string>
#include <tuple>

// ECMA-48 -> ANSI X3.64 -> ISO 6429 ... implemented as VT100 by Digital
//
// https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
//
// https://www.xfree86.org/4.8.0/ctlseqs.html
namespace Vt100 {
/**
 * colors as used in Render.
 */
enum class Color {
  Black = 0,
  Red,
  Green,
  Yellow,
  Blue,
  Magenta,
  Cyan,
  White,
  BrightBlack = 60 + Black,
  BrightRed = 60 + Red,
  BrightGreen = 60 + Green,
  BrightYellow = 60 + Yellow,
  BrightBlue = 60 + Blue,
  BrightMagenta = 60 + Magenta,
  BrightCyan = 60 + Cyan,
  BrightWhite = 60 + White
};

namespace {
constexpr uint8_t kRenderForegroundOffset = 30;
constexpr uint8_t kRenderBackgroundOffset = kRenderForegroundOffset + 10;
}  // namespace

/**
 * types of "Character Attributes".
 */
enum class Render {
  Default = 0,
  Bold = 1,
  Faint = 2,   // not win32, not xterm
  Italic = 3,  // not win32, not xterm
  Underline = 4,
  SlowBlink = 5,   // not win32
  RapidBlink = 6,  // not xterm
  Inverse = 7,
  Conceal = 8,
  CrossedOut = 9,  // not xterm

  FontDefault = 10,
  Font1 = 11,
  Font2 = 12,
  Font3 = 13,
  Font4 = 14,
  Font5 = 15,
  Font6 = 16,
  Font7 = 17,
  Font8 = 18,
  Font9 = 19,

  Fraktur = 20,          // not xterm
  DoublyUnderline = 21,  // not xterm
  Normal = 22,
  NoItalic = 20 + Italic,  // not xterm
  NoUnderline = 20 + Underline,
  NoBlink = 20 + SlowBlink,
  NoInverse = 20 + Inverse,  // not xterm
  NoConceal = 20 + Conceal,
  NoCrossedOut = 20 + CrossedOut,  // not xterm

  // 30..39

  ForegroundBlack = kRenderForegroundOffset + static_cast<int>(Color::Black),
  ForegroundRed = kRenderForegroundOffset + static_cast<int>(Color::Red),
  ForegroundGreen = kRenderForegroundOffset + static_cast<int>(Color::Green),
  ForegroundYellow = kRenderForegroundOffset + static_cast<int>(Color::Yellow),
  ForegroundBlue = kRenderForegroundOffset + static_cast<int>(Color::Blue),
  ForegroundMagenta =
      kRenderForegroundOffset + static_cast<int>(Color::Magenta),
  ForegroundCyan = kRenderForegroundOffset + static_cast<int>(Color::Cyan),
  ForegroundWhite = kRenderForegroundOffset + static_cast<int>(Color::White),
  ForegroundExtended = kRenderForegroundOffset + 8,
  ForegroundDefault = kRenderForegroundOffset + 9,

  // 40..49

  BackgroundBlack = kRenderBackgroundOffset + static_cast<int>(Color::Black),
  BackgroundRed = kRenderBackgroundOffset + static_cast<int>(Color::Red),
  BackgroundGreen = kRenderBackgroundOffset + static_cast<int>(Color::Green),
  BackgroundYellow = kRenderBackgroundOffset + static_cast<int>(Color::Yellow),
  BackgroundBlue = kRenderBackgroundOffset + static_cast<int>(Color::Blue),
  BackgroundMagenta =
      kRenderBackgroundOffset + static_cast<int>(Color::Magenta),
  BackgroundCyan = kRenderBackgroundOffset + static_cast<int>(Color::Cyan),
  BackgroundWhite = kRenderBackgroundOffset + static_cast<int>(Color::White),
  BackgroundExtended = kRenderBackgroundOffset + 8,
  BackgroundDefault = kRenderBackgroundOffset + 9,

  Framed = 51,
  Encircled = 52,
  Overlined = 53,
  NotFramed = 54,
  NotOverlined = 55,
  IdeogramUnderline = 60,
  IdeogramDoubleUnderline = 61,
  IdeogramOverline = 62,
  IdeogramDoubleOverline = 63,
  IdeogramStressMarking = 64,
  NoIdeogram = 65,

  // 90..97

  ForegroundBrightBlack =
      kRenderForegroundOffset + static_cast<int>(Color::BrightBlack),
  ForegroundBrightRed =
      kRenderForegroundOffset + static_cast<int>(Color::BrightRed),
  ForegroundBrightGreen =
      kRenderForegroundOffset + static_cast<int>(Color::BrightGreen),
  ForegroundBrightYellow =
      kRenderForegroundOffset + static_cast<int>(Color::BrightYellow),
  ForegroundBrightBlue =
      kRenderForegroundOffset + static_cast<int>(Color::BrightBlue),
  ForegroundBrightMagenta =
      kRenderForegroundOffset + static_cast<int>(Color::BrightMagenta),
  ForegroundBrightCyan =
      kRenderForegroundOffset + static_cast<int>(Color::BrightCyan),
  ForegroundBrightWhite =
      kRenderForegroundOffset + static_cast<int>(Color::BrightWhite),

  // 100..107

  BackgroundBrightBlack =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightBlack),
  BackgroundBrightRed =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightRed),
  BackgroundBrightGreen =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightGreen),
  BackgroundBrightYellow =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightYellow),
  BackgroundBrightBlue =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightBlue),
  BackgroundBrightMagenta =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightMagenta),
  BackgroundBrightCyan =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightCyan),
  BackgroundBrightWhite =
      kRenderBackgroundOffset + static_cast<int>(Color::BrightWhite),
};

using value_type = uint16_t;

enum class Csi {
  // insert char
  ICH = '@',
  // cursor up
  CUU = 'A',
  // cursor down
  CUD = 'B',
  // cursor forward
  CUF = 'C',
  // cursor backward
  CUB = 'D',
  // next line
  CNL = 'E',
  // prev line
  CPL = 'F',
  // cursor horizontal absolute
  CHA = 'G',
  // cursor position absolute
  CUP = 'H',
  // cursor tab forward
  CHT = 'I',
  // erase in display
  ED = 'J',
  // erase in line
  EL = 'K',
  // insert line
  IL = 'L',
  // delete line
  DL = 'M',
  // delete char
  DCH = 'P',
  // scroll up
  SU = 'S',
  // scroll down
  SD = 'T',
  // erase char
  ECH = 'X',
  // cursor tab backwards
  CBT = 'Z',
  // device attributes
  DA = 'c',
  // vertical
  VPA = 'd',
  // horizontal vertical position (same as CUP?)
  HVP = 'f',
  // tab clear
  TBC = 'g',
  // set mode
  SM = 'h',
  // Media Copy
  MC = 'i',
  // reset mode
  RM = 'l',
  // render
  SGR = 'm',
  // Device Status Report
  DSR = 'n',
  // Soft Terminal Reset
  STR = 'p',
  // set scrolling margins
  DECSTBM = 'r',
  // save cursor
  SC = 's',
  // Reverse Attributes in Rectangular Area
  DECRARA = 't',
  // restore cursor
  SR = 'u',
  DECCRA = 'v',
  DECEFR = 'w',
  // DECREQTPARM and others
  DECREQTPARM = 'x',
  // DECERA and others
  DECERA = 'z',
  DECSLE = '{',
  DECRQLP = '|',
};

using Rgb = std::array<uint8_t, 3>;

// high-level functions

enum class Erase { LeftAndCur = 0, RightAndCur, All };

/**
 * get 'text rendering attributes' ESC sequence.
 *
 * SGR
 */
std::string HARNESS_EXPORT render(Render r);

/**
 * get 'change foreground color' ESC sequence.
 */
std::string HARNESS_EXPORT foreground(Color c);

/**
 * get 'change foreground color' ESC sequence.
 */
std::string HARNESS_EXPORT foreground(const Rgb &rgb);

/**
 * get 'change foreground color ESC' sequence.
 */
std::string HARNESS_EXPORT foreground(uint8_t ndx);

/**
 * get 'change background color' ESC sequence.
 */
std::string HARNESS_EXPORT background(Color c);

/**
 * get 'change background color' ESC sequence.
 */
std::string HARNESS_EXPORT background(const Rgb &rgb);

/**
 * get 'change background color' ESC sequence.
 */
std::string HARNESS_EXPORT background(uint8_t ndx);

/**
 * get 'reset attributes' ESC sequence.
 */
std::string HARNESS_EXPORT reset();

/**
 * get 'cursor up' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_up(Vt100::value_type n = 1);

/**
 * get 'cursor down' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_down(Vt100::value_type n = 1);

/**
 * get 'cursor forward' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_forward(Vt100::value_type n = 1);

/**
 * get 'cursor back' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_back(Vt100::value_type n = 1);

/**
 * get 'cursor next line' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_next_line(Vt100::value_type n = 1);

/**
 * get 'cursor previous line' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_prev_line(Vt100::value_type n = 1);

/**
 * get 'set cursor absolute position' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_abs_col(Vt100::value_type col = 1);

/**
 * get 'set cursor absolute row' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_abs_row(Vt100::value_type row = 1);

/**
 * get 'set cursor to absolute row' ESC sequence.
 */
std::string HARNESS_EXPORT cursor_abs_pos(Vt100::value_type row = 1,
                                          Vt100::value_type col = 1);
/**
 * get 'erase in display' ESC sequence.
 */
std::string HARNESS_EXPORT
erase_in_display(Vt100::Erase n = Vt100::Erase::LeftAndCur);

/**
 * get 'erase in line' ESC sequence.
 */
std::string HARNESS_EXPORT
erase_in_line(Vt100::Erase n = Vt100::Erase::LeftAndCur);

/**
 * get 'scroll up' ESC sequence.
 */
std::string HARNESS_EXPORT scroll_up(Vt100::value_type n = 1);

/**
 * get 'scroll down' ESC sequence.
 */
std::string HARNESS_EXPORT scroll_down(Vt100::value_type n = 1);

/**
 * get 'save cursor position' ESC sequence.
 */
std::string HARNESS_EXPORT save_cursor_pos();

/**
 * get 'restore cursor position' ESC sequence.
 */
std::string HARNESS_EXPORT restore_cursor_pos();

/**
 * get 'set window title' ESC sequence.
 */
std::string HARNESS_EXPORT window_title(const std::string &title);
}  // namespace Vt100

#endif
