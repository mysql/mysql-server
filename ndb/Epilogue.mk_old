# .KEEP_STATE:
# bk test !!!

###
# For building some intermediary targets in /tmp (only useful on solaris)
ifneq ($(NDB_BUILDROOT),)
NDB_TOPABS := $(shell cd $(NDB_TOP) && /bin/pwd)
NDB_BUILDDIR := $(subst $(NDB_TOPABS),$(NDB_BUILDROOT),$(CURDIR))/
ifeq ($(wildcard $(NDB_BUILDDIR)),)
dummy := $(shell mkdir -p $(NDB_BUILDDIR))
endif
endif

###
CCFLAGS_TOP += -DNDB_$(NDB_OS) -DNDB_$(NDB_ARCH) -DNDB_$(NDB_COMPILER)

ifdef BIN_TARGET
BIN_EXE = Y
endif

###
#
# OS specifics
#

# Disable shared libraries on HP-UX for the time being.
ifeq ($(NDB_OS), HPUX)
	SO_LIB := N
	PIC_LIB := N
	PIC_ARCHIVE := N
	NONPIC_ARCHIVE := Y
endif

ifeq ($(NDB_OS), OSE)
	SO_LIB := N
	PIC_LIB := N
	PIC_ARCHIVE := N
	NONPIC_ARCHIVE := Y

ifdef BIN_TARGET
	BIN_LIB_TARGET := lib$(BIN_TARGET).a
	BIN_TARGET := lib$(BIN_TARGET).a
endif
endif

ifeq ($(NDB_OS), SOFTOSE)
	SO_LIB := N
	PIC_LIB := N
	PIC_ARCHIVE := N

ifdef BIN_TARGET
	BIN_EXE_TARGET := $(BIN_TARGET)
	BIN_LIB_TARGET := lib$(BIN_TARGET).a
	EXTRA_MAIN := osemain.o
endif
endif

ifeq ($(filter OSE, $(NDB_OS)),)
	BIN_EXE_TARGET := $(BIN_TARGET)
endif


ifeq ($(NDB_OS), MACOSX)
.LIBPATTERNS= lib%.dylib lib%.a
endif

###
#
#

###
# External dependencies definition : the place we store libraries 
# we get from outside the NDB development group. 
EXTERNAL_DEPENDS_TOP=$(NDB_TOP)/src/external/$(NDB_OS).$(NDB_ARCH)


###
#
# TYPE Handling

#
# TYPE := kernel
# 
ifneq ($(filter kernel, $(TYPE)),)
CCFLAGS_LOC += \
        -I$(call fixpath,$(NDB_TOP)/src/kernel/vm) \
        -I$(call fixpath,$(NDB_TOP)/src/kernel/error) \
	-I$(call fixpath,$(NDB_TOP)/src/kernel) \
	-I$(call fixpath,$(NDB_TOP)/include/kernel) \
	-I$(call fixpath,$(NDB_TOP)/include/transporter) \
	-I$(call fixpath,$(NDB_TOP)/include/debugger) \
	-I$(call fixpath,$(NDB_TOP)/include/mgmcommon) \
	-I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
        -I$(call fixpath,$(NDB_TOP)/include/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/include/util) \
	-I$(call fixpath,$(NDB_TOP)/include/portlib) \
	-I$(call fixpath,$(NDB_TOP)/include/logger)
endif

#
# TYPE := ndbapi
#
ifneq ($(filter ndbapi, $(TYPE)),)
CCFLAGS_LOC += \
	-I$(call fixpath,$(NDB_TOP)/include/kernel) \
	-I$(call fixpath,$(NDB_TOP)/include/transporter) \
	-I$(call fixpath,$(NDB_TOP)/include/debugger) \
	-I$(call fixpath,$(NDB_TOP)/include/mgmcommon) \
	-I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
        -I$(call fixpath,$(NDB_TOP)/include/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/include/util) \
	-I$(call fixpath,$(NDB_TOP)/include/portlib) \
	-I$(call fixpath,$(NDB_TOP)/include/logger)
endif

#
# TYPE := ndbapiclient
#
ifneq ($(filter ndbapiclient, $(TYPE)),)
CCFLAGS_LOC += \
        -I$(call fixpath,$(NDB_TOP)/include/ndbapi)

