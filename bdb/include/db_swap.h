/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: db_swap.h,v 11.5 2000/03/28 16:14:36 bostic Exp $
 */

#ifndef _DB_SWAP_H_
#define	_DB_SWAP_H_

/*
 * Little endian <==> big endian 32-bit swap macros.
 *	M_32_SWAP	swap a memory location
 *	P_32_COPY	copy potentially unaligned 4 byte quantities
 *	P_32_SWAP	swap a referenced memory location
 */
#define	M_32_SWAP(a) {							\
	u_int32_t _tmp;							\
	_tmp = a;							\
	((u_int8_t *)&a)[0] = ((u_int8_t *)&_tmp)[3];			\
	((u_int8_t *)&a)[1] = ((u_int8_t *)&_tmp)[2];			\
	((u_int8_t *)&a)[2] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)&a)[3] = ((u_int8_t *)&_tmp)[0];			\
}
#define	P_32_COPY(a, b) {						\
	((u_int8_t *)b)[0] = ((u_int8_t *)a)[0];			\
	((u_int8_t *)b)[1] = ((u_int8_t *)a)[1];			\
	((u_int8_t *)b)[2] = ((u_int8_t *)a)[2];			\
	((u_int8_t *)b)[3] = ((u_int8_t *)a)[3];			\
}
#define	P_32_SWAP(a) {							\
	u_int32_t _tmp;							\
	P_32_COPY(a, &_tmp);						\
	((u_int8_t *)a)[0] = ((u_int8_t *)&_tmp)[3];			\
	((u_int8_t *)a)[1] = ((u_int8_t *)&_tmp)[2];			\
	((u_int8_t *)a)[2] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)a)[3] = ((u_int8_t *)&_tmp)[0];			\
}

/*
 * Little endian <==> big endian 16-bit swap macros.
 *	M_16_SWAP	swap a memory location
 *	P_16_COPY	copy potentially unaligned 2 byte quantities
 *	P_16_SWAP	swap a referenced memory location
 */
#define	M_16_SWAP(a) {							\
	u_int16_t _tmp;							\
	_tmp = (u_int16_t)a;						\
	((u_int8_t *)&a)[0] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)&a)[1] = ((u_int8_t *)&_tmp)[0];			\
}
#define	P_16_COPY(a, b) {						\
	((u_int8_t *)b)[0] = ((u_int8_t *)a)[0];			\
	((u_int8_t *)b)[1] = ((u_int8_t *)a)[1];			\
}
#define	P_16_SWAP(a) {							\
	u_int16_t _tmp;							\
	P_16_COPY(a, &_tmp);						\
	((u_int8_t *)a)[0] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)a)[1] = ((u_int8_t *)&_tmp)[0];			\
}

#define	SWAP32(p) {							\
	P_32_SWAP(p);							\
	(p) += sizeof(u_int32_t);					\
}
#define	SWAP16(p) {							\
	P_16_SWAP(p);							\
	(p) += sizeof(u_int16_t);					\
}

/*
 * DB has local versions of htonl() and ntohl() that only operate on pointers
 * to the right size memory locations, the portability magic for finding the
 * real ones isn't worth the effort.
 */
#if defined(WORDS_BIGENDIAN)
#define	DB_HTONL(p)
#define	DB_NTOHL(p)
#else
#define	DB_HTONL(p)	P_32_SWAP(p)
#define	DB_NTOHL(p)	P_32_SWAP(p)
#endif

#endif /* !_DB_SWAP_H_ */
