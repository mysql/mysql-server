#	@(#)Makefile.inc	5.6 (Berkeley) 6/4/91

# stdlib sources
.PATH: ${srcdir}/${MACHINE}/stdlib ${srcdir}/stdlib

SRCS+= exit.c strtod.c getopt.c rand.c random.c strtol.c strtoul.c

# SRCS+=abort.c atexit.c atoi.c atof.c atol.c bsearch.c calloc.c div.c \
# 	getenv.c heapsort.c labs.c ldiv.c malloc.c multibyte.c \
# 	putenv.c qsort.c radixsort.c setenv.c system.c
