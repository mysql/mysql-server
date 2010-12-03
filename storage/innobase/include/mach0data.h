/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************************//**
@file include/mach0data.h
Utilities for converting data from the database file
to the machine format.

Created 11/28/1995 Heikki Tuuri
***********************************************************************/

#ifndef mach0data_h
#define mach0data_h

#include "univ.i"
#include "ut0byte.h"

/* The data and all fields are always stored in a database file
in the same format: ascii, big-endian, ... .
All data in the files MUST be accessed using the functions in this
module. */

/*******************************************************//**
The following function is used to store data in one byte. */
UNIV_INLINE
void
mach_write_to_1(
/*============*/
	byte*	b,	/*!< in: pointer to byte where to store */
	ulint	n);	 /*!< in: ulint integer to be stored, >= 0, < 256 */
/********************************************************//**
The following function is used to fetch data from one byte.
@return	ulint integer, >= 0, < 256 */
UNIV_INLINE
ulint
mach_read_from_1(
/*=============*/
	const byte*	b)	/*!< in: pointer to byte */
	__attribute__((nonnull, pure));
/*******************************************************//**
The following function is used to store data in two consecutive
bytes. We store the most significant byte to the lower address. */
UNIV_INLINE
void
mach_write_to_2(
/*============*/
	byte*	b,	/*!< in: pointer to two bytes where to store */
	ulint	n);	 /*!< in: ulint integer to be stored, >= 0, < 64k */
/********************************************************//**
The following function is used to fetch data from two consecutive
bytes. The most significant byte is at the lowest address.
@return	ulint integer, >= 0, < 64k */
UNIV_INLINE
ulint
mach_read_from_2(
/*=============*/
	const byte*	b)	/*!< in: pointer to two bytes */
	__attribute__((nonnull, pure));

/********************************************************//**
The following function is used to convert a 16-bit data item
to the canonical format, for fast bytewise equality test
against memory.
@return	16-bit integer in canonical format */
UNIV_INLINE
uint16
mach_encode_2(
/*==========*/
	ulint	n)	/*!< in: integer in machine-dependent format */
	__attribute__((const));
/********************************************************//**
The following function is used to convert a 16-bit data item
from the canonical format, for fast bytewise equality test
against memory.
@return	integer in machine-dependent format */
UNIV_INLINE
ulint
mach_decode_2(
/*==========*/
	uint16	n)	/*!< in: 16-bit integer in canonical format */
	__attribute__((const));
/*******************************************************//**
The following function is used to store data in 3 consecutive
bytes. We store the most significant byte to the lowest address. */
UNIV_INLINE
void
mach_write_to_3(
/*============*/
	byte*	b,	/*!< in: pointer to 3 bytes where to store */
	ulint	n);	 /*!< in: ulint integer to be stored */
/********************************************************//**
The following function is used to fetch data from 3 consecutive
bytes. The most significant byte is at the lowest address.
@return	ulint integer */
UNIV_INLINE
ulint
mach_read_from_3(
/*=============*/
	const byte*	b)	/*!< in: pointer to 3 bytes */
	__attribute__((nonnull, pure));
/*******************************************************//**
The following function is used to store data in four consecutive
bytes. We store the most significant byte to the lowest address. */
UNIV_INLINE
void
mach_write_to_4(
/*============*/
	byte*	b,	/*!< in: pointer to four bytes where to store */
	ulint	n);	 /*!< in: ulint integer to be stored */
/********************************************************//**
The following function is used to fetch data from 4 consecutive
bytes. The most significant byte is at the lowest address.
@return	ulint integer */
UNIV_INLINE
ulint
mach_read_from_4(
/*=============*/
	const byte*	b)	/*!< in: pointer to four bytes */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a ulint in a compressed form (1..5 bytes).
@return	stored size in bytes */
UNIV_INLINE
ulint
mach_write_compressed(
/*==================*/
	byte*	b,	/*!< in: pointer to memory where to store */
	ulint	n);	/*!< in: ulint integer to be stored */
