include .defs.mk

TYPE =			*

A_LIB =			N
PIC_LIB =		Y
SO_LIB =		Y

LIB_TARGET =		NDB_ODBC

LIB_TARGET_ARCHIVES =	$(LIB_DIRS:%=odbc%) NDB_API

# Overide Defs.mk
LDFLAGS_LAST = -lstdc++ -lm

XXX = \
			ndbapi \
			mgmsrvcommon \
			transporter \
			general \
			signaldataprint \
			portlib \
			logger \
			trace

ifeq ($(NDB_OS),WIN32)

LIB_DIRS = \
			handles \
			dictionary \
			codegen \
			executor \
			common

SOURCES += NdbOdbc.cpp
CFLAGS_NdbOdbc.cpp += -I. -I$(call fixpath,driver) 

PIC_ARCHIVE := Y
NONPIC_ARCHIVE := N
ARCHIVE_TARGET := ndb_odbcwin32
LIB_TARGET_ARCHIVES += ndb_odbcwin32

ifeq (RELEASE, $(NDB_VERSION))
WIN_LIBS += /VERSION:2.0x /NODEFAULTLIB:"odbc32" /INCREMENTAL:NO /DLL /SUBSYSTEM:WINDOWS /MACHINE:IX86 /OPT:REF /OPT:ICF /DEF:NdbOdbc.def odbc32.lib odbccp32.lib user32.lib
else
ifeq (RELEASE_TRACE, $(NDB_VERSION))
WIN_LIBS += /VERSION:2.0x /NODEFAULTLIB:"odbc32" /INCREMENTAL:NO /DLL /SUBSYSTEM:WINDOWS /MACHINE:IX86 /OPT:REF /OPT:ICF /DEF:NdbOdbc.def odbc32.lib odbccp32.lib user32.lib
else
WIN_LIBS += /VERSION:2.0x /NODEFAULTLIB:"odbc32" /INCREMENTAL /DLL /DEBUG /SUBSYSTEM:WINDOWS /MACHINE:IX86 /DEF:NdbOdbc.def odbc32.lib odbccp32.lib user32.lib
endif
endif

else

LIB_DIRS = \
			driver \
			handles \
			dictionary \
			codegen \
			executor \
			common

endif

include Extra.mk
include $(NDB_TOP)/Epilogue.mk

# yo

test:
	$(MAKE) -j4
	$(MAKE) -C $(NDB_TOP)/tools/ndbsql
	$(MAKE) -C $(NDB_TOP)/test/odbc/driver tidy
	$(MAKE) -C $(NDB_TOP)/test/odbc/driver
	$(MAKE) -C $(NDB_TOP)/test/ndbapi/testOIBasic
