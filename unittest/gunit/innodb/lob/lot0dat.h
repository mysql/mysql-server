/*****************************************************************************

Copyright (c) 2016, 2017 Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/
#ifndef _lot0dat_h_
#define _lot0dat_h_

#include "lot0types.h"

/** Generate LOB data */

namespace lob_data {

byte* generate_lob(lobid_t* id, ulint size);
byte* generate_lob(lobid_t* id, char x, ulint size);

/** Get an LOB from the given file. */
std::pair<byte*, ulint> get_lob(lobid_t* id, const char* filename);
void remove_lob(lobid_t id);

};

#endif // _lot0dat_h_
