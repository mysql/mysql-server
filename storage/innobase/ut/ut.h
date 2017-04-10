/*****************************************************************************

Copyright (c) 1994, 2015, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file ut/ut.h
Various utilities

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#ifndef ut_ut_h
#define ut_ut_h

#include "univ.i"
/*************************************************************//**
Prints the contents of a memory buffer in hex and ascii. */
void
ut_print_buf(
/*=========*/
	FILE*		file,	/*!< in: file where to print */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len);	/*!< in: length of the buffer */

/**********************************************************//**
Prints a timestamp to a file. */
void
ut_print_timestamp(
/*===============*/
	FILE*	file)	/*!< in: file where to print */
	UNIV_COLD;
/**********************************************************//**
Sprintfs a timestamp to a buffer, 13..14 chars plus terminating NUL. */
void
ut_sprintf_timestamp(
/*=================*/
	char*	buf); /*!< in: buffer where to sprintf */
/*************************************************************//**
Prints the contents of a memory buffer in hex. */
void
ut_print_buf_hex(
/*=============*/
	std::ostream&	o,	/*!< in/out: output stream */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len);	/*!< in: length of the buffer */
/*************************************************************//**
Prints the contents of a memory buffer in hex and ascii. */
void
ut_print_buf(
/*=========*/
	std::ostream&	o,	/*!< in/out: output stream */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len);	/*!< in: length of the buffer */
/**********************************************************************//**
Like ut_strlcpy, but if src doesn't fit in dst completely, copies the last
(size - 1) bytes of src, not the first.
@return strlen(src) */
ulint
ut_strlcpy_rev(
/*===========*/
	char*		dst,	/*!< in: destination buffer */
	const char*	src,	/*!< in: source buffer */
	ulint		size);	/*!< in: size of destination buffer */
#endif
