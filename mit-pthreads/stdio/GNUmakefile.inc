
#	from: @(#)Makefile.inc	5.7 (Berkeley) 6/27/91
#	$Id$

# Thread safe stdio sources
VPATH:= ${VPATH}:${srcdir}/stdio

SRCS:=	clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c fgetc.c \
	fgetline.c fgetpos.c fgets.c fileno.c findfp.c flags.c fopen.c \
	fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
	fseek.c fsetpos.c ftell.c funopen.c fvwrite.c fwalk.c fwrite.c \
	getc.c getchar.c gets.c getw.c makebuf.c perror.c putc.c \
	putchar.c puts.c putw.c refill.c remove.c rewind.c rget.c scanf.c \
	setbuf.c setbuffer.c setvbuf.c sscanf.c \
	stdio.c ungetc.c \
	vfscanf.c vscanf.c vsscanf.c \
	wsetup.c putc_unlocked.c putchar_unlocked.c getc_unlocked.c \
	getchar_unlocked.c strerror.c wbuf.c xprintf.c $(SRCS)

# tempnam.c tmpnam.c tmpfile.c
#
# SRCS:= sys_errlist.c $(SRCS)
# sys_errlist.c: make_errlist
#	(FOO=`pwd`; cd ${srcdir}/stdio/make_errlist; \
#    $(MAKE) CC=$(CC) srcdir=${srcdir} objdir=$${FOO})
#	`pwd`/make_errlist;
