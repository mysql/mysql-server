include .defs.mk

TYPE =			*

NONPIC_ARCHIVE =	N

PIC_ARCHIVE =		Y

ARCHIVE_TARGET =	odbcdriver

SOURCES =		driver.cpp

SOURCES_EXTRA =		$(shell perl Func.pl name .cpp)

include ../Extra.mk
include $(NDB_TOP)/Epilogue.mk
