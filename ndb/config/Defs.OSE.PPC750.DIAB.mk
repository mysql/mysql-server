###
#
# Defines
SHELL      := /bin/sh

C++        := dplus 
CC         := dcc
AR_RCS     := $(PURE) ar rcs 
SO         := dar -r

MAKEDEPEND := g++ -M -nostdinc
PIC        := 

RPCGENFLAGS := -MA -C -N

###
#
# Flags
# 
CCFLAGS_INCLUDE = -I/vobs/cello/cls/rtosi_if/include -I/vobs/cello/cls/rtosi_if/include.mp750 -I/vobs/cello/cls/rtosi_if/include.ppc 
CCFLAGS_TOP =  -tPPC750EH -DBIG_ENDIAN -D_BIG_ENDIAN       -DPPC -DPPC750 -DOSE_DELTA -DMP -Xlint -Xforce-prototypes -DINLINE=__inline__  -Xansi -Xsmall-data=0 -Xsmall-const=0 -Xstrings-in-text

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -XO
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -XO -g
else
VERSION_FLAGS += -g
endif
endif

CCFLAGS = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_INCLUDE)
CFLAGS  = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_INCLUDE) 

LDFLAGS_TOP = 

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)



