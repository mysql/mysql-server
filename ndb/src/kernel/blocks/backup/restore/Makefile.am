
ndbtools_PROGRAMS = restore

restore_SOURCES = main.cpp Restore.cpp

LDADD_LOC = $(top_srcdir)/ndb/src/libndbclient.la

include $(top_srcdir)/ndb/config/common.mk.am

INCLUDES += -I.. -I$(top_srcdir)/include  -I$(top_srcdir)/ndb/include  -I$(top_srcdir)/ndb/src/ndbapi  -I$(top_srcdir)/ndb/include/ndbapi -I$(top_srcdir)/ndb/include/util  -I$(top_srcdir)/ndb/include/portlib -I$(top_srcdir)/ndb/include/kernel
