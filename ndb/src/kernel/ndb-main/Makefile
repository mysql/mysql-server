include .defs.mk

TYPE := kernel

BIN_TARGET := ndb
BIN_TARGET_ARCHIVES := mgmapi \
	cmvmi dbacc dbdict dbdih dblqh dbtc \
        dbtup ndbfs ndbcntr qmgr trix backup dbutil suma grep dbtux \
	transporter   \
	kernel	\
	error \
        trace     \
        signaldataprint \
	mgmsrvcommon \
	portlib      \
        logger       \
	general 

# Source files of non-templated classes (.cpp files)
SOURCES = 			\
        Main.cpp                \
        SimBlockList.cpp

CCFLAGS_LOC = -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/cmvmi) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbacc) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbdict) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbdih) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dblqh) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbtc) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbtup) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/ndbfs) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/missra) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/ndbcntr) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/qmgr) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/trix) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/backup) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbutil) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/suma) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/grep) \
              -I$(call fixpath,$(NDB_TOP)/src/kernel/blocks/dbtux)

include $(NDB_TOP)/Epilogue.mk
