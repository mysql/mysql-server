#	from: @(#)Makefile.inc	5.21 (Berkeley) 5/24/91
#	$Id$

# gen sources
.PATH: ${srcdir}/gen

SRCS+= ttyname.c isatty.c directory.c popen.c time.c ctime.c difftime.c \
    syslog.c eprintf.c getpwent.c getpwnamuid.c pwd_internal.c

#SRCS+=	alarm.c assert.c clock.c crypt.c ctermid.c ctype_.c \
#	disklabel.c err.c errlst.c exec.c fnmatch.c frexp.c \
#	fstab.c fts.c getcap.c getcwd.c getgrent.c getlogin.c getmntinfo.c \
#	getpass.c getpwent.c getsubopt.c getttyent.c getusershell.c glob.c \
#	infinity.c initgroups.c isatty.c isctype.c isinf.c mktemp.c nice.c \
#	nlist.c pause.c psignal.c raise.c \
#	scandir.c setjmperr.c \
#	setmode.c setrgid.c setruid.c siginterrupt.c \
#	siglist.c signal.c sigsetops.c syslog.c \
#	termios.c time.c times.c timezone.c ttyslot.c \
#	ualarm.c unvis.c utime.c valloc.c vis.c 
#
# gen/regexp sources
#SRCS+=	regerror.c regexp.c regsub.c

