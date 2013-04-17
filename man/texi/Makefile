# -*- Mode: Makefile -*-

.DEFAULT_GOAL=default
TOKUROOT=../../
include $(TOKUROOT)toku_include/Makefile.include

MANPAGES = tdb_create tdb_del tdb_put tdb_open tdb_log_max
MANPAGES_TEXI = $(patsubst %,%.texi,$(MANPAGES))
MANPAGES_POD = $(patsubst %,%.pod,$(MANPAGES))
MANPAGES_3   = $(patsubst %,%.3,$(MANPAGES))

SECTIONS = intro
SECTIONS_TEXI = $(patsubst %,%.texi,$(SECTIONS))

.PHONY: default

ifeq ($(OS_CHOICE),windows)
default: $(MANPAGES_3) ;
else
default: $(MANPAGES_3) tokudb.dvi;
endif

build: default;

tokudb.dvi: tokudb.texi $(MANPAGES_TEXI) $(SECTIONS_TEXI)
	LC_ALL=C texi2dvi4a2ps -c tokudb.texi


$(MANPAGES_POD): everyman.texi
%.pod: %.texi
	perl texi2pod.pl $< > $@
%.3: %.pod
	pod2man --center "TokuDB Programmer's Manual" --section 3 $< > $@
clean:
	rm -f tokudb.aux tokudb.dvi tokudb.cp tokudb.fn tokudb.ky tokudb.log tokudb.pg tokudb.tmp tokudb.toc tokudb.tp tokudb.vr
