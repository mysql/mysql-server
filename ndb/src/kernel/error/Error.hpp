/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ERROR_H
#define ERROR_H

/**
 * Errorcodes for NDB
 *
 * These errorcodes should be used whenever a condition
 * is detected where it's necesssary to shutdown NDB.
 *
 * Example: When another node fails while a NDB node are performing
 * a system restart the node should be shutdown. This 
 * is kind of an error but the cause of the error is known
 * and a proper errormessage describing the problem should
 * be printed in error.log. It's therefore important to use
 * the proper errorcode.
 *
 * TODO: In the future the errorcodes should be classified
 *
 */

enum ErrorCategory
{
   warning,
   ecError,
   fatal,
   assert
};

const int ERR_BASE = 1000;

// Errorcodes for all blocks except filseystem
const int ERR_ERR_BASE = ERR_BASE + 1300;
const int       ERR_ERROR_PRGERR                = ERR_ERR_BASE+1;
const int       ERR_NODE_NOT_IN_CONFIG          = ERR_ERR_BASE+2;
const int       ERR_SYSTEM_ERROR                = ERR_ERR_BASE+3;
const int       ERR_INDEX_NOTINRANGE            = ERR_ERR_BASE+4;
const int       ERR_ARBIT_SHUTDOWN              = ERR_ERR_BASE+5;
const int       ERR_POINTER_NOTINRANGE          = ERR_ERR_BASE+6;
const int       ERR_PROGRAMERROR                = ERR_ERR_BASE+7;
const int       ERR_SR_OTHERNODEFAILED          = ERR_ERR_BASE+8;
const int       ERR_NODE_NOT_DEAD               = ERR_ERR_BASE+9;
const int       ERR_SR_REDOLOG                  = ERR_ERR_BASE+10;
const int       ERR_SR_RESTARTCONFLICT          = ERR_ERR_BASE+11;
const int       ERR_NO_MORE_UNDOLOG             = ERR_ERR_BASE+12; 
const int       ERR_SR_UNDOLOG                  = ERR_ERR_BASE+13; 
const int       ERR_MEMALLOC                    = ERR_ERR_BASE+27;
const int       BLOCK_ERROR_JBUFCONGESTION      = ERR_ERR_BASE+34;
const int       ERROR_TIME_QUEUE_SHORT          = ERR_ERR_BASE+35;
const int       ERROR_TIME_QUEUE_LONG           = ERR_ERR_BASE+36;
const int       ERROR_TIME_QUEUE_DELAY          = ERR_ERR_BASE+37;
const int       ERROR_TIME_QUEUE_INDEX          = ERR_ERR_BASE+38;
const int       BLOCK_ERROR_BNR_ZERO            = ERR_ERR_BASE+39;
const int       ERROR_WRONG_PRIO_LEVEL          = ERR_ERR_BASE+40;
const int       ERR_NDBREQUIRE                  = ERR_ERR_BASE+41;
const int       ERR_ERROR_INSERT                = ERR_ERR_BASE+42;
const int       ERR_INVALID_CONFIG              = ERR_ERR_BASE+50;
const int       ERR_OUT_OF_LONG_SIGNAL_MEMORY   = ERR_ERR_BASE+51;

// Errorcodes for NDB filesystem
const int AFS_ERR_BASE = ERR_BASE + 1800;
const int       AFS_ERROR_NOPATH                 = AFS_ERR_BASE+1;
const int       AFS_ERROR_CHANNALFULL           = AFS_ERR_BASE+2;
const int       AFS_ERROR_NOMORETHREADS         = AFS_ERR_BASE+3;
const int       AFS_ERROR_PARAMETER             = AFS_ERR_BASE+4;
const int       AFS_ERROR_INVALIDPATH           = AFS_ERR_BASE+5;
const int       AFS_ERROR_MAXOPEN               = AFS_ERR_BASE+6;
const int       AFS_ERROR_ALLREADY_OPEN         = AFS_ERR_BASE+7;

#endif                                 // ERROR_H
