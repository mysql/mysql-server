#	@(#)Makefile.inc	5.6 (Berkeley) 6/4/91

# stdlib sources
VPATH:= ${VPATH}:${srcdir}/stdlib

SRCS:= abort.c exit.c strtod.c getopt.c rand.c random.c strtol.c strtoul.c \
       system.c $(SRCS)
