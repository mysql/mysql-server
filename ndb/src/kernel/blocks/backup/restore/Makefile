include .defs.mk

TYPE := ndbapi ndbapiclient 

BIN_TARGET := restore
BIN_TARGET_LIBS := 
BIN_TARGET_ARCHIVES := NDB_API general

CCFLAGS_LOC = -I.. -I$(NDB_TOP)/src/ndbapi 

#ifneq ($(MYSQLCLUSTER_TOP),)
#CCFLAGS_LOC +=-I$(MYSQLCLUSTER_TOP)/include -D USE_MYSQL
#LDFLAGS_LOC += -L$(MYSQLCLUSTER_TOP)/libmysql_r/ -lmysqlclient_r
#endif

SOURCES = main.cpp Restore.cpp

include $(NDB_TOP)/Epilogue.mk


