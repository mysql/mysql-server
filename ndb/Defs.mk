include $(NDB_TOP)/config/config.mk
include $(NDB_TOP)/config/Defs.$(NDB_VERSION).mk
include $(NDB_TOP)/config/Defs.$(NDB_OS).$(NDB_ARCH).$(NDB_COMPILER).mk

ifeq ($(NDB_OS), WIN32)
# Windows specific definitions
OBJEXT    := obj
LIBEXT    := lib
LIBPREFIX :=
fixpath   = `cygpath -w $1`
ar_rcs    = lib -out:`cygpath -w $1` $2
link_so   = link -DLL -OUT:`cygpath -w $1` $(WIN_LIBS) $2
#check-odbc = Y
USE_EDITLINE := N
#STRCASECMP is defined in include/portlib/PortDefs.h to _strcmpi
else
#Common definitions for almost all non-Windows environments
OBJEXT    := o
LIBEXT    := a
LIBPREFIX := lib
fixpath   = $1
ar_rcs    = $(AR_RCS) $1 $2
#check-odbc = $(findstring sqlext.h, $(wildcard /usr/include/sqlext.h) $(wildcard /usr/local/include/sqlext.h))
CCFLAGS_TOP += -DHAVE_STRCASECMP

endif

ifeq ($(NDB_OS), WIN32)
CCFLAGS_TOP += -DHAVE_STRDUP
NDB_STRLCPY := Y
NDB_STRLCAT := Y
SHLIBEXT  := dll
endif

ifeq ($(NDB_OS), LINUX)
CCFLAGS_TOP += -DHAVE_STRDUP
NDB_STRLCAT := Y
NDB_STRLCPY := Y
SHLIBEXT  := so
endif

ifeq ($(NDB_OS), SOLARIS)
CCFLAGS_TOP += -DHAVE_STRDUP
NDB_STRLCAT := Y
NDB_STRLCPY := Y
SHLIBEXT  := so
endif

ifeq ($(NDB_OS), HPUX)
CCFLAGS_TOP += -DHAVE_STRDUP
NDB_STRLCAT := Y
NDB_STRLCPY := Y
SHLIBEXT := sl
endif

ifeq ($(NDB_OS), MACOSX)
CCFLAGS_TOP += -DHAVE_STRLCAT
CCFLAGS_TOP += -DHAVE_STRLCAT
CCFLAGS_TOP += -DHAVE_STRLCPY
CCFLAGS_TOP += -DNDBOUT_UINTPTR
SHLIBEXT := dylib
endif

ifeq ($(NDB_OS), OSE)
NDB_STRDUP := Y
NDB_STRLCAT := Y
NDB_STRLCPY := Y
SHLIBEXT := so
endif

ifeq ($(NDB_OS), SOFTOSE)
NDB_STRDUP := Y
NDB_STRLCAT := Y
NDB_STRLCPY := Y
SHLIBEXT := so
endif

ifeq ($(NDB_SCI), Y)
CCFLAGS_TOP += -DHAVE_SCI
endif

ifneq ($(findstring OSE, $(NDB_OS)),)
USE_EDITLINE := N
endif
