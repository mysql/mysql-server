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
endif

ifeq ($(NDB_OS), WIN32)
SHLIBEXT  := dll
endif

ifeq ($(NDB_OS), LINUX)
SHLIBEXT  := so
endif

ifeq ($(NDB_OS), SOLARIS)
SHLIBEXT  := so
endif

ifeq ($(NDB_OS), HPUX)
SHLIBEXT := sl
endif

ifeq ($(NDB_OS), MACOSX)
SHLIBEXT := dylib
endif

ifeq ($(NDB_OS), OSE)
SHLIBEXT := so
endif

ifeq ($(NDB_OS), SOFTOSE)
SHLIBEXT := so
endif

ifeq ($(NDB_SCI), Y)
CCFLAGS_TOP += -DHAVE_NDB_SCI
endif

ifeq ($(NDB_SHM), Y)
CCFLAGS_TOP += -DHAVE_NDB_SHM
endif

ifneq ($(findstring OSE, $(NDB_OS)),)
USE_EDITLINE := N
endif
