###
#
# Defines
SHELL       := /bin/sh

C++         := gcc 
CC          := gcc
CXX         := gcc
AR_RCS      := $(PURE) ar rcs 
#SO          := g++ -dynamiclib -Wl,-segprot,__TEXT,rwx,rwx -o 
SO          := gcc -dynamiclib -o 

SHLIBEXT   := dylib

MAKEDEPEND  := gcc -M
PIC         := -fPIC

RPCGENFLAGS := -M -C -N

ETAGS       := etags
CTAGS       := ctags

###
#
# Flags
# 
CCFLAGS_WARNINGS = -Wno-long-long -Wall -Winline #-Werror#-pedantic
# Add these for more warnings -Weffc++ -W
CCFLAGS_TOP = -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -D_BIG_ENDIAN
CXX_FLAGS_TOP = -fno-rtti -felide-constructors -fno-exceptions -fno-omit-fram-pointer
C_FLAGS_TOP += -fno-omit-frame-pointer 

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -O3
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -O3 -g
else
VERSION_FLAGS += -g
endif
endif

CCFLAGS = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(CXXFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS)
CFLAGS  = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(C_FLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS) 

LDFLAGS_TOP =  

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)

#LDFLAGS_LAST = -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic
LDFLAGS_LAST = -lstdc++

