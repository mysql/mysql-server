/* ==== stdio.h ============================================================
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 1993 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)stdio.h	5.17 (Berkeley) 6/3/91
 *	$Id$
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <sys/cdefs.h>
#include <pthread/types.h>
#include <pthread/posix.h>
#include <sys/__stdio.h>

#ifndef NULL
#define	NULL	0
#endif

#define	_FSTDIO				/* Define for new stdio with functions. */

/*
 * NB: to fit things in six character monocase externals, the stdio
 * code uses the prefix `__s' for stdio objects, typically followed
 * by a three-character attempt at a mnemonic.
 */

/* stdio buffers */
struct __sbuf {
	unsigned char *_base;
	int	_size;
};

/*
 * stdio state variables.
 *
 * The following always hold:
 *
 *	if (_flags&(__SLBF|__SWR)) == (__SLBF|__SWR),
 *		_lbfsize is -_bf._size, else _lbfsize is 0
 *	if _flags&__SRD, _w is 0
 *	if _flags&__SWR, _r is 0
 *
 * This ensures that the getc and putc macros (or inline functions) never
 * try to write or read from a file that is in `read' or `write' mode.
 * (Moreover, they can, and do, automatically switch from read mode to
 * write mode, and back, on "r+" and "w+" files.)
 *
 * _lbfsize is used only to make the inline line-buffered output stream
 * code as compact as possible.
 *
 * _ub, _up, and _ur are used when ungetc() pushes back more characters
 * than fit in the current _bf, or when ungetc() pushes back a character
 * that does not match the previous one in _bf.  When this happens,
 * _ub._base becomes non-nil (i.e., a stream has ungetc() data iff
 * _ub._base!=NULL) and _up and _ur save the current values of _p and _r.
 */
typedef	struct __sFILE {
	unsigned char 	*_p;		/* current position in (some) buffer */
	int				_r;			/* read space left for getc() */
	int				_w;			/* write space left for putc() */
	short			_flags;		/* flags, below; this FILE is free if 0 */
	short			_file;		/* fileno, if Unix descriptor, else -1 */
	struct	__sbuf 	_bf;		/* the buffer (at least 1 byte, if !NULL) */
	int				_lbfsize;	/* 0 or -_bf._size, for inline putc */

	/* separate buffer for long sequences of ungetc() */
	struct	__sbuf 	_ub;		/* ungetc buffer */
	unsigned char 	*_up;		/* saved _p when _p is doing ungetc data */
	int				_ur;		/* saved _r when _r is counting ungetc data */

	/* tricks to meet minimum requirements even when malloc() fails */
	unsigned char 	_ubuf[3];	/* guarantee an ungetc() buffer */
	unsigned char 	_nbuf[1];	/* guarantee a getc() buffer */

	/* separate buffer for fgetline() when line crosses buffer boundary */
	struct	__sbuf 	_lb;		/* buffer for fgetline() */

	/* Unix stdio files get aligned to block boundaries on fseek() */
	int				_blksize;	/* stat.st_blksize (may be != _bf._size) */
	int				_offset;	/* current lseek offset */
} FILE;

__BEGIN_DECLS
extern FILE __sF[];
__END_DECLS

#define	__SLBF		0x0001		/* line buffered */
#define	__SNBF		0x0002		/* unbuffered */
#define	__SRD		0x0004		/* OK to read */
#define	__SWR		0x0008		/* OK to write */
	/* RD and WR are never simultaneously asserted */
#define	__SRW		0x0010		/* open for reading & writing */
#define	__SEOF		0x0020		/* found EOF */
#define	__SERR		0x0040		/* found error */
#define	__SMBF		0x0080		/* _buf is from malloc */
#define	__SAPP		0x0100		/* fdopen()ed in append mode */
#define	__SSTR		0x0200		/* this is an sprintf/snprintf string */
#define	__SOPT		0x0400		/* do fseek() optimisation */
#define	__SNPT		0x0800		/* do not do fseek() optimisation */
#define	__SOFF		0x1000		/* set iff _offset is in fact correct */
#define	__SMOD		0x2000		/* true => fgetline modified _p text */

/*
 * The following three definitions are for ANSI C, which took them
 * from System V, which brilliantly took internal interface macros and
 * made them official arguments to setvbuf(), without renaming them.
 * Hence, these ugly _IOxxx names are *supposed* to appear in user code.
 *
 * Although numbered as their counterparts above, the implementation
 * does not rely on this.
 */