BIN_TARGET_LIBS  += NDB_API
endif

#
# TYPE := mgmapiclient
#
ifneq ($(filter mgmapiclient, $(TYPE)),)
CCFLAGS_LOC += \
        -I$(call fixpath,$(NDB_TOP)/include/mgmapi)

BIN_TARGET_LIBS += MGM_API
endif

#
# TYPE := ndbapitest
#
ifneq ($(filter ndbapitest, $(TYPE)),)
CCFLAGS_LOC += \
        -I$(call fixpath,$(NDB_TOP)/include/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/include/util) \
	-I$(call fixpath,$(NDB_TOP)/include/portlib) \
        -I$(call fixpath,$(NDB_TOP)/test/include) \
	-I$(call fixpath,$(NDB_TOP)/include/mgmapi)

BIN_TARGET_LIBS += NDBT
LDFLAGS_LOC += -lNDB_API -lMGM_API -lm

endif

#
# TYPE := signalsender 
#
ifneq ($(filter signalsender, $(TYPE)),)
CCFLAGS_LOC += \
        -I$(call fixpath,$(NDB_TOP)/include/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/src/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/src/ndbapi/signal-sender) \
        -I$(call fixpath,$(NDB_TOP)/include/util) \
	-I$(call fixpath,$(NDB_TOP)/include/portlib) \
        -I$(call fixpath,$(NDB_TOP)/include/transporter) \
        -I$(call fixpath,$(NDB_TOP)/include/mgmcommon) \
        -I$(call fixpath,$(NDB_TOP)/include/kernel)

BIN_TARGET_LIBS += NDB_API
BIN_TARGET_ARCHIVES += editline signal-sender

endif


#
# TYPE := repserver
#
ifneq ($(filter repserver, $(TYPE)),)
CCFLAGS_LOC += \
        -I$(call fixpath,$(NDB_TOP)/include/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/src) \
        -I$(call fixpath,$(NDB_TOP)/src/ndbapi) \
        -I$(call fixpath,$(NDB_TOP)/src/ndbapi/signal-sender) \
        -I$(call fixpath,$(NDB_TOP)/include/util) \
	-I$(call fixpath,$(NDB_TOP)/include/portlib) \
        -I$(call fixpath,$(NDB_TOP)/include/transporter) \
        -I$(call fixpath,$(NDB_TOP)/include/mgmcommon) \
        -I$(call fixpath,$(NDB_TOP)/include/kernel)
endif

#
# TYPE := odbcclient
#

ifneq ($(filter odbcclient, $(TYPE)),)
TYPE += util
LDFLAGS_LOC += -lm
#ifneq ($(call check-odbc),)
ifneq ($(NDB_ODBC),N)
ifeq ($(NDB_OS), SOLARIS)
CCFLAGS_LOC += -I/usr/local/include
BIN_TARGET_LIBS_DIRS += /usr/local/lib
BIN_TARGET_LIBS += odbc odbcinst NDBT
endif
ifeq ($(NDB_OS), LINUX)
BIN_TARGET_LIBS += odbc odbcinst NDBT
endif
ifeq ($(NDB_OS), MACOSX)
BIN_TARGET_LIBS += odbc odbcinst NDBT
endif
ifeq ($(NDB_OS), IBMAIX)
BIN_TARGET_LIBS += odbc odbcinst NDBT
endif
ifeq ($(NDB_OS), TRU64X)
BIN_TARGET_LIBS += odbc odbcinst NDBT
endif
else
BIN_EXE = N
endif
endif

#
# TYPE := *
#
#
# TYPE := util
#
ifneq ($(filter util, $(TYPE)),)
CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include/util) \
               -I$(call fixpath,$(NDB_TOP)/include/portlib) \
               -I$(call fixpath,$(NDB_TOP)/include/logger)
BIN_TARGET_LIBS += logger general portlib
endif

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include) -I$(call fixpath,$(NDB_TOP)/../include)

ifeq ($(NDB_SCI), Y)
BIN_TARGET_LIBS += sisci
BIN_TARGET_LIBS_DIRS += $(EXTERNAL_DEPENDS_TOP)/sci/lib