/*********************************************************//**
Returns the size of an ulint when written in the compressed form.
@return	compressed size in bytes */
UNIV_INLINE
ulint
mach_get_compressed_size(
/*=====================*/
	ulint	n)	/*!< in: ulint integer to be stored */
	__attribute__((const));
/*********************************************************//**
Reads a ulint in a compressed form.
@return	read integer */
UNIV_INLINE
ulint
mach_read_compressed(
/*=================*/
	const byte*	b)	/*!< in: pointer to memory from where to read */
	__attribute__((nonnull, pure));
/*******************************************************//**
The following function is used to store data in 6 consecutive
bytes. We store the most significant byte to the lowest address. */
UNIV_INLINE
void
mach_write_to_6(
/*============*/
	byte*		b,	/*!< in: pointer to 6 bytes where to store */
	ib_uint64_t	id);	/*!< in: 48-bit integer */
/********************************************************//**
The following function is used to fetch data from 6 consecutive
bytes. The most significant byte is at the lowest address.
@return	48-bit integer */
UNIV_INLINE
ib_uint64_t
mach_read_from_6(
/*=============*/
	const byte*	b)	/*!< in: pointer to 6 bytes */
	__attribute__((nonnull, pure));
/*******************************************************//**
The following function is used to store data in 7 consecutive
bytes. We store the most significant byte to the lowest address. */
UNIV_INLINE
void
mach_write_to_7(
/*============*/
	byte*		b,	/*!< in: pointer to 7 bytes where to store */
	ib_uint64_t	n);	/*!< in: 56-bit integer */
/********************************************************//**
The following function is used to fetch data from 7 consecutive
bytes. The most significant byte is at the lowest address.
@return	56-bit integer */
UNIV_INLINE
ib_uint64_t
mach_read_from_7(
/*=============*/
	const byte*	b)	/*!< in: pointer to 7 bytes */
	__attribute__((nonnull, pure));
/*******************************************************//**
The following function is used to store data in 8 consecutive
bytes. We store the most significant byte to the lowest address. */
UNIV_INLINE
void
mach_write_to_8(
/*============*/
	byte*		b,	/*!< in: pointer to 8 bytes where to store */
	ib_uint64_t	n);	/*!< in: 64-bit integer to be stored */
/********************************************************//**
The following function is used to fetch data from 8 consecutive
bytes. The most significant byte is at the lowest address.
@return	64-bit integer */
UNIV_INLINE
ib_uint64_t
mach_read_from_8(
/*=============*/
	const byte*	b)	/*!< in: pointer to 8 bytes */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a 64-bit integer in a compressed form (5..9 bytes).
@return	size in bytes */
UNIV_INLINE
ulint
mach_ull_write_compressed(
/*======================*/
	byte*		b,	/*!< in: pointer to memory where to store */
	ib_uint64_t	n);	/*!< in: 64-bit integer to be stored */
/*********************************************************//**
Returns the size of a 64-bit integer when written in the compressed form.
@return	compressed size in bytes */
UNIV_INLINE
ulint
mach_ull_get_compressed_size(
/*=========================*/
	ib_uint64_t	n);	/*!< in: 64-bit integer to be stored */
/*********************************************************//**
Reads a 64-bit integer in a compressed form.
@return	the value read */
UNIV_INLINE
ib_uint64_t
mach_ull_read_compressed(
/*=====================*/
	const byte*	b)	/*!< in: pointer to memory from where to read */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a 64-bit integer in a compressed form (1..11 bytes).
@return	size in bytes */
UNIV_INLINE
ulint
mach_ull_write_much_compressed(
/*===========================*/
	byte*		b,	/*!< in: pointer to memory where to store */
	ib_uint64_t	n);	/*!< in: 64-bit integer to be stored */