#define	_IOFBF		0			/* setvbuf should set fully buffered */
#define	_IOLBF		1			/* setvbuf should set line buffered */
#define	_IONBF		2			/* setvbuf should set unbuffered */

#define	BUFSIZ		1024		/* size of buffer used by setbuf */
#define	EOF			(-1)

/*
 * FOPEN_MAX is a minimum maximum, and should be the number of descriptors
 * that the kernel can provide without allocation of a resource that can
 * fail without the process sleeping.  Do not use this for anything.
 */
#define	FOPEN_MAX	20			/* must be <= OPEN_MAX <sys/syslimits.h> */
#define	FILENAME_MAX	1024	/* must be <= PATH_MAX <sys/syslimits.h> */

/* System V/ANSI C; this is the wrong way to do this, do *not* use these. */
#ifndef _ANSI_SOURCE
#define	P_tmpdir	"/var/tmp/"
#endif
#define	L_tmpnam	1024	/* XXX must be == PATH_MAX */
#ifndef	TMP_MAX
#define	TMP_MAX		308915776
#endif

#ifndef SEEK_SET
#define	SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef SEEK_END
#define	SEEK_END	2	/* set file offset to EOF plus offset */
#endif

#define	stdin		(&__sF[0])
#define	stdout		(&__sF[1])
#define	stderr		(&__sF[2])

/*
 * Functions defined in ANSI C standard.
 */
__BEGIN_DECLS
void	clearerr 	__P_((FILE *));
int	 	fclose 		__P_((FILE *));
int	 	feof 		__P_((FILE *));
int	 	ferror 		__P_((FILE *));
int	 	fflush 		__P_((FILE *));
int	 	fgetc 		__P_((FILE *));
int	 	fgetpos 	__P_((FILE *, fpos_t *));
char  * fgets 		__P_((char *, size_t, FILE *));
FILE  * fopen 		__P_((const char *, const char *));
int	 	fprintf 	__P_((FILE *, const char *, ...));
int	 	fputc 		__P_((int, FILE *));
int	 	fputs 		__P_((const char *, FILE *));
size_t	fread 		__P_((void *, size_t, size_t, FILE *));
FILE  * freopen 	__P_((const char *, const char *, FILE *));
int	 	fscanf 		__P_((FILE *, const char *, ...));
int	 	fseek 		__P_((FILE *, long, int));
int	 	fsetpos 	__P_((FILE *, const fpos_t *));
long	ftell 		__P_((const FILE *));
size_t	fwrite 		__P_((const void *, size_t, size_t, FILE *));
int	 	getc 		__P_((FILE *));
int	 	getchar 	__P_((void));
char  * gets 		__P_((char *));

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
extern int sys_nerr;			/* perror(3) external variables */
/* Under NetBSD and BSD 4.4, at least, this is expected to be a const
   array of pointers to const.  If you take `const' back out of this
   declaration, please make it conditional on __NetBSD__ and bsd4_4.  */
#ifdef HAVE_SYS_ERRLIST_WITHOUT_CONST
extern char *sys_errlist[];
#else
extern const char *const sys_errlist[];
#endif
#endif

void	perror 		__P_((const char *));
int		printf 		__P_((const char *, ...));
int		putc 		__P_((int, FILE *));
int		putchar		__P_((int));
int		puts 		__P_((const char *));
int	 	remove 		__P_((const char *));
int	 	rename 		__P_((const char *, const char *));
void	rewind 		__P_((FILE *));
int	 	scanf		__P_((const char *, ...));
void	setbuf 		__P_((FILE *, char *));
int	 	setvbuf 	__P_((FILE *, char *, int, size_t));
int	 	sprintf 	__P_((char *, const char *, ...));
int	 	sscanf 		__P_((const char *, const char *, ...));
FILE  *	tmpfile 	__P_((void));
char  *	tmpnam 		__P_((char *));
int	 	ungetc 		__P_((int, FILE *));
int	 	vfprintf 	__P_((FILE *, const char *, pthread_va_list));
int	 	vprintf 	__P_((const char *, pthread_va_list));
int	 	vsprintf 	__P_((char *, const char *, pthread_va_list));
char *mprintf __P_((const char *, ...));
char *vmprintf __P_((const char *, pthread_va_list));
__END_DECLS

