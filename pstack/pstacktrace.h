/* $Header$ */

/*
 * Debugging macros.
 */

#ifndef	pstacktrace_h_
#define	pstacktrace_h_

#define	PSTACK_DEBUG 1
#undef PSTACK_DEBUG

#ifdef PSTACK_DEBUG
#	define	TRACE_PUTC(a)		putc a
#	define	TRACE_FPUTS(a)		fputs a
#	define	TRACE_FPRINTF(a)	fprintf a
#else /* PSTACK_DEBUG */
#	define	TRACE_PUTC(a)		(void)0
#	define	TRACE_FPUTS(a)		(void)0
#	define	TRACE_FPRINTF(a)	(void)0
#endif /* !PSTACK_DEBUG */

#endif /* pstacktrace_h_ */

