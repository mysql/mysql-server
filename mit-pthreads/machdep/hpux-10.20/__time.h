/* $Id$ */

#ifndef __sys_stdtypes_h

#ifndef _SYS__TIME_H_
#define	_SYS__TIME_H_

#ifndef	_CLOCK_T
#define	_CLOCK_T
typedef	long	clock_t;
#endif

#ifndef	_TIME_T
#define _TIME_T
typedef	long	time_t;
#endif

#ifndef	_SIZE_T
#define _SIZE_T
typedef	unsigned int	size_t;
#endif

#define CLOCKS_PER_SEC	1000000

#if !defined(_ANSI_SOURCE) && !defined(CLK_TCK)
#define CLK_TCK		60
#endif /* not ANSI */

#endif

#endif /* !_SYS__TIME_H_ */
