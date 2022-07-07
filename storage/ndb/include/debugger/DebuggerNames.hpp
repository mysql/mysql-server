/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
    Use is subject to license terms.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DEBUGGER_NAMES
#define DEBUGGER_NAMES

#include <kernel_types.h>
#include <signaldata/SignalDataPrint.hpp>

/**
 * getSignalName
 *
 * NOTES: Very quick
 *
 * RETURNS: Signal name or 0 if none found
 */
const char * 
getSignalName(GlobalSignalNumber gsn, const char * defualtValue = "Unknown");

/**
 * getBlockName
 *
 * NOTES: Very quick
 *
 * RETURNS: Block name or 
 *          defValue if not a valid block number
 */
const char * 
getBlockName(BlockNumber blockNo, const char * defValue = nullptr);

/**
 * getBlockNo
 *
 * NOTES: Very slow
 *
 * RETURNS: BlockNo or 0 if none found
 */
BlockNumber
getBlockNo(const char * blockName);

/**
 * Find a print function for a signal
 *
 * RETURNS: 0 if none found
 */
SignalDataPrintFunction findPrintFunction(GlobalSignalNumber);

#endif
