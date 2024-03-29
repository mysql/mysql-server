/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/vt100.h"

#include <string>
#include <vector>

namespace Vt100 {
#define ESC "\x1b"
constexpr const char BEL{0x07};
constexpr const char CSI[]{ESC "["};  // Control Sequence Introducer
constexpr const char OSC[]{ESC "]"};  // Operating system command

std::string reset() { return ESC "c"; }

template <Vt100::value_type N>
static std::string not_num_to_string(Vt100::value_type n) {
  return (n == N) ? "" : std::to_string(n);
}

static std::string csi(Csi c, const std::string &s) {
  return CSI + s + static_cast<char>(c);
}

static std::string csi(Csi c) { return csi(c, ""); }

template <Vt100::value_type DEF>
static std::string csi_default(Csi c, Vt100::value_type n) {
  return csi(c, not_num_to_string<DEF>(n));
}

template <Vt100::value_type DEF, size_t N>
static std::string csi_default(Csi c,
                               const std::array<Vt100::value_type, N> &fields) {
  std::string s;
  bool is_first = true;
  for (const auto &field : fields) {
    if (!is_first) {
      s += ";";
    }
    s += not_num_to_string<DEF>(field);
    is_first = false;
  }
  return csi(c, s);
}

std::string cursor_up(Vt100::value_type n) {
  return csi_default<1>(Csi::CUU, n);
}
std::string cursor_down(Vt100::value_type n) {
  return csi_default<1>(Csi::CUD, n);
}
std::string cursor_forward(Vt100::value_type n) {
  return csi_default<1>(Csi::CUF, n);
}
std::string cursor_back(Vt100::value_type n) {
  return csi_default<1>(Csi::CUB, n);
}
std::string cursor_next_line(Vt100::value_type n) {
  return csi_default<1>(Csi::CNL, n);
}
std::string cursor_prev_line(Vt100::value_type n) {
  return csi_default<1>(Csi::CPL, n);
}
std::string cursor_abs_col(Vt100::value_type n) {
  return csi_default<1>(Csi::CHA, n);
}
std::string cursor_abs_row(Vt100::value_type n) {
  return csi_default<1>(Csi::VPA, n);
}
std::string cursor_abs_pos(Vt100::value_type row, Vt100::value_type col) {
  return csi_default<1, 2>(Csi::CUP, {row, col});
}

std::string erase_in_display(Vt100::Erase n) {
  return csi_default<0>(Csi::ED, static_cast<Vt100::value_type>(n));
}
std::string erase_in_line(Vt100::Erase n) {
  return csi_default<0>(Csi::EL, static_cast<Vt100::value_type>(n));
}
std::string scroll_up(Vt100::value_type n) {
  return csi_default<1>(Csi::SU, n);
}
std::string scroll_down(Vt100::value_type n) {
  return csi_default<1>(Csi::SD, n);
}
std::string save_cursor_pos() { return csi(Csi::SC); }
std::string restore_cursor_pos() { return csi(Csi::SR); }

std::string window_title(const std::string &title) {
  return OSC + std::to_string(2) + ";" + title + BEL;
}

/**
 * VT100, 7-bit colors
 */
static std::string render(Vt100::value_type n) {
  return csi_default<0>(Csi::SGR, n);
}

template <Vt100::value_type DEF, size_t N>
static std::string render(const std::array<Vt100::value_type, N> &fields) {
  return csi_default<DEF>(Csi::SGR, fields);
}

std::string render(Render r) {
  return render(static_cast<Vt100::value_type>(r));
}

std::string foreground(Color c) {
  return render(static_cast<Vt100::value_type>(Render::ForegroundBlack) +
                static_cast<Vt100::value_type>(c));
}

std::string foreground(const Rgb &rgb) {
  return render<65535, 5>(
      {static_cast<Vt100::value_type>(Render::ForegroundExtended), 2, rgb[0],
       rgb[1], rgb[2]});
}

std::string foreground(uint8_t ndx) {
  return render<0, 3>(
      {static_cast<Vt100::value_type>(Render::ForegroundExtended), 5, ndx});
}

std::string background(Color c) {
  return render(static_cast<int>(Render::BackgroundBlack) +
                static_cast<int>(c));
}

std::string background(const Rgb &rgb) {
  return render<65535, 5>(
      {static_cast<Vt100::value_type>(Render::BackgroundExtended), 2, rgb[0],
       rgb[1], rgb[2]});
}

std::string background(uint8_t ndx) {
  return render<0, 3>(
      {static_cast<Vt100::value_type>(Render::BackgroundExtended), 5, ndx});
}

}  // namespace Vt100
