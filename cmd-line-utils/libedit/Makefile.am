## Process this file with automake to create Makefile.in

ASRC =			$(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c
AHDR =			vi.h emacs.h common.h

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include

noinst_LIBRARIES =	libedit.a

libedit_a_SOURCES =	chared.c el.c eln.c history.c historyn.c map.c prompt.c chartype.c \
			readline.c search.c tokenizer.c tokenizern.c vi.c common.c emacs.c \
			hist.c keymacro.c parse.c read.c refresh.c sig.c terminal.c \
			tty.c help.c fcns.c filecomplete.c \
			np/unvis.c np/strlcpy.c np/vis.c np/strlcat.c \
			np/fgetln.c np/wcsdup.c

libedit_a_LIBADD = 		@LIBEDIT_LOBJECTS@
libedit_a_DEPENDENCIES 	= 	@LIBEDIT_LOBJECTS@

pkginclude_HEADERS =	readline/readline.h

noinst_HEADERS =	chared.h chartype.h el.h el_terminal.h histedit.h keymacro.h parse.h \
			refresh.h sig.h sys.h config.h hist.h map.h prompt.h read.h \
			search.h tty.h filecomplete.h np/vis.h

EXTRA_DIST = makelist.sh

CLEANFILES = makelist common.h emacs.h vi.h fcns.h help.h fcns.c help.c

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
eln.o: vi.h emacs.h common.h help.h fcns.h
history.o: vi.h emacs.h common.h help.h fcns.h 
historyn.o: vi.h emacs.h common.h help.h fcns.h 
map.o: vi.h emacs.h common.h help.h fcns.h
chartype.o: vi.h emacs.h common.h help.h fcns.h
prompt.o: vi.h emacs.h common.h help.h fcns.h
readline.o: vi.h emacs.h common.h help.h fcns.h
search.o: vi.h emacs.h common.h help.h fcns.h
tokenizer.o: vi.h emacs.h common.h help.h fcns.h
tokenizern.o: vi.h emacs.h common.h help.h fcns.h
vi.o: vi.h emacs.h common.h help.h fcns.h
common.o: vi.h emacs.h common.h help.h fcns.h
emacs.o: vi.h emacs.h common.h help.h fcns.h 
hist.o: vi.h emacs.h common.h help.h fcns.h 
keymacro.o: vi.h emacs.h common.h help.h fcns.h 
parse.o: vi.h emacs.h common.h help.h fcns.h
read.o: vi.h emacs.h common.h help.h fcns.h
refresh.o: vi.h emacs.h common.h help.h fcns.h
sig.o: vi.h emacs.h common.h help.h fcns.h
terminal.o: vi.h emacs.h common.h help.h fcns.h
tty.o: vi.h emacs.h common.h help.h fcns.h 
help.o: vi.h emacs.h common.h help.h fcns.h
fcns.o: vi.h emacs.h common.h help.h fcns.h
filecomplete.o: vi.h emacs.h common.h help.h fcns.h
