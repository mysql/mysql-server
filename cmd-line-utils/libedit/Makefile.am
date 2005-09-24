## Process this file with automake to create Makefile.in
# Makefile for the GNU readline library.
# Copyright (C) 1994,1996,1997 Free Software Foundation, Inc.

ASRC =			$(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c
AHDR =			vi.h emacs.h common.h

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(srcdir)/../.. -I..

noinst_LIBRARIES =	libedit.a

libedit_a_SOURCES =	chared.c el.c history.c map.c prompt.c readline.c \
			search.c tokenizer.c vi.c common.c emacs.c \
			hist.c key.c parse.c read.c refresh.c sig.c term.c \
			tty.c help.c fcns.c

EXTRA_libedit_a_SOURCES =	np/unvis.c np/strlcpy.c np/vis.c np/strlcat.c \
				np/fgetln.c

libedit_a_LIBADD = 		@LIBEDIT_LOBJECTS@
libedit_a_DEPENDENCIES 	= 	@LIBEDIT_LOBJECTS@

pkginclude_HEADERS =	readline/readline.h

noinst_HEADERS =	chared.h el.h el_term.h histedit.h key.h parse.h refresh.h sig.h \
			sys.h tokenizer.h config.h hist.h map.h prompt.h read.h \
			search.h tty.h libedit_term.h vis.h

EXTRA_DIST = makelist.sh np/unvis.c np/strlcpy.c np/vis.c np/vis.h np/strlcat.c np/fgetln.c

CLEANFILES = makelist common.h emacs.h vi.h fcns.h help.h fcns.c help.c

DEFS = -DUNDEF_THREADS_HACK -DHAVE_CONFIG_H -DNO_KILL_INTR 

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''AWK''@!@AWK@!' \
	$< > $@-t
	@MV@ $@-t $@

vi.h: $(srcdir)/vi.c makelist
	sh ./makelist -h $(srcdir)/vi.c > $@.tmp && \
	mv $@.tmp $@

emacs.h: $(srcdir)/emacs.c makelist
	sh ./makelist -h $(srcdir)/emacs.c > $@.tmp && \
	mv $@.tmp $@

common.h: $(srcdir)/common.c makelist
	sh ./makelist -h $(srcdir)/common.c > $@.tmp && \
	mv $@.tmp $@

help.c: ${ASRC} makelist
	sh ./makelist -bc ${ASRC} > $@.tmp && \
	mv $@.tmp $@

help.h: ${ASRC} makelist
	sh ./makelist -bh ${ASRC} > $@.tmp && \
	mv $@.tmp $@

fcns.h: ${AHDR} makelist
	sh ./makelist -fh ${AHDR} > $@.tmp && \
	mv $@.tmp $@

fcns.c: ${AHDR} fcns.h makelist
	sh ./makelist -fc ${AHDR} > $@.tmp && \
	mv $@.tmp $@

#%.o: vi.h emacs.h common.h help.h fcns.h
#objects := $(patsubst %.c,%.o,$(wildcard *.c))
#$(objects):  vi.h emacs.h

chared.o: vi.h emacs.h common.h help.h fcns.h
el.o: vi.h emacs.h common.h help.h fcns.h
history.o: vi.h emacs.h common.h help.h fcns.h 
map.o: vi.h emacs.h common.h help.h fcns.h
prompt.o: vi.h emacs.h common.h help.h fcns.h
readline.o: vi.h emacs.h common.h help.h fcns.h
search.o: vi.h emacs.h common.h help.h fcns.h
tokenizer.o: vi.h emacs.h common.h help.h fcns.h
vi.o: vi.h emacs.h common.h help.h fcns.h
common.o: vi.h emacs.h common.h help.h fcns.h
emacs.o: vi.h emacs.h common.h help.h fcns.h 
hist.o: vi.h emacs.h common.h help.h fcns.h 
key.o: vi.h emacs.h common.h help.h fcns.h 
parse.o: vi.h emacs.h common.h help.h fcns.h
read.o: vi.h emacs.h common.h help.h fcns.h
refresh.o: vi.h emacs.h common.h help.h fcns.h
sig.o: vi.h emacs.h common.h help.h fcns.h
term.o: vi.h emacs.h common.h help.h fcns.h
tty.o: vi.h emacs.h common.h help.h fcns.h 
help.o: vi.h emacs.h common.h help.h fcns.h
fcns.o: vi.h emacs.h common.h help.h fcns.h

# Don't update the files from bitkeeper
%::SCCS/s.%
