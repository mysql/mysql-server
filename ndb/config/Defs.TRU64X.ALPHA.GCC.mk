###
#
# Defines
SHELL       := /bin/sh

C++         := g++ 
CC          := gcc
AR_RCS      := $(PURE) ar rcs 
SO          := g++ -shared -o

MAKEDEPEND  := g++ -M
PIC         := -fPIC

RPCGENFLAGS := -M -C -N

ETAGS       := etags
CTAGS       := ctags

###
#
# Flags
# 
CCFLAGS_WARNINGS = -Wno-long-long -Wall #-pedantic
# Add these for more warnings -Weffc++ -W
CCFLAGS_TOP = -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS
CCFLAGS_TOP += -fno-rtti

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -O3
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -O3 -g
else
VERSION_FLAGS += -g
endif
endif

CCFLAGS = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS)
CFLAGS  = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS) 

LDFLAGS_TOP = -lpthread -lrt

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)
