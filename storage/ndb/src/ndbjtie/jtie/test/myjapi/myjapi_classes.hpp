/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

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
/*
 * myjapi_classes.hpp
 */

#ifndef myjapi_classes_hpp
#define myjapi_classes_hpp

// API to implement against
#include "myapi.hpp"

// libraries
#include "jtie.hpp"

// ---------------------------------------------------------------------------
// API Type Definitions
// ---------------------------------------------------------------------------

JTIE_DEFINE_PEER_CLASS_MAPPING(A, myjapi_A)
JTIE_DEFINE_PEER_CLASS_MAPPING(B0, myjapi_B0)
JTIE_DEFINE_PEER_CLASS_MAPPING(B1, myjapi_B1)
JTIE_DEFINE_PEER_CLASS_MAPPING(C0, myjapi_CI_C0)
JTIE_DEFINE_PEER_CLASS_MAPPING(C1, myjapi_CI_C1)
JTIE_DEFINE_PEER_CLASS_MAPPING(D0, myjapi_D0)
JTIE_DEFINE_PEER_CLASS_MAPPING(D1, myjapi_D1)
JTIE_DEFINE_PEER_CLASS_MAPPING(D2, myjapi_D2)

JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(C0::C0E, C0_C0E)

// ---------------------------------------------------------------------------

#endif // myjapi_classes_hpp
