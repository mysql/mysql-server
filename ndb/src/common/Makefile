include .defs.mk

LIB_DIRS := \
        portlib		\
        debugger	\
        util		\
        logger

ifneq ($(USE_EDITLINE), N)
LIB_DIRS += editline
endif

DIRS := transporter mgmcommon

include $(NDB_TOP)/Epilogue.mk
