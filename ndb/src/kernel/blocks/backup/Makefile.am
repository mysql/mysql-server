
noinst_LIBRARIES = libbackup.a

libbackup_a_SOURCES = Backup.cpp BackupInit.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
