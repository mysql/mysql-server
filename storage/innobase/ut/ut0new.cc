/*****************************************************************************

Copyright (c) 2014, 2014, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file ut/ut0new.cc
Instrumented memory allocator.

Created May 26, 2014 Vasil Dimov
*******************************************************/

#include "univ.i"

/* The implementation is in ut0new.h. ut0new.cc is only needed to declare
the global variables involved and those will be provided by ut0new.h when
the macro ut0new_cc is defined. */

#define ut0new_cc
#include "ut0new.h"
