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
CCFLAGS_WARNINGS = -Wno-long-long -Wall -pedantic -Wno-sign-compare -ansi
CC_FLAGS_OSE = -DUSE_OSEDEF_H -DOSE_DELTA -DOS_DEBUG -DBIG_ENDIAN
CCFLAGS_TOP = $(CC_FLAGS_OSE) $(CC_FLAGS_WARNINGS) -DNDB_STRDUP

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -O3
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -O3 -g
else
VERSION_FLAGS += -g -DOS_DEBUG
endif
endif

OSE_LOC = /opt/as/OSE/OSE4.3.1

CCFLAGS_LOC_OSESTD = -I$(OSE_LOC)/sfk-solaris2/std-include
CCFLAGS_LOC_OSE = -I$(OSE_LOC)/sfk-solaris2/include -I$(OSE_LOC)/sfk-solaris2/krn-solaris2/include -I$(NDB_TOP)/src/env/softose 


CCFLAGS = $(CCFLAGS_LOC_OSE) $(CCFLAGS_LOC_OSESTD) $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS)
CFLAGS  = $(CCFLAGS_LOC_OSE) $(CCFLAGS_LOC_OSESTD) $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) $(CCFLAGS_WARNINGS) 

LDLIBS_LOC = -L$(NDB_TOP)/lib -L$(OSE_LOC)/sfk-solaris2/lib -L$(OSE_LOC)/sfk-solaris2/krn-solaris2/lib 

LDLIBS_TOP = 

LDLIBS_LAST = -lsoftose_env -lsoftose_krn -llnh -lefs -lshell -lfss -ltosv -lrtc -lheap -linetutil -linetapi -lsoftose -lsoftose_env -lsoftose_krn -losepthread -lrtc -lnsl -lsocket -lpthread -lcrt -lm  

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC)  $(LDFLAGS)



