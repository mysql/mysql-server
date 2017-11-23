#ifndef SQL_GIS_SRID_H_INCLUDED
#define SQL_GIS_SRID_H_INCLUDED

// Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, 51 Franklin
// Street, Suite 500, Boston, MA 02110-1335 USA.

#include <cstdint>

namespace gis {
/// A spatial reference system ID (SRID).
///
/// This type matches the SRID storage format in MySQL.
typedef std::uint32_t srid_t;
}

#endif  // SQL_GIS_SRID_H_INCLUDED
