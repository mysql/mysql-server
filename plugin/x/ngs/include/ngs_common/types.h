/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef X_NGS_INCLUDE_NGS_COMMON_TYPES_H_
#define X_NGS_INCLUDE_NGS_COMMON_TYPES_H_

#include <vector>


namespace ngs
{

typedef std::vector<std::pair<const char*, std::size_t> >   Const_buffer_sequence;

} // namespace ngs

#endif // X_NGS_INCLUDE_NGS_COMMON_TYPES_H_