/*
 * Functions defined in POSIX 1003.1.
 */
#ifndef _ANSI_SOURCE
#define	L_ctermid	1024	/* size for ctermid(); PATH_MAX */
#define L_cuserid	9	/* size for cuserid(); UT_NAMESIZE + 1 */

__BEGIN_DECLS
char  * ctermid __P_((char *));
char  * cuserid __P_((char *));
FILE  * fdopen __P_((int, const char *));
int	 	fileno __P_((FILE *));
__END_DECLS
#endif /* not ANSI */

/*
 * Functions defined in POSIX 1003.4a. (1c)
 */
#ifndef _ANSI_SOURCE
__BEGIN_DECLS
void	flockfile __P_((FILE *));
void	funlockfile __P_((FILE *));
int		ftrylockfile __P_((FILE *));
__END_DECLS
#endif /* not ANSI */

/*
 * Routines that are purely local.
 */
#if !defined (_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
__BEGIN_DECLS
char	*fgetline __P_((FILE *, size_t *));
int	 fpurge __P_((FILE *));
int	 getw __P_((FILE *));
int	 pclose __P_((FILE *));
FILE	*popen __P_((const char *, const char *));
int	 putw __P_((int, FILE *));
void	 setbuffer __P_((FILE *, char *, int));
int	 setlinebuf __P_((FILE *));
char	*tempnam __P_((const char *, const char *));
int	 snprintf __P_((char *, size_t, const char *, ...));
int	 vsnprintf __P_((char *, size_t, const char *, pthread_va_list));
int	 vscanf __P_((const char *, pthread_va_list));
int	 vsscanf __P_((const char *, const char *, pthread_va_list));
__END_DECLS

/*
 * This is a #define because the function is used internally and
 * (unlike vfscanf) the name __svfscanf is guaranteed not to collide
 * with a user function when _ANSI_SOURCE or _POSIX_SOURCE is defined.
 */
#define	 vfscanf	__svfscanf

/*
 * Stdio function-access interface.
 */
__BEGIN_DECLS
FILE	*funopen __P_((const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *)));
__END_DECLS
#define	fropen(cookie, fn) funopen(cookie, fn, 0, 0, 0)
#define	fwopen(cookie, fn) funopen(cookie, 0, fn, 0, 0)
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */

/*
 * Functions internal to the implementation.
 */
__BEGIN_DECLS
int	__srget __P_((FILE *));
int	__svfscanf __P_((FILE *, const char *, pthread_va_list));
int	__swbuf __P_((int, FILE *));
__END_DECLS

/*
 * The __sfoo macros are here so that we can 
 * define function versions in the C library.
 */
#define	__sgetc(p) (--(p)->_r < 0 ? __srget(p) : (int)(*(p)->_p++))

__BEGIN_DECLS
int 	__getc 					__P_((FILE *));
__END_DECLS

#define getc(fp)				__getc(fp)
#define	getchar()				getc(stdin)
#define getc_unlocked(fp)		__sgetc(fp)
#define getchar_unlocked()		getc_unlocked(stdin)

#ifdef __CAN_DO_EXTERN_INLINE
__INLINE int __sputc(int _c, FILE *_p)
{
	if (--_p->_w >= 0 || (_p->_w >= _p->_lbfsize && (char)_c != '\n'))
		return (*_p->_p++ = _c);
	else
		return (__swbuf(_c, _p));
}
#else
__BEGIN_DECLS
int 	__sputc 				__P_((int, FILE *));
__END_DECLS
#endif

__BEGIN_DECLS
int 	__putc 					__P_((int, FILE *));
__END_DECLS

#define putc(x, fp)				__putc(x, fp)
#define	putchar(x)				putc(x, stdout)
#define putc_unlocked(x, fp)	__sputc(x, fp)
#define putchar_unlocked(x)		putc_unlocked(x, stdout)

#define	__sfeof(p)		(((p)->_flags & __SEOF) != 0)
#define	__sferror(p)	(((p)->_flags & __SERR) != 0)
#define	__sfileno(p)	((p)->_file)

#define	feof(p)		__sfeof(p)
#define	ferror(p)	__sferror(p)

#ifndef _ANSI_SOURCE
#define	fileno(p)	__sfileno(p)
#endif

#endif
