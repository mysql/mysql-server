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

#ifndef MYSQLHARNESS_VT100_FILTER_INCLUDED
#define MYSQLHARNESS_VT100_FILTER_INCLUDED

#include "harness_export.h"

#include <streambuf>

class HARNESS_EXPORT Vt100Filter : public std::streambuf {
 public:
  Vt100Filter(std::streambuf *sbuf, bool strip_vt100 = true)
      : sbuf_{sbuf}, strip_vt100_{strip_vt100} {}

 protected:
  int_type overflow(int_type ch) override;

 private:
  enum class State { PLAIN, ESC, CSI, CSI_PARAM, CSI_INTERMEDIATE, OSC, ST };

  std::streambuf *sbuf_;
  bool strip_vt100_;
  State state_{State::PLAIN};
};

#endif
