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

RPCGENFLAGS := -MA -C -N

###
#
# Flags
# 
CCFLAGS_WARNINGS = -Wno-long-long -Wall -pedantic 
# -Wno-sign-compare Use this flag if you are annoyed with all the warnings
CCFLAGS_TOP = -DSOLARIS -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -DNO_COMMAND_HANDLER

# SOLARIS 6 should use the same settings as SOLARIS7
# if something in the SOLARIS 7 port does not work for SOLARIS 6
# it can be ifdefed using 
# if ! defined NDB_SOLRIS6
CCFLAGS_TOP = -DNDB_SOLARIS

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -O3
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -O3
else
VERSION_FLAGS += -g
endif
endif

CCFLAGS = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS)
CFLAGS  = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS) 

LDFLAGS_TOP = -lpthread -lsocket -lnsl -lposix4

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)


