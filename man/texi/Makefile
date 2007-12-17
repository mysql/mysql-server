MANPAGES = tdb_create
MANPAGES_TEXI = $(patsubst %,%.texi,$(MANPAGES))
MANPAGES_POD = $(patsubst %,%.pod,$(MANPAGES))
MANPAGES_3   = $(patsubst %,%.3,$(MANPAGES))

SECTIONS = intro
SECTIONS_TEXI = $(patsubst %,%.texi,$(SECTIONS))

default: $(MANPAGES_3) tokudb.dvi

tokudb.dvi: tokudb.texi $(MANPAGES_TEXI) $(SECTIONS_TEXI)
	texi2dvi4a2ps tokudb.texi


$(MANPAGES_POD): everyman.texi
%.pod: %.texi
	perl texi2pod.pl $< > $@
%.3: %.pod
	pod2man --center "TokuDB Programmer's Manual" --section 3 $< > $@
