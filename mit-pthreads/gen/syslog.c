/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)syslog.c	5.14 (Berkeley) 5/20/88";
#endif /* LIBC_SCCS and not lint */


/*
 * SYSLOG -- print message on log file
 *
 * This routine looks a lot like printf, except that it
 * outputs to the log file instead of the standard output.
 * Also:
 *	adds a timestamp,
 *	prints the module name in front of the message,
 *	has some other formatting types (or will sometime),
 *	adds a newline on the end of the message.
 *
 * The output of this routine is intended to be read by /etc/syslogd.
 *
 * Author: Eric Allman
 * Modified to use UNIX domain IPC by Ralph Campbell
 * Modified for pthreads and made more POSIX-compliant by Greg Hudson
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

int socket();
char *strerror(int);		/* For systems that don't prototype it */

#define	MAXLINE	1024		/* max message size */

#define PRIFAC(p)	(((p) & LOG_FACMASK) >> 3)
					/* XXX should be in <syslog.h> */
#define IMPORTANT 	LOG_ERR

static void basic_init(void);

static char	_log_name[] = "/dev/log";
static char	ctty[] = "/dev/console";

static int	LogFile = -1;		/* fd for log */
static int	LogStat	= 0;		/* status bits, set by openlog() */
static char	*LogTag = "syslog";	/* string to tag the entry with */
static int	LogMask = 0xff;		/* mask of priorities to be logged */
static int	LogFacility = LOG_USER;	/* default facility code */

static pthread_mutex_t basic_init_lock = PTHREAD_MUTEX_INITIALIZER;

static struct sockaddr SyslogAddr;	/* AF_UNIX address of local logger */

static void basic_init()
{
    pthread_mutex_lock(&basic_init_lock);
    if (LogFile < 0)
	openlog(LogTag, LogStat | LOG_NDELAY, 0);
    pthread_mutex_unlock(&basic_init_lock);
}

void syslog(int pri, char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsyslog(pri, fmt, args);
	va_end(args);
}

void vsyslog(int pri, char *fmt, va_list args)
{
	char buf[MAXLINE + 1], outline[MAXLINE + 1];
	register char *b, *f, *o;
	register int c;
	time_t now;
	int olderrno = errno, fd;

	/* Do a basic initialization if user didn't call openlog(). */
	if (LogFile < 0)
	    basic_init();

	/* see if we should just throw out this message */
	if ((unsigned) PRIFAC(pri) >= LOG_NFACILITIES ||
	    (LOG_MASK(pri & LOG_PRIMASK) & LogMask) == 0 ||
	    (pri &~ (LOG_PRIMASK|LOG_FACMASK)) != 0)
		return;

	/* set default facility if none specified */
	if ((pri & LOG_FACMASK) == 0)
		pri |= LogFacility;

	/* build the message */
	o = outline;
	(void)sprintf(o, "<%d>", pri);
	o += strlen(o);
	time(&now);
	(void)sprintf(o, "%.15s ", ctime(&now) + 4);
	o += strlen(o);
	if (LogTag) {
		strcpy(o, LogTag);
		o += strlen(o);
	}
	if (LogStat & LOG_PID) {
		(void)sprintf(o, "[%d]", getpid());
		o += strlen(o);
	}
	if (LogTag) {
		strcpy(o, ": ");
		o += 2;
	}

	b = buf;
	f = fmt;
	while ((c = *f++) != '\0' && c != '\n' && b < &buf[MAXLINE]) {
	        char *strerror();

		if (c != '%') {
			*b++ = c;
			continue;
		}
		if ((c = *f++) != 'm') {
			*b++ = '%';
			*b++ = c;
			continue;
		}
		strcpy(b, strerror(olderrno));
		b += strlen(b);
	}
	*b++ = '\n';
	*b = '\0';
	vsprintf(o, buf, args);
	c = strlen(outline);
	if (c > MAXLINE)
		c = MAXLINE;

	/* output the message to the local logger */
	if (sendto(LogFile, outline, c, 0, &SyslogAddr, sizeof SyslogAddr) >= 0)
		return;
	if (!(LogStat & LOG_CONS))
		return;

	/* output the message to the console */
	fd = open(ctty, O_WRONLY);
	alarm(0);
	strcat(o, "\r");
	o = strchr(outline, '>') + 1;
	write(fd, o, c + 1 - (o - outline));
	close(fd);
}

/*
 * OPENLOG -- open system log
 */

void openlog(char *ident, int logstat, int logfac)
{
	int flags;

	if (ident != NULL)
		LogTag = ident;
	LogStat = logstat;
	if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0)
		LogFacility = logfac;
	if (LogFile >= 0)
		return;
	SyslogAddr.sa_family = AF_UNIX;
	strncpy(SyslogAddr.sa_data, _log_name, sizeof SyslogAddr.sa_data);
	if (LogStat & LOG_NDELAY) {
		LogFile = socket(AF_UNIX, SOCK_DGRAM, 0);
		flags = fcntl(LogFile, F_GETFD);
		fcntl(LogFile, F_SETFD, flags & O_NONBLOCK);
	}
}

/*
 * CLOSELOG -- close the system log
 */

void closelog()
{
	(void) close(LogFile);
	LogFile = -1;
}

/*
 * SETLOGMASK -- set the log mask level
 */
int setlogmask(int pmask)
{
	int omask;

	omask = LogMask;
	if (pmask != 0)
		LogMask = pmask;
	return (omask);
}
