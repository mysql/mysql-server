###
#
# Defines
SHELL      := /bin/sh

C++        := CC
CC         := /opt/as/forte6/SUNWspro/bin/cc
AR_RCS     := $(PURE) CC -xar -o
SO         := CC -G -z text  -o

MAKEDEPEND := CC -xM1
PIC        := -KPIC
ETAGS       := etags
CTAGS       := ctags

RPCGENFLAGS := -MA -C -N

###
#
# Flags

CCFLAGS_TOP = -mt -DSOLARIS -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS

ifneq ($(PURE),)
	CCFLAGS_TOP += -xs
	CCFLAGS_TOP += -DNDB_PURIFY
endif

ifeq (RELEASE, $(NDB_VERSION))
VERSION_FLAGS += -xO3
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
VERSION_FLAGS += -xO3 -g
else
VERSION_FLAGS += -g
endif
endif

CCFLAGS = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS)
CFLAGS  = $(CCFLAGS_LOC) $(CCFLAGS_TOP) $(USER_FLAGS) $(VERSION_FLAGS) 

LDFLAGS_TOP = -L/opt/as/forte6/SUNWspro/WS6/lib -lpthread -lsocket -lnsl -lrt

LDFLAGS = $(LDFLAGS_LOC) $(LDFLAGS_TOP)

LDLIBS = $(LDLIBS_LOC) $(LDLIBS_TOP)

LINK.cc = $(PURE) $(C++) -xildoff $(CCFLAGS) $(LDFLAGS)

LINK.c = $(PURE) $(CC) $(CFLAGS) $(LDFLAGS)




