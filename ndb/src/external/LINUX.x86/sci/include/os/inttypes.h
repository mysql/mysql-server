/* $Id: inttypes.h,v 1.1 2002/12/13 12:17:21 hin Exp $  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2000                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation; either version 2 of the License,              *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 


#ifndef _SCI_OS_INTTYPES_H_
#define _SCI_OS_INTTYPES_H_

/*
 * --------------------------------------------------------------------------------------
 * Basic types of various sizes.
 * --------------------------------------------------------------------------------------
 */
typedef unsigned char      unsigned8;
typedef unsigned short     unsigned16;
typedef unsigned int       unsigned32;
typedef unsigned long long unsigned64;

typedef signed char        signed8;
typedef signed short       signed16;
typedef signed int         signed32;
typedef signed long long   signed64;


#ifdef CPU_WORD_IS_64_BIT
typedef unsigned64         uptr_t;
typedef signed64           iptr_t;
#else
typedef unsigned32         uptr_t;
typedef signed32           iptr_t;
#endif

#endif /* _SCI_OS_INTTYPES_H_ */
