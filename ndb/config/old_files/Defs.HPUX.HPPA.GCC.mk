###
#
# Defines
SHELL      := /bin/sh

C++        := g++
CC         := gcc
AR_RCS     := ar rcs 
SO         := ld -b -o

SHLIBEXT   := sl

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
CCFLAGS_TOP = -DHPUX -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -DNO_COMMAND_HANDLER

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

LDFLAGS_TOP = -lpthread -lnsl -lrt

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)

