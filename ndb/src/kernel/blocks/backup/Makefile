include .defs.mk

TYPE := kernel

#ifneq ($(MYSQLCLUSTER_TOP),)
DIRS := restore
#endif

ARCHIVE_TARGET := backup

SOURCES = Backup.cpp BackupInit.cpp

include $(NDB_TOP)/Epilogue.mk

$(NDB_TOP)/bin/readBackupFile: read.o
	$(C++) -o $@ read.o \
           $(NDB_TOP)/lib/libportlib.a $(NDB_TOP)/lib/libgeneral.a

