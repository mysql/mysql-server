###
#
# Defines
SHELL       := /bin/sh

C++         := gcc$(GCC_VERSION)
CC          := gcc$(GCC_VERSION)
AR_RCS      := $(PURE) ar rcs 
SO          := gcc$(GCC_VERSION) -shared -lpthread -o 
#SO          := gcc$(GCC_VERSION) -shared -o 

MAKEDEPEND  := gcc$(GCC_VERSION) -M
#MAKEDEPEND  := gcc$(GCC_VERSION) -M  -nostdinc -nostdinc++
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
CCFLAGS_WARNINGS = -Wno-long-long -Wall -pedantic
else
CCFLAGS_WARNINGS = -Wno-long-long -Wall
endif
# Add these for more warnings -Weffc++ -W
CCFLAGS_TOP = 
#CCFLAGS_TOP = -DSAFE_MUTEX
CCFLAGS_TOP += -fno-rtti -fno-exceptions

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

LINK.cc = $(PURE) $(CC) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)

LDFLAGS_LAST = -lrt -lpthread $(NDB_TOP)/src/common/portlib/gcc.cpp 
#LDFLAGS_LAST = -lrt $(NDB_TOP)/src/common/portlib/gcc.cpp $(NDB_TOP)/../mysys/libmysys.a $(NDB_TOP)/../dbug/libdbug.a $(NDB_TOP)/../regex/libregex.a $(NDB_TOP)/../strings/libmystrings.a -lpthread
