###
#
# Defines
SHELL      := /bin/sh

C++        := g++ 
CC         := gcc
AR_RCS     := $(PURE) ar rcs 
SO         := g++ -shared -o

MAKEDEPEND := g++ -M
PIC        := -fPIC

###
#
# Flags
# 
NDB_STRDUP := Y
CCFLAGS_WARNINGS = -Wall -pedantic -Wno-sign-compare
CC_FLAGS_OSE = -DSPARC -DSIM -DOSE_DELTA -DMP 
CCFLAGS_TOP = $(CC_FLAGS_OSE) $(CC_FLAGS_WARNINGS) -DNDB_STRDUP

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -O3
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -O3 -g
else
VERSION_FLAGS += -g
endif
endif


CCFLAGS_LOC_OSE= -I/vobs/cello/cls/rtosi_if/include.sparc


CCFLAGS = $(CCFLAGS_LOC_OSE) $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS)
CFLAGS  = $(CCFLAGS_LOC_OSE) $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS) 

LDLIBS_LOC = -L$(NDB_TOP)/lib -L$(OSE_LOC)/sfk-solaris2/lib -L$(OSE_LOC)/sfk-solaris2/krn-solaris2/lib 

LDLIBS_TOP = 

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC)  $(LDFLAGS)



