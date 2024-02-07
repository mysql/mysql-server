/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.

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
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBINFO_SCAN_OPERATION_HPP
#define NDBINFO_SCAN_OPERATION_HPP

#include <cassert>

#include "ndb_types.h"

#include "NdbInfoRecAttr.hpp"

class NdbInfoScanOperation {
 public:
  class Seek {
    // The below members are only valid for Mode::value
    const bool m_inclusive;
    const bool m_low;
    const bool m_high;

   public:
    enum class Mode { value, first, last, next, previous };
    Seek(Mode m, bool inclusive = false, bool low = false, bool high = false)
        : m_inclusive(inclusive), m_low(low), m_high(high), mode(m) {}
    const Mode mode;
    bool inclusive() const {
      assert(mode == Mode::value);
      return m_inclusive;
    }
    bool low() const {
      assert(mode == Mode::value);
      return m_low;
    }
    bool high() const {
      assert(mode == Mode::value);
      return m_high;
    }
  };

  virtual int readTuples() = 0;
  virtual const NdbInfoRecAttr *getValue(const char *anAttrName) = 0;
  virtual const NdbInfoRecAttr *getValue(Uint32 anAttrId) = 0;
  virtual int execute() = 0;
  virtual int nextResult() = 0;
  virtual void initIndex(Uint32) = 0;
  virtual bool seek(Seek, int value = 0) = 0;
  virtual ~NdbInfoScanOperation() {}
};

#endif
