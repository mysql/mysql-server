###
#
# Defines
SHELL      := /bin/sh

CXX        := gcc 
C++        := g++ 
CC         := gcc
AR_RCS     := ar rcs 
SO         := gcc -G -o

#GXX_VERSION := $(shell gcc --version | sed -e 's,.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*,\1,1' -e q)

MAKEDEPEND := g++ -M
PIC        := -fPIC

RPCGENFLAGS := -MA -C -N
ETAGS       := etags
CTAGS       := ctags

###
#
# Flags
# 
CCFLAGS_WARNINGS = -Wno-long-long -W -Wall -pedantic 
# -Wno-sign-compare Use this flag if you are annoyed with all the warnings
CCFLAGS_TOP = -DSOLARIS -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -DNO_COMMAND_HANDLER
CCFLAGS_TOP += -fno-rtti

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -O2
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -O2 -g
else
VERSION_FLAGS += -g
endif
endif

CCFLAGS = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS)
CFLAGS  = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS) 

LDFLAGS_TOP =

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(CXX) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)

LDFLAGS_LAST = -lpthread -lsocket -lnsl -lrt -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic

