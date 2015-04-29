/* Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBINFO_SCAN_OPERATION_HPP
#define NDBINFO_SCAN_OPERATION_HPP

#include "NdbInfoRecAttr.hpp"

class NdbInfoScanOperation {
public:
  virtual int readTuples() = 0;
  virtual const NdbInfoRecAttr* getValue(const char * anAttrName) = 0;
  virtual const NdbInfoRecAttr* getValue(Uint32 anAttrId) = 0;
  virtual int execute() = 0;
  virtual int nextResult() = 0;
  virtual ~NdbInfoScanOperation() {}
};


#endif
