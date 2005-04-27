include .defs.mk

TYPE := *

PIC_ARCHIVE    := Y
NONPIC_ARCHIVE := Y
ARCHIVE_TARGET := ndbclient

A_LIB       := N
SO_LIB      := Y
PIC_LIB     := Y
LIB_TARGET  := ndbclient_extra

LDFLAGS_LAST = -lstdc++ -lm

LIB_TARGET_ARCHIVES := $(ARCHIVE_TARGET) \
                       ndbapi            \
                       mgmapi            \
                       newtonapi         \
                       transporter       \
                       general           \
                       signaldataprint   \
                       mgmsrvcommon      \
                       portlib           \
                       logger            \
                       trace             \
                       odbcdriver        \
                       odbchandles       \
                       odbcdictionary    \
                       odbccodegen       \
                       odbcexecutor      \
                       odbccommon

SOURCES = ndbclient_dummy.cpp

include $(NDB_TOP)/Epilogue.mk

