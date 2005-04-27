include .defs.mk

TYPE := *

PIC_ARCHIVE    := Y
NONPIC_ARCHIVE := Y
ARCHIVE_TARGET := ndbbaseclient

A_LIB       := Y
SO_LIB      := Y
PIC_LIB     := Y
LIB_TARGET  := ndbclient

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
                       trace

SOURCES = ndbbaseclient_dummy.cpp

include $(NDB_TOP)/Epilogue.mk

