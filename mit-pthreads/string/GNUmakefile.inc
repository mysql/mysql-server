#	from: @(#)Makefile.inc	5.21 (Berkeley) 5/24/91
#	$Id$

# gen sources
VPATH:= ${VPATH}:${srcdir}/string

SRCS:= strtok.c $(SRCS)