CCFLAGS_LOC += -I$(call fixpath,$(EXTERNAL_DEPENDS_TOP)/sci/include)
endif

#
# TYPE Handling
###

###
#
# First rule
#
first:
	$(MAKE) libs
	$(MAKE) bins

ifeq ($(findstring all,$(replace-targets)),)
all: first
endif

###
#
# Nice to have rules
api:	libs
	$(MAKE) -C $(NDB_TOP)/src/ndbapi bins

mgm:	libs
	$(MAKE) -C $(NDB_TOP)/src/mgmsrv bins

ndb:	libs
	$(MAKE) -C $(NDB_TOP)/src/kernel/ndb-main bins

apitest:	first
	$(MAKE) -C $(NDB_TOP)/test/ndbapi all

#-lNDBT:
#	$(MAKE) -C $(NDB_TOP)/test/src all
#
#-lNDB_API:	libs
#	$(MAKE) -C $(NDB_TOP)/src/ndbapi bins	

#
# Libs/Bins
#
ifdef PREREQ_LOC
_libs:: $(PREREQ_LOC)
_bins:: $(PREREQ_LOC)
endif

L_DIRS := $(LIB_DIRS) $(DIRS)
B_DIRS := $(BIN_DIRS) $(DIRS)
A_DIRS := $(LIB_DIRS) $(BIN_DIRS) $(DIRS)

_libs::

_bins::

