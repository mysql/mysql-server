include .defs.mk

TYPE := ndbapitest

BIN_TARGET := flex_bench_mysql

# Source files of non-templated classes (.C files)
SOURCES = flex_bench_mysql.cpp

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/../include)
BIN_TARGET_LIBS_DIRS += $(NDB_TOP)/../libmysql_r/.libs
BIN_TARGET_LIBS += z mysqlclient_r

include $(NDB_TOP)/Epilogue.mk

