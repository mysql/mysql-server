# -*- Mode: Makefile -*-

.DEFAULT_GOAL= build
TOKUROOT=../../
INCLUDEDIRS=-I. -I$(TOKUROOT)newbrt
DEPEND_COMPILE+= \
	*.h \
#end

SKIP_RANGETREERULE=1

include $(TOKUROOT)toku_include/Makefile.include

RANGETREE        = rangetree.$(AEXT)
RANGETREE_LINEAR = rangetree_linear.$(AEXT)
RANGETREE_TLOG   = rangetree_tlog.$(AEXT)
RANGETREE_LOG    = rangetree_log.$(AEXT)


#This line is so we can have a \ at the end of the previous.

RANGETREE       =rangetree.$(AEXT)

LIBRARIES=$(RANGETREE_LINEAR) $(RANGETREE_TLOG) $(RANGETREE) # $(RANGETREE_LOG)

$(RANGETREE_TLOG):   log_nooverlap.$(OEXT)
$(RANGETREE_LINEAR): linear.$(OEXT)
$(RANGETREE_LOG):    log.$(OEXT)

$(RANGETREE_TLOG) $(RANGETREE_LINEAR) $(RANGETREE_LOG): CPPFLAGS+=-I$(TOKUROOT)include

$(RANGETREE): $(RANGETREE_TLOG)
	cp $< $@

.PHONY: build check
build: $(LIBRARIES)

build check clean:
	cd tests;$(MAKE) $@

