/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
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
 * $Id: db_swap.h,v 11.11 2004/01/28 03:36:02 bostic Exp $
 */

#ifndef _DB_SWAP_H_
#define	_DB_SWAP_H_

/*
 * Little endian <==> big endian 64-bit swap macros.
 *	M_64_SWAP	swap a memory location
 *	P_64_COPY	copy potentially unaligned 4 byte quantities
 *	P_64_SWAP	swap a referenced memory location
 */
#undef	M_64_SWAP
#define	M_64_SWAP(a) {							\
	u_int64_t _tmp;							\
	_tmp = a;							\
	((u_int8_t *)&a)[0] = ((u_int8_t *)&_tmp)[7];			\
	((u_int8_t *)&a)[1] = ((u_int8_t *)&_tmp)[6];			\
	((u_int8_t *)&a)[2] = ((u_int8_t *)&_tmp)[5];			\
	((u_int8_t *)&a)[3] = ((u_int8_t *)&_tmp)[4];			\
	((u_int8_t *)&a)[4] = ((u_int8_t *)&_tmp)[3];			\
	((u_int8_t *)&a)[5] = ((u_int8_t *)&_tmp)[2];			\
	((u_int8_t *)&a)[6] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)&a)[7] = ((u_int8_t *)&_tmp)[0];			\
}
#undef	P_64_COPY
#define	P_64_COPY(a, b) {						\
	((u_int8_t *)b)[0] = ((u_int8_t *)a)[0];			\
	((u_int8_t *)b)[1] = ((u_int8_t *)a)[1];			\
	((u_int8_t *)b)[2] = ((u_int8_t *)a)[2];			\
	((u_int8_t *)b)[3] = ((u_int8_t *)a)[3];			\
	((u_int8_t *)b)[4] = ((u_int8_t *)a)[4];			\
	((u_int8_t *)b)[5] = ((u_int8_t *)a)[5];			\
	((u_int8_t *)b)[6] = ((u_int8_t *)a)[6];			\
	((u_int8_t *)b)[7] = ((u_int8_t *)a)[7];			\
}
#undef	P_64_SWAP
#define	P_64_SWAP(a) {							\
	u_int64_t _tmp;							\
	P_64_COPY(a, &_tmp);						\
	((u_int8_t *)a)[0] = ((u_int8_t *)&_tmp)[7];			\
	((u_int8_t *)a)[1] = ((u_int8_t *)&_tmp)[6];			\
	((u_int8_t *)a)[2] = ((u_int8_t *)&_tmp)[5];			\
	((u_int8_t *)a)[3] = ((u_int8_t *)&_tmp)[4];			\
	((u_int8_t *)a)[4] = ((u_int8_t *)&_tmp)[3];			\
	((u_int8_t *)a)[5] = ((u_int8_t *)&_tmp)[2];			\
	((u_int8_t *)a)[6] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)a)[7] = ((u_int8_t *)&_tmp)[0];			\
}

/*
 * Little endian <==> big endian 32-bit swap macros.
 *	M_32_SWAP	swap a memory location
 *	P_32_COPY	copy potentially unaligned 4 byte quantities
 *	P_32_SWAP	swap a referenced memory location
 */
#undef	M_32_SWAP
#define	M_32_SWAP(a) {							\
	u_int32_t _tmp;							\
	_tmp = a;							\
	((u_int8_t *)&a)[0] = ((u_int8_t *)&_tmp)[3];			\
	((u_int8_t *)&a)[1] = ((u_int8_t *)&_tmp)[2];			\
	((u_int8_t *)&a)[2] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)&a)[3] = ((u_int8_t *)&_tmp)[0];			\
}
#undef	P_32_COPY
#define	P_32_COPY(a, b) {						\
	((u_int8_t *)b)[0] = ((u_int8_t *)a)[0];			\
	((u_int8_t *)b)[1] = ((u_int8_t *)a)[1];			\
	((u_int8_t *)b)[2] = ((u_int8_t *)a)[2];			\
	((u_int8_t *)b)[3] = ((u_int8_t *)a)[3];			\
}
#undef	P_32_SWAP
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
#undef	M_16_SWAP
#define	M_16_SWAP(a) {							\
	u_int16_t _tmp;							\
	_tmp = (u_int16_t)a;						\
	((u_int8_t *)&a)[0] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)&a)[1] = ((u_int8_t *)&_tmp)[0];			\
}
#undef	P_16_COPY
#define	P_16_COPY(a, b) {						\
	((u_int8_t *)b)[0] = ((u_int8_t *)a)[0];			\
	((u_int8_t *)b)[1] = ((u_int8_t *)a)[1];			\
}
#undef	P_16_SWAP
#define	P_16_SWAP(a) {							\
	u_int16_t _tmp;							\
	P_16_COPY(a, &_tmp);						\
	((u_int8_t *)a)[0] = ((u_int8_t *)&_tmp)[1];			\
	((u_int8_t *)a)[1] = ((u_int8_t *)&_tmp)[0];			\
}

#undef	SWAP32
#define	SWAP32(p) {							\
	P_32_SWAP(p);							\
	(p) += sizeof(u_int32_t);					\
}
#undef	SWAP16
#define	SWAP16(p) {							\
	P_16_SWAP(p);							\
	(p) += sizeof(u_int16_t);					\
}

/*
 * Berkeley DB has local versions of htonl() and ntohl() that operate on
 * pointers to the right size memory locations; the portability magic for
 * finding the real system functions isn't worth the effort.
 */
#undef	DB_HTONL
#define	DB_HTONL(p) do {						\
	if (!__db_isbigendian())					\
		P_32_SWAP(p);						\
} while (0)
#undef	DB_NTOHL
#define	DB_NTOHL(p) do {						\
	if (!__db_isbigendian())					\
		P_32_SWAP(p);						\
} while (0)

#endif /* !_DB_SWAP_H_ */
