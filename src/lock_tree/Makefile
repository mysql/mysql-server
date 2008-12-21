# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../../
INCLUDEDIRS=-I. -I$(TOKUROOT)newbrt -I../range_tree -I$(TOKUROOT)src
DEPEND_COMPILE+= \
	*.h \
	../range_tree/*.h \
#end

SKIP_LOCKTREERULE=1
include $(TOKUROOT)include/Makefile.include

LOCKTREE         = locktree.$(AEXT)
LOCKTREE_LINEAR  = locktree_linear.$(AEXT)
LOCKTREE_TLOG    = locktree_tlog.$(AEXT)
LOCKTREE_LOG     = locktree_log.$(AEXT)

OBJS_RAW = \
	db_id \
	idlth \
	lth \
	rth \
#end

LIBRARIES=$(LOCKTREE_LINEAR) $(LOCKTREE_TLOG) $(LOCKTREE) # $(LOCKTREE_LOG)

$(LOCKTREE_TLOG):                   $(OBJS) locktree_nooverlap.$(OEXT)
$(LOCKTREE_LOG) $(LOCKTREE_LINEAR): $(OBJS) locktree.$(OEXT)

$(LOCKTREE): $(LOCKTREE_TLOG)
	cp $< $@

.PHONY: build check
build: $(LIBRARIES)

locktree_nooverlap.$(OEXT): locktree.c $(DEPEND_COMPILE)
	$(CC) $< -DTOKU_RT_NOOVERLAPS $(O_FROM_C_FLAGS)

build check clean:
	cd tests;$(MAKE) $@

