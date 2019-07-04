/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/vt100_filter.h"

static constexpr char Esc{'\x1b'};
static constexpr char Bel{'\x07'};

static bool is_csi_param(Vt100Filter::int_type ch) {
  return ch >= 0x30 && ch <= 0x3f;
}
static bool is_csi_intermediate(Vt100Filter::int_type ch) {
  return ch >= 0x20 && ch <= 0x2f;
}
static bool is_csi_final(Vt100Filter::int_type ch) {
  return ch >= 0x40 && ch <= 0x7e;
}
static bool is_osc_final(Vt100Filter::int_type ch) { return ch == Bel; }

Vt100Filter::int_type Vt100Filter::overflow(Vt100Filter::int_type ch) {
  if (traits_type::eq_int_type(traits_type::eof(), ch)) {
    return traits_type::not_eof(ch);
  }

  if (!strip_vt100_) {
    sbuf_->sputc(ch);

    return ch;
  }

  if (ch == 0x7f) {
    // DEL is always ignored
    return ch;
  }

  if (ch == Esc) {
    state_ = State::PLAIN;
  }

  bool forward_ch = false;
  switch (state_) {
    case State::PLAIN:
      if (ch == Esc) {
        state_ = State::ESC;
      } else {
        forward_ch = true;
      }
      break;
    case State::ESC:
      if (ch >= 0x40 && ch <= 0x5f) {
        // uppercase
        if (ch == '[') {
          state_ = State::CSI;
        } else if (ch == ']') {
          state_ = State::OSC;
        } else if (ch == 'P' || ch == 'X' || ch == '_' || ch == '^') {
          state_ = State::ST;
        } else {
          // single char
          state_ = State::PLAIN;
        }
      } else if (ch >= 0x60 && ch <= 0x7e) {
        // lowercase
        //
        // single char
        state_ = State::PLAIN;
      } else {
        // unexpected char
        forward_ch = true;
        state_ = State::PLAIN;
      }
      break;
    case State::ST:
      // stay in this state until an Esc is seen. It is handled earlier
      break;

    case State::CSI: {
      if (is_csi_param(ch)) {
        state_ = State::CSI_PARAM;
      } else if (is_csi_intermediate(ch)) {
        state_ = State::CSI_INTERMEDIATE;
      } else if (is_csi_final(ch)) {
        // done
        state_ = State::PLAIN;
      } else {
        // unexpected char
        forward_ch = true;
        state_ = State::PLAIN;
      }
      break;
    }
    case State::CSI_PARAM: {
      if (is_csi_param(ch)) {
        state_ = State::CSI_PARAM;
      } else if (is_csi_intermediate(ch)) {
        state_ = State::CSI_INTERMEDIATE;
      } else if (is_csi_final(ch)) {
        // done
        state_ = State::PLAIN;
      } else {
        // unexpected char
        forward_ch = true;
        state_ = State::PLAIN;
      }
      break;
    }
    case State::CSI_INTERMEDIATE: {
      if (is_csi_intermediate(ch)) {
        state_ = State::CSI_INTERMEDIATE;
      } else if (is_csi_final(ch)) {
        // done
        state_ = State::PLAIN;
      } else {
        // unexpected char
        forward_ch = true;
        state_ = State::PLAIN;
      }
      break;
    }
    case State::OSC: {
      if (is_osc_final(ch)) {
        state_ = State::PLAIN;
      } else {
        state_ = State::OSC;
      }
      break;
    }
  }

  if (forward_ch) {
    sbuf_->sputc(ch);
  }

  return ch;
}
