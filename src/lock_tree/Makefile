# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../../
INCLUDEDIRS=-I. -I$(TOKUROOT)newbrt -I../range_tree -I$(TOKUROOT)src
DEPEND_COMPILE+= \
	*.h \
	../range_tree/*.h \
#end

SKIP_LOCKTREERULE=1
include $(TOKUROOT)toku_include/Makefile.include

LOCKTREE         = locktree.$(AEXT)
LOCKTREE_LINEAR  = locktree_linear.$(AEXT)
LOCKTREE_TLOG    = locktree_tlog.$(AEXT)
LOCKTREE_LOG     = locktree_log.$(AEXT)

OBJS_RAW = \
	idlth \
	lth \
	rth \
	txnid_set \
	wfg \
#end

LIBRARIES=$(LOCKTREE_LINEAR) $(LOCKTREE_TLOG) $(LOCKTREE) # $(LOCKTREE_LOG)

$(LOCKTREE_TLOG):                   $(OBJS) locktree_nooverlap.$(OEXT)
$(LOCKTREE_LOG) $(LOCKTREE_LINEAR): $(OBJS) locktree.$(OEXT)

locktree.$(OEXT) locktree_nooverlap.$(OEXT) $(OBJS): CPPFLAGS+=-I$(TOKUROOT)include

$(LOCKTREE): $(LOCKTREE_TLOG)
	cp $< $@

.PHONY: build check
build: $(LIBRARIES)

locktree_nooverlap.$(OEXT): CPPFLAGS+=-DTOKU_RT_NOOVERLAPS
locktree_nooverlap.$(OEXT): locktree.c $(DEPEND_COMPILE)
	$(CC) $< -c $(CPPFLAGS) $(CFLAGS) $(OOUTPUT)$@

build check clean:
	cd tests;$(MAKE) $@
