/*	@(#)stdtypes.h 1.6 90/01/04 SMI	*/

/*
 * Suppose you have an ANSI C or POSIX thingy that needs a typedef
 * for thingy_t.  Put it here and include this file wherever you
 * define the thingy.  This is used so that we don't have size_t in
 * N (N > 1) different places and so that we don't have to have
 * types.h included all the time and so that we can include this in
 * the lint libs instead of termios.h which conflicts with ioctl.h.
 */
#ifndef	__sys_stdtypes_h
#define	__sys_stdtypes_h

#ifndef _SIGSET_T_
#define _SIGSET_T_
typedef	int		sigset_t;	/* signal mask - may change */
#endif

#ifndef _SPEED_T_
#define _SPEED_T_
typedef	unsigned int	speed_t;	/* tty speeds */
#endif

#ifndef _TCFLAG_T_
#define _TCFLAG_T_
typedef	unsigned long	tcflag_t;	/* tty line disc modes */
#endif

#ifndef _CC_T_
#define _CC_T_
typedef	unsigned char	cc_t;		/* tty control char */
#endif

#ifndef _PID_T_
#define _PID_T_
typedef	int		pid_t;		/* process id */
#endif

#ifndef _MODE_T_
#define _MODE_T_
typedef	unsigned short	mode_t;		/* file mode bits */
#endif

#ifndef _NLINK_T_
#define _NLINK_T_
typedef	short		nlink_t;	/* links to a file */
#endif

#ifndef _CLOCK_T_
#define _CLOCK_T_
typedef	long		clock_t;	/* units=ticks (typically 60/sec) */
#endif

#ifndef _TIME_T_
#define _TIME_T_
typedef	long		time_t;		/* value = secs since epoch */
#endif

#ifndef _SIZE_T_
#define _SIZE_T_
typedef	int		size_t;		/* ??? */
#endif

#ifndef _PTRDIFF_T_
#define _PTRDIFF_T_
typedef int		ptrdiff_t;	/* result of subtracting two pointers */
#endif

#ifndef _WCHAR_T_
#define _WCHAR_T_
typedef	unsigned short	wchar_t;	/* big enough for biggest char set */
#endif

#endif	/* !__sys_stdtypes_h */
