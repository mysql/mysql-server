/*
 * Copyright (c) 2007 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVUTIL_H_
#define _EVUTIL_H_

/** @file evutil.h

  Common convenience functions for cross-platform portability and
  related socket manipulations.

 */

#ifdef __cplusplus
extern "C" {
#endif

#include <config.h>
#include <errno.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <stdarg.h>

#ifdef HAVE_UINT64_T
#define ev_uint64_t uint64_t
#define ev_int64_t int64_t
#elif defined(WIN32)
#define ev_uint64_t unsigned __int64
#define ev_int64_t signed __int64
#elif SIZEOF_LONG_LONG == 8
#define ev_uint64_t unsigned long long
#define ev_int64_t long long
#elif SIZEOF_LONG == 8
#define ev_uint64_t unsigned long
#define ev_int64_t long
#else
#error "No way to define ev_uint64_t"
#endif

#ifdef HAVE_UINT32_T
#define ev_uint32_t uint32_t
#elif defined(WIN32)
#define ev_uint32_t unsigned int
#elif SIZEOF_LONG == 4
#define ev_uint32_t unsigned long
#elif SIZEOF_INT == 4
#define ev_uint32_t unsigned int
#else
#error "No way to define ev_uint32_t"
#endif

#ifdef HAVE_UINT16_T
#define ev_uint16_t uint16_t
#elif defined(WIN32)
#define ev_uint16_t unsigned short
#elif SIZEOF_INT == 2
#define ev_uint16_t unsigned int
#elif SIZEOF_SHORT == 2
#define ev_uint16_t unsigned short
#else
#error "No way to define ev_uint16_t"
#endif

#ifdef HAVE_UINT8_T
#define ev_uint8_t uint8_t
#else
#define ev_uint8_t unsigned char
#endif

#if !defined(__cplusplus) && defined(_MSC_VER)
#define inline __inline
#endif

#if defined(_MSC_VER)
#define __func__ __FUNCTION__
#endif

int evutil_socketpair(int d, int type, int protocol, int sv[2]);
int evutil_make_socket_nonblocking(int sock);
#ifdef WIN32
#define EVUTIL_CLOSESOCKET(s) closesocket(s)
#else
#define EVUTIL_CLOSESOCKET(s) close(s)
#endif

#ifdef WIN32
#define EVUTIL_SOCKET_ERROR() WSAGetLastError()
#define EVUTIL_SET_SOCKET_ERROR(errcode)		\
	do { WSASetLastError(errcode); } while (0)
#else
#define EVUTIL_SOCKET_ERROR() (errno)
#define EVUTIL_SET_SOCKET_ERROR(errcode)		\
		do { errno = (errcode); } while (0)
#endif

/*
 * Manipulation functions for struct timeval
 */
#ifdef HAVE_TIMERADD
#define evutil_timeradd(tvp, uvp, vvp) timeradd((tvp), (uvp), (vvp))
#define evutil_timersub(tvp, uvp, vvp) timersub((tvp), (uvp), (vvp))
#else
#define evutil_timeradd(tvp, uvp, vvp)							\
	do {														\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;			\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
		if ((vvp)->tv_usec >= 1000000) {						\
			(vvp)->tv_sec++;									\
			(vvp)->tv_usec -= 1000000;							\
		}														\
	} while (0)
#define	evutil_timersub(tvp, uvp, vvp)						\
	do {													\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {							\
			(vvp)->tv_sec--;								\
			(vvp)->tv_usec += 1000000;						\
		}													\
	} while (0)
#endif /* !HAVE_HAVE_TIMERADD */

#ifdef HAVE_TIMERCLEAR
#define evutil_timerclear(tvp) timerclear(tvp)
#else
#define	evutil_timerclear(tvp)	(tvp)->tv_sec = (tvp)->tv_usec = 0
#endif

#define	evutil_timercmp(tvp, uvp, cmp)							\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?							\
	 ((tvp)->tv_usec cmp (uvp)->tv_usec) :						\
	 ((tvp)->tv_sec cmp (uvp)->tv_sec))

#ifdef HAVE_TIMERISSET
#define evutil_timerisset(tvp) timerisset(tvp)
#else
#define	evutil_timerisset(tvp)	((tvp)->tv_sec || (tvp)->tv_usec)
#endif


/* big-int related functions */
ev_int64_t evutil_strtoll(const char *s, char **endptr, int base);


#ifdef HAVE_GETTIMEOFDAY
#define evutil_gettimeofday(tv, tz) gettimeofday((tv), (tz))
#else
int evutil_gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

int evutil_snprintf(char *buf, size_t buflen, const char *format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 3, 4)))
#endif
	;
int evutil_vsnprintf(char *buf, size_t buflen, const char *format, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* _EVUTIL_H_ */
