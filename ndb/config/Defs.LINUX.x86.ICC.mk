###
#
# Defines
SHELL       := /bin/sh

C++         := icc
CC          := icc
AR_RCS      := $(PURE) ar rcs 
SO          := g++$(GCC_VERSION) -shared -lpthread -o

MAKEDEPEND  := g++$(GCC_VERSION) -M
PIC         := -fPIC

RPCGENFLAGS := -M -C -N

ETAGS       := etags
CTAGS       := ctags

###
#
# Flags
# 
# gcc3.3 __THROW problem if -pedantic and -O2
ifeq ($(NDB_VERSION),DEBUG)
CCFLAGS_WARNINGS = 
else
CCFLAGS_WARNINGS = 
endif
# Add these for more warnings -Weffc++ -W
CCFLAGS_TOP = -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS
CCFLAGS_TOP += 

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

LDFLAGS_TOP = -lpthread -lrt

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)
