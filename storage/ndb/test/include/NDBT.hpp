/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_TEST_HPP
#define NDB_TEST_HPP

/**
 * NdbTest.hpp 
 * This is the main include file to include in test programs 
 * It will include all the other include files in the NDBT-toolkit
 *
 */

#include "NDBT_ReturnCodes.h"

#ifdef __cplusplus
#include "NDBT_Table.hpp"
#include "NDBT_Tables.hpp"
#include "NDBT_Error.hpp"
#include "NDBT_ResultRow.hpp"
#include "NDBT_Output.hpp"

#define CHK_NDB_READY(a) \
  if ((a)->waitUntilReady() != 0) \
  { \
    return NDBT_FAILED; \
  }

#endif


#endif
