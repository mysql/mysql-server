/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
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
 * getGsn
 *
 * NOTES: Very slow
 *
 * RETURNS: Gsn or 0 if none found
 */
GlobalSignalNumber
getGsn(const char * signalName);

/**
 * getBlockName
 *
 * NOTES: Very quick
 *
 * RETURNS: Block name or 
 *          defValue if not a valid block number
 */
const char * 
getBlockName(BlockNumber blockNo, const char * defValue = 0);

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