/*********************************************************//**
Returns the size of a 64-bit integer when written in the compressed form.
@return	compressed size in bytes */
UNIV_INLINE
ulint
mach_ull_get_much_compressed_size(
/*==============================*/
	ib_uint64_t	n)	/*!< in: 64-bit integer to be stored */
	__attribute__((const));
/*********************************************************//**
Reads a 64-bit integer in a compressed form.
@return	the value read */
UNIV_INLINE
ib_uint64_t
mach_ull_read_much_compressed(
/*==========================*/
	const byte*	b)	/*!< in: pointer to memory from where to read */
	__attribute__((nonnull, pure));
/*********************************************************//**
Reads a ulint in a compressed form if the log record fully contains it.
@return	pointer to end of the stored field, NULL if not complete */
UNIV_INTERN
byte*
mach_parse_compressed(
/*==================*/
	byte*	ptr,	/*!< in: pointer to buffer from where to read */
	byte*	end_ptr,/*!< in: pointer to end of the buffer */
	ulint*	val);	/*!< out: read value */
/*********************************************************//**
Reads a 64-bit integer in a compressed form
if the log record fully contains it.
@return pointer to end of the stored field, NULL if not complete */
UNIV_INLINE
byte*
mach_ull_parse_compressed(
/*======================*/
	byte*		ptr,	/*!< in: pointer to buffer from where to read */
	byte*		end_ptr,/*!< in: pointer to end of the buffer */
	ib_uint64_t*	val);	/*!< out: read value */
#ifndef UNIV_HOTBACKUP
/*********************************************************//**
Reads a double. It is stored in a little-endian format.
@return	double read */
UNIV_INLINE
double
mach_double_read(
/*=============*/
	const byte*	b)	/*!< in: pointer to memory from where to read */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a double. It is stored in a little-endian format. */
UNIV_INLINE
void
mach_double_write(
/*==============*/
	byte*	b,	/*!< in: pointer to memory where to write */
	double	d);	/*!< in: double */
/*********************************************************//**
Reads a float. It is stored in a little-endian format.
@return	float read */
UNIV_INLINE
float
mach_float_read(
/*============*/
	const byte*	b)	/*!< in: pointer to memory from where to read */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a float. It is stored in a little-endian format. */
UNIV_INLINE
void
mach_float_write(
/*=============*/
	byte*	b,	/*!< in: pointer to memory where to write */
	float	d);	/*!< in: float */
/*********************************************************//**
Reads a ulint stored in the little-endian format.
@return	unsigned long int */
UNIV_INLINE
ulint
mach_read_from_n_little_endian(
/*===========================*/
	const byte*	buf,		/*!< in: from where to read */
	ulint		buf_size)	/*!< in: from how many bytes to read */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a ulint in the little-endian format. */
UNIV_INLINE
void
mach_write_to_n_little_endian(
/*==========================*/
	byte*	dest,		/*!< in: where to write */
	ulint	dest_size,	/*!< in: into how many bytes to write */
	ulint	n);		/*!< in: unsigned long int to write */
/*********************************************************//**
Reads a ulint stored in the little-endian format.
@return	unsigned long int */
UNIV_INLINE
ulint
mach_read_from_2_little_endian(
/*===========================*/
	const byte*	buf)		/*!< in: from where to read */
	__attribute__((nonnull, pure));
/*********************************************************//**
Writes a ulint in the little-endian format. */
UNIV_INLINE
void
mach_write_to_2_little_endian(
/*==========================*/
	byte*	dest,		/*!< in: where to write */
	ulint	n);		/*!< in: unsigned long int to write */

/*********************************************************//**
Convert integral type from storage byte order (big endian) to
host byte order.
@return	integer value */
UNIV_INLINE
ullint
mach_read_int_type(
/*===============*/
	const byte*	src,		/*!< in: where to read from */
	ulint		len,		/*!< in: length of src */
	ibool		unsigned_type);	/*!< in: signed or unsigned flag */
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "mach0data.ic"
#endif

#endif
