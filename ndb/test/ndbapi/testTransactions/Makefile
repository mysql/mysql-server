include .defs.mk

TYPE := ndbapitest

BIN_TARGET := testTransactions

SOURCES := testTransactions.cpp
CFLAGS_testTransactions.cpp := -I$(call fixpath,$(NDB_TOP)/include/kernel)

include $(NDB_TOP)/Epilogue.mk