libs:	_libs $(patsubst %, _libs_%, $(L_DIRS))
$(patsubst %, _libs_%, $(L_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _libs_%,%,$@) libs

bins:	_bins $(patsubst %, _bins_%, $(B_DIRS))
$(patsubst %, _bins_%, $(B_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _bins_%,%,$@) bins

###
#
# Links
_links:
	-$(NDB_TOP)/tools/make-links.sh $(NDB_TOP)/include `pwd`

links:	_links $(patsubst %, _links_%, $(A_DIRS))
$(patsubst %, _links_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _links_%,%,$@) links


####
#
# OSE build_spec (
ifdef SOURCES
BS := Y
endif

ifdef SOURCES_c
BS := Y
endif

_build_spec: Makefile
ifdef BS
	@echo "TYPE = SWU"                > build.spec
	@echo "include $(NDB_TOP)/Ndb.mk" >> build.spec
#	@for i in $(CCFLAGS_LOC); do echo "INC += $$i" >> build.spec ; done
	@for i in $(patsubst -I%, %, $(CCFLAGS_LOC)); do echo "INC += $$i" >> build.spec ; done
	@echo "INC += /vobs/cello/cls/rtosi_if/include" >> build.spec
	@echo "INC += /vobs/cello/cls/rtosi_if/include.@@@" >> build.spec
	@echo "INC += /vobs/cello/cls/rtosi_if/include.<<<" >> build.spec
endif

build_spec: _build_spec $(patsubst %, _build_spec_%, $(A_DIRS))
$(patsubst %, _build_spec_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _build_spec_%,%,$@) build_spec

###
#
# Phony targets

.PHONY:	$(A_DIRS)

###
#
# Dummy rule

DUMMY:

###
#
# Definitions of...

PIC_DIR     := $(NDB_BUILDDIR).pic
A_TMP_DIR   := $(NDB_BUILDDIR).a_tmp
SO_TMP_DIR  := $(NDB_BUILDDIR).so_tmp
PIC_TMP_DIR := $(NDB_BUILDDIR).pic_tmp

$(PIC_DIR):
	mkdir -p $(PIC_DIR)

SRC_C   := $(filter %.C, $(SOURCES))
SRC_CPP := $(filter %.cpp, $(SOURCES))
SRC_CC  := $(filter %.cc, $(SOURCES))
SRC_c   := $(filter %.c, $(SOURCES)) $(filter %.c, $(SOURCES.c))
SRC_YPP := $(filter %.ypp, $(SOURCES))
SRC_LPP	:= $(filter %.lpp, $(SOURCES))

OBJECTS := $(SRC_C:%.C=%.$(OBJEXT)) \
           $(SRC_CPP:%.cpp=%.$(OBJEXT)) \
           $(SRC_CC:%.cc=%.$(OBJEXT)) \
           $(SRC_c:%.c=%.$(OBJEXT)) \
	   $(SRC_YPP:%.ypp=%.tab.$(OBJEXT)) \
	   $(SRC_LPP:%.lpp=%.yy.$(OBJEXT)) \
	   $(OBJECTS_LOC)

PIC_OBJS := $(OBJECTS:%=$(PIC_DIR)/%)

LIB_DIR := $(NDB_TOP)/lib
BIN_DIR := $(NDB_TOP)/bin

###
#
# ARCHIVE_TARGET
#
ifdef ARCHIVE_TARGET

ifndef NONPIC_ARCHIVE
NONPIC_ARCHIVE := Y
endif

ifeq ($(NONPIC_ARCHIVE), Y)
_libs::	$(LIB_DIR)/$(LIBPREFIX)$(ARCHIVE_TARGET).$(LIBEXT)
$(LIB_DIR)/$(LIBPREFIX)$(ARCHIVE_TARGET).$(LIBEXT) : $(OBJECTS)
	$(call ar_rcs,$@,$(OBJECTS))

endif # NONPIC_ARCHIVE := Y

ifeq ($(PIC_ARCHIVE), Y)
_libs::	$(PIC_DIR) $(LIB_DIR)/$(LIBPREFIX)$(ARCHIVE_TARGET)_pic.$(LIBEXT)
$(LIB_DIR)/$(LIBPREFIX)$(ARCHIVE_TARGET)_pic.$(LIBEXT) : $(PIC_OBJS)
	cd $(PIC_DIR) && $(call ar_rcs,../$@,$(OBJECTS))

PIC_DEP := Y

endif # PIC_ARCHIVE := Y

endif # ARCHIVE_TARGET

###
#
# LIB_TARGET
#
ifdef LIB_TARGET

ifeq ($(A_LIB), Y)

A_LIB_ARCHIVES := $(LIB_TARGET_ARCHIVES:%=$(LIB_DIR)/$(LIBPREFIX)%.$(LIBEXT))

_bins::	$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(LIBEXT)
$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(LIBEXT) : $(A_LIB_ARCHIVES)
	@rm -rf $(A_TMP_DIR) && mkdir $(A_TMP_DIR)
	cd $(A_TMP_DIR) && for i in $^; do ar -x ../$$i; done && $(call ar_rcs,../$@,*.$(OBJEXT))
	$(NDB_TOP)/home/bin/ndb_deploy $@
endif # A_LIB := Y

ifeq ($(SO_LIB), Y)
ifneq ($(NDB_OS), WIN32)
SO_LIB_ARCHIVES := $(LIB_TARGET_ARCHIVES:%=$(LIB_DIR)/$(LIBPREFIX)%_pic.$(LIBEXT))

_bins::	$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(SHLIBEXT)
$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(SHLIBEXT) : $(SO_LIB_ARCHIVES)
	@rm -rf $(SO_TMP_DIR) && mkdir $(SO_TMP_DIR)
	cd $(SO_TMP_DIR) && for i in $^; do ar -x ../$$i; done
ifneq ($(NDB_OS), MACOSX)
	$(SO) $@.new $(SO_TMP_DIR)/*.$(OBJEXT) -L$(LIB_DIR) $(LIB_TARGET_LIBS) $(LDFLAGS_LAST)
	rm -f $@; mv $@.new $@
else
	$(SO) $@ $(SO_TMP_DIR)/*.$(OBJEXT) -L$(LIB_DIR) $(LIB_TARGET_LIBS) $(LDFLAGS_LAST)
endif
ifeq ($(NDB_VERSION), RELEASE)
ifneq ($(NDB_OS), MACOSX)
	strip $@
endif
endif
	$(NDB_TOP)/home/bin/ndb_deploy $@
else # WIN32
SO_LIB_ARCHIVES := $(LIB_TARGET_ARCHIVES:%=$(LIB_DIR)/$(LIBPREFIX)%_pic.$(LIBEXT))

_bins::	$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(SHLIBEXT)
$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(SHLIBEXT) : $(SO_LIB_ARCHIVES)
	@rm -rf $(SO_TMP_DIR) && mkdir $(SO_TMP_DIR)
	cd $(SO_TMP_DIR) && for i in $^; do ar -x ../$$i; done
	$(call link_so,$@.new,$(SO_TMP_DIR)/*.$(OBJEXT))
	rm -f $@; mv $@.new $@
#ifeq ($(NDB_VERSION), RELEASE)
#	strip $@
#endif

endif
endif # SO_LIB := Y

ifeq ($(PIC_LIB), Y)

PIC_LIB_ARCHIVES := $(LIB_TARGET_ARCHIVES:%=$(LIB_DIR)/$(LIBPREFIX)%_pic.$(LIBEXT))

_bins::	$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET)_pic.$(LIBEXT)
$(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET)_pic.$(LIBEXT) : $(PIC_LIB_ARCHIVES)
	@rm -rf $(PIC_TMP_DIR) && mkdir $(PIC_TMP_DIR)
	cd $(PIC_TMP_DIR) && for i in $^; do ar -x ../$$i; done && $(call ar_rcs,../$@,*.$(OBJEXT))

endif # PIC_LIB := Y

endif # LIB_TARGET

###
#
# BIN_TARGET
#
ifeq ($(BIN_EXE), Y)
ifneq ($(NDB_OS), WIN32)
BIN_LIBS := $(BIN_TARGET_ARCHIVES:%=$(LIB_DIR)/$(LIBPREFIX)%.$(LIBEXT)) 
BIN_LIBS += $(BIN_TARGET_LIBS:%=-l%) 

BIN_DEPS := $(OBJECTS) $(EXTRA_MAIN) $(BIN_LIBS)
BIN_LIB_DIRS := $(BIN_TARGET_LIBS_DIRS:%=-L%)

BIN_FLAGS := $(BIN_LIB_DIRS) $(BIN_DEPS)

VPATH := $(LIB_DIR) $(BIN_TARGET_LIBS_DIRS)
_bins::	$(BIN_DIR)/$(BIN_TARGET)
$(BIN_DIR)/$(BIN_TARGET) : $(BIN_DEPS)
	$(LINK.cc) $(LDFLAGS) $(LDLIBS) -L$(LIB_DIR) $(BIN_FLAGS) -o $@.new $(LDFLAGS_LAST)
	rm -f $@; mv $@.new $@
ifeq ($(NDB_VERSION), RELEASE)
ifneq ($(NDB_OS), MACOSX)
	strip $@
endif
endif
	$(NDB_TOP)/home/bin/ndb_deploy $@
else # WIN32
BIN_LIBS := $(foreach lib,$(BIN_TARGET_ARCHIVES),$(call fixpath,$(LIB_DIR)/$(LIBPREFIX)$(lib).$(LIBEXT)))
BIN_LIBS += $(BIN_TARGET_LIBS:%=$(LIBPREFIX)%.$(LIBEXT)) 

BIN_DEPS := $(OBJECTS) $(BIN_TARGET_ARCHIVES:%=$(LIB_DIR)/$(LIBPREFIX)%.$(LIBEXT)) 
BIN_LIB_DIRS := -libpath:$(call fixpath,$(LIB_DIR)) $(BIN_TARGET_LIBS_DIRS:%=-libpath:%)

BIN_FLAGS := $(BIN_LIB_DIRS)

VPATH := $(LIB_DIR) $(BIN_TARGET_LIBS_DIRS)
_bins::	$(BIN_DIR)/$(BIN_TARGET).exe
$(BIN_DIR)/$(BIN_TARGET).exe : $(BIN_DEPS)
	$(LINK.cc) -out:$(call fixpath,$@.new) $(OBJECTS) $(BIN_FLAGS) $(BIN_LIBS)
	rm -f $@; mv $@.new $@
ifeq ($(NDB_VERSION), RELEASE)
	strip $@
endif

endif
endif

###
#
# SOURCES.sh
#
ifdef SOURCES.sh

BIN_SRC := $(SOURCES.sh:%=$(BIN_DIR)/%)

_bins::	$(BIN_SRC)

$(BIN_SRC) : $(SOURCES.sh)
	rm -f $(^:%=$(BIN_DIR)/%)
	cp $^ $(BIN_DIR)
endif

#
# Compile rules PIC objects
#
ifeq ($(NDB_OS), WIN32)
OUT := -Fo
else
OUT := -o 
endif

$(PIC_DIR)/%.$(OBJEXT): %.C
	$(C++) $(OUT)$@ -c $(CCFLAGS) $(CFLAGS_$<) $(PIC) $<

$(PIC_DIR)/%.$(OBJEXT): %.cpp
	$(C++) $(OUT)$@ -c $(CCFLAGS) $(CFLAGS_$<) $(PIC) $<

$(PIC_DIR)/%.$(OBJEXT): %.cc
	$(C++) $(OUT)$@ -c $(CCFLAGS) $(CFLAGS_$<) $(PIC) $<

$(PIC_DIR)/%.$(OBJEXT): %.c
	$(CC)  $(OUT)$@ -c $(CFLAGS)  $(CFLAGS_$<) $(PIC) $<

#
# Compile rules
#
%.$(OBJEXT) : %.cpp
	$(C++) $(OUT)$@ -c $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

%.$(OBJEXT) : %.C
	$(C++) $(OUT)$@ -c $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

%.$(OBJEXT) : %.cc
	$(C++) $(OUT)$@ -c $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

%.$(OBJEXT) : %.c
	$(CC)  $(OUT)$@ -c $(CFLAGS)  $(CFLAGS_$<) $(NON_PIC) $<

%.s : %.C
	$(C++) -S $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

%.s : %.cpp
	$(C++) -S $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

%.s : %.cc
	$(C++) -S $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

%.s : %.c
	$(CC) -S $(CCFLAGS) $(CFLAGS_$<) $(NON_PIC) $<

BISON = bison
BISONHACK = :
%.tab.cpp %.tab.hpp : %.ypp
	$(BISON) $<
	$(BISONHACK) $*.tab.cpp $*.tab.hpp

FLEX = flex
FLEXHACK = :
%.yy.cpp : %.lpp
	$(FLEX) -o$@ $<
	$(FLEXHACK) $@

###
#
# Defines regarding dependencies

DEPMK	:= $(NDB_BUILDDIR).depend.mk

DEPDIR  := $(NDB_BUILDDIR).depend

DEPENDENCIES := $(SRC_C:%.C=$(DEPDIR)/%.d) \
                $(SRC_CC:%.cc=$(DEPDIR)/%.d) \
                $(SRC_CPP:%.cpp=$(DEPDIR)/%.d) \
                $(SRC_c:%.c=$(DEPDIR)/%.d) \
		$(SRC_YPP:%.ypp=$(DEPDIR)/%.tab.d) \
		$(SRC_LPP:%.lpp=$(DEPDIR)/%.yy.d)

###
#
# Dependency rule

_depend: $(DEPMK)

depend: _depend $(patsubst %, _depend_%, $(A_DIRS))

$(patsubst %, _depend_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _depend_%,%,$@) depend

###
#
# Clean dependencies

_clean_dep:
	-rm -rf $(DEPMK) $(DEPDIR)/*

clean_dep: _clean_dep $(patsubst %, _clean_dep_%, $(A_DIRS))

$(patsubst %, _clean_dep_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _clean_dep_%,%,$@) clean_dep

###
# 
# Generate dependencies

$(DEPDIR):
	-@mkdir -p $(DEPDIR)

$(DEPDIR)/%.d: %.C
	@echo Generating depend for $<
	@$(MAKEDEPEND) $(CCFLAGS) $(CFLAGS_$<) $< >$@

$(DEPDIR)/%.d: %.c
	@echo Generating depend for $<
	@$(MAKEDEPEND) $(CCFLAGS) $(CFLAGS_$<) $< >$@

$(DEPDIR)/%.d: %.cpp
	@echo Generating depend for $<
	@$(MAKEDEPEND) $(CCFLAGS) $(CFLAGS_$<) $< >$@

$(DEPDIR)/%.d: %.cc
	@echo Generating depend for $<
	@$(MAKEDEPEND) $(CCFLAGS) $(CFLAGS_$<) $< >$@

ifeq ($(NDB_OS), WIN32)
ifndef PIC_DEP
DEP_PTN := -e 's/\(.*\)\.o[ :]*/\1.$(OBJEXT) $(DEPDIR)\/\1.d : /g'
else
DEP_PTN := -e 's/\(.*\)\.o[ :]*/\1.$(OBJEXT) $(PIC_DIR)\/\1.$(OBJEXT) $(DEPDIR)\/\1.d : /g'
endif
else
ifndef PIC_DEP
DEP_PTN := -e 's!\(.*\)\.$(OBJEXT)[ :]*!\1.$(OBJEXT) $(DEPDIR)\/\1.d : !g'
else
DEP_PTN := -e 's!\(.*\)\.$(OBJEXT)[ :]*!\1.$(OBJEXT) $(PIC_DIR)\/\1.$(OBJEXT) $(DEPDIR)\/\1.d : !g'
endif
endif
#DEP_PTN += -e 's!/usr/include/[-+a-zA-Z0-9_/.]*!!g'
#DEP_PTN += -e 's!/usr/local/lib/gcc-lib/[-+a-zA-Z0-9_/.]*!!g'

$(DEPMK): $(DEPDIR) $(SRC_YPP:%.ypp=%.tab.hpp) $(SRC_LPP:%.lpp=%.yy.cpp) $(DEPENDENCIES) $(wildcard $(NDB_TOP)/.update.d)
	@echo "updating .depend.mk"
	@sed $(DEP_PTN) /dev/null $(DEPENDENCIES) >$(DEPMK)

###
#
# clean
#
_clean:
	-rm -rf SunWS_cache $(PIC_DIR)/SunWS_cache
ifeq ($(NONPIC_ARCHIVE), Y)
	-rm -f $(OBJECTS) $(LIB_DIR)/$(LIBPREFIX)$(ARCHIVE_TARGET).$(LIBEXT)
endif
ifeq ($(PIC_ARCHIVE), Y)
	-rm -f $(PIC_OBJS) $(LIB_DIR)/$(LIBPREFIX)$(ARCHIVE_TARGET)_pic.$(LIBEXT)
endif
ifdef BIN_TARGET
	-rm -f $(OBJECTS)
endif
ifdef LIB_TARGET
ifeq ($(A_LIB), Y)
	-rm -f $(A_TMP_DIR)/*
endif
ifeq ($(SO_LIB), Y)
	-rm -f $(SO_TMP_DIR)/*
endif
ifeq ($(PIC_LIB), Y)
	-rm -f $(PIC_TMP_DIR)/*
endif
endif
ifneq ($(SRC_YPP),)
	-rm -f $(SRC_YPP:%.ypp=%.tab.[hc]pp) $(SRC_YPP:%.ypp=%.output)
endif
ifneq ($(SRC_LPP),)
	-rm -f $(SRC_LPP:%.lpp=%.yy.*)
endif
ifdef CLEAN_LOC
	-rm -f $(CLEAN_LOC)
endif

###
#
# clean all
#
clobber: cleanall
_cleanall: _clean clean_links
	-rm -f osemain.con osemain.c
ifdef LIB_TARGET
ifeq ($(A_LIB), Y)
	-rm -f $(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(LIBEXT)
endif
ifeq ($(SO_LIB), Y)
	-rm -f $(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET).$(SHLIBEXT)
endif
ifeq ($(PIC_LIB), Y)
	-rm -f $(LIB_DIR)/$(LIBPREFIX)$(LIB_TARGET)_pic.$(LIBEXT)
endif
endif
ifdef BIN_TARGET
	-rm -f $(BIN_DIR)/$(BIN_TARGET)
endif

clean_links:

###
#
# Dist clean
#
_distclean: _tidy
	rm -rf $(DEPDIR) $(PIC_DIR) $(PIC_TMP_DIR) $(SO_TMP_DIR) $(A_TMP_DIR) Sources build.spec

###
#
# tidy
#
_tidy: _cleanall _clean_dep
	-rm -f *~ *.$(OBJEXT) *.$(LIBEXT) *.${SHLIBEXT} 

#
# clean cleanall tidy - recursion
#
ifeq ($(findstring clean,$(replace-targets)),)
clean: _clean $(patsubst %, _clean_%, $(A_DIRS))
endif

$(patsubst %, _clean_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _clean_%,%,$@) clean

cleanall: _cleanall $(patsubst %, _cleanall_%, $(A_DIRS))

$(patsubst %, _cleanall_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _cleanall_%,%,$@) cleanall

tidy: _tidy $(patsubst %, _tidy_%, $(A_DIRS))

$(patsubst %, _tidy_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _tidy_%,%,$@) tidy

distclean: _distclean $(patsubst %, _distclean_%, $(A_DIRS))

$(patsubst %, _distclean_%, $(A_DIRS)) : DUMMY
	$(MAKE) -C $(patsubst _distclean_%,%,$@) distclean

###
#
# Guess configuration

$(NDB_TOP)/config/config.mk: $(NDB_TOP)/config/GuessConfig.sh
	$(NDB_TOP)/config/GuessConfig.sh -D

$(NDB_TOP)/config/Defs....mk: $(NDB_TOP)/config/config.mk
$(NDB_TOP)/config/Defs..mk: $(NDB_TOP)/config/config.mk

###
# Soft ose envirment stuff
#
osemain.con: $(NDB_TOP)/src/env/softose/osemain_con.org
	cp $< $@
	echo "PRI_PROC(init_$(BIN_TARGET), init_$(BIN_TARGET), 65535, 3, ndb, 0, NULL)" >> $@

osemain.c: $(OSE_LOC)/sfk-solaris2/krn-solaris2/src/osemain.c
	ln -s $< $@

osemain.o : osemain.con

$(DEPDIR)/osemain.d : osemain.con

###
#
# These target dont want dependencies

NO_DEP=clean clobber cleanall tidy clean_dep $(DEPDIR) build_spec \
       $(NDB_TOP)/config/config.mk distclean osemain.con osemain.c

ifeq ($(filter $(NO_DEP), $(MAKECMDGOALS)),)
ifneq ($(strip $(DEPENDENCIES)),)
    include $(DEPMK)
endif
endif

###
#
# Auxiliary targets

sources: Sources

Sources: Makefile
	@rm -f $@
	@for f in Makefile $(A_DIRS) $(SOURCES) $(SOURCES.c); do echo $$f; done >$@

###
#
# TAG generation for emacs and vi folks
#
# In emacs "Esc- ." or "M- ." to find a symbol location
# In vi use the :\tag command 
# by convention:
#	TAGS is used with emacs 
#	tags is used with vi
#
# Hopefully the make is being done from  $(NDB_TOP)/src
# and your TAGS/tags file then is in the same directory.

TAGS: DUMMY
	rm -f TAGS	
	find $(NDB_TOP) -name "*.[ch]"   | xargs $(ETAGS) --append
	find $(NDB_TOP) -name "*.[ch]pp" | xargs $(ETAGS) --append

tags: DUMMY
	rm -f tags	
	find $(NDB_TOP) -name "*.[ch]"   | xargs $(CTAGS) --append
	find $(NDB_TOP) -name "*.[ch]pp" | xargs $(CTAGS) --append

install:
        

ebrowse: DUMMY
	cd $(NDB_TOP); rm -f EBROWSE 
	cd $(NDB_TOP); find . -name "*.hpp" -or -name "*.cpp" -or -name "*.h" -or -name "*.c" > tmpfile~ 
	cd $(NDB_TOP); ebrowse --file tmpfile~ 
	cd $(NDB_TOP); rm -f tmpfile~

srcdir = $(NDB_TOP)
top_distdir = $(NDB_TOP)/..
mkinstalldirs := /bin/sh ../mkinstalldirs
distdir = $(top_distdir)/$(PACKAGE)-$(VERSION)/ndb

distdir:
	$(mkinstalldirs) $(distdir)
	@list='$(shell /bin/sh SrcDist.sh)'; for file in $$list; do \
	  if test -f $$file || test -d $$file; then d=.; else d=$(srcdir); fi; \
	  dir=`echo "$$file" | sed -e 's,/[^/]*$$,,'`; \
	  if test "$$dir" != "$$file" && test "$$dir" != "."; then \
	    dir="/$$dir"; \
	    $(mkinstalldirs) "$(distdir)$$dir"; \
	  else \
	    dir=''; \
	  fi; \
	  if test -f $$d/$$file; then \
	    test -f $(distdir)/$$file \
	    || cp -p $$d/$$file $(distdir)/$$file \
	    || exit 1; \
	  fi; \
	done
