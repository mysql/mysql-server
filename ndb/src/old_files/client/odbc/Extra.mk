# before Epilogue.mk

CCFLAGS_LOC +=		-I..

CCFLAGS_LOC += \
			-I$(call fixpath,$(NDB_TOP)/include/ndbapi) \
			-I$(call fixpath,$(NDB_TOP)/include/util) \
			-I$(call fixpath,$(NDB_TOP)/include/portlib)

ifeq ($(NDB_OS),SOLARIS)

CCFLAGS_LOC +=		-I/usr/local/include

ifeq ($(NDB_COMPILER),GCC)
LIBS_LOC +=		-Wl,-z,text
CCFLAGS_WARNINGS +=	-Wno-unused -Wformat
CCFLAGS_TOP +=		-D__STL_PTHREADS
endif

ifeq ($(NDB_COMPILER),FORTE6)
LIBS_LOC +=		-z text
LIBS_SPEC +=		/usr/lib/libCrun.so.1
endif

LIB_TARGET_LIBS +=	-lthread -lrt

endif
ifneq ($(filter $(NDB_OS), LINUX MACOSX IBMAIX TRU64X),)

LIBS_LOC +=		-Wl,-z,text
CCFLAGS_WARNINGS +=	-Wno-unused -Wformat
GCC_VER :=		$(shell $(CC) --version)

ifeq ($(GCC_VER),2.96)
CCFLAGS_TOP +=		-D__STL_PTHREADS
CCFLAGS_TOP +=		-fmessage-length=0
endif

CCFLAGS_TOP +=		-fno-rtti

LIB_TARGET_LIBS +=	-lpthread

endif

ifeq ($(NDB_OS),WIN32)
ifeq (RELEASE, $(NDB_VERSION))
CCFLAGS_WIN +=		/MT /GR /GS /Zi -D_WINDOWS -D_USRDLL -DNDBODBC_EXPORTS -DNO_COMMAND_HANDLER -D_MBCS -D_WINDLL -U_LIB
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
CCFLAGS_WIN +=		/MT /GR /GS /Zi -D_WINDOWS -D_USRDLL -DNDBODBC_EXPORTS -DNO_COMMAND_HANDLER -D_MBCS -D_WINDLL -U_LIB
else
CCFLAGS_WIN +=		/MT /GR /GS /Zi -D_WINDOWS -D_USRDLL -DNDBODBC_EXPORTS -DNO_COMMAND_HANDLER -D_MBCS -D_WINDLL -U_LIB
endif
endif
endif

CCFLAGS_TOP +=		-DYYDEBUG=0 -fexceptions

CCFLAGS_TOP +=		-DHAVE_LONG_LONG
