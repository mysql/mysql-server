/* $Id: sisci_version.h,v 1.1 2002/12/13 12:17:21 hin Exp $  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2000                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU Lesser General Public License as published by *
 * the Free Software Foundation; either version 2.1 of the License,            *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 


#ifndef SISCI_VERSION_H
#define SISCI_VERSION_H


#define SISCI_API_VER_MAJOR    0x01
#define SISCI_API_VER_MAJORC    "1"

#define SISCI_API_VER_MINOR   0x010
#define SISCI_API_VER_MINORC   "10"
#define SISCI_API_VER_MICRO   0x005
#define SISCI_API_VER_MICROC    "5"

#define SISCI_SIGN_VERSION_MASK 0xfffff000  /* used to mask off API_VER_MICRO */

#define SISCI_API_VERSION  (SISCI_API_VER_MAJOR << 24 |  SISCI_API_VER_MINOR << 12 | SISCI_API_VER_MICRO)

/* the rules are:
 *
 * Changes in API_VER_MICRO should be binary compatible, New flags, functions added. No changes to user code 
 * required if new features is not needed.
 * 
 * Changes in API_VER_MINOR requires recompilation of user code.
 *
 * Changes in the API_VER_MAJOR will most likely require changes to user code. This should not happen very 
 * often...
 *
 */

#ifndef BUILD_DATE
#define BUILD_DATE      __DATE__
#endif

#ifndef BUILD_NAME   
#define BUILD_NAME     ""
#endif

#define API_VERSION "SISCI API version " SISCI_API_VER_MAJORC "." SISCI_API_VER_MINORC "."SISCI_API_VER_MICROC " ( "BUILD_NAME" "BUILD_DATE" )" 

#endif


/* Version info:                                                        */
/*                                                                      */
/* 1.5.2  First SISCI version                                           */
/* 1.5.3  Some bug fixes                                                */
/* 1.5.4  Some bug fixes                                                */
/* 1.5.5  No release                                                    */
/* 1.5.6  Lock flag implemented in function SCIConnectSegment           */
/* 1.5.7  Expanded query functionality                                  */
/* 1.5.8  Updated error checking (sequence) functionality for D320      */
/* 1.6.0  Updated error checking (sequence) D320 and IRM 1.9 support    */
/* 1.9.0  Ported to Solaris_sparc, Solaris_x86 and Linux. IRM 1.9.      */
/* 1.9.1  Some bug fixes                                                */
/* 1.9.2  Added more adapter queries                                    */
/* 1.9.3  Bug fix in SCIMapLocalSegment and SCIMapRemoteSegment         */
/* 1.9.4  NT Release Developers Kit 2.40                                */ 
/* 1.9.5  Added flush after data transfer in SCIMemCopy()               */
/* 1.9.5  NT Release Developers Kit 2.44                                */
/* 1.10.0: 
 * New SCIInitialize(), SCITerminate() functions.
 * Support for D330
 *
 *
 */


